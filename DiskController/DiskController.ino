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

  // redo directory reading in a more useful way.

  // Todo: Refactor to get rid of dynamic allocation.

  // Todo: detect SD Card removal/insertion?
 
  // Todo: Because P:65 seek uses a signed int, we should probably fail to 
  //       open any file larger than 2^31 - 1 bytes.

#include <SD.h>
#include <stdlib.h>

// P65 uses the values that cc65 defines in its fcntl.h. These differ
// from Arduino SDK, Linux, etc.
#define P65_O_RDONLY 0x01
#define P65_O_WRONLY 0x02
#define P65_O_RDWR   0x03
#define P65_O_CREAT  0x10
#define P65_O_TRUNC  0x20
#define P65_O_APPEND 0x40
#define P65_O_EXCL   0x80

// P65 errno values. These are based off the cc65 errno values, but with
// bit 7 set so they're all negative values (except EOK).
#define P65_EOK    0
#define P65_ENOENT   0x80 |  1
#define P65_EINVAL   0x80 |  7
#define P65_EEXIST   0x80 |  9
#define P65_EIO      0x80 | 11
#define P65_ENOSYS   0x80 | 13
#define P65_EBADF    0x80 | 16
#define P65_EUNKNOWN 0x80 | 18

// The SD library doesn't actually use the lseek "whence" values, but they
// do get defined somewhere. These are copies of the values used by P:65,
// which come from cc65.
#define P65_SEEK_CUR        0
#define P65_SEEK_END        1
#define P65_SEEK_SET        2



constexpr int data0 = 1;
constexpr int data1 = 3;
constexpr int data2 = 4;
constexpr int data3 = 5;
constexpr int data4 = 6;
constexpr int data5 = 7;
constexpr int data6 = 8;
constexpr int data7 = 9; 
constexpr int ca1 = 0;
constexpr int ca2 = 2;    // needs to be 2 or 3 so we can recv interrupts.

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
  //++signal_received;
  signal_received = 1;
}

char ReadByte ()
{
  while (digitalRead(ca2) == HIGH)
    ;

  char result = (digitalRead(data0)==HIGH)?1:0;
  result += (digitalRead(data1)==HIGH)?2:0;
  result += (digitalRead(data2)==HIGH)?4:0;
  result += (digitalRead(data3)==HIGH)?8:0;
  result += (digitalRead(data4)==HIGH)?16:0;
  result += (digitalRead(data5)==HIGH)?32:0;
  result += (digitalRead(data6)==HIGH)?64:0;
  result += (digitalRead(data7)==HIGH)?128:0;

  noInterrupts();
  signal_received = 0;
  interrupts();

  digitalWrite (ca1, HIGH);

//  while (digitalRead(ca2) == LOW)
//    ;

  // we'll do this to make sure we don't somehow miss the positive edge of ca2
  do {} while (signal_received == 0);
  noInterrupts();
  signal_received = 0;
  interrupts();

  digitalWrite (ca1, LOW);

  return result;

}


void WriteByte (char data)
{
  while (digitalRead(ca2) == HIGH)
    ;

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

  noInterrupts();
  signal_received = 0;
  interrupts();

  // data is ready to be read, de-assert wait
  digitalWrite (ca1, HIGH);

//  while (digitalRead(ca2) == LOW)
//    ;

  // we'll do this to make sure we don't somehow miss the positive edge of ca2
  do {} while (signal_received == 0);
  noInterrupts();
  signal_received = 0;
  interrupts();

  pinMode (data0, INPUT);
  pinMode (data1, INPUT);
  pinMode (data2, INPUT);
  pinMode (data3, INPUT);
  pinMode (data4, INPUT);
  pinMode (data5, INPUT);
  pinMode (data6, INPUT);
  pinMode (data7, INPUT);


  digitalWrite (ca1, LOW);

}
 


const int buflen = 64;
char command_buffer[buflen];
int command_buffer_index = 0;
char command_output[buflen];
//int command_output_index = 0;

class FileIO
{
  public:
  virtual ~FileIO() {;}
  virtual int getChar() {return P65_ENOSYS;}
  virtual int putChar(char) {return P65_ENOSYS;}
  virtual long int seek(long int offset, int whence) {return 0x80000000 | P65_ENOSYS;}
};


struct dirent
{
  char d_name[13]; // 8.3 plus terminating zero
  char d_type;     // 1 for regular file, 2 for directory
  uint32_t d_size; // file size in bytes
};

class DirectoryReader2: public FileIO
{
  public:
  File dir;
  File entry;
  int read_position;
  int write_position;
  struct dirent dirent;
  char* buffer;

  DirectoryReader2 (File _dir)
  {
    static_assert(sizeof(dirent) == 18);
    dir = _dir;
    // we need to rewind the directory here. otherwise, the 
    // listing will start after the last file we opened.
    // Which says something interesting about how the SD
    // library is implemented.
    dir.rewindDirectory();
    read_position = 0;
    write_position = 0;
    buffer = (char*)&dirent;
  }
  
  virtual ~DirectoryReader2 ()
  {
    dir.close();
    entry.close();
  }
  
  int getChar() override
  {
    if (read_position == write_position)
    {
      // try to get another entry
      if (entry = dir.openNextFile())
      {
        strncpy (dirent.d_name, entry.name(), 12);
        dirent.d_name[12] = 0;
        dirent.d_type = entry.isDirectory()?2:1;
        dirent.d_size = entry.size();
        /*
        if (entry.isDirectory())
        {
          write_position = snprintf (command_output, buflen, "%-12s       <dir>\r\n", entry.name());          
        }
        else
        {
          write_position = snprintf (command_output, buflen, "%-12s  %10lu\r\n", entry.name(), entry.size());
        }
        */
        write_position = sizeof(struct dirent);
        read_position = 0;
        entry.close();
      }
      else
      {
        dir.close();
        return -1;
      }
    }
    char retval = buffer[read_position];
    ++read_position;
    //Serial.println (retval);
    return retval;
  }
  
};


// old string-format directory reader
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
  
  int getChar() override
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


class FileRW: public FileIO
{
  public:
  File file;
  int file_open;
  FileRW (File f)
  {
    file = f;//SD.open(filename);
    file_open = true;
  }
  
  virtual ~FileRW ()
  {
    if (file_open)
      file.close();
  }
  
  int getChar() override
  {
    if (!file_open)
      return -1;
    int retval = file.read();
    return retval;
  }
  
  int putChar(char ch) override
  {
    if (file_open)
    {
      return file.write (ch);
    }
    else
      return -1;
  }
  
  long int seek(long int offset, int whence) override
  {
    //return offset; // debug
    if (!file_open)
      return 0x80000000 | P65_EBADF;
    if (whence == P65_SEEK_CUR)
    {
      long int current = (long int)file.position();
      if (file.seek((uint32_t)(current + offset)))
        return (long int)file.position();
      else
        return 0x80000000 | P65_EIO;
    }
    else if (whence == P65_SEEK_END)
    {
      long int end = (long int)file.size();
      if (file.seek((uint32_t)(end + offset)))
        return (long int)file.position();
      else
        return 0x80000000 | P65_EIO;
    }
    else if (whence == P65_SEEK_SET)
    {
      if (file.seek((uint32_t)offset))
        return (long int)file.position();
      else
        return 0x80000000 | P65_EIO;
    }
    else
      return 0x80000000 | P65_EINVAL; // bad whence value
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


FileIO* channel_io[3] = {NULL,NULL,NULL};
char state = ' ';


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
  attachInterrupt (digitalPinToInterrupt(ca2)/*0*/, InterruptHandler, RISING);
  
  pinMode (13,OUTPUT);
  digitalWrite (13,LOW);

  //digitalWrite (ca1, LOW);
  
  delayMicroseconds(50);
  digitalWrite (ca1, LOW);
  //digitalWrite (ca2, LOW);
  
  pinMode (10, OUTPUT);
  if (!SD.begin(10))
    ErrorFlash();
      
  //Serial.begin (9600);

  state = ' ';
}


// The values in SD library for mode flags don't match the values used by
// cc65 and P:65. So we need to translate.
char TranslateMode (char in)
{
  char mode = 0;
  if (in & P65_O_RDONLY)
    mode |= O_RDONLY;
  if (in & P65_O_WRONLY)
    mode |= O_WRONLY;
  if (in & P65_O_CREAT)
    mode |= O_CREAT;
  if (in & P65_O_APPEND)
    mode |= O_APPEND;
  if (in & P65_O_TRUNC)
    mode |= O_TRUNC;
  if (in & P65_O_EXCL)
    mode |= O_EXCL;

  return mode;
}


/** Parse a file open command and create file reader/writer.
 *  Returns a CommandResponse object with status and an
 *  optional error message.
 *  Note that we have to specify the return type as 
 *  "class CommandResponse*" or the Arduino IDE gives a completely
 *  nonsensical error message.
 */
class CommandResponse* HandleFileOpen (char* command_buffer)
{
  if (strlen(command_buffer) < 4)
  {
    char error = P65_EINVAL;
    return new CommandResponse (&error, 1);
  }
  
  int channel = command_buffer[1] - 48;  // convert ascii to int
  if (channel < 1 || channel > 3)
  {
    char error = P65_EINVAL;
    return new CommandResponse (&error, 1);
  }

  char mode = command_buffer[2];
  char sd_mode = TranslateMode(mode);
  
  char* filename = command_buffer + 3;

  if (mode & P65_O_WRONLY)
  {
    if ((mode & P65_O_TRUNC) && (SD.exists(filename)))
      SD.remove(filename);
    delete (channel_io[channel]);
    File f = SD.open (filename, FILE_WRITE);
    if (f)
    {
      if (f.isDirectory())
      {
        // opening a directory for writing is not allowed! On Linux this
        // would return with EISDIR, but that's not an option here.
        f.close();
        char error = P65_EUNKNOWN;
        return new CommandResponse (&error, 1);
      }
      else
      {
        channel_io[channel] = new FileRW (f);
        char error = 1; // regular file
        return new CommandResponse (&error, 1);
      }
    }
    else
    {
      char error = P65_EIO;
      return new CommandResponse (&error, 1);
    }
  }
  else if (mode & P65_O_RDONLY)
  { 
    // While "/" is a valid filename to pass to SD.open() in order to read the root
    // directory, it is not accepted by SD.exists(). So we have to treat it specially
    // here.
    if (!SD.exists(filename) && strcmp(filename,"/"))
    {
      char error = P65_ENOENT;
      return new CommandResponse (&error, 1);
    }
    delete (channel_io[channel]);
    File f = SD.open(filename);
    if (f)
    {
      if (f.isDirectory())
      {
        channel_io[channel] = new DirectoryReader2 (f);
        char error = 2; // directory
        return new CommandResponse (&error, 1);
      }
      else
      {
        channel_io[channel] = new FileRW (f);
        char error = 1; // regular file
        return new CommandResponse (&error, 1);
      }
    }
    else
    {
      char error = P65_EIO;
      return new CommandResponse (&error, 1);
    }
  }
  
  // fallthrough error
  char error = P65_EINVAL;
  return new CommandResponse (&error, 1);
}


char HandleDeleteFile (char* command_buffer)
{
  char* filename = command_buffer + 3;
  if (*filename == 0)
    return P65_EINVAL;
  if (!SD.exists(filename))
    return P65_ENOENT;
  if (SD.remove(filename))
    return P65_EOK;
  else
    return P65_EIO;
}


char HandleDeleteDirectory (char* command_buffer)
{
  char* filename = command_buffer + 6;
  if (*filename == 0)
    return P65_EINVAL;
  if (!SD.exists(filename))
    return P65_ENOENT;
  if (SD.rmdir(filename))
    return P65_EOK;
  else
    return P65_EIO;
}


char HandleMkdir (char* command_buffer)
{
  char* filename = command_buffer + 6;
  if (*filename == 0)
    return P65_EINVAL;
  if (SD.exists(filename))
    return P65_EEXIST;
  if (SD.mkdir(filename))
    return P65_EOK;
  else
    return P65_EIO;
}


char HandleFileClose (char* command_buffer)
{
  int channel = command_buffer[1] - 48; // cheap conversion
  if (channel < 1 || channel > 3)
    return P65_EINVAL;
  delete (channel_io[channel]);
  channel_io[channel] = 0;
  return P65_EOK;
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
      WriteByte((char)2);   // eof
    }
    break;
  case 0x18:  // 6502 wants to write a byte
    char c = ReadByte();
    if (channel == 0)
    {
      if (state == ' ')
      {
        if (c == 'k')
        {
          state = 'k';
        }
        else
        {
          command_buffer[command_buffer_index++] = c;
          state = 's'; // read a null-terimnated string
        }
      }
      else if (state == 'k')
      {
        // on reflection, maybe this was dumb. And just string encoding
        // with, say, hex for the int, would be simpler in the long run.
        // the seek command is a channel number, long int, and char
        command_buffer[command_buffer_index++] = c;
        if (command_buffer_index == 6)
        {
          delete (channel_io[0]);
          channel_io[0] = nullptr;
          command_buffer_index = 0;
          state = ' ';

          // ready to execute
          int seek_channel = command_buffer[0];
          long int offset = *(long int*)(command_buffer + 1);
          int whence = command_buffer[5];
          if ((seek_channel > 0) && (seek_channel < 3) && channel_io[seek_channel])
          {
            long int response_code = channel_io[seek_channel]->seek(offset, whence);
            channel_io[0] = new CommandResponse ((char*)&response_code,4);
          }
          else
          {
            // invalid or not open channel number
            long int response_code = 0x80000000 | P65_EINVAL;
            channel_io[0] = new CommandResponse ((char*)&response_code,4);
          }
        }
      }
      else if (state == 's')
      {
        // the command is a 0-terminated array of 0-terminated strings.
        if (command_buffer_index < buflen)
          command_buffer[command_buffer_index++] = c;
        if ((c == 0) && (command_buffer_index > 1) && (command_buffer[command_buffer_index-2] == 0))
        {
          bool length_error = (command_buffer_index >= buflen);
          //Serial.print ("command buffer is '");
          //Serial.print (command_buffer);
          //Serial.println ("'\n");
          command_buffer_index = 0;
          delete (channel_io[0]);
          channel_io[0] = nullptr;
          state = ' ';

          if (length_error)
          {
            char response_code = P65_EINVAL;
            channel_io[0] = new CommandResponse (&response_code,1);
          }
          // else if (!strncmp (command_buffer, "ls ", 3))
          // {
          //   channel_io[0] = new DirectoryReader (command_buffer + 3);
          // }
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
            char response_code = HandleDeleteFile (command_buffer);
            channel_io[0] = new CommandResponse (&response_code,1);
          }
          else if (!strncmp (command_buffer, "rmdir ",6))
          {
            char response_code = HandleDeleteDirectory (command_buffer);
            channel_io[0] = new CommandResponse (&response_code,1);
          }
          else if (!strncmp (command_buffer, "mkdir ",6))
          {
            char response_code = HandleMkdir (command_buffer);
            channel_io[0] = new CommandResponse (&response_code,1);
          }
        }
      }
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


