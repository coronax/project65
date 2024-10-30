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

// A very stripped-down UNIX-like mv program for the P:65 computer.
//
// Because the Arduino SD library doesn't give me a way to rename files,
// we basically have to do a copy & then remove the original. This doesn't
// sound dangerous at all.
//
// Because the P:65 has tight constraints on the number of files it can have
// open at one time, and because that includes directories opened with 
// opendir(), we don't want to do the most straightforward recursive version
// of the recursive copy. Instead, we're going to open a directory, copy the
// regular files, and put the subdirectories into a linked list qdir that 
// we'll pull from once we've closed the directory we're currently looking
// at.
//
// TODO: There's a fair amount of code here that assumes bufferlen will always
// be "big enough". Not very safe. We'll revisit that once we have a more 
// rigourous filestyem layer and we've decided what the maximum possible
// path length looks like.



#include <unistd.h>
#include <dirent.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <p65.h>

int verbose = 1;

char buffer1[128], buffer2[128];
const int bufferlen = 128;


// We can't recursively open a bunch of directories because of the open files
// limit. So instead we'll store those instructions ina list/stack so we can
// process them while only having one or two files open at a time.
typedef struct qdir_entry {
    struct qdir_entry* next;
    char* src;
    char* dst;
} qdir_entry;

qdir_entry* qdir_first = NULL;
qdir_entry* qdir_last = NULL;


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
	fprintf (stderr, "mv: ");
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
void QueueDirectoryTail (const char* _src, const char* _dst)
{
    qdir_entry* d = (qdir_entry*)malloc(sizeof(qdir_entry));
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



// for rm stage, add to front. ugh.
void QueueDirectoryHead (const char* _src, const char* _dst)
{
    qdir_entry* d = (qdir_entry*)malloc(sizeof(qdir_entry));
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

    if (qdir_first)
    {
        d->next = qdir_first;
        qdir_first = d;
    }
    else
    {
        qdir_first = qdir_last = d;
    }
}



// Remove head of qdir without freeing it.
qdir_entry* UnlinkQdirHead()
{
    if (qdir_first)
    {
        qdir_entry* d = qdir_first;
        qdir_first = d->next;
        if (qdir_first == NULL)
            qdir_last = NULL;
        return d;
    }
    return NULL;
}


void FreeQdirEntry (qdir_entry* d)
{
    free(d->src);
    free(d->dst);
    free(d);
}



// Copy a single regular file. Will error out if src or dst are directories.
// Returns the number of errors encountered.
int CopyFile (char* src, char* dst)
{
    //printf ("CopyFile %s %s\r\n", src,dst);
    if (copyfile (src, dst) != -1)
    {
        if (verbose)
            printf ("%s -> %s\r\n", src, dst);
        return 0;
    }
    else
    {
        warn ("failed: %s", src);
        return 1;
    }
}



// Copy the directories recorded in qdir.
// Returns the number of errors encountered;
int CopyFolder()
{
    DIR* dir;
    struct dirent* d;
    char *src, *dst;
    int num_errors = 0;
    qdir_entry* qdir;

    while (qdir = UnlinkQdirHead())
    {
        // src is a directory. dst either does not exist or is a dir.
        src = qdir->src;
        dst = qdir->dst;
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
                        QueueDirectoryTail (buffer1, buffer2);
                    }
                    else if (d->d_type == 1)
                    {
                        num_errors += CopyFile (buffer1, buffer2);
                    }
                    else
                    {
                        warn ("unknown type: (%s)", buffer1);
                        ++num_errors;
                    }
                }
                closedir(dir);
            }
            else
            {
                warn("opendir failed: %s", src);
                ++num_errors;
            }
        }
        else
        {
            warn ("mkdir failed: %s", dst);
            ++num_errors;
        }
        FreeQdirEntry(qdir);
    }

    return num_errors;
}



// Delete a single regular file. 
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
// CJ BUG deletefolder requires the qdir struct to be local because of 
// how the recursion works.
int DeleteFolder(const char* src)
{
    DIR* dir;
    struct dirent* d;
    qdir_entry* qdir = NULL;
    //printf ("DeleteFolder %s\r\n", src);
    if (dir = opendir(src))
    {
        while (d = readdir(dir))
        {
            snprintf (buffer1, bufferlen, "%s/%s", src, d->d_name);
            if (d->d_type == 2) // directory
            {
                QueueDirectoryHead (buffer1, "");
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

    while (qdir = UnlinkQdirHead())
    {
        DeleteFolder (qdir->src);
        FreeQdirEntry (qdir);
    }

    DeleteFile (src);
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
    int num_errors = 0;
    char* copy_of_src = strdup(src);
    // we make a copy of src because src is typically in one of the global
    // buffers, and it'll get overwritten as we copy folders before we 
    // need it again to call DeleteFolder.

    if (CheckForIllegalRecursion (src, dst))
    {
        warn ("cannot copy a directory into itself. Omitting directory %s", src);
    }
    else if (is_directory(src))
    {
        QueueDirectoryHead (src, dst);
        num_errors += CopyFolder();
        if (num_errors == 0)
            DeleteFolder (copy_of_src);
    }
    else
    {
        // simple file copy
        //if (rename (src, dst) == -1)
        //    warn ("failed: %s", src);
        num_errors += CopyFile (src,dst);
        if (num_errors == 0)
            DeleteFile (copy_of_src);
    }

    free(copy_of_src);
}



// The canonical filename starts with /, and does not end with /.
// We don't care about case here because we use a case-insensitive
// compare when checking for recursion.
// Returns 1 if there's enough room in the buffer (length if bufferlen).
// Returns 0 if there isn't room.
int CanonicalizeFilename (char* buffer, const char* path)
{
    int n;
    buffer[0] = 0;
    if ((path[0] == '/') && (strlen(path) < bufferlen - 1))
        n = snprintf (buffer, bufferlen, path);
    else if (strlen(path) < bufferlen - 2)
    {
        n = snprintf (buffer, bufferlen, "/%s", path);
    }
    else
        return 0;

    //n = strlen(buffer);
    if (buffer[n-1] == '/')
        buffer[n-1] = 0;
    return 1;
}



void usage (void)
{
    fprintf(stderr, "usage: mv [-v] source dest\r\n");
    fprintf(stderr, "   or: mv [-v] source1 ... dest_folder\r\n");
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
        case 'v':
            verbose = !verbose;
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
            if (0 == CanonicalizeFilename(buffer1, src))
            {
                warn ("path too long: %s", src);
                exit(1);
            }
            if ((0 == CanonicalizeFilename(buffer2, dst))
                || (strlen(buffer2) + strlen(fname) > bufferlen-2))
            {
                warn ("path too long: %s", dst);
                exit(1);
            }
            
            strcat(buffer2, "/");
            strcat(buffer2, fname);

            //printf ("buffer1: '%s'\r\n", buffer1);
            //printf ("buffer2: '%s'\r\n", buffer2);

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
            if (0 == CanonicalizeFilename(buffer1, src))
            {
                warn ("path too long: %s", src);
                exit(1);
            }
            if (0 == CanonicalizeFilename(buffer2, dst))
            {
                warn ("path too long: %s", dst);
                exit(1);
            }
            DoCopy (buffer1, buffer2);
        }
        else
            usage();
    }
}

