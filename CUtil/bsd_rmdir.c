/*-
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright (c) 1992, 1993, 1994
 *	The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */
 
// Note: From https://github.com/freebsd/freebsd-src/blob/main/bin/rmdir/rmdir.c

//#include <err.h> CJ
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h> // CJ
//#include <oserror.h> // CJ
static int rm_path(char *);
//static void usage(void) __dead2; CJ
static void usage(void);

static int pflag;
static int vflag;


//void warn("%s", *argv)  // CJ I assume this is from err.h
void warn (const char* format, char* msg)
{
	//printf("warn");
	fprintf (stderr, ": ");
	if (format)
	{
		fprintf (stderr, format, msg);
		fprintf (stderr, ": ");
	}
	if (__oserror != 0)
		fprintf (stderr, "%s\r\n", __stroserror(_oserror));
	else
		fprintf (stderr, "%s\r\n", strerror(errno));
//	if (oserror != 0)
//		fprintf (stderr, osstrerror(oserror));
//	else
//		fprintf (stderr, "%s\r\n", strerror(errno));
}

int
main(int argc, char *argv[])
{
	int ch, errors;

	while ((ch = getopt(argc, argv, "pv")) != -1)
		switch(ch) {
		case 'p':
			pflag = 1;
			break;
		case 'v':
			vflag = 1;
			break;
		case '?':
		default:
			usage();
		}
	argc -= optind;
	argv += optind;

	if (argc == 0)
		usage();

	for (errors = 0; *argv; argv++) {
		if (rmdir(*argv) < 0) {
			warn("%s", *argv);
			errors = 1;
		} else {
			if (vflag)
				printf("%s\n", *argv);
			if (pflag)
				errors |= rm_path(*argv);
		}
	}

	exit(errors);
}

static int
rm_path(char *path)
{
	char *p;

	p = path + strlen(path);
	while (--p > path && *p == '/')
		;
	*++p = '\0';
	while ((p = strrchr(path, '/')) != NULL) {
		/* Delete trailing slashes. */
		while (--p >= path && *p == '/')
			;
		*++p = '\0';
		if (p == path)
			break;

		if (rmdir(path) < 0) {
			warn("%s", path);
			return (1);
		}
		if (vflag)
			printf("%s\n", path);
	}

	return (0);
}

static void
usage(void)
{

	(void)fprintf(stderr, "usage: rmdir [-pv] directory ...\n");
	exit(2);
}
