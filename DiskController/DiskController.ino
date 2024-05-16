/* Copyright (c) 2017, Christopher Just
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

/* Source code for the ATmega328P-based "disk controller" for the 
 * Project:65 homebrew computer. It uses the Arduino SD-card library 
 * to read/write Fat32 filesystems, and uses a parallel interface to 
 * communicate with a 6522 VIA on the P:65 computer.
 */

  // Todo: Try to redo the interface using pullup
  // resistors, and having the pins on each side
  // of the interface either low or high-impedance,
  // to better ensure both sides never try to drive
  // the bus simultaneously. 

  // Todo: Management of current directory, cwd
  // support, etc.

  // Todo: detect SD Card removal/insertion?
 
  // DONE: Change file open flags to a more Unix-style bitmask.

#include <SD.h>
#include <stdlib.h>
 
const int data0 = 1;
const int data1 = 3;
const int data2 = 4;
const int data3 = 5;
const int data4 = 6;
const int data5 = 7;
const int data6 = 8;
const int data7 = 9; 
const int ca1 = 0;
const int ca2 = 2;    // needs to be 2 or 3 so we can recv interrupts.

volatile int signal_received = 0;

void ErrorFlash()
{
  pinMode (13, OUTPUT);
  
  for (;;)
  {
    digitalWrite (13, HIGH);
    delay (500);
    digitalWrite (13, LOW);
    delay (500);
  }
}

 
void InterruptHandler()
{
  ++signal_received;
}

char ReadByte ()
{
  do {} while (signal_received == 0);
  noInterrupts();
  --signal_received;
  interrupts();

  pinMode (data0, INPUT);
  pinMode (data1, INPUT);
  pinMode (data2, INPUT);
  pinMode (data3, INPUT);
  pinMode (data4, INPUT);
  pinMode (data5, INPUT);
  pinMode (data6, INPUT);
  pinMode (data7, INPUT);

  // make sure pullups aren't on - is this needed?
  /*
  digitalWrite (data0, LOW);
  digitalWrite (data1, LOW);
  digitalWrite (data2, LOW);
  digitalWrite (data3, LOW);
  digitalWrite (data4, LOW);
  digitalWrite (data5, LOW);
  digitalWrite (data6, LOW);
  digitalWrite (data7, LOW);
  */

  char result = (digitalRead(data0)==HIGH)?1:0;
  result += (digitalRead(data1)==HIGH)?2:0;
  result += (digitalRead(data2)==HIGH)?4:0;
  result += (digitalRead(data3)==HIGH)?8:0;
  result += (digitalRead(data4)==HIGH)?16:0;
  result += (digitalRead(data5)==HIGH)?32:0;
  result += (digitalRead(data6)==HIGH)?64:0;
  result += (digitalRead(data7)==HIGH)?128:0;
  
  digitalWrite (ca1, LOW);
  digitalWrite (ca1, HIGH);  // pulse indicates we've read

  return result;
}


void WriteByte (char data)
{
  pinMode (data0, OUTPUT);
  pinMode (data1, OUTPUT);
  pinMode (data2, OUTPUT);
  pinMode (data3, OUTPUT);
  pinMode (data4, OUTPUT);
  pinMode (data5, OUTPUT);
  pinMode (data6, OUTPUT);
  pinMode (data7, OUTPUT);
  
  digitalWrite (data0, data & 0x01);
  digitalWrite (data1, data & 0x02);
  digitalWrite (data2, data & 0x04);
  digitalWrite (data3, data & 0x08);
  digitalWrite (data4, data & 0x10);
  digitalWrite (data5, data & 0x20);
  digitalWrite (data6, data & 0x40);
  digitalWrite (data7, data & 0x80);

  digitalWrite (ca1, LOW);  // pulse indicates we've written
  digitalWrite (ca1, HIGH);

  do {} while (signal_received == 0);
  noInterrupts();
  --signal_received;
  interrupts();
}
 

char data[] = "HELLO, WORLD!\n";
int i = 0;
int len = 14;

const int buflen = 64;
char command_buffer[buflen];
int command_buffer_index = 0;
char command_output[buflen];
int command_output_index = 0;

class FileIO
{
  public:
  virtual ~FileIO() {;}
  virtual int getChar() {return 0;}
  virtual void putChar(char) {;}
};


class DirectoryReader: public FileIO
{
  public:
  File dir;
  File entry;
  int read_position;
  int write_position;
  int count;
  DirectoryReader (char* dirname)
  {
    dir = SD.open(dirname);
    // we need to rewind the directory here. otherwise, the 
    // listing will start after the last file we opened.
    // Which says something interesting about how the SD
    // library is implemented.
    dir.rewindDirectory();
    read_position = 0;
    write_position = 0;
    count = 0;
  }
  
  virtual ~DirectoryReader ()
  {
    dir.close();
    entry.close();
  }
  
  virtual int getChar()
  {
    if (read_position == write_position)
    {
      // try to get another entry
      if (entry = dir.openNextFile())
      {
        if (entry.isDirectory())
        {
          write_position = snprintf (command_output, buflen, "%-12s       <dir>\r\n", entry.name());          
        }
        else
        {
          write_position = snprintf (command_output, buflen, "%-12s  %10lu\r\n", entry.name(), entry.size());
        }
        read_position = 0;
        entry.close();
      }
      else
      {
        dir.close();
        return -1;
      }
    }
    char retval = command_output[read_position];
    ++read_position;
    //Serial.println (retval);
    return retval;
  }
  
};



class FileReader: public FileIO
{
  public:
  File file;
  int read_position;
  int write_position;
  int count;
  int file_open;
  FileReader (char* filename)
  {
    file = SD.open(filename);
    file_open = true;
    read_position = 0;
    write_position = 0;
    count = 0;
  }
  
  virtual ~FileReader ()
  {
    if (file_open)
      file.close();
  }
  
  virtual int getChar()
  {
    if (!file_open)
      return -1;
    int retval = file.read();
    if (retval == -1)
    {
      file.close();
      file_open = false;
    }
    return retval;
  }
  
};


class FileWriter: public FileIO
{
  public:
  File file;
  int read_position;
  int write_position;
  int count;
  int file_open;
  FileWriter (char* filename)
  {
    file = SD.open(filename, FILE_WRITE);
    file_open = true;
    read_position = 0;
    write_position = 0;
    count = 0;
  }
  
  virtual ~FileWriter ()
  {
    if (file_open)
      file.close();
  }
  
  virtual void putChar(char ch)
  {
    if (file_open)
    {
      file.write (ch);
    }
  }
  
  virtual int getChar()
  {
    return -1;
  }
  
};


class CommandResponse: public FileIO
{
  public:
  int read_position;
  int write_position;
  char buffer[30];
  CommandResponse (const char* output, int len)
  {
    memcpy (buffer, output, len);
    read_position = 0;
    write_position = len;
  }
  
  void Append (const char* output, int len)
  {
    memcpy (buffer+write_position, output, len);
    write_position += len;
  }
  
  virtual ~CommandResponse ()
  { ; }
  
  virtual int getChar()
  {
    int retval = -1;
    if (read_position < write_position)
    {
      retval = buffer[read_position];
      ++read_position;
    }
    return retval;
  }
  
};

//FileIO* command_io = 0;
FileIO* channel_io[3] = {NULL,NULL,NULL};



void setup()
{
  pinMode (data0, INPUT);
  pinMode (data1, INPUT);
  pinMode (data2, INPUT);
  pinMode (data3, INPUT);
  pinMode (data4, INPUT);
  pinMode (data5, INPUT);
  pinMode (data6, INPUT);
  pinMode (data7, INPUT);
  pinMode (ca1, OUTPUT);
  pinMode (ca2, INPUT);
  attachInterrupt (digitalPinToInterrupt(ca2)/*0*/, InterruptHandler, FALLING);
  
  pinMode (13,OUTPUT);
  digitalWrite (13,LOW);
  
  delayMicroseconds(50);
  digitalWrite (ca1, HIGH);
  digitalWrite (ca2, LOW);
  
  pinMode (10, OUTPUT);
  if (!SD.begin(10))
    ErrorFlash();
    
  //channel_io[0] = channel_io[1] = 0;
  
  //Serial.begin (9600);
}

// P65 uses the values that cc65 defines in its fcntl.h. These differ
// from Arduino SDK, Linux, etc.
#define P65_O_RDONLY 0x01
#define P65_O_WRONLY 0x02
#define P65_O_RDWR   0x03
#define P65_O_CREAT  0x10
#define P65_O_TRUNC  0x20
#define P65_O_APPEND 0x40
#define P65_O_EXCL   0x80

/** Parse a file open command and create file reader/writer.
 *  Returns a CommandResponse object with status and an
 *  optional error message.
 *  Note that we have to specify the return type as 
 *  "class CommandResponse*" or the Arduino IDE gives a completely
 *  nonsensical error message.
 */
class CommandResponse* HandleFileOpen (char* command_buffer)
{
  //char* colon_location = strchr (command_buffer,':');
  if (strlen(command_buffer) < 4)
  {
    return new CommandResponse ("0Bad Command Format", 19);
  }
  
  int channel = command_buffer[1] - 48;  // cheap conversion
  if (channel < 1 || channel > 3)
  {
    return new CommandResponse ("0Bad Channel Number", 19);
  }

  char mode = command_buffer[2];
  
  char* filename = command_buffer + 3;

  if (mode & P65_O_RDONLY) //!strncmp (command_buffer+2, "r:", 2))
  { 
    if (!SD.exists(filename))
      return new CommandResponse ("0File Not Found", 15);
    delete (channel_io[channel]);
    channel_io[channel] = new FileReader (filename);
    return new CommandResponse ("1",1);
  }
  else if (mode & P65_O_WRONLY) //!strncmp (command_buffer+2, "w:", 2))
  {
    if ((mode & P65_O_TRUNC) && (SD.exists(filename)))
      SD.remove(filename);
    delete (channel_io[channel]);
    channel_io[channel] = new FileWriter (filename);
    return new CommandResponse ("1", 1);
  }
//  else if (!strncmp (command_buffer+2, "a:", 2))
//  {
//    // sd library gives us the append behavior by default
//    delete (channel_io[channel]);
//    channel_io[channel] = new FileWriter (filename);
//    return new CommandResponse ("1", 1);
//  }
  
  return 0;
}


int HandleDeleteFile (char* command_buffer)
{
  char* filename = command_buffer + 3;
  if ((*filename == 0) || !SD.exists(filename))
    return 0;
  if (SD.remove(filename))
    return 1;
  else
    return 0;
}


int HandleDeleteDirectory (char* command_buffer)
{
  char* filename = command_buffer + 6;
  if ((*filename == 0) || !SD.exists(filename))
    return 0;
  if (SD.rmdir(filename))
    return 1;
  else
    return 0;
}


int HandleMkdir (char* command_buffer)
{
  char* filename = command_buffer + 6;
  if (*filename == 0)
    return 0;
  if (SD.mkdir(filename))
    return 1;
  else
    return 0;
}


int HandleFileClose (char* command_buffer)
{
  //return 1;
  int channel = command_buffer[1] - 48; // cheap conversion
  if (channel < 1 || channel > 3)
    return 0;
  delete (channel_io[channel]);
  channel_io[channel] = 0;
  return 1;
}


void loop()
{
  char channel = ReadByte();
  char command = ReadByte();
  
  switch (command)
  {
  case 0x19:  // 6502 wants to read a byte
    if (channel_io[channel])
    {
      int ch = channel_io[channel]->getChar();
      if (ch == -1)
      {
        WriteByte((char)2);   // eof
      }
      else
      {
        WriteByte ((char)0);
        WriteByte ((char)ch);
      }
    }
    else
    {
      WriteByte((char)2);
    }
    break;
  case 0x18:  // 6502 wants to write a byte
    char c = ReadByte();
    if (channel == 0)
    {  
      command_buffer[command_buffer_index] = c;
      if (c == 0 || command_buffer_index == buflen-1)
      {
        command_buffer[command_buffer_index] = 0;
        //Serial.print ("command buffer is '");
        //Serial.print (command_buffer);
        //Serial.println ("'\n");
        command_buffer_index = 0;
        if (!strncmp (command_buffer, "ls ", 3))
        {
          delete (channel_io[0]);
          channel_io[0] = new DirectoryReader (command_buffer + 3);
        }
        else if (!strncmp (command_buffer, "o",1))
        {
          channel_io[0] = HandleFileOpen (command_buffer);
        }
        else if (!strncmp (command_buffer, "c",1))
        {
          HandleFileClose (command_buffer); 
        }
        else if (!strncmp (command_buffer, "rm ",3))
        {
          int response_code = HandleDeleteFile (command_buffer);
          if (response_code)
            channel_io[0] = new CommandResponse ("1",1);
          else
            channel_io[0] = new CommandResponse ("0",1);
        }
        else if (!strncmp (command_buffer, "rmdir ",6))
        {
          int response_code = HandleDeleteDirectory (command_buffer);
          if (response_code)
            channel_io[0] = new CommandResponse ("1",1);
          else
            channel_io[0] = new CommandResponse ("0",1);
        }
         else if (!strncmp (command_buffer, "mkdir ",6))
        {
          int response_code = HandleMkdir (command_buffer);
          if (response_code)
            channel_io[0] = new CommandResponse ("1",1);
          else
            channel_io[0] = new CommandResponse ("0",1);
        }
     }
      else
        ++command_buffer_index;
    }
    else
    {
      if (channel_io[channel])
      {
        channel_io[channel]->putChar(c);
      }
    }
    break;
  }    
  
}


