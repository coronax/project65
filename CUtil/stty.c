/* Copyright (c) 2024, Christopher Just
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without 
 * modification, are permitted provided that the following conditions 
 * are met:
 *
 *    Redistributions of source code must retain the above copyright 
 *    notice, this list of conditions and the following disclaimer.
 *
 *    Redistributions in binary form must reproduce the above 
 *    copyright notice, this list of conditions and the following 
 *    disclaimer in the documentation and/or other materials 
 *    provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS 
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT 
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS 
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE 
 * COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, 
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, 
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; 
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER 
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, 
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) 
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED 
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <unistd.h>
#include <dirent.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <p65.h>

// Utility for controlling serial port properties.
// Right now the only one that can be meaningfully set is the baud rate.

int verbose = 1;

void usage (void)
{
    fprintf(stderr, "usage: stty [-v] newrate\r\n");
    exit(2);
}



int main (int argc, char** argv)
{
    int ch;
    unsigned long j;
    unsigned int i;
    unsigned int result;

    while ((ch = getopt(argc, argv, "v")) != -1)
    {
        switch(ch)
        {
        case 'v':
            verbose = !verbose;
            break;
        case '?':
        default:
            usage();
        }
    }

    // ah... i'm sending my ioctls to fd 0, which is actually the tty device. Having that
    // separate from the serial device itself haunts me again!!! 
    // TTY device has now been updated to forward these requests to serial.

    if (argc - optind == 0)
    {
        result = ioctl (0, IO_SER_RATE, 0);
        printf ("%u bps\r\n", result);
    }
    else if (argc - optind == 1)
    {
        j = strtoul(argv[optind],NULL,10);
        if (j > 65535UL)
        {
            printf ("Value out of range\r\n");
        }
        else
        {
            i = (unsigned int)j;
            printf ("setting rate to %u\r\n", i);
            result = ioctl (0, IO_SER_RATE, i);
            if (result == -1)
                printf ("Set bps failed\r\n");
            else
                printf ("%u bps\r\n", result);
        }
    }
    else
        usage();

    return 0;
}
