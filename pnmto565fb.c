/*
 * pnmto565fb.c -- read a PNM (actually just PGM or PPM) file,
 *  and write it to a 16 bit "565" frame buffer.  multiple
 *  images can be specified, and they will be cycled either
 *  when a specified delay expires, or a SIGUSR1 is received.
 *  the program will clean up and redisplay the VT on which
 *  it was invoked on most signals.  SIGTERM is ignored, since
 *  the immediate application is to present a splash screen
 *  during shutdown, and we wish it to stick around for a bit
 *  while shutdown commences.
 *
 * Copyright (C) 2009, Paul G Fox
 *
 * Some code was borrowed from the ppmtofb package, which is also
 * GPLed, and bears the following notices:
 *  (c) Copyright 1996 by Geert Uytterhoeven
 *  (c) Copyright 1996-2000 by Chris Lawrence
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
 */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>
#include <getopt.h>
#include <stdarg.h>
#include <signal.h>
#include <setjmp.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <linux/kd.h>
#include <linux/vt.h>
#include <linux/fb.h>

#define MAX_FB_WIDTH 1200
#define MAX_FB_HEIGHT 900

char *prog;
extern char *optarg;
extern int optind, opterr, optopt;

int filler;
int dcon;

char *devfb = "/dev/fb";

void vt_deinit(void);
int consolefd = -1;
int orig_vt = -1;

sigjmp_buf nxt_jmpbuf;

__attribute__((noreturn)) void
die(const char *fmt, ...)
{
    va_list ap;

    fflush(stdout);
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);

    vt_deinit();

    exit(1);
}

void
warn(const char *fmt, ...)
{
    va_list ap;

    fflush(stdout);
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);
}


void
sighandler(int signo)
{
    signal(signo, SIG_IGN);
    die("Caught signal %d. Exiting\n", signo);
}

void
signextimage(int signo)
{
    longjmp(nxt_jmpbuf, 1);
}

void
dcon_control(int freeze)
{
    static int fd = -1;

    if (!dcon) return;

    if (fd < 0) {
        fd = open("/sys/devices/platform/dcon/freeze", O_WRONLY);
        if (fd < 0) return;
    }

    if (write(fd, freeze ? "1\n" : "0\n", 2) != 2)
        /* suppress warning */ ;
    usleep(50000);
}

void dcon_freeze(void)
{
    dcon_control(1);
}

void dcon_thaw(void)
{
    dcon_control(0);
}

void chvt(int num)
{
    if (ioctl(consolefd,VT_ACTIVATE,num)) {
        perror("VT_ACTIVATE");
        exit(1);
    }
}

int fgconsole(void)
{
    struct vt_stat vtstat;
    if (ioctl(consolefd, VT_GETSTATE, &vtstat)) {
        perror("VT_GETSTATE");
        exit(1);
    }
    return vtstat.v_active;
}

void switch_to_vt(int num)
{
    int i = 5;

    do {
        chvt(num);
        if (fgconsole() == num) return;
        usleep(100000);
    } while (--i > 0);

    warn("VT change timed out\n");
}


void
vt_deinit(void)
{
    struct vt_mode vterm;

    if (consolefd) {
        ioctl(consolefd, KDSETMODE, KD_TEXT);
        if (ioctl(consolefd, VT_GETMODE, &vterm) != -1) {
            vterm.mode = VT_AUTO;
            ioctl(consolefd, VT_SETMODE, &vterm);
        }
        if (orig_vt >= 0) {
            switch_to_vt(orig_vt);
            orig_vt = -1;
        }
        close(consolefd);
        consolefd = -1;
    }
    dcon_thaw();
}

void
vt_setup(void)
{
    int i, fd, vtno;
    char vtname[11];
    struct vt_mode vterm;

    if ((fd = open("/dev/tty0", O_WRONLY, 0)) < 0)
        die("Cannot open /dev/tty0: %s\n", strerror(errno));

    if (ioctl(fd, VT_OPENQRY, &vtno) < 0 || vtno == -1)
        die("Cannot find a free VT\n");

    close(fd);

    sprintf(vtname, "/dev/tty%d", vtno);        /* /dev/tty1-64 */
    if ((consolefd = open(vtname, O_RDWR | O_NDELAY, 0)) < 0)
        die("Cannot open %s: %s\n", vtname, strerror(errno));

    /*
     * Linux doesn't switch to an active vt after the last close of a vt,
     * so we do this ourselves by remembering which is active now.
     */
    orig_vt = fgconsole();

    /*
     * Detach from the controlling tty to avoid char loss
     */
    if ((i = open("/dev/tty", O_RDWR)) >= 0) {
        ioctl(i, TIOCNOTTY, 0);
        close(i);
    }

    /*
     * now get the VT
     */
    switch_to_vt(vtno);

    if (ioctl(consolefd, VT_GETMODE, &vterm) < 0)
        die("ioctl VT_GETMODE: %s\n", strerror(errno));

    vterm.mode = VT_PROCESS;
    vterm.relsig = 0;
    vterm.acqsig = 0;
    if (ioctl(consolefd, VT_SETMODE, &vterm) < 0)
        die("ioctl VT_SETMODE: %s\n", strerror(errno));

    /*
     *  switch to graphics mode
     */
    if (ioctl(consolefd, KDSETMODE, KD_GRAPHICS) < 0)
        die("ioctl KDSETMODE KD_GRAPHICS: %s\n", strerror(errno));
}

void
usage(void)
{
    fprintf(stderr,
        "usage: %s [-s SECS] [-d ] [ -f <devfb-name> ] pnmfile ...\n"
        " writes 565 data from successive images to /dev/fb)\n"
        "    -d to freeze the XO DCON while an image is being painted\n"
        "    -s SECS  to sleep between images\n"
        "    -f /dev/fb0 to open /dev/fb0 (defaults to /dev/fb)\n",
        prog);
    exit(1);
}

inline unsigned short
reduce_24_to_rgb565(unsigned char *sp)
{
    unsigned short p;
    p = ((sp[0] | ((sp[0] & 0x07) + 0x03)) & 0xF8) << 8;
    p |= ((sp[1] | ((sp[1] & 0x03) + 0x01)) & 0xFC) << 3;
    p |= ((sp[2] | ((sp[2] & 0x07) + 0x03))) >> 3;
    return p;
}

inline unsigned short
reduce_8grey_to_rgb565(unsigned char *sp)
{
    unsigned short p;
    p = ((sp[0] | ((sp[0] & 0x07) + 0x03)) & 0xF8) << 8;
    p |= ((sp[0] | ((sp[0] & 0x03) + 0x01)) & 0xFC) << 3;
    p |= ((sp[0] | ((sp[0] & 0x07) + 0x03))) >> 3;
    return p;
}

inline unsigned long
expand_24_to_argb32(unsigned char *sp)
{
    unsigned long p;
    p  = 0xff000000;
    p |= sp[0] << 16;
    p |= sp[1] << 8;
    p |= sp[2] << 0;
    return p;
}

inline unsigned long
expand_8grey_to_argb32(unsigned char *sp)
{
    unsigned long p;
    p  = 0xff000000;
    p |= *sp << 16;
    p |= *sp << 8;
    p |= *sp << 0;
    return p;
}

void
showimage(char *name, void *v_fb_map, int fb_bpp, int w, int h)
{
    FILE *fp = 0;
    int fd;
    int a, xdim, ydim, maxval, magic, bpp;
    int c, i, j, m;
    int top = 1;
    int pixel;
    unsigned char buf[256];
    int topfillrows, leftfillcols;
    int bottomfillrows, rightfillcols;
    unsigned short *sfb_map = v_fb_map;
    unsigned long *lfb_map = v_fb_map;
    // unsigned short *ofb_map = v_fb_map;
    unsigned char *filemap;
    unsigned char *filemem;
    struct stat sb;
    

    if (!strcmp("-", name))
        fp = stdin;
    else
        fp = fopen(name, "r");

    if (!fp) {
        perror(name);
        usage();
    }

    a = fscanf(fp, "P%d\n", &magic);
    /* skip comments */
    if (magic == 4) {
        fprintf(stderr, "PBM files unsupported\n");
        /* but they could be -- we just haven't needed to */
        exit(1);
    }
    if (magic == 5 || magic == 6) {
        while (c = fgetc(fp), c == '#') {
            if (!fgets((char *) buf, 256, fp)) {
                fprintf(stderr, "short PNM header");
                exit(1);
            }
        }
        ungetc(c, fp);
    }
    a += fscanf(fp, "%d %d\n", &xdim, &ydim);
    a += fscanf(fp, "%d\n", &maxval);
    if (a != 4 || (magic != 6 && magic != 5)) {
        fprintf(stderr, "Cannot read PNM header");
        exit(1);
    }
#if 0
    fprintf(stderr, "PNM %s image: size %dx%d, maxval %d\n",
            (magic == 5) ? "gray" : "color", xdim, ydim, maxval);
#endif

    topfillrows = (h - ydim) / 2;
    bottomfillrows = (h - ydim) - topfillrows;
    leftfillcols = (w - xdim) / 2;
    rightfillcols = (w - xdim) - leftfillcols;
    //  fprintf(stderr, "%d %d %d %d\n",
    //     topfillrows, bottomfillrows, leftfillcols, rightfillcols);

    if (magic == 6)
        bpp = 3;
    else
        bpp = 1;
   
    fd = fileno(fp);

    fstat(fd, &sb);

    readahead(fd, 0, sb.st_size);

    filemap = mmap(NULL, sb.st_size, PROT_READ,
                              MAP_PRIVATE | MAP_POPULATE, fd, 0);
    filemem = filemap + ftell(fp);

    if (filemap == MAP_FAILED) {
        fprintf(stderr, "Could not map image\n");
        exit(1);
    }

    for (j = 0; j < ydim; j++) {
        for (i = 0; i < xdim; i++) {
            if (fb_bpp == 2) {
                if (bpp == 3)
                    pixel = reduce_24_to_rgb565(filemem);
                else
                    pixel = reduce_8grey_to_rgb565(filemem);
            } else {
                if (bpp == 3)
                    pixel = expand_24_to_argb32(filemem);
                else
                    pixel = expand_8grey_to_argb32(filemem);
            }
            filemem += bpp;

            if (top) {
                // delayed fill of top margin, now we know its value
                filler = pixel;
                m = ((topfillrows * w) + leftfillcols);
                if (fb_bpp == 2)
                    while (m--) *sfb_map++ = filler;
                else
                    while (m--) *lfb_map++ = filler;
                top = 0;
            }
            if (fb_bpp == 2)
                *sfb_map++ = pixel;
            else
                *lfb_map++ = pixel;
        }
        m = (w - xdim);
        if (fb_bpp == 2)
            while (m--) *sfb_map++ = filler;
        else
            while (m--) *lfb_map++ = filler;
    }
    m = ((bottomfillrows * w) - leftfillcols);
    if (fb_bpp == 2)
        while (m--) *sfb_map++ = filler;
    else
        while (m--) *lfb_map++ = filler;

    // fprintf(stderr, "%d\n", fb_map - ofb_map);
    munmap(filemap, sb.st_size);
    fclose(fp);
}

int
main(int argc, char *argv[])
{
    int sleeptime = 10;
    int c, fb = 0;
    int fb_bpp;
    void *fb_map = NULL;
    struct fb_var_screeninfo vinfo;


    prog = argv[0];

    while ((c = getopt(argc, argv, "ds:f:")) != -1) {
        switch (c) {
        case 's':
            sleeptime = atoi(optarg);
            break;
        case 'd':
            dcon = 1;
            break;
        case 'f':
            devfb = optarg;
            break;
        default:
            usage();
            break;
        }
    }

    if (optind > argc) {
        usage();
    }

    // ignore sigterm so we stay up longer during shutdown
    // signal(SIGTERM, SIG_IGN);

    signal(SIGINT, sighandler);
    signal(SIGSEGV, sighandler);
    signal(SIGILL, sighandler);
    signal(SIGFPE, sighandler);
    signal(SIGBUS, sighandler);
    signal(SIGXCPU, sighandler);
    signal(SIGXFSZ, sighandler);
    signal(SIGUSR2, sighandler);

    signal(SIGUSR1, signextimage);

    fb = open(devfb, O_RDWR);
    if (fb < 0) {
        fprintf(stderr, "open of %s: %s\n", devfb, strerror(errno));
        exit(1);
    }

    /* check for 24 bits */
    if (ioctl (fb, FBIOGET_VSCREENINFO, &vinfo)) {
        fprintf (stderr, "Couldn't get vinfo.\n");
        exit(1);
    }
    fb_bpp = vinfo.bits_per_pixel / 8;

    if (fb_bpp != 2 && fb_bpp != 4) {
        fprintf (stderr, "Unsupported bpp: %d\n", fb_bpp * 8);
        exit(1);
    }

    if (vinfo.xres > MAX_FB_WIDTH || vinfo.yres > MAX_FB_HEIGHT) {
        fprintf(stderr, "Unsupported res: %dx%d\n", vinfo.xres, vinfo.yres);
        exit(1);
    }

    dcon_freeze();
    atexit(dcon_thaw);

    fb_map = mmap(NULL, vinfo.xres * vinfo.yres * fb_bpp,
                         PROT_READ | PROT_WRITE, MAP_SHARED, fb, 0);
    vt_setup();

    while (optind < argc) {
        dcon_freeze();
        showimage(argv[optind++], fb_map, fb_bpp, vinfo.xres, vinfo.yres);
        dcon_thaw();

        if (!sigsetjmp(nxt_jmpbuf, 1) && sleeptime) {
            sleep(sleeptime);
        }
    }

    vt_deinit();

    return 0;
}
