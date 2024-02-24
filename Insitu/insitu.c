/* Copyright (c) 2013, Christopher Just
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

/* Compile with: cl65 -t p65 --config p65.cfg insitu.c insitu_io.asm */

#include <stdlib.h>
#include <string.h>

// declarations for assembly functions in insitu_io.asm

/** Print a single character. */
void __fastcall__ sendchar (char ch);

/** Print the hex representation of ch. */
void __fastcall__ print_hex (char ch);

/** Write a null-terminated string. */
void __fastcall__ outputstring(char* ch);

/** Perform an XModem download. */
void __fastcall__ XModem (void);

/** Blocking version of get character. */
char __fastcall__ getch (void);

void __fastcall__ disable_interrupts (void)
{
	__asm__ ("sei");
}

void __fastcall__ enable_interrupts (void)
{
	__asm__ ("cli");
}


// I wouldn't ordinarily use a bunch of global variables for a program 
// like this, but given the limitations of a 6502 system and cc65
// there are probably actual advantages to doing things this way.

// buffer for command-line parsing.
char command_buffer[80];
//char output_buffer[80];
// pointer to receive buffer.
char* image_buffer = NULL;
// size of receive buffer.
int image_buffer_size = 0;
// nonzero if an image has been downloaded.
int image_buffer_filled = 0;

// test data for the write & verify test routines.
char test_data[] = "bah weep granna weep";



// Reads a command string from the user, with some minimal processing
// of backspace & such. Returns when the user presses Enter (which is
// not included in the returned string).
void ReadCommand()
{
	int i = 0;
	char ch;
	command_buffer[0] = 0;
	outputstring ("> ");
	//outputstring ("btw, ReadCommand::i is at 0x");
	//_print_hex(((int)(&i))>>8);
	//_print_hex(((int)(&i))&0xff);
	//while (i < 79)
	for (;;)
	{
		ch = getch();
		if (ch == '\n' || ch == '\r')
		{
			sendchar('\r');
			sendchar('\n');
			break;
		}
		else if (ch == 8) 		// backspace
		{
			if (i == 0)
				sendchar(7); 	// bell
			else
			{
				--i;
				sendchar(ch);
			}
		}
		else if (i == 79)    	// end of buffer
		{
			sendchar(7);		// bell
		}
		else if (ch)
		{
			command_buffer[i++] = ch;
			sendchar(ch);
		}
	}
	command_buffer[i] = 0;
}



/** Write data from a buffer to an address in EEPROM.
 *  (It can write to RAM, too, but that'd be silly).
 *  @param data Data to be written.
 *  @param data_length Length of data array.
 *  @param dest Destination address in EEPROM.
 *  @return 0 for failure, 1 for success.
 */
unsigned int WriteROM (char* data, int data_length, char* dest)
{
	unsigned int i = 0, j = 0;
	unsigned int error_count = 0;
	
#if 0
	// just a test loop. it almost seems like some ram is being overwritten.
	for (i = 0; i < data_length; ++i)
	{
		if (i%256 == 0)
			sendchar('*');
	}
#endif

	for (i = 0; i < data_length; ++i)
	{
		
		dest[i] = data[i];
		
		// For page mode writes, we only need to pause and read at the
		// end of a page. For byte-mode, we need to pause after every
		// byte we read.
		if (1)//(i % 64 == 63)
		{
			// Wait until we read back the data we wrote.
			// In cc65, we don't have to worry about 
			// using the volatile keyword.
			for (j = 0; j < 65000U; ++j)
			{
				if (dest[i] == data[i])
					break;
			}
			if (j == 65000U) // took too long - write failed?
				++error_count;
		}

		// let's have some progress indication, ok?
		if (i%256 == 255)
			sendchar('.');
	}

	return error_count;
}



/** Compare data in buffer to an address in EEPROM.
 *  @param data Data to be written.
 *  @param data_length Length of data array.
 *  @param dest Destination address in EEPROM.
 *  @return The number of differences between data
 *          and dest.
 */
unsigned int VerifyROM (char* data, int data_length, char* dest)
{
	unsigned int i = 0, num_errors = 0;
	for (i = 0; i < data_length; ++i)
	{
		if (dest[i] != data[i])
			++num_errors;
		
		// let's have some progress indication, ok?
		if (i%256 == 0)
			sendchar('.');

	}
	return num_errors;
}



int main (void)
{
	// The address of the xmodem receive buffer is tored at 0x020C.
	char** xmodem_save_addr = (char**)0x020c;
	unsigned int error_count = 0;

	disable_interrupts();
	outputstring ("Insitu EEPROM Programmer.\r\n");
	
	image_buffer_size = 8 * 1024;
	image_buffer = (char*)malloc(2*image_buffer_size);
	if (image_buffer == NULL)
	{
		outputstring ("Failed to allocate EEPROM image buffer.\r\n");
		return 1;
	}

	*xmodem_save_addr = image_buffer;
	image_buffer_filled = 0; // set to 1 once we download an image
	
	outputstring ("  XModem receive buffer at ");
	print_hex((char)((int)image_buffer >> 8));
	print_hex((char)((int)image_buffer & 0x00ff));
	outputstring (".\r\n");
	outputstring ("  Enter 'help' for options.\r\n");
	
	for (;;)
	{
		ReadCommand();

		if (!strcmp (command_buffer, "help"))
		{
			outputstring ("Commands:\r\n");
			outputstring ("  download - Download a ROM image using XModem.\r\n");
			outputstring ("  write    - Write the current image.\r\n");
			outputstring ("  verify   - Compare the current image and the ROM contents.\r\n");

		}
		else if (!strcmp (command_buffer, "writetest"))
		{
			outputstring ("Writing test data.\r\n");
			if (WriteROM (test_data, strlen(test_data), (char*)0xFF00))
				outputstring ("Write succeeded.\r\n");
			else
				outputstring ("Write failed.\r\n");
		}
		else if (!strcmp (command_buffer, "verifytest"))
		{
			outputstring ("Verifying test data.\r\n");
			if (strncmp (test_data, (char*)0xFF00, strlen(test_data)))
				outputstring ("Verification failed.\r\n");
			else
				outputstring ("Verification succeeded.\r\n");
		}
		else if (!strcmp (command_buffer, "write"))
		{
			if (image_buffer_filled)
			{
				outputstring ("Writing EEPROM image.\r\n");
				error_count = WriteROM (image_buffer, image_buffer_size, (char*)0xE000);
				if (error_count == 0)
					outputstring ("Write succeeded.\r\n");
				else
				{
					//sprintf (output_buffer, "Write failed: %d errors.\r\n", error_count);
					outputstring ("Write failed.\r\n");
				}
			}
			else
			{
				outputstring ("Error - no EEPROM image loaded.\r\n");
			}
		}
		else if (!strcmp (command_buffer, "verify"))
		{
			if (image_buffer_filled)
			{
				int num_errors;
				outputstring ("Verifying EEPROM image.\r\n");
				num_errors = VerifyROM (image_buffer, image_buffer_size, (char*)0xE000);
				if (num_errors > 0)
					outputstring ("Verification failed.\r\n");
				else
					outputstring ("Verification succeeded.\r\n");
			}
			else
			{
				outputstring ("Error - no EEPROM image loaded.\r\n");
			}
		}
		else if (!strcmp (command_buffer, "download"))
		{
			outputstring ("Downloading EEPROM image to RAM buffer.\r\n");
			XModem();
			// Is there a good way to check if the xmodem DL has succeeded?
			outputstring ("XModem download complete.\r\n");
			image_buffer_filled = 1;
		}
		else if (!strcmp (command_buffer, "quit"))
		{
			return 0;
		}
		else
		{
			outputstring ("syntax error.\r\n");
		}
	}

	enable_interrupts();

	return 0;
}
	
