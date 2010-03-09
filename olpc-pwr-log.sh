#!/bin/sh
# Copyright 2008,2009,2010 One Laptop per Child
# Power logging script 
# Released under GPLv3

# variable naming:  because this file is sourced from within
#  powerd, name conflicts are an issue.  "extern" globals (i.e.,
#  shared with powerd) are all caps.  private globals are caps,
#  but start with "pwr_".  local variables are all lowercase, and
#  are declared local.

pwrlog_read_battery_eeprom()
{
    local addr count

    addr=$1
    count=$2

    if [ -e $BATTERY_INFO/eeprom ]
    then
        dd if=$BATTERY_INFO/eeprom bs=1 \
            skip=$addr count=$count 2>/dev/null |
            od -A n -t x1
    else
        echo NA
    fi
}

pwrlog_write_header()
{ 
    local buildver

    if [ -e /bootpart/boot/olpc_build ]
    then
        buildver=$(< /bootpart/boot/olpc_build )
    elif [ -e /boot/olpc_build ]
    then
        buildver=$(< /boot/olpc_build )
    fi

    cat >$pwr_PWRLOG_LOGFILE <<-EOF
<Header>
powerd_log_ver: $pwr_POWERD_LOG_VERSION
Format: $pwr_POWERD_LOG_FORMAT
DATE: $(date)
ECVER: $(< /ofw/ec-name)
OFWVER: $(< /ofw/openprom/model)
MODEL: $(< /ofw/model)
SERNUM: $pwr_SERNUM
BATTECH: $(< $BATTERY_INFO/technology)
BATMFG: $(< $BATTERY_INFO/manufacturer)
BATSER: $pwr_DS_SERNUM
BUILD: $buildver
CHGCNT: $(pwrlog_read_battery_eeprom 74 2)
CHGSOC: $(pwrlog_read_battery_eeprom 76 1)
DISCNT: $(pwrlog_read_battery_eeprom 77 2)
DISSOC: $(pwrlog_read_battery_eeprom 79 1)
XOVER: $XO
</Header>
<StartData>
EOF
    # no more:
    # MFG_SER: $(pwrlog_read_battery_eeprom 64 5)
}

# convert a number into 2's complement
pwrlog_conv_2s_comp()
{
    if [ $1 -gt 32767 ]
    then
        echo $(( 0 - (65535-$1+1) ))
    else
        echo $1
    fi
}

pwrlog_get_acr()
{
    local acr_temp

    acr_temp=$(< $BATTERY_INFO/$pwr_ACR_PROP )
    test $XO = "1" && acr_temp=$(pwrlog_conv_2s_comp ${acr_temp:-0})
    echo ${acr_temp:-0}
}

pwrlog_write_data()
{
    local caplevel volt curr temp acr stat reason

    reason=${1:-}

    caplevel=$(< $BATTERY_INFO/$pwr_CAPACITY )
    volt=$(< $BATTERY_INFO/voltage_avg)
    curr=$(< $BATTERY_INFO/current_avg)
    temp=$(< $BATTERY_INFO/temp)
    stat=$(< $BATTERY_INFO/status)

    acr=$(pwrlog_get_acr)

    echo "$(seconds),$caplevel,$volt,$curr,$temp,$acr,$stat",$reason \
        >> $pwr_PWRLOG_LOGFILE 
}

pwrlog_filedate()
{
    date "+%y%m%d_%H%M%S"
}

pwrlog_filename()
{
    echo /var/log/pwr-$pwr_SERNUM-$pwr_DS_SERNUM-$1.csv 
}

pwrlog_take_reading()
{
    local now reason fdate newfile

    reason=${1:-}
    
    now=$(seconds)
    newfile=;

    case $reason in
    *-event)  # "soft" events
        if (( now - ${pwr_LASTLOGTIME:-0} < $pwr_LOG_INTERVAL ))
        then
            return
        fi
        ;;
    esac

    pwr_LASTLOGTIME=$now

    # if we started with a battery, and someone pulls it, don't
    # switch logs because they'll probably reinsert the same one.
    # if we started without a battery, we'll switch files when
    # they insert, to get the battery s/n in the name.
    if [ $(< $BATTERY_INFO/present ) -eq 1 ]
    then
        pwr_DS_SERNUM=$(< $BATTERY_INFO/serial_number )
        if [ "${pwr_PREV_DS_SERNUM:-}" != "$pwr_DS_SERNUM" ]
        then
            fdate=$(pwrlog_filedate)
            pwr_PWRLOG_LOGFILE=$(pwrlog_filename $fdate)
        fi
        pwr_PREV_DS_SERNUM=$pwr_DS_SERNUM
    else
        if [ ! "$pwr_DS_SERNUM" ]
        then
            pwr_DS_SERNUM=nobattery
            fdate=$(pwrlog_filedate)
        fi
        pwr_PWRLOG_LOGFILE=$(pwrlog_filename $fdate)
    fi

    # if the file we may have been writing to doesn't exist
    # then create a new one.  always create logs with a current
    # timestamp.
    if [ ! -e $pwr_PWRLOG_LOGFILE ]
    then
        fdate=$(pwrlog_filedate)
        pwr_PWRLOG_LOGFILE=$(pwrlog_filename $fdate)
        pwrlog_write_header
        newfile=true
    fi
    pwrlog_write_data $reason

    case $reason in
    resume) # don't delay this
        ;;
    shutdown)
        pwrlog_logcopy
        ;;
    *)
        # copy logs out of /var every 30 minutes
        if (( now - ${pwr_LASTCOPYTIME:-0} >= $pwr_LOGCOPY_MINUTES * 60 ))
        then
            test "$newfile" || pwrlog_logcopy
            pwr_LASTCOPYTIME=$now
        fi
        ;;
    esac
}

pwrlog_init()
{

    pwr_LOG_INTERVAL=${1:-60}
    pwr_LOGCOPY_MINUTES=30

    pwr_POWERD_LOG_VERSION="0.1"
    pwr_POWERD_LOG_FORMAT="1"

    if [ ! "$BATTERY_INFO" ]
    then
        BATTERY_INFO=/sys/class/power_supply/olpc-battery
    fi

    if [ -e $BATTERY_INFO/capacity_level ]
    then
        pwr_CAPACITY=capacity_level
    else
        pwr_CAPACITY=capacity
    fi

    if [ -e $BATTERY_INFO/charge_counter ]
    then
        pwr_ACR_PROP="charge_counter"
    else
        pwr_ACR_PROP="accum_current"
    fi

    pwr_SERNUM=$(< /ofw/serial-number )

}


# feed this the wall clock time in seconds you wish to delay
# It will spin until that time has passed.  If the system is suspeneded
# it may sleep more.
pwrlog_wallclock_delay()
{
    local wall_period date1 expire

    wall_period=5
    date1=$(seconds)
    expire=$((date1+$1))
    while [ $(seconds) -lt $expire ]; do
        sleep $wall_period
    done
}

pwrlog_logcopy()
{
        if [ ! -d "$PWRLOGDIR" ]
        then
            echo "$0: Bad PWRLOGDIR: '$PWRLOGDIR'" >&2
            return 1
        fi

        # anything to copy?
        test "$(echo /var/log/pwr-*.csv 2>/dev/null)" || return 0

        # copy any likely logs
        cp /var/log/pwr-*.csv $PWRLOGDIR || return 0
        
        # assume that only the most recently written log is still
        # active, and that others have been preserved
        oldfiles="$(ls -t /var/log/pwr-*.csv | sed 1d)"
        test "$oldfiles" && rm -f $oldfiles
}

# powerd sets a variable to indicate we've been source'd
if [ ! "$pwrlog_inside_powerd" ]
then

    if [ "${1:-}" = logcopy ]
    then
        pwrlog_logcopy
        exit
    fi

    # for standalone testing
    if [ -e /sys/class/dmi/id/product_version ]
    then
        XO=$(< /sys/class/dmi/id/product_version )
    else
        XO="1"
    fi

    seconds()
    {
        echo $SECONDS
    }

    pwrlog_loop()
    {
        while :
        do 
            pwrlog_take_reading pwrlog-loop
            pwrlog_wallclock_delay $DELAY
            # sleep $DELAY
        done    
    }

    DELAY=20
    SECONDS=$(date +%s)
    pwrlog_init $DELAY

    pwrlog_loop

fi
