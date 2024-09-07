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

// A very very stripped down optionless ls for the P:65.

// CJ Todo: remove fixed limit on dir size
// CJ Todo: accept file names as arguments instead of just directories.

#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <dirent.h>
#include <sys/stat.h>

struct dirent* list[200];
const int max_count = 200;


// sort comparison function
int cmp_dirent (const void* p1, const void* p2)
{
	struct dirent* d1 = *(struct dirent**)p1;
	struct dirent* d2 = *(struct dirent**)p2;
	int result = d2->d_type - d1->d_type;
	if (result == 0)
		result = strcmp (d1->d_name, d2->d_name);
	return result;
}



void DisplayDirectory (const char* name)
{
	int count = 0, i;
	long int total_size = 0;
	struct dirent *d, *d2;
	DIR* dir;

	dir = opendir(name);
	if (dir)
	{
		int filecount = 0;
		int dircount = 0;
		while (d = readdir(dir))
		{
			if (d->d_type == 2)
				++dircount;
			else
			{
				++filecount;
				total_size += d->d_size;
			}
			if (count < max_count)
			{
				d2 = malloc(sizeof(struct dirent));
				memcpy (d2, d, sizeof(struct dirent));
				list[count++] = d2;
			}
			
		}
		closedir(dir);
		
		qsort (list, count, sizeof(struct dirent*), cmp_dirent);
		
		for (i = 0; i < count; ++i)
		{
			d = list[i];
			if (d->d_type == 2)
				printf ("%-12s   <DIR>\r\n", d->d_name);
			else
				printf ("%-12s     %10ld\r\n", d->d_name, d->d_size);
		}
		
		printf ("%4d File(s)     %10ld\r\n", filecount, total_size);
		printf ("%4d Dir(s)\r\n\r\n", dircount);
	}
	else
	{
		if (__oserror != 0)
			printf ("oserr: %d %s\r\n", _oserror, __stroserror(_oserror));
		else
			printf ("errno: %d %s\r\n", errno, strerror(errno));
	}
}


void DisplayFile (const char* filename, struct stat* st)
{
	printf ("%-12s     %10ld\r\n", filename, st->st_size);
}


int main(int argc, char** argv)
{
	int i;
	struct stat st;

	if (argc == 1)
		DisplayDirectory ("/");
	else
		for (i = 1; i < argc; ++i)
		{
			//printf ("sizeof struct stat is %d\r\n", sizeof(struct stat));
			if (0 == stat(argv[i],&st))
			{
				//printf ("stat success\r\n");
				//printf ("device number: %ld\r\n", st.st_dev);
				//printf ("file type: %d\r\n", st.st_mode);
				//printf ("file size: %ld\r\n", st.st_size);
				if ((st.st_mode & S_IFMT) == S_IFDIR)
				{
					if (i > 1)
						printf ("\r\n");
					printf ("%s:\r\n", argv[i]);
					DisplayDirectory (argv[i]);
				}
				else
				{
					DisplayFile (argv[i], &st);
				}
			}
			else
			{
				//printf ("stat failed\r\n");
				if (i > 1)
					printf ("\r\n");
				printf ("%s: ", argv[i]);
				if (__oserror != 0)
					printf ("%d %s\r\n", _oserror, __stroserror(_oserror));
				else
					printf ("%d %s\r\n", errno, strerror(errno));				
			}
		}
}
