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
    local buildver comment

    comment="${*:-}"

    if [ -e /bootpart/boot/olpc_build ]
    then
        buildver=$(< /bootpart/boot/olpc_build )
    elif [ -e /boot/olpc_build ]
    then
        buildver=$(< /boot/olpc_build )
    fi

    cat >$pwr_PWRLOG_LOGFILE <<-EOF
powerd_log_ver: $pwr_POWERD_LOG_VERSION
Format: $pwr_POWERD_LOG_FORMAT
DATE: $(date)
COMMENT: $comment
ECVER: $(< /ofw/ec-name)
OFWVER: $(< /ofw/openprom/model)
MODEL: $(< /ofw/model)
SERNUM: $pwr_SERNUM
BATTECH: $(< $BATTERY_INFO/technology)
BATMFG: $(< $BATTERY_INFO/manufacturer)
BATSER: $(< $BATTERY_INFO/serial_number )
BUILD: $buildver
CHGCNT: $(pwrlog_read_battery_eeprom 74 2)
CHGSOC: $(pwrlog_read_battery_eeprom 76 1)
DISCNT: $(pwrlog_read_battery_eeprom 77 2)
DISSOC: $(pwrlog_read_battery_eeprom 79 1)
XOVER: $XO
<StartData>
EOF
    # no more:
    # MFG_SER: $(pwrlog_read_battery_eeprom 64 5)
}

# convert a number into 2's complement
pwrlog_conv_2s_comp()
{
    if (( $1 > 32767 ))
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
    local capacity volt curr temp acr stat reason

    reason=${1:-}

    capacity=$(< $BATTERY_INFO/capacity )
    volt=$(< $BATTERY_INFO/voltage_avg)
    curr=$(< $BATTERY_INFO/current_avg)
    temp=$(< $BATTERY_INFO/temp)
    stat=$(< $BATTERY_INFO/status)

    acr=$(pwrlog_get_acr)

    echo "$(seconds),$capacity,$volt,$curr,$temp,$acr,$stat",$reason \
        >> $pwr_PWRLOG_LOGFILE 
}

pwrlog_filedate()
{
    date "+%y%m%d_%H%M%S"
}

pwrlog_filename()
{
    echo /var/log/pwr-$pwr_SERNUM-$(pwrlog_filedate).csv 
}

pwrlog_take_reading()
{
    local now reason newfile battery_present battery_changed
    reason=${1:-}
    shift  # the rest of the arguments may be used below
    
    newfile=;
    battery_changed=;

    # start a new log evertime someone inserts a battery
    battery_present=$(< $BATTERY_INFO/present )
    if [ "$battery_present" != "${pwr_BATTERY_WAS_PRESENT:=$battery_present}" ]
    then
        if [ "$battery_present" = 1 ]
        then
            pwr_BATTERY_WAS_PRESENT=1
            newfile=true
            battery_changed=-batinserted
        else
            battery_changed=-batremoved
        fi
        pwr_BATTERY_WAS_PRESENT="$battery_present" 
    fi

    now=$(seconds)

    if [ ! "$battery_changed" ]
    then
        case $reason in
        new-pwrlog-event)
            ;;
        *-event)  # "soft" events -- rate limit them
            if (( now - ${pwr_LASTLOGTIME:-0} < $pwr_LOG_INTERVAL ))
            then
                return
            fi
            ;;
        esac
    fi

    pwr_LASTLOGTIME=$now

    if [ "$battery_present" = 1 -o "$battery_changed" ]
    then
        # if the file we may have been writing to doesn't exist
        # then create a new one.  always create logs with a current
        # timestamp.
        if [ $reason = "new-pwrlog-event" -o "$newfile" ]
        then
            pwr_PWRLOG_LOGFILE=$(pwrlog_filename)
            pwrlog_write_header "${*:-}"
        fi
        pwrlog_write_data $reason$battery_changed

    fi

    case $reason in
    resume) # don't delay this
        ;;
    shutdown)
        pwrlog_logcopy
        ;;
    *)
        # copy logs out of /var every 30 minutes
        if [ "$battery_changed" ] || \
            (( now - ${pwr_LASTCOPYTIME:-0} >= $pwr_LOGCOPY_MINUTES * 60 ))
        then
            if [ ! "$newfile" ]
            then
                pwrlog_logcopy
            fi
            pwr_LASTCOPYTIME=$now
        fi
        ;;
    esac
}

pwrlog_init()
{

    pwr_LOG_INTERVAL=${1:-60}
    pwr_LOG_DIR=${2:-}
    pwr_MAX_LOG_SIZE=${3:-50}             # Kbytes
    pwr_MAX_LOGDIR_SIZE=${4:-5120}        # Kbytes

    pwr_LOGCOPY_MINUTES=30

    pwr_POWERD_LOG_VERSION="0.2"
    pwr_POWERD_LOG_FORMAT="2"

    if [ ! "$BATTERY_INFO" ]  # usually set by powerd
    then
        BATTERY_INFO=/sys/class/power_supply/olpc-battery
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
    while (( $(seconds) < $expire ))
    do
        sleep $wall_period
    done
}

pwrlog_logcopy()
{
    local size oldfiles log copyall

    copyall="${1:-}"

    if [ ! -d "$pwr_LOG_DIR" ]
    then
        echo "$0: Bad or missing log directory: '$pwr_LOG_DIR'" >&2
        return 1
    fi

    # anything to copy?
    test "$(echo /var/log/pwr-*.csv 2>/dev/null)" || return 0

    # copy any likely logs
    cp /var/log/pwr-*.csv $pwr_LOG_DIR || return 0

    # assume that only the most recently written log is still
    # active, and that others have been preserved
    if [ "$copyall" ]
    then
        oldfiles="$(ls -t /var/log/pwr-*.csv)"
    else
        oldfiles="$(ls -t /var/log/pwr-*.csv | sed 1d)"
    fi
    test "$oldfiles" && rm -f $oldfiles

    # keep up to 10M. after that, delete oldest first
    size=( $(du -sk $pwr_LOG_DIR) )   # size in 1K blocks
    if (( ${size[0]} > $pwr_MAX_LOGDIR_SIZE ))    # bigger than 10Mb?
    then
        for log in $(ls -rt $pwr_LOG_DIR)
        do
            rm -f $pwr_LOG_DIR/$log
            size=( $(du -sk $pwr_LOG_DIR) )   # blocks
            (( ${size[0]} < $pwr_MAX_LOGDIR_SIZE )) && break
        done
    fi

    if [ "${pwr_PWRLOG_LOGFILE:-}" ]
    then
        # remove current log file if it's too big.  (it's been copied)
        if (( $(stat -c%s $pwr_PWRLOG_LOGFILE ) > $pwr_MAX_LOG_SIZE * 1024))
        then
            rm -f $pwr_PWRLOG_LOGFILE 
        fi
    fi
        
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
    LOGDIR="/home/olpc/olpc-pwr-logs"

    mkdir -p $LOGDIR
    chown olpc:olpc $LOGDIR

    pwrlog_init $DELAY $LOGDIR

    pwrlog_loop

fi
