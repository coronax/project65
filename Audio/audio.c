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

/* Compile with: cl65 -t none --config p65.cfg ctest.c p65.lib */

#include <stdlib.h>
//#include <conio.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>

//void __fastcall__ XModem (void);


void __fastcall__ msleep(int ms);


// Note values: with counter starting at 00, our frequency is 122.05 Hz,
// our wavelength is 8.19ms.
// So each count value should be worth .00003201 seconds = 0.032 ms.
// 

typedef struct Span
{
	int Time;
	unsigned int Notes[4];
} Span;


Span* song = NULL;
unsigned int songlen = 0;
unsigned int range_begin = 0;
unsigned int range_end = 0;

char command_buffer[80];

int verbose = 0;

// Sound register, word length. Bits 0-11 specify the frequency,
// bit 12 is set to enable sound, unset to disable. Bits 13-15 are unused.
unsigned int* const w1 = (unsigned int*)(0xa000);

/*
unsigned char GetHexit (unsigned char c)
{
	if ((c >= '0') && (c <= '9'))
		return c - '0';
	if ((c >= 'a') && (c <= 'f'))
		return c - 'a' + 10;
	if ((c >= 'A') && (c <= 'F'))
		return c - 'a' + 10;
	return 255;
}
*/



void PlaySong()
{
    //unsigned int* r = (unsigned int*)(0xa000);
    int i, j;
    int time;

    for (i = range_begin; i < range_end; ++i)
    {
        Span* s = &song[i];
        j = 0;
        time = 0;
        while (time < s->Time)
        {
            if ((j == 0) || (s->Notes[j] != 0))
            {
                *w1 = s->Notes[j];
                msleep(20);
                time += 2;
            }
            ++j;
            if (j > 3)
                j = 0;
        }
    }

    *w1 = 0x0000;
    return;
}

// global variables used by the music player
volatile int current_span;
int next_span_at; // a count of interrupts until we move to next Span
char current_note;
int next_note_at;
int t;
volatile char done; // true when song is done playing.
Span* s;
char* int_sp;


// Not actually used for playing, just for sorting out the algorithm
#if 0
void SongInterrupt()
{
    // an interrupt called by our 100 Hz timer
    unsigned int* const w1 = (unsigned char*)(0xa000);

    ++t;

    if (done)
        return;

    if (t >= next_span_at)
    {
        ++current_span;
        if (current_span >= songlen)
        {
            *w1 = 0x0000; // silence music
            done = 1;
        }
        else
        {
            // start a new span
            //Span* s = &song[current_span];
            s += 1;
            t = 0;
            next_span_at = s->Time;
            current_note = 0;
            *w1 = s->Notes[current_note];
            //next_note_at = 2; // 20ms ? Is that way too slow? dunno!
        }
    }
/*
    if (t == next_note_at)
    {
        // note multiplexing
        //Span* s = &song[current_span];
        next_note_at += 2;
        ++current_note;
        if (current_note > 3)
            current_note = 0;
        if ((current_note == 0) || (s->Notes[current_note] != 0))
            *r = s->Notes[current_note];
    }
*/
}
#endif

void __fastcall__ EnableInterrupt();
void __fastcall__ RemoveInterrupt();

void PlaySongInt()
{
    //unsigned int* w1 = (unsigned int*)(0xa000);

    current_span = range_begin;
    next_span_at = 0;
    next_note_at = 0;
    t = 0;
    s = &song[range_begin];
    done = 0;
    int_sp = NULL; //malloc(256); //?
    EnableInterrupt();

    while (!done)
    {
        sleep(1);
        printf ("%d\r\n", current_span);
    }
    RemoveInterrupt();
    *w1 = 0x00ff;

    printf ("done is %d; current span is %d\r\n", (int)done, current_span);
}

// quick-and-dirty substitute for BSD-like warn().
void warn (const char* format, const char* msg)
{
	fprintf (stderr, "audio: ");
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

int LoadSong (char* filename)
{
    int result = 0;
    FILE* f;
    int len;
    int version = 0;
    int address = 0; // save unfortunately inserts the load address. hm.

    // free the previous song struct
    if (song)
    {
        free(song);
        song = NULL;
        songlen = 0;
    }

    f = fopen (filename, "rb");
    if (f)
    {
        fread(&address,2,1,f);
        fread(&version,2,1,f);
        fread(&len, 2,1,f);
        //printf ("version is %d\r\n", version);
        //printf ("len is %d\r\n", len);
        song = malloc (len * sizeof(struct Span));
        if (song)
        {
            if (len == fread(song, sizeof(struct Span), len, f))
            {
                songlen = len;
                result = 1;
                printf ("loaded %s; #spans is %d\r\n", filename, songlen);
            }
        }
        fclose(f);
    }
    //else
    //{
    //    warn ("%s", filename);
    //}

    return result;
}

void usage (void)
{
    fprintf(stderr, "usage: audo -r begin[-end] [-v] file1 \r\n");
    exit(2);
}


int main (int argc, char** argv)
{
    //	_print_hex = _print_hex;
    //	char** xmodem_save_addr = (char**)0x020c;
    //unsigned char val1, val2;
    //unsigned char* r1 = (unsigned char*)(0xa000);
    //unsigned char* r2 = (unsigned char*)(0xa001);
    //unsigned int* w1 = (unsigned int*)(0xa000);
    unsigned int tmp; // for parsing numbers
    unsigned int new_range_begin, new_range_end;
    long tl; // for parsing
    //unsigned char ch;
    int i = 0;
    char *end, *end2;

    int opt;
    optind = 1; // work around for reentrancy
    while ((opt = getopt(argc, argv, "r:v")) != -1)
    {
        switch(opt)
        {
        case 'r':
            //range option either begin or begin-end
            range_begin = strtol (optarg, &end, 10);
            if (*end != '\0')
            {
                ++end;
                range_end = strtol (end, NULL, 10);
            }
            break;
        case 'v':
            verbose = !verbose;
            break;
        case '?':
        default:
            usage();
        }
    }

    if (argc - optind > 1)
        usage();

    printf ("P65 Audio Player\r\n");

    *w1 = 0x0000; // initialize audio

    if (argc - optind == 1)
    {
        //printf ("Loading %s\r\n", argv[optind]);
        if (LoadSong (argv[optind]))
        {
            if (range_begin >= songlen)
            {
                printf("Invalid range_begin: %d\r\n", range_begin);
                range_begin = 0;
            }
            if (range_end == 0)
            {
                range_end = songlen;
            }
            else if (range_end > songlen)
            {
                printf("Invalid range_end: %d\r\n", range_end);
                range_end = songlen;
            }
            printf ("range is %d - %d\r\n", range_begin, range_end);

        }
        else
        {
            warn ("load failed: %s", argv[optind]);
        }
    }


    for (;;)
    {
        printf ("%% "); // prompt
        fgets (command_buffer, 80, stdin);
        //printf ("read buffer\r\n");
        if (command_buffer[0] == 'q')
        {
            *w1 = 0x0000;  // kill audio
            break;
        }
        else if (command_buffer[0] == 'r')
        {
            new_range_begin = range_begin;
            new_range_end = range_end;
            errno = 0;
            tmp = strtol (command_buffer + 1, &end, 10);
            if ((errno == 0) && (end != command_buffer+1))
            {
                new_range_begin = tmp;
                if (*end != '\0')
                {
                    ++end;
                    errno = 0;
                    tmp = strtol (end, &end2, 10);
                    if ((errno == 0) && (end2 != end))
                        new_range_end = tmp;
                }

                if ((songlen > 0) && (new_range_end > songlen))
                    new_range_end = songlen;

                if (new_range_begin < new_range_end)
                {
                    range_begin = new_range_begin;
                    range_end = new_range_end;
                }

            }
            printf ("range is %d - %d\r\n", range_begin, range_end);
        }
        else if (command_buffer[0] == 's')
        {
            printf ("Playing scale\r\n");
            *w1 = 0xfb00;
            msleep(2000);
            *w1 = 0xfc00;
            msleep(2000);
            *w1 = 0xfd00;
            msleep(2000);
            *w1 = 0xfe00;
            msleep(2000);
            *w1 = 0xff00;
            msleep(2000);
            *w1 = 0xfe00;
            msleep(2000);
            *w1 = 0xfd00;
            msleep(2000);
            *w1 = 0xfc00;
            msleep(2000);
            *w1 = 0xfb00;
            msleep(2000);
            *w1 = 0x00ff;
        }
        else if (command_buffer[0] == 'S')
        {
            if (songlen > 0)
            {
                printf ("Playing song\r\n");
                PlaySong();
            }
            else
                printf ("No song loaded\r\n");
        }
        else if (command_buffer[0] == 'i')
        {
            if (songlen > 0)
            {
                printf ("Playing song using interrupt driver\r\n");
                PlaySongInt();
                printf ("finished\r\n");
            }
            else
                printf ("No song loaded\r\n");
        }
        /*
        else if (!strncmp (command_buffer, "on", 2))
        {
            *w1 = 0xff00; // turn on audio;
        }
        else if (!strncmp (command_buffer, "off", 3))
        {
            *w1 = 0x0000; // turn off audio;
        }
        */
        else
        {
            tl = strtol (command_buffer, &end, 16);
            if ((tl >= 0) && (tl < 4096))
            {
                tmp = (unsigned int)tl;
                printf ("playing hex code %x\r\n", tmp);
                *w1 = 0xf000 | tmp;
                msleep (2000);
                *w1 = 0x0000;
            }
            else
            {
              printf ("Invalid hex code\r\n");
            }
/*
            val1 = GetHexit(command_buffer[0]);
            val2 = GetHexit(command_buffer[1]);
            if ((val1 > 15) || (val2 > 15))
            {
              printf ("Invalid hex code\r\n");
            }
            else
            {
              printf ("playing hex code %c%c\r\n", command_buffer[0],command_buffer[1]);
              *r1 = (val1 << 4) | val2;
              msleep(2000);
              *r1 = 0xff;
            }
*/
        }

    }

    return 0;
}
	
