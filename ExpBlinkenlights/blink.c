/* ExpBlinkenlights
 * Copyright (c) 2024 Christopher Just
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

/* Compile with: cl65 -t p65 --config p65.cfg blink.c blink_io.asm */

#include <stdlib.h>
#include <string.h>

// declarations for asm functions:

// Busy wait
void __fastcall__ sleep(int ms);

// Non-blocking get character. Returns 0 if no char available
char __fastcall__ getch (void);

// Calls outputstring() in P:65 ROM
void __fastcall__ (*outputstring)(char* ch) = (void*)0xffe3;


// Each of these Pattern functions sets & clears the lines
// at locations $A000, $A001, and $A002. For convenience,
// we treat these 3 8-bit registers as a single 32-bit
// word (along with $A003, though there isn't any hardware
// there on the current version of the board).

// On the blinkenlights expansion board, the LEDs are
// connected between the IO lines and +5V. That means
// the LEDs are lit when the lines are 0, and unlit
// when the lines are 1. Write FF or 00 to a register
// to turn all its LEDs off or on, respectively.


// the classic Knight Rider / Larson scanner
void Pattern1 (unsigned long int* b, int count)
{
	int i,j;
	unsigned long int v = 0x01;
	*b = 0xffffff;
	for (j = 0; j < count; ++j)
	{
	for (i = 0; i < 23; ++i)
	{
		sleep(50);
		v = v << 1;
		*b = ~v;
	}
	for (i = 0; i < 23; ++i)
	{
		sleep(50);
		v = v >> 1;
		*b = ~v;
	}
	}
}


// mirrored Larson scanners
void Pattern2 (unsigned long int* b, int count)
{
	int i,j;
	unsigned long int v1 = 0x03, v2 = 0x00c00000;

	for (j = 0; j < count; ++j)
	{
		v1 = 0x03;
		v2 = 0x00c00000;
		for (i = 0; i < 22; ++i)
		{
			sleep(50);
			v1 = v1 << 1;
			v2 = v2 >> 1;
			*b = ~(v1 | v2);
		}
	}
}


// 4on/4off pattern
void Pattern3 (unsigned long int* b, int count)
{
	int j;
	for (j = 0; j < count; ++j)
	{
		sleep(200);
		*b = 0x00f0f0f0;
		sleep(200);
		*b = 0x000f0f0f;
	}
}


// 2on/2off pattern
void Pattern4 (unsigned long int* b, int count)
{
	int j;
	for (j = 0; j < count; ++j)
	{
		sleep(200);
		*b = 0x00aaaaaa;
		sleep(200);
		*b = 0x00555555;
	}
}



int main (void)
{
	unsigned long int* r1 = (unsigned long int*)(0xa000); // expansion card base address

	outputstring ("Expansion Card blinkenlights test.\r\n");

	for (;;)
	{
		if (getch() == 'q')
			break;
		Pattern3 (r1, 10);
		if (getch() == 'q')
			break;
		Pattern2 (r1,5);
		if (getch() == 'q')
			break;
		Pattern1 (r1,5);
		if (getch() == 'q')
			break;
		Pattern4 (r1,10);
	}

	// Turn off all lights
	*r1 = 0xffffff;
	
	return 0;
}
	
