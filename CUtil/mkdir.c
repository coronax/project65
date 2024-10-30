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

// A very stripped-down UNIX-like mkdir program for the P:65 computer.

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <p65.h>

int verbose = 1;



// quick-and-dirty substitute for BSD-like warn().
void warn (const char* format, const char* msg)
{
	fprintf (stderr, "mkdir: ");
	if (format)
	{
		fprintf (stderr, format, msg);
	}
	if (__oserror != 0)
		fprintf (stderr, ": %d %s", _oserror, __stroserror(_oserror));
	else if (errno != 0)
		fprintf (stderr, ": %s", strerror(errno));
    fprintf (stderr, "\r\n");
}



void usage (void)
{
    fprintf(stderr, "usage: mkdir [-v] dir1 ...\r\n");
    exit(2);
}



int main (int argc, char** argv)
{
    int ch, i;

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

    if (argc - optind < 1)
        usage();

    for (i = optind; i < argc; ++i)
    {
        if (0 == mkdir (argv[i]))
        {
            if (verbose)
                warn ("created directory '%s'\r\n", argv[i]);
        }
        else
            warn ("cannot create directory '%s'", argv[i]);
    }

    return 0;
}

