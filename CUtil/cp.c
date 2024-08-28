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

// A very stripped-down UNIX-like cp program for the P:65 computer.
//
// Because the P:65 has tight constraints on the number of files it can have
// open at one time, and because that includes directories opened with 
// opendir(), we don't want to do the most straightforward recursive version
// of the recursive copy. Instead, we're going to open a directory, copy the
// regular files, and put the subdirectories into a linked list qdir that 
// we'll pull from once we've closed the directory we're currently looking
// at.


#include <unistd.h>
#include <dirent.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <p65.h>

int recursive = 0;
int verbose = 1;

char buffer1[128], buffer2[128];
const int bufferlen = 128;


// We add to the back of qdir and pull from the front.
struct qdir_entry {
    struct qdir_entry* next;
    char* src;
    char* dst;
};

struct qdir_entry* qdir_first = NULL;
struct qdir_entry* qdir_last = NULL;

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
    errno = _oserror = 0; // don't need these
    return result;
}

// returns the last name element of path
const char* GetFilename(const char* path)
{
    int i = strlen(path) - 1;
    for (; i >= 0; --i)
        if (path[i] == '/')
            return path + i + 1;
    return path;
}



// quick-and-dirty substitute for BSD-like warn().
void warn (const char* format, const char* msg)
{
	fprintf (stderr, "cp: ");
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



// Add a directory that needs to be copied to the tail of qdir.
void QueueDirectory (const char* _src, const char* _dst)
{
    struct qdir_entry* d = (struct qdir_entry*)malloc(sizeof(struct qdir_entry));
    char* src = strdup(_src);
    char* dst = strdup(_dst);
    //printf ("QueueDirectory %s %s\r\n", _src, _dst);
    if (d == NULL || src == NULL || dst == NULL)
    {
        warn("%s",_src);
        exit(1);
    }
    d->src = src;
    d->dst = dst;
    d->next = NULL;

    if (qdir_last)
    {
        qdir_last->next = d;
        qdir_last = d;
    }
    else
    {
        qdir_first = qdir_last = d;
    }
}



// Remove & deallocate the head of qdir.
void FreeQdirHead()
{
    if (qdir_first)
    {
        struct qdir_entry* d = qdir_first;
        qdir_first = d->next;
        if (qdir_first == NULL)
            qdir_last = NULL;
        free(d->src);
        free(d->dst);
        free(d);
    }
}



// Copy a single regular file. Will error out if src or dst are directories.
void CopyFile (char* src, char* dst)
{
    //printf ("CopyFile %s %s\r\n", src,dst);
    if (copyfile (src, dst) != -1)
    {
        if (verbose)
            printf ("%s -> %s\r\n", src, dst);
    }
    else
        warn ("failed: %s", src);
}



// Copy the directories recorded in qdir.
int CopyFolder()
{
    DIR* dir;
    struct dirent* d;
    char *src, *dst;

    while (qdir_first)
    {
        // src is a directory. dst either does not exist or is a dir.
        src = qdir_first->src;
        dst = qdir_first->dst;
        //printf ("CopyFolder loop: %s %s\r\n", src, dst);

        if (is_directory (dst) || (mkdir(dst) != -1))
        {
            if (verbose)
                printf ("%s -> %s\r\n", src, dst);
            if (dir = opendir(src))
            {
                while (d = readdir(dir))
                {
                    snprintf (buffer1, bufferlen, "%s/%s", src, d->d_name);
                    snprintf (buffer2, bufferlen, "%s/%s", dst, d->d_name);
                    //printf ("Reading direntry %s\r\n", d->d_name);
                    if (d->d_type == 2)
                    {
                        // directory
                        QueueDirectory (buffer1, buffer2);
                    }
                    else if (d->d_type == 1)
                    {
                        CopyFile (buffer1, buffer2);
                    }
                    else
                    {
                        warn ("unknown type: (%s)", buffer1);
                    }
                }
                closedir(dir);
            }
            else
            {
                warn("opendir failed: %s", src);
            }
        }
        else
        {
            warn ("mkdir failed: %s", dst);
        }
        FreeQdirHead();
    }
}



// Returns 1 if src is a parent folder of dst. Src and dst are both in 
// canonical form (see CanonicalizeFilename).
int CheckForIllegalRecursion (const char* src, const char* dst)
{
    int n = strlen(src);
    if ((0 == strnicmp (src, dst, n))
        && ((dst[n] == 0) || (dst[n] == '/')))
        return 1;
    return 0;
}



void DoCopy (char* src, char* dst)
{
    // Set up whatever direct or recursive copy we need for a particular 
    // src & dst pair.
    //printf ("DoCopy: %s  %s\r\n", src, dst);

    if (CheckForIllegalRecursion (src, dst))
    {
        warn ("cannot copy a directory into itself. Omitting directory %s", src);
    }
    else if (is_directory(src))
    {
        if (recursive)
        {
            QueueDirectory (src, dst);
            CopyFolder();
        }
        else
        {
            warn ("-r not specified. omitting directory %s", src);
        }
    }
    else
    {
        // simple file copy
        CopyFile (src,dst);
    }
}



// The canonical filename starts with /, and does not end with /.
// We don't care about case here because we use a case-insensitive
// compare when checking for recursion.
void CanonicalizeFilename (char* buffer, const char* path)
{
    int n;
    buffer[0] = 0;
    if (path[0] == '/')
        snprintf (buffer, bufferlen, path);
        //strncpy (buffer, bufferlen, path, bufferlen);
    else
    {
        snprintf (buffer, bufferlen, "/%s", path);
        //buffer[0] = '/';
        //strncpy (buffer+1, bufferlen-1, path);
    }
    n = strlen(buffer);
    if (buffer[n-1] == '/')
        buffer[n-1] = 0;
}



void usage (void)
{
    fprintf(stderr, "usage: cp [-rv] source dest\r\n");
    fprintf(stderr, "   or: cp [-rv] source1 ... dest_folder\r\n");
    exit(2);
}



int main (int argc, char** argv)
{
    int ch, i;
    char *src, *dst;
    const char *fname;

    while ((ch = getopt(argc, argv, "rv")) != -1)
    {
        switch(ch)
        {
        case 'r':
            recursive = 1;
            break;
        case 'v':
            verbose = 1;
            break;
        case '?':
        default:
            usage();
        }
    }

    if (argc - optind < 2)
        usage();

    // Is the last argument a directory?
    if (is_directory(argv[argc-1]))
    {
        // If the last argument is a directory, we can accept an arbitrary
        // number of arguments, and copy each of them individually.
        for (i = optind; i < argc-1; ++i)
        {
            // We're copying _into_ the destination directory, so we need
            // to fix up the dest filename appropriately.
            src = argv[i];
            dst = argv[argc-1];
            fname = GetFilename(src);
            CanonicalizeFilename(buffer1, src);
            CanonicalizeFilename(buffer2, dst);
            strcat(buffer2, "/");
            strcat(buffer2, fname);
            DoCopy (buffer1, buffer2);
        }
    }
    else
    {
        // If the last argument isn't a dir, then we need to have exactly
        // one source and one destination (though they can be either a
        // file or a directory).
        if (argc - optind == 2)
        {
            //just copy src to dst. dst either a file or doesn't exist.
            src = argv[optind];
            dst = argv[argc-1];
            CanonicalizeFilename(buffer1, src);
            CanonicalizeFilename(buffer2, dst);
            DoCopy (buffer1, buffer2);
        }
        else
            usage();
    }
}

