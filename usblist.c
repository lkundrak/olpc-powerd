/*
 * usblist.c -- provide list of selected properties of currently
 * installed USB devices, for the purposes of allowing powerd to
 * inhibit suspend based on the presence of those properities.
 *
 * usblist.c is based on:
 * libusb example program to list devices on the bus
 * Copyright (C) 2007 Daniel Drake <dsd@gentoo.org>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

 /*
  * doit: cc -o usblist usblist.c -I /usr/include/libusb-1.0 -lusb-1.0
  */

#include <stdio.h>
#include <sys/types.h>

#include <libusb-1.0/libusb.h>


static const struct {
        int class;
        char *class_name;
} clas_info[] = {
        { 0x00,       ">ifc"  },
        { 0x01,       "audio" },
        { 0x02,       "commc" },
        { 0x03,       "hid"   },
        { 0x05,       "pid"   },
        { 0x06,       "still" },
        { 0x07,       "print" },
        { 0x08,       "stor." },
        { 0x09,       "hub"   },
        { 0x0a,       "data"  },
        { 0x0b,       "scard" },
        { 0x0d,       "c-sec" },
        { 0x0e,       "video" },
        { 0x0f,       "perhc" },
        { 0xdc,       "diagd" },
        { 0xe0,       "wlcon" },
        { 0xef,       "misc"  },
        { 0xfe,       "app."  },
        { 0xff,       "vend." },
        {-1,          "unk."  }         /* leave as last */
};

static const char *class_decode(const int class)
{
        int i;

        for (i = 0; clas_info[i].class != -1; i++)
                if (clas_info[i].class == class)
                        break;
        return clas_info[i].class_name;
}


void print_devs(libusb_device **devs)
{
        libusb_device *dev;
        struct libusb_device_descriptor desc;
        struct libusb_config_descriptor *config;
        int i, j, a, r;

        i = 0;
        while ((dev = devs[i++]) != NULL) {
                r = libusb_get_device_descriptor(dev, &desc);
                if (r < 0) {
                        fprintf(stderr, "failed to get device descriptor");
                        return;
                }

                printf( "id=%04x\n"
                        "id=%04x:%04x\n",
                        desc.idVendor,
                        desc.idVendor, desc.idProduct);

                if (desc.bDeviceClass != 0) {
                        printf( "class=%02x\n"
                                "class=%s\n",
                                desc.bDeviceClass,
                                class_decode(desc.bDeviceClass));
                }

                r = libusb_get_active_config_descriptor (dev, &config);
                if (r < 0) {
                        fprintf(stderr, "failed to get active config descriptor");
                        return;
                }
                for (j = 0; j < config->bNumInterfaces; j++) {
                    for (a = 0; a < config->interface[j].num_altsetting; a++) {
                        printf(
                            "class=%02x\n"
                            "class=%s\n",
                            config->interface[j].altsetting[a].bInterfaceClass,
                            class_decode(config->interface[j].altsetting[a].bInterfaceClass));
                    }
                }
                libusb_free_config_descriptor(config);
        }
}

int main(void)
{
        libusb_device **devs;
        int r;
        ssize_t cnt;

        r = libusb_init(NULL);
        if (r < 0)
                return r;

        cnt = libusb_get_device_list(NULL, &devs);
        if (cnt < 0)
                return (int) cnt;

        print_devs(devs);
        libusb_free_device_list(devs, 1);

        libusb_exit(NULL);
        return 0;
}

