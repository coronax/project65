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

// A simple utility to print or set the date & time.

#include <time.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

int set = 0;
int verbose = 1;

void usage (void)
{
    fputs("usage: date [-v] [-s datestring]\r\n", stderr);
    fputs("Datestring formats:\r\n", stderr);
    fputs("  yyyy-mm-dd hh:mm AM\r\n", stderr);
    exit(2);
}


char datestring[80];
const int datestring_len = 80;

int main (int argc, char** argv)
{
    int ch;
    int error = 0; // keeps track of parsing errors in set
//    char *src;

//    printf ("argc = %d\r\n", argc);
//    for (i = 0; i < argc; ++i)
//        printf ("arg: '%s'\r\n",argv[i]);
//    exit(0);

    while ((ch = getopt(argc, argv, "s:v")) != -1)
    {
        switch(ch)
        {
        case 's':
            set = 1;
            //printf ("optarg is '%s'\r\n", optarg);
            strncpy (datestring, optarg, datestring_len);
            datestring[datestring_len-1] = 0;
            break;
        case 'v':
            verbose = !verbose;
            break;
        case '?':
        default:
            usage();
        }
    }

    if (!set)
    {
        // just print the date
    	time_t t = time(NULL);
        fputs(ctime(&t), stdout);
        fputs("\r\n", stdout);
    }
    else
    {
        // Initialize a tm struct with isdst == -1
        struct tm tm = {0,0,0,0,0,0,0,0,-1};
        time_t newtime;
	    struct timespec ts;
        char* c;
        const char* delim = " \t-:";
        
        //printf ("datestring is '%s'\r\n", datestring);
        c = strtok (datestring, delim);
        if (c)
            tm.tm_year = atoi(c) - 1900;
        else
            error = 1;
        c = strtok (NULL, delim);
        if (c)
            tm.tm_mon = atoi(c) - 1; // 0 based?
        else
            error = 1;
        c = strtok (NULL, delim);
        if (c)
            tm.tm_mday = atoi(c);
        else
            error = 1;
        c = strtok (NULL, delim);
        if (c)
            tm.tm_hour = atoi(c);
        else
            error = 1;
        c = strtok (NULL, delim);
        if (c)
            tm.tm_min = atoi(c);
        else
            error = 1;
        c = strtok (NULL, delim);
        if (c)
            if (stricmp(c,"pm") == 0)
                tm.tm_hour += 12;

        if (error == 0)
        {
            newtime = mktime(&tm);
            if (newtime == -1)
                error = 1;
        }

        if (error)
        {
            fputs ("Error parsing time\r\n", stderr);
        }
        else
        {
            ts.tv_sec = newtime;
            ts.tv_nsec = 0;
            clock_settime(0,&ts);
        }
        
    }

	return 0;
}
