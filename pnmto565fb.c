/*
 * pnmto565fb.c -- write a PNM (actually just PGM or PPM) file,
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
 * The VT initialization and teardown code was borrowed almost
 * entirely from the ppmtofb package, which is also GPLed, and
 * bears the following notices:
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
#include <linux/kd.h>
#include <linux/vt.h>

#define FB_SIZE_B 1200*900*2
#define FB_WIDTH 1200
#define FB_HEIGHT 900

char *prog;
extern char *optarg;
extern int optind, opterr, optopt;

int center_it = 0;
int fill_it = 0;

static void vt_deinit(void);
int consolefd;
int activevt;

sigjmp_buf nxt_jmpbuf;

void
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

static void
Warn(const char *fmt, ...)
{
    va_list ap;

    fflush(stdout);
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);
}


static void
SigHandler(int signo)
{
    signal(signo, SIG_IGN);
    die("Caught signal %d. Exiting\n", signo);
}

static void
SigNextImage(int signo)
{
    // signal(signo, SigNextImage);
    longjmp(nxt_jmpbuf, 1);
}


/* VT init and teardown borrowed almost completely from ppmtofb, which
 * is GPL'ed, and:
 *  (c) Copyright 1996 by Geert Uytterhoeven
 *  (c) Copyright 1996-2000 by Chris Lawrence
 */

static void
vt_deinit(void)
{
    struct vt_mode VT;

    if (consolefd) {
	ioctl(consolefd, KDSETMODE, KD_TEXT);
	if (ioctl(consolefd, VT_GETMODE, &VT) != -1) {
	    VT.mode = VT_AUTO;
	    ioctl(consolefd, VT_SETMODE, &VT);
	}
	if (activevt >= 0) {
	    ioctl(consolefd, VT_ACTIVATE, activevt);
	    activevt = -1;
	}
	close(consolefd);
	consolefd = 0;
    }
}

void
vt_setup(void)
{
    int i, fd, vtno;
    char vtname[11];
    struct vt_stat vts;
    struct vt_mode VT;

    if ((fd = open("/dev/tty0", O_WRONLY, 0)) < 0)
	die("Cannot open /dev/tty0: %s\n", strerror(errno));

    if (ioctl(fd, VT_OPENQRY, &vtno) < 0 || vtno == -1)
	die("Cannot find a free VT\n");

    close(fd);

    sprintf(vtname, "/dev/tty%d", vtno);	/* /dev/tty1-64 */
    if ((consolefd = open(vtname, O_RDWR | O_NDELAY, 0)) < 0)
	die("Cannot open %s: %s\n", vtname, strerror(errno));

    /*
     * Linux doesn't switch to an active vt after the last close of a vt,
     * so we do this ourselves by remembering which is active now.
     */
    if (ioctl(consolefd, VT_GETSTATE, &vts) == 0)
	activevt = vts.v_active;

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

    if (ioctl(consolefd, VT_ACTIVATE, vtno) != 0)
	Warn("ioctl VT_ACTIVATE: %s\n", strerror(errno));
    if (ioctl(consolefd, VT_WAITACTIVE, vtno) != 0)
	Warn("ioctl VT_WAITACTIVE: %s\n", strerror(errno));

    if (ioctl(consolefd, VT_GETMODE, &VT) < 0)
	die("ioctl VT_GETMODE: %s\n", strerror(errno));

    VT.mode = VT_PROCESS;
    VT.relsig = 0;
    VT.acqsig = 0;
    if (ioctl(consolefd, VT_SETMODE, &VT) < 0)
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
	"usage: %s [pnmfile] (writes 565 data to /dev/fb)\n"
	"    -c to center the image in the framebuffer\n"
	"    -f to explicitly fill around the image\n"
	"    -s SECS  to sleep between images\n",
	prog);
    exit(1);
}

unsigned short
reduce_24_to_rgb565(unsigned char *sp)
{
    unsigned short p;
#if 1
    p = ((sp[0] | ((sp[0] & 0x07) + 0x03)) & 0xF8) << 8;
    p |= ((sp[1] | ((sp[1] & 0x03) + 0x01)) & 0xFC) << 3;
    p |= ((sp[2] | ((sp[2] & 0x07) + 0x03))) >> 3;
#else
    p = (sp[0] & 0xF8) << 8;
    p |= (sp[1] & 0xFC) << 3;
    p |= (sp[2]) >> 3;
#endif
    return p;
}

unsigned short
reduce_8grey_to_rgb565(unsigned char *sp)
{
    unsigned short p;
#if 1
    p = ((sp[0] | ((sp[0] & 0x07) + 0x03)) & 0xF8) << 8;
    p |= ((sp[0] | ((sp[0] & 0x03) + 0x01)) & 0xFC) << 3;
    p |= ((sp[0] | ((sp[0] & 0x07) + 0x03))) >> 3;
#else
    p = (sp[0] & 0xF8) << 8;
    p |= (sp[0] & 0xFC) << 3;
    p |= (sp[0]) >> 3;
#endif
    return p;
}

void
showimage(char *name, unsigned short *fb_map)
{
    FILE *fp = 0;
    int a, n, xdim, ydim, maxval, magic;
    int c, i, j;
    unsigned char buf[256];
    int h = FB_HEIGHT;
    int w = FB_WIDTH;

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
    if (magic == 5 || magic == 6) {
	while (c = fgetc(fp), c == '#') {
	    fgets((char *) buf, 256, fp);
	}
	ungetc(c, fp);
    }
    a += fscanf(fp, "%d %d\n", &xdim, &ydim);
    a += fscanf(fp, "%d\n", &maxval);
    if (a != 4 || (magic != 6 && magic != 5)) {
	fprintf(stderr, "Cannot read PNM header");
    }
#if 0
    fprintf(stderr, "PNM %s image: size %dx%d, maxval %d\n",
	    (magic == 5) ? "gray" : "color", xdim, ydim, maxval);
#endif

    if (center_it) {
	if (fill_it)
	    memset(fb_map, 0, ((((h - ydim) / 2) * w) + (w - xdim) / 2) * 2);
	fb_map += ((((h - ydim) / 2) * w) + (w - xdim) / 2);
    }
    if (magic == 6) {
	for (j = 0; j < ydim; j++) {
	    for (i = 0; i < xdim; i++) {
		n = fread(buf, 3, 1, fp);
		if (n != 1)
		    break;
		*fb_map++ = reduce_24_to_rgb565(buf);
	    }
	    if (center_it) {
		if (fill_it)
		    memset(fb_map, 0, (w - xdim) * 2);
		fb_map += w - xdim;
	    }
	}
    } else {
	for (j = 0; j < ydim; j++) {
	    for (i = 0; i < xdim; i++) {
		n = fread(buf, 1, 1, fp);
		if (n != 1)
		    break;
		*fb_map++ = reduce_8grey_to_rgb565(buf);
	    }
	    if (center_it) {
		if (fill_it)
		    memset(fb_map, 0, (w - xdim) * 2);
		fb_map += w - xdim;
	    }
	}
    }
    if (center_it) {
	if (fill_it)
	    memset(fb_map, 0, (((h - ydim) / 2) * w) + (w - xdim) / 2);
	fb_map += ((((h - ydim) / 2) * w) + (w - xdim) / 2);
    }
    fclose(fp);
}

int
main(int argc, char *argv[])
{
    int sleeptime = 10;
    int c, fb = 0;
    unsigned short *fb_map = NULL;

    prog = argv[0];

    while ((c = getopt(argc, argv, "cfs:")) != -1) {
	switch (c) {
	case 'c':
	    center_it = 1;
	    break;
	case 'f':
	    fill_it = 1;
	    break;
	case 's':
	    sleeptime = atoi(optarg);
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

    signal(SIGINT, SigHandler);
    signal(SIGSEGV, SigHandler);
    signal(SIGILL, SigHandler);
    signal(SIGFPE, SigHandler);
    signal(SIGBUS, SigHandler);
    signal(SIGXCPU, SigHandler);
    signal(SIGXFSZ, SigHandler);
    signal(SIGUSR2, SigHandler);

    signal(SIGUSR1, SigNextImage);

    fb = open("/dev/fb", O_RDWR);
    if (fb < 0) {
	perror("open of /dev/fb");
	exit(1);
    }

    fb_map = (unsigned short *) mmap(NULL, FB_SIZE_B,
			 PROT_READ | PROT_WRITE, MAP_SHARED, fb, 0);
    vt_setup();

    while (optind < argc) {
	showimage(argv[optind++], fb_map);

	if (!sigsetjmp(nxt_jmpbuf, 1) && sleeptime) {
	    sleep(sleeptime);
	}
    }


    vt_deinit();

    return 0;
}
