/*
 *
 * olpc-switchd.c -- monitors the XO laptops hardware switches
 * (power, lid, ebook) and generates a common set of events
 * when they change.  optionally polls the state of AC connection
 * and battery level, and generates events for those as well.
 *
 * Copyright (C) 2009, Paul G Fox
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be
 * useful, but WITHOUT ANY WARRANTY; without even the implied
 * warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 * PURPOSE.  See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program; if not, write to the Free
 * Software Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139,
 * USA.
 *
 */


#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <ctype.h>
#include <signal.h>
#include <wait.h>
#include <syslog.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <linux/input.h>
#include <time.h>
#include <utime.h>

/* are we still in the foreground? */
int daemonized;

/* log to syslog (instead of stderr) */
int logtosyslog;

/* higher values for more debug */
int debug;

/* suppress any actual tranmission or injection of data */
int noxmit;

/* output event fifo */
char *output_fifo;

/* sysactive path -- touched at most once every 5 seconds for switch activity */
char *sysactive_path;

/* how often to poll AC and battery state */
int pollinterval;

/* how many pollintervals should pass before we send a timer event */
int timeout_polls;

extern char *optarg;
extern int optind, opterr, optopt;

/* output event fifo */
int fifo_fd = -1;

/* input event devices */
int pwr_fd = -1;
int lid_fd = -1;
int ebk_fd = -1;
int acpwr_fd = -1;
int got_switches = 0;
#define SEARCH_SWITCHES 10

int maxfd = -1;  /* for select */

/* bit array ops */
#define bits2bytes(x) ((((x)-1)/8)+1)
#define test_bit(bit, array) ( array[(bit)/8] & (1 << (bit)%8) )

#define MAX(a, b) (((a) > (b)) ? (a) : (b))

typedef struct input_id input_id_t;


char *me;

void
usage(void)
{
    fprintf(stderr,
        "usage: %s [options]\n"
        "   '-F <fifoname>'  Gives the name of the output fifo\n"
        "\n"
        " Daemon options:\n"
        "   '-f' to keep program in foreground.\n"
        "   '-l' use syslog, rather than stderr, for messages.\n"
        "   '-d' for debugging (repeat for more verbosity).\n"
        "   '-X' don't actually pass on received keystrokes (for debug).\n"
        "\n"
        "   '-p N' If set, the presence of external power and the condition\n"
        "        of the battery will be polled every N seconds.\n"
        "   '-t T' If set, will send a 'timer' event every T * N seconds\n"
        "   '-A <activity_indicator>'  Gives path whose modification time\n"
        "        will indicate (approximate) recent switch/button activity.\n"
        "        (Touched at most once every 5 seconds)\n"
        "(olpc-switchd version %d)\n"
        , me, VERSION);
    exit(1);
}

void
report(const char *fmt, ...)
{
    va_list ap;

    va_start(ap, fmt);
    if (logtosyslog && debug <= 1) {
        vsyslog(LOG_NOTICE, fmt, ap);
    } else {
        fprintf(stderr, "%s: ", me);
        vfprintf(stderr, fmt, ap);
        fputc('\n', stderr);
    }
}

void
dbg(int level, const char *fmt, ...)
{
    va_list ap;

    if (debug < level) return;

    va_start(ap, fmt);
    if (logtosyslog)
        vsyslog(LOG_NOTICE, fmt, ap);

    fputc(' ', stderr);
    vfprintf(stderr, fmt, ap);
    fputc('\n', stderr);
}

__attribute__((noreturn)) void
die(const char *fmt, ...)
{
    va_list ap;

    va_start(ap, fmt);
    if (logtosyslog && debug <= 1) {
        vsyslog(LOG_ERR, fmt, ap);
        syslog(LOG_ERR, "exiting -- %m");
    } else {
        fprintf(stderr, "%s: ", me);
        vfprintf(stderr, fmt, ap);
        fprintf(stderr, " - %s", strerror(errno));
        fputc('\n', stderr);
    }
    exit(1);
}

#ifdef commentary
/*
 pertinent output of "grep ^ /sys/class/input/ * /name"
 on XO-1.5:
    /sys/class/input/input0/name:OLPC AC power jack
    /sys/class/input/input1/name:Power Button
    /sys/class/input/input2/name:Lid Switch
    /sys/class/input/input4/name:EBook Switch
 on XO-1:
    /sys/class/input/input0/name:OLPC PM
    /sys/class/input/input1/name:OLPC lid switch
    /sys/class/input/input2/name:OLPC ebook switch
    /sys/class/input/input3/name:OLPC AC power jack
*/
#endif

void strtolower(char *s)
{
    while (*s) {
        *s = tolower(*s);
        s++;
    }
}

int
setup_input()
{
    int i, ret;
    int dfd;
    char devname[128];
    char name[32];

    for (i = 0; i < SEARCH_SWITCHES && got_switches < 4; i++) {

        snprintf(devname, sizeof(devname), "/dev/input/event%d", i);
        if ((dfd = open(devname, O_RDONLY)) < 0)
            continue;

        if(ioctl(dfd, EVIOCGNAME(sizeof(name)), name) < 0) {
            report("failed ioctl EVIOCGNAME on %d", i);
            close(dfd);
            continue;
        }

        strtolower(name);

        if (strstr(name, "olpc ac power"))
            acpwr_fd = dfd;
        else if (strstr(name, "olpc pm"))  // XO-1
            pwr_fd = dfd;
        else if (strstr(name, "power button"))  // XO-1.5
            pwr_fd = dfd;
        else if (strstr(name, "lid switch"))
            lid_fd = dfd;
        else if (strstr(name, "ebook switch"))
            ebk_fd = dfd;
        else
            continue;

        got_switches++;
        maxfd = MAX(maxfd, dfd);

    }

    /* must find at least power button and lid switches */
    ret = -1;
    if (pwr_fd == -1)
        report("didn't find power button");
    else if (lid_fd == -1)
        report("didn't find lid switch");
    else {
        report("found %s %d switches",
                (got_switches == 4) ? "all":"just", got_switches);
        ret = 0;
    }

    return ret;
}

void
indicate_activity(void)
{
    static time_t lastactivity;
    time_t now;

    now = time(0);

    if (now - lastactivity < 5)
        return;

    if (utime(sysactive_path, NULL)) {
        if (errno == ENOENT) {  /* try to create it */
            int fd = open(sysactive_path, O_RDWR | O_CREAT,
                      S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH |
                      S_IWOTH);
            if (fd >= 0)
                close(fd);
        } else {
            static int reported = 0;
            if (!reported) {
                report("touch of %s failed: %s",
                    sysactive_path, strerror(errno));
                reported = 1;
            }
        }
    }
    lastactivity = now;
}

void
send_event(char *evt, int seconds, char *extra)
{
    char evtbuf[128];
    char *space;
    int n;

    space = extra ? " " : "";
    if (!extra) extra = "";
    n = snprintf(evtbuf, 128, "%s %d%s%s\n", evt, seconds, space, extra);

    if (fifo_fd < 0)
        fifo_fd = open(output_fifo, O_WRONLY|O_NONBLOCK);
    if (fifo_fd < 0)
        return;

    if (write(fifo_fd, evtbuf, n) < 0) {
        if (errno != EPIPE)
            die("fifo write failed");
        dbg(1, "got write signal");
    } else {
        dbg(1, "sending %s", evtbuf);
    }

    close(fifo_fd);
    fifo_fd = -1;
}

long
round_secs(struct input_event *ev)
{
    return ev->time.tv_sec + ((ev->time.tv_usec > 500000) ? 1 : 0);
}

void
power_button_event()
{
    struct input_event ev[1];

    if (read(pwr_fd, ev, sizeof(ev)) != sizeof(ev))
        die("bad read from power button");

    dbg(3, "pwr: ev sec %d usec %d type %d code %d value %d",
        ev->time.tv_sec, ev->time.tv_usec,
        ev->type, ev->code, ev->value);

    if (ev->type == EV_KEY && ev->code == KEY_POWER && ev->value == 1)
        send_event("powerbutton", round_secs(ev), 0);
}

void
lid_event()
{
    struct input_event ev[1];

    if (read(lid_fd, ev, sizeof(ev)) != sizeof(ev))
        die("bad read from lid switch");

    dbg(3, "lid: ev sec %d usec %d type %d code %d value %d",
        ev->time.tv_sec, ev->time.tv_usec,
        ev->type, ev->code, ev->value);

    if (ev->type == EV_SW && ev->code == SW_LID) {
        if (ev->value)
            send_event("lidclose", round_secs(ev), 0);
        else
            send_event("lidopen", round_secs(ev), 0);
    }
}

void
ebook_event()
{
    struct input_event ev[1];

    if (read(ebk_fd, ev, sizeof(ev)) != sizeof(ev))
        die("bad read from ebook switch");

    dbg(3, "ebk: ev sec %d usec %d type %d code %d value %d",
        ev->time.tv_sec, ev->time.tv_usec,
        ev->type, ev->code, ev->value);

    if (ev->type == EV_SW && ev->code == SW_TABLET_MODE) {
        if (ev->value)
            send_event("ebookclose", round_secs(ev), 0);
        else
            send_event("ebookopen", round_secs(ev), 0);
    }
}

void
acpwr_event()
{
    struct input_event ev[1];

    if (read(acpwr_fd, ev, sizeof(ev)) != sizeof(ev))
        die("bad read from AC power jack");

    dbg(3, "acpwr: ev sec %d usec %d type %d code %d value %d",
        ev->time.tv_sec, ev->time.tv_usec,
        ev->type, ev->code, ev->value);

#ifndef SW_DOCK
#define SW_DOCK 5
#endif
    if (ev->type == EV_SW && ev->code == SW_DOCK) {
        if (ev->value)
            send_event("ac-online", round_secs(ev), 0);
        else
            send_event("ac-offline", round_secs(ev), 0);
    }
}

int
read_ac_online(void)
{
    int fd, r;
    char buf[4];
    fd = open("/sys/class/power_supply/olpc-ac/online", O_RDONLY);
    if (fd < 0)
        return 0;

    r = read(fd, buf, 1);
    close(fd);

    if (r != 1 || (buf[0] != '0' && buf[0] != '1')) {
        return 0;
    }

    return buf[0] - '0';
}

int
read_battery_capacity(void)
{
    int fd, r;
    char buf[4];
    fd = open("/sys/class/power_supply/olpc-battery/capacity", O_RDONLY);
    if (fd < 0)
        return 0;

    r = read(fd, buf, 2);
    close(fd);
    if (r < 1 || (buf[0] < '0' || buf[0] > '9')) {
        return 0;
    }

    buf[3] = '\0';

    return atoi(buf);
}

char *
read_battery_status(void)
{
    int fd, r;
    static char buf[40];
    fd = open("/sys/class/power_supply/olpc-battery/capacity", O_RDONLY);
    if (fd < 0)
        return 0;

    r = read(fd, buf, sizeof(buf));
    close(fd);
    if (r < 1)
        buf[0] = '\0';
    else
        buf[sizeof(buf)-1] = '\0';

    return buf;
}

int
poll_power_sources(void)
{
    int online, capacity;
    static int was_online = -1;
    static int was_capacity = -1;
    static char was_status[20];
    char *status;
    int sent = 0;

    /* if we don't have an AC jack input device, poll it here */
    if (acpwr_fd < 0) {
        online = read_ac_online();
        if (was_online != online) {
            send_event(online ? "ac-online" : "ac-offline", time(0), 0);
            was_online = online;
            sent = 1;
        }
    }

    capacity = read_battery_capacity();
    status = read_battery_status();
    if (was_capacity != capacity || strcmp(status, was_status) != 0) {
        char evbuf[32];
        was_capacity = capacity;
        strcpy(was_status, status);
        snprintf(evbuf, 32, "%d", capacity);
        send_event("battery", time(0), evbuf);
        sent = 1;
    }

    return sent;
}

void
data_loop(void)
{
    fd_set inputs, errors;
    struct timeval tv;
    struct timeval *tvp;
    int r;

    while (1) {
        FD_ZERO(&inputs);
        FD_SET(pwr_fd, &inputs);
        FD_SET(lid_fd, &inputs);
        FD_SET(ebk_fd, &inputs);
        if (acpwr_fd >= 0)
            FD_SET(acpwr_fd, &inputs);

        FD_ZERO(&errors);
        FD_SET(pwr_fd, &errors);
        FD_SET(lid_fd, &errors);
        FD_SET(ebk_fd, &errors);
        if (acpwr_fd >= 0)
            FD_SET(acpwr_fd, &errors);

        if (pollinterval) {
            tv.tv_sec = pollinterval;
            tv.tv_usec = 0;
            tvp = &tv;
        } else {
            tvp = 0;
        }

        r = select(maxfd+1, &inputs, NULL, &errors, tvp);
        if (r < 0)
            die("select failed");

        if (!poll_power_sources() && r == 0 && timeout_polls) {
            static int tp = 2;

            if (--tp <= 0) {
                char timeout[16];
                snprintf(timeout, sizeof(timeout), "%d",
                    pollinterval * timeout_polls);
                send_event("timer", time(0), timeout);
                tp = timeout_polls;
            }
        }

        if (r > 0) {
            if (FD_ISSET(pwr_fd, &errors))
                die("select reports error on power button");
            if (FD_ISSET(lid_fd, &errors))
                die("select reports error on lid switch");
            if (FD_ISSET(ebk_fd, &errors))
                die("select reports error on ebook switch");
            if (acpwr_fd >= 0 && FD_ISSET(acpwr_fd, &errors))
                die("select reports error on ac power jack");

            if (FD_ISSET(pwr_fd, &inputs))
                power_button_event();
            if (FD_ISSET(lid_fd, &inputs))
                lid_event();
            if (FD_ISSET(ebk_fd, &inputs))
                ebook_event();
            if (acpwr_fd >= 0 && FD_ISSET(acpwr_fd, &inputs))
                acpwr_event();

            if (sysactive_path)
                indicate_activity();
        }
    }
}

void
sighandler(int sig)
{
    die("got signal %d", sig);
}

int
main(int argc, char *argv[])
{
    int foreground = 0;
    char *cp, *eargp;
    int c;

    me = argv[0];
    cp = strrchr(argv[0], '/');
    if (cp) me = cp + 1;

    while ((c = getopt(argc, argv, "fldXp:t:F:A:")) != -1) {
        switch (c) {

        /* daemon options */
        case 'f':
            foreground = 1;
            break;
        case 'l':
            logtosyslog = 1;
            break;
        case 'd':
            debug++;
            break;
        case 'X':
            noxmit = 1;
            break;

        case 'p':
            pollinterval = strtol(optarg, &eargp, 10);
            if (*eargp != '\0' || pollinterval <= 0)
                usage();
            break;

        case 't':
            timeout_polls = strtol(optarg, &eargp, 10); 
            if (*eargp != '\0' || pollinterval <= 0)
                usage();
            break;

        case 'F':
            output_fifo = optarg;
            break;

        case 'A':
            sysactive_path = optarg;
            break;

        default:
            usage();
            break;
        }
    }

    if (optind < argc) {
        report("found non-option argument(s)");
        usage();
    }

    if (!output_fifo) {
        report("output fifo is required");
        usage();
    }

    report("starting version %d", VERSION);
    if (pollinterval)
        report("will poll power sources every %d seconds", pollinterval);
    else
        report("not polling power sources", pollinterval);

    if (setup_input() < 0)
        die("unable to find all input devices");

    signal(SIGTERM, sighandler);
    signal(SIGHUP, sighandler);
    signal(SIGINT, sighandler);
    signal(SIGQUIT, sighandler);
    signal(SIGABRT, sighandler);
    signal(SIGUSR1, sighandler);
    signal(SIGUSR2, sighandler);

    signal(SIGPIPE, SIG_IGN);

    if (!foreground) {
        if (daemon(0, 0) < 0)
            die("failed to daemonize");
        daemonized = 1;
    }

    data_loop();

    return 0;
}
