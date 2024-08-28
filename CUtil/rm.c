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

// A very stripped-down UNIX-like rm program for the P:65 computer.
//
// Because the P:65 has tight constraints on the number of files it can have
// open at one time, and because that includes directories opened with 
// opendir(), we don't want to do the most straightforward recursive version
// of the recursive remove. Instead, we remove regular files and put 
// directories into a queue so that we can traverse them after closing the
// current directory.


#include <unistd.h>
#include <dirent.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <p65.h>

int force = 0;
int recursive = 0;
int verbose = 1;

char buffer1[128];
const int bufferlen = 128;


// We add to the back of qdir and pull from the front.
struct qdir_entry {
    struct qdir_entry* next;
    char* src;
    char* dst;
};

int __fastcall__ getfdtype(int fd);

// returns 1 iff name exists & is a directory,
// 0 otherwise
int is_directory (const char* name)
{
    int result = 0;
    int fd = open (name, O_RDONLY);
    if ((fd >= 0) && (getfdtype(fd) == 2))
        result = 1;
    close(fd);
    return result;
}



// quick-and-dirty substitute for BSD-like warn().
void warn (const char* format, const char* msg)
{
	fprintf (stderr, "rm: ");
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



// for rm we need a stack. So we'll add and delete to front of qdir
// Add a directory that needs to be copied to the tail of qdir.
void QueueDirectory (struct qdir_entry* *qdir_first, const char* _src)
{
    struct qdir_entry* d = (struct qdir_entry*)malloc(sizeof(struct qdir_entry));
    char* src = strdup(_src);
    //printf ("QueueDirectory %s %s\r\n", _src, _dst);
    if (d == NULL || src == NULL)
    {
        warn("%s",_src);
        exit(1);
    }
    d->src = src;
    d->next = *qdir_first;

    *qdir_first = d;
}



// Remove & deallocate the head of qdir.
void FreeQdirHead(struct qdir_entry* *qdir_first)
{
    if (*qdir_first)
    {
        struct qdir_entry* d = *qdir_first;
        *qdir_first = d->next;
        free(d->src);
        free(d);
    }
}



// Copy a single regular file. Will error out if src or dst are directories.
void DeleteFile (const char* src)
{
    //printf ("DeleteFile %s\r\n", src);
    if (remove (src) != -1)
    {
        if (verbose)
            printf ("removed '%s'\r\n", src);
    }
    else
        warn ("cannot remove %s", src);
}



// Copy the directories recorded in qdir.
int DeleteFolder(const char* src)
{
    DIR* dir;
    struct dirent* d;
    struct qdir_entry* qdir = NULL;
    //printf ("DeleteFolder %s\r\n");
    if (dir = opendir(src))
    {
        while (d = readdir(dir))
        {
            snprintf (buffer1, bufferlen, "%s/%s", src, d->d_name);
            if (d->d_type == 2) // directory
            {
                QueueDirectory (&qdir, buffer1);
            }
            else if (d->d_type == 1) // regular file
            {
                DeleteFile (buffer1);
            }
            else
            {
                warn ("unknown type: (%s)", buffer1);
            }
        }
        closedir(dir);
    }

    while (qdir)
    {
        DeleteFolder (qdir->src);
        FreeQdirHead (&qdir);
    }

    DeleteFile (src);
}



void usage (void)
{
    fprintf(stderr, "usage: rm [-rfv] file1 ...\r\n");
    exit(2);
}



int main (int argc, char** argv)
{
    int ch, i;
    char *src;

    while ((ch = getopt(argc, argv, "rfv")) != -1)
    {
        switch(ch)
        {
        case 'f':
            force = 1;
            break;
        case 'r':
            recursive = 1;
            break;
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
        src = argv[i];
        if (is_directory(src))
        {
            if (recursive)
            {
                DeleteFolder (src);
            }
            else
                warn ("cannot remove %s: is a directory", src);
        }
        else
        {
            DeleteFile (src);
        }
    }

    return 0;
}

