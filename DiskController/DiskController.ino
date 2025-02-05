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

// Todo: Because P:65 seek uses a signed int, we should probably fail to
//       open any file larger than 2^31 - 1 bytes.

// Todo: File objects are being passed around by value and don't have 
//       destructors. That kind of works because they just hold a pointer
//       to another SdFile struct that's part of the SdFat lib. BUT that
//       means we probably ought to be careful about making sure we
//       close File objects. We can't depend on RAII to do this for us.

#include <SD.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>


// P65 uses the values that cc65 defines in its fcntl.h. These differ
// from Arduino SDK, Linux, etc.
constexpr uint8_t P65_O_RDONLY = 0x01;
constexpr uint8_t P65_O_WRONLY = 0x02;
constexpr uint8_t P65_O_RDWR = 0x03;
constexpr uint8_t P65_O_CREAT = 0x10;
constexpr uint8_t P65_O_TRUNC = 0x20;
constexpr uint8_t P65_O_APPEND = 0x40;
constexpr uint8_t P65_O_EXCL = 0x80;

// P65 errno values. These are based off the cc65 errno values, but with
// bit 7 set so they're all negative values (except EOK).
constexpr uint8_t P65_EOK = 0;
constexpr uint8_t P65_ENOENT = 0x80 | 1;
constexpr uint8_t P65_EINVAL = 0x80 | 7;
constexpr uint8_t P65_EEXIST = 0x80 | 9;
constexpr uint8_t P65_EIO = 0x80 | 11;
constexpr uint8_t P65_ENOSYS = 0x80 | 13;
constexpr uint8_t P65_ERANGE = 0x80 | 15;
constexpr uint8_t P65_EBADF = 0x80 | 16;
constexpr uint8_t P65_EUNKNOWN = 0x80 | 18;
constexpr uint8_t P65_ENOTDIR = 0x80 | 20;
constexpr uint8_t P65_EISDIR = 0x80 | 21;
constexpr uint8_t P65_EBADCMD = 0x80 | 22;

// The SD library doesn't actually use the lseek "whence" values, but they
// do get defined somewhere. These are copies of the values used by P:65,
// which come from cc65.
constexpr uint8_t P65_SEEK_CUR = 0;
constexpr uint8_t P65_SEEK_END = 1;
constexpr uint8_t P65_SEEK_SET = 2;

// Indices of the first and last file channels. Command channel is 0.
constexpr int MIN_CHANNEL = 1;
constexpr int MAX_CHANNEL = 2;


constexpr int data0 = 1;
constexpr int data1 = 3;
constexpr int data2 = 4;
constexpr int data3 = 5;
constexpr int data4 = 6;
constexpr int data5 = 7;
constexpr int data6 = 8;
constexpr int data7 = 9;
constexpr int ca1 = 0;
constexpr int ca2 = 2;  // needs to be 2 or 3 so we can recv interrupts.

volatile int signal_received = 0;

void ErrorFlash()
{
    pinMode(13, OUTPUT);

    for (;;)
    {
        digitalWrite(13, HIGH);
        delay(500);
        digitalWrite(13, LOW);
        delay(500);
    }
}

/*
void InterruptHandler()
{
    //++signal_received;
    signal_received = 1;
}
*/


char ReadByte()
{
    while (PIND & 4) // wait for CA2 to go low
        ;

    char result = 
        ((PIND & 0b00000010) >> 1) |
        ((PIND & 0b11111000) >> 2) |
        ((PINB & 0b00000011) << 6);

    PORTD |= 1; // Set CA1 high

    // Wait for CA2 to go high
    while ((PIND & 4) == 0)
        ;

    PORTD &= 0b11111110;  // set CA1 low

    return result;
}


void WriteByte(char data)
{
    while (PIND & 4) // wait for CA2 to go low
        ;

    DDRD = 0b11111011;
    DDRB |= 0b00000011;

    PORTD = ((data & 0b00000001) << 1) | ((data & 0b11111110) << 2);
    PORTB = (PORTB & 0b11111100) | (data >> 6);

    // data is ready to be read, de-assert wait
    PORTD |= 1; // Set CA1 high

    // Wait for CA2 to go high
    while ((PIND & 4) == 0)
        ;

    // Set data lines to inputs.
    DDRD = 0b00000001;
    DDRB &= 0b11111100;

    PORTD &= 0b11111110; // Set CA1 low
}



/* Writes a character with standard escaping. -1 is treated as EOF. */
void WriteEscapedChar (int ch)
{
    if (ch == -1)
    {
        WriteByte((char)0x1b);  // Send EOF: esc -1
        WriteByte((char)0xff);
    }
    else if (ch == 0x1b)  // escape character
    {
        WriteByte((char)0x1b);
        WriteByte((char)0x1b);  // Send escaped escape
    }
    else
    {
        WriteByte((char)ch);
    }
}



void WriteEscapedError (uint8_t error_code)
{
    WriteByte((char)0x1b);
    WriteByte('e');
    WriteByte((char)error_code);
}





class FileIO
{
  public:

    virtual ~FileIO()
    {
        ;
    }
    // 6502 wants to read a char
    virtual int getChar()
    {
        return -1;
    }
    // 6502 writes a char
    virtual void putChar(char)
    {
        ;
    }
    virtual void seek()
    {
        // read the rest of the command (5 bytes)
        ReadByte();
        ReadByte();
        ReadByte();
        ReadByte();
        ReadByte();
        WriteByte (P65_ENOSYS);
    }
    virtual void flush()
    {
        // Flush any buffered writes
        ;
    }
    // 6502 reads n chars
    virtual void  read()
    {
        ReadByte(); // read 2 bytes of count
        ReadByte();
        WriteEscapedError(P65_ENOSYS);
    }
    // 6502 writes n chars
    virtual void write()
    {
        // some dubious type punning here
        int count;
        unsigned char* c = (unsigned char*)&count;
        c[0] = ReadByte();
        c[1] = ReadByte();
        for (int i = 0; i < count; ++i)
            ReadByte();
        WriteByte((char)P65_EBADF);
        WriteByte((char)0xFF);
    }
};



struct dirent
{
    char d_name[13];  // 8.3 plus terminating zero
    char d_type;      // 1 for regular file, 2 for directory
    uint32_t d_size;  // file size in bytes
};



class DirectoryReader2 : public FileIO
{
    File dir;
    File entry;
    bool dir_open;
    int read_position = 0;
    int write_position = 0;
    //  struct dirent dirent = {};
    //  unsigned char* buffer = (unsigned char*)&dirent;
    char buffer[sizeof(struct dirent)];
    struct dirent* dirent = (struct dirent*)buffer;
  public:

    DirectoryReader2(File _dir)
    {
        static_assert(sizeof(struct dirent) == 18);
        dir = _dir;
        // we need to rewind the directory here. otherwise, the
        // listing will start after the last file we opened.
        dir.rewindDirectory();
        dir_open = true;
    }

    ~DirectoryReader2() override
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
                strncpy(dirent->d_name, entry.name(), 12);
                dirent->d_name[12] = 0;
                dirent->d_type = entry.isDirectory() ? 2 : 1;
                dirent->d_size = entry.size();
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
        unsigned char retval = buffer[read_position];
        ++read_position;
        return retval;
    }

    void read() override
    {
        int count;
        unsigned char* c = (unsigned char*)&count;
        c[0] = ReadByte();
        c[1] = ReadByte();

        if (!dir_open)
        {
            WriteEscapedError(P65_EBADF);
            return;
        }

        for (int i = 0; i < count; ++i)
        {
            int ch = getChar();
            WriteEscapedChar(ch);
            if (ch == -1)
                break;
        }

    }
};



class FileRW : public FileIO
{
// CJ BUG all the file open stuff here is probably not needed.
private:
    File file;
    bool use_buffered_io = false;
    uint8_t mode = 0; // file mode expressed using P65_O_* / cc65 mode values.
    
    static constexpr int buflen = 32;
    unsigned char buffer[buflen];   // for buffered reads or writes - but not both!
    int read_position = 0;
    int write_position = 0; 

public:

    FileRW(File f, uint8_t _mode)
    {
        file = f;
        mode = _mode;
        // only use FileRW's buffer if the file is readonly or
        // writeonly.
        use_buffered_io = ((mode & P65_O_RDWR) != P65_O_RDWR);
    }

    ~FileRW() override
    {
        flush();
        file.close();
    }

    int getChar() override
    {
        if (use_buffered_io)
            return nextchar();
        else
            return file.read();
    }

    void putChar(char ch) override
    {
        if (use_buffered_io)
            wrchar(ch);
        else
            file.write(ch);
    }

    void seek() override
    {
        long int offset;
        int whence;
        unsigned char* c = (unsigned char*)&offset;
        c[0] = ReadByte();
        c[1] = ReadByte();
        c[2] = ReadByte();
        c[3] = ReadByte();
        c = (unsigned char*)&whence;
        c[0] = ReadByte();
        c[1] = 0;

        flush(); // flush any buffered writes

        if (whence == P65_SEEK_CUR)
        {
            long int current = (long int)file.position();
            if (file.seek((uint32_t)(current + offset)))
            {
                read_position = write_position = 0;
                WriteByte(0);
                long int pos = file.position();
                char* buf = (char*)(&pos);
                WriteByte(buf[0]);
                WriteByte(buf[1]);
                WriteByte(buf[2]);
                WriteByte(buf[3]);
            }
            else
            {
                WriteByte(P65_EIO);
            }
        }
        else if (whence == P65_SEEK_END)
        {
            long int end = (long int)file.size();
            if (file.seek((uint32_t)(end + offset)))
            {
                read_position = write_position = 0;
                WriteByte(0);
                long int pos = file.position();
                char* buf = (char*)(&pos);
                WriteByte(buf[0]);
                WriteByte(buf[1]);
                WriteByte(buf[2]);
                WriteByte(buf[3]);
            }
            else
            {
                WriteByte(P65_EIO);
            }
        }
        else if (whence == P65_SEEK_SET)
        {
            if (file.seek((uint32_t)offset))
            {
                read_position = write_position = 0;
                WriteByte(0);
                long int pos = file.position();
                char* buf = (char*)(&pos);
                WriteByte(buf[0]);
                WriteByte(buf[1]);
                WriteByte(buf[2]);
                WriteByte(buf[3]);
            }
            else
            {
                WriteByte(P65_EIO);
            }
        }
        else
        {
            WriteByte(P65_EINVAL);
        }
    }



    void read() override
    {
        int count;
        unsigned char* c = (unsigned char*)&count;
        c[0] = ReadByte();
        c[1] = ReadByte();

        if (use_buffered_io)
        {
            for (int i = 0; i < count; ++i)
            {
                // CJ, is there any benefit to using the buffer read method? Yes!
                int ch = nextchar();
                WriteEscapedChar (ch);
                if (ch == -1)
                    break;
            }
        }
        else
        {
            for (int i = 0; i < count; ++i)
            {
                int ch = file.read();
                WriteEscapedChar (ch);
                if (ch == -1)
                    break;
            }
        }
    }

    void flush() override
    {
        if ((mode & P65_O_RDWR) == P65_O_WRONLY)
        {
            file.write (buffer, write_position);
            write_position = 0;
        }
    }

    void write() override
    {
        int count;
        unsigned char* c = (unsigned char*)&count;
        c[0] = ReadByte();
        c[1] = ReadByte();

        int written_count = 0;
        if (use_buffered_io)
        {
            // write-only files use buffered write
            for (int i = 0; i < count; ++i)
            {
                char ch = ReadByte();
                written_count += wrchar(ch);
            }
        }
        else
        {
            // read-write files don't use buffer
            for (int i = 0; i < count; ++i)
            {
                char ch = ReadByte();
                written_count += file.write (ch);
            }
        }
        WriteByte (((unsigned char*)&written_count)[0]);
        WriteByte (((unsigned char*)&written_count)[1]);
    }

private:

    // buffered char read. Only use for read-only files.
    inline int nextchar()
    {
        if (read_position >= write_position)
        {
            read_position = 0;
            write_position = file.read(buffer, buflen);
            if (write_position == 0)
                return -1;
        }

        return buffer[read_position++];
    }

    // buffered char write - only for writeonly files
    inline int wrchar(char ch)
    {
        buffer[write_position++] = ch;
        if (write_position == buflen)
        {
            // Is there a realistic concern here of write not writing the
            // full amount? If so, what do we do? Return 0?
            file.write(buffer, write_position);
            write_position = 0;
        }
        return 1;
    }


};



class CommandHandler : public FileIO
{
private:
    static constexpr int buflen = 96;
    char command_buffer[buflen];
    int command_buffer_index = 0;
    bool command_buffer_overrun = false;

    int read_position = 0;
    int write_position = 0;
    char buffer[30];

public:

    CommandHandler() = default;

    void SetCommandResponse(const char* output, int len)
    {
        memcpy(buffer, output, len);
        read_position = 0;
        write_position = len;
    }

    void SetCommandResponse(char code)
    {
        buffer[0] = code;
        read_position = 0;
        write_position = 1;
    }

private:
    int nextChar()
    {
        int retval = -1;

        if (command_buffer_overrun)
        {
            // On the first read after a command buffer overrun, we reset the
            // command buffer. Is this sufficient to restore things after some
            // miscommunication? Almost certainly not.
            command_buffer_overrun = false;
            command_buffer_index = 0;
        }

        if (read_position < write_position)
        {
            retval = buffer[read_position];
            ++read_position;
        }
        return retval;
    }
public:

    void putChar(char ch) override
    {
        // the command is a 0-terminated array of 0-terminated strings. So we look for
        // two consecutive 0s to begin processing.
        if (command_buffer_index < buflen)
            command_buffer[command_buffer_index++] = ch;
        else
            command_buffer_overrun = true;
        if ((ch == 0) && (command_buffer_index > 1) && (command_buffer[command_buffer_index - 2] == 0))
        {
            bool length_error = (command_buffer_index >= buflen);
            //Serial.print ("command buffer is '");
            //Serial.print (command_buffer);
            //Serial.println ("'\n");
            command_buffer_index = 0;

            if (length_error)
            {
                SetCommandResponse(P65_EINVAL);
            }
            else if (!strcmp(command_buffer, "rm"))
            {
                SetCommandResponse(HandleDeleteFile(command_buffer));
            }
            else if (!strcmp(command_buffer, "rmdir"))
            {
                SetCommandResponse(HandleDeleteDirectory(command_buffer));
            }
            else if (!strcmp(command_buffer, "mkdir"))
            {
                SetCommandResponse(HandleMkdir(command_buffer));
            }
            else if (!strcmp(command_buffer, "cp"))
            {
                SetCommandResponse(HandleCopyFile(command_buffer));
            }
            else if (!strcmp(command_buffer, "stat"))
            {
                HandleStat(command_buffer);
            }
            else if (!strncmp(command_buffer, "o", 1))
            {
                SetCommandResponse(HandleFileOpen(command_buffer));
            }
            else if (!strncmp(command_buffer, "c", 1))
            {
                HandleFileClose(command_buffer);
            }
            else
            {
                SetCommandResponse (P65_EBADCMD);
                //channel_io[0] = new CommandResponse(P65_EBADCMD);
            }
        }
    }

    int getChar() override
    {
        return nextChar();
    }

    void read() override
    {
        int count;
        unsigned char* c = (unsigned char*)&count;
        c[0] = ReadByte();
        c[1] = ReadByte();

        for (int i = 0; i < count; ++i)
        {
            int ch = nextChar();
            WriteEscapedChar(ch);
            if (ch == -1)
                break;
        }
    }

    void write() override
    {
        // CJ BUG. It would be nice if this stopped processing
        // when we reached the end of a command, and return the
        // actual # of bytes processed.
        int count;
        unsigned char* c = (unsigned char*)&count;
        c[0] = ReadByte();
        c[1] = ReadByte();

        for (int i = 0; i < count; ++i)
        {
            char ch = ReadByte();
            putChar(ch);
        }

        WriteByte (((unsigned char*)&count)[0]);
        WriteByte (((unsigned char*)&count)[1]);
    }
};


constexpr int FileIOSize = max (sizeof(FileIO), max (sizeof(FileRW), sizeof(DirectoryReader2)));



// for use with an invalid channel # or null channel_io member.
FileIO default_io;
CommandHandler command_handler;

// channel IO handlers. Channel 0 handler is permanent. Data channels
// will have handlers emplaced & removed when files are opened and
// closed.
FileIO* channel_io[MAX_CHANNEL + 1] = {&command_handler, nullptr, nullptr};
char ChannelHandlerBuffer[2][FileIOSize];

void SetCommandResponse(const char* output, int len)
{
    command_handler.SetCommandResponse(output, len);
}

void SetCommandResponse(char code)
{
    command_handler.SetCommandResponse(code);
}

// OK, I was having trouble getting placement new to compile so I added this.
// I'm really quite alarmed right now.
void *operator new(size_t size, char* c) {
  return c;
}

template<class T, typename... Args>
void SetChannel(int channel, Args&&... args)
{
    // Am I safe without std::forward on args? I think it just amounts to a wierd static cast
    if ((channel >= MIN_CHANNEL) && (channel <= MAX_CHANNEL))
    {
        // destruct previous channel object
        if (channel_io[channel])
        {
            channel_io[channel]->~FileIO();
        }
        // and create new one
        channel_io[channel] = new((char*)ChannelHandlerBuffer[channel-1]) T(args...);
    }
} 

void ClearChannel(int channel)
{
    if ((channel >= MIN_CHANNEL) && (channel <= MAX_CHANNEL) && channel_io[channel])
    {
        channel_io[channel]->~FileIO();
        channel_io[channel] = nullptr;
    }
} 

class FileIO* GetIOHandler (char channel)
{
    if ((channel < 0) || (channel > MAX_CHANNEL) || (channel_io[channel] == nullptr))
        return &default_io;
    else
        return channel_io[channel];
}




void setup()
{
    pinMode(data0, INPUT);
    pinMode(data1, INPUT);
    pinMode(data2, INPUT);
    pinMode(data3, INPUT);
    pinMode(data4, INPUT);
    pinMode(data5, INPUT);
    pinMode(data6, INPUT);
    pinMode(data7, INPUT);
    pinMode(ca1, OUTPUT);
    pinMode(ca2, INPUT);
    //attachInterrupt(digitalPinToInterrupt(ca2) /*0*/, InterruptHandler, RISING);


    pinMode(13, OUTPUT);
    digitalWrite(13, LOW);

    //digitalWrite (ca1, LOW);

    delayMicroseconds(50);
    digitalWrite(ca1, LOW);
    //digitalWrite (ca2, LOW);

    pinMode(10, OUTPUT);
    if (!SD.begin(10))
        ErrorFlash();

    //Serial.begin (9600);
}



// The values in SD library for mode flags don't match the values used by
// cc65 and P:65. So we need to translate.
uint8_t TranslateMode(uint8_t in)
{
    uint8_t mode = 0;
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

/* Declarations used by HandleStat */
struct timespec
{
    uint32_t tv_sec;
    long tv_nsec;
};
struct stat
{
    uint32_t st_dev;
    uint32_t st_ino;
    unsigned char st_mode;
    uint32_t st_nlink;
    unsigned char st_uid;
    unsigned char st_gid;
    int32_t st_size;
    struct timespec st_atim;
    struct timespec st_ctim;
    struct timespec st_mtim;
};

void HandleStat(char* command_buffer)
{
    char* filename = command_buffer + 5;
    if (strlen(filename) < 1)
        return SetCommandResponse(P65_EINVAL);

    // While "/" is a valid filename to pass to SD.open() in order to read the root
    // directory, it is not accepted by SD.exists(). So we have to treat it specially
    // here.
    if (!SD.exists(filename) && strcmp(filename, "/"))
    {
        return SetCommandResponse(P65_ENOENT);
    }

    File f = SD.open(filename, O_RDONLY);
    if (!f)
        return SetCommandResponse(P65_EIO);

    char buffer[1 + sizeof(struct stat)];
    buffer[0] = P65_EOK;
    struct stat* s = (struct stat*)(buffer + 1);

    s->st_dev = 0;  // this probably should be filled in on 6502 side
    s->st_ino = 0;  // yeah, we don't really have inodes.
    s->st_mode = (f.isDirectory() ? 2 : 1);
    s->st_nlink = 1;  // I guess?
    s->st_uid = 0;
    s->st_gid = 0;
    s->st_size = f.size();
    s->st_atim = s->st_ctim = s->st_mtim = {};

    f.close();
    return SetCommandResponse(buffer, 1 + sizeof(struct stat));
}



/** Parse a file open command and create file reader/writer.
 *  On success, the return value is the file type. On failure, an error code.
 */
char HandleFileOpen(char* command_buffer)
{
    if (strlen(command_buffer) < 4)
    {
        return P65_EINVAL;
    }

    int channel = command_buffer[1] - 48;  // convert ascii to int
    if (channel < MIN_CHANNEL || channel > MAX_CHANNEL)
    {
        return P65_EINVAL;
    }

    uint8_t mode = command_buffer[2];
    uint8_t sd_mode = TranslateMode(mode);

    char* filename = command_buffer + 3;

    ClearChannel(channel);

    if (mode & P65_O_WRONLY) // includes read/write
    {
        if ((mode & P65_O_TRUNC) && (SD.exists(filename)))
            SD.remove(filename);
        File f = SD.open(filename, sd_mode/*FILE_WRITE*/);
        if (f)
        {
            if (f.isDirectory())
            {
                // opening a directory for writing is not allowed!
                f.close();
                return P65_EISDIR;
            }
            else
            {
                SetChannel<FileRW>(channel, f, mode);
                return 1;  // return filetype for regular file
            }
        }
        else
        {
            return P65_EIO;
        }
    }
    else if (mode & P65_O_RDONLY)
    {
        // While "/" is a valid filename to pass to SD.open() in order to read the root
        // directory, it is not accepted by SD.exists(). So we have to treat it specially
        // here.
        if (!SD.exists(filename) && strcmp(filename, "/"))
        {
            return P65_ENOENT;
        }
        File f = SD.open(filename);
        if (f)
        {
            if (f.isDirectory())
            {
                SetChannel<DirectoryReader2>(channel, f);
                return 2;  // return filetype directory
            }
            else
            {
                SetChannel<FileRW>(channel, f, mode);
                return 1;  // return filetype regular file
            }
        }
        else
        {
            return P65_EIO;
        }
    }

    // fallthrough error
    return P65_EINVAL;
}



char HandleDeleteFile(char* command_buffer)
{
    char* filename = command_buffer + 3;
    if (*filename == 0)
        return P65_EINVAL;
    if (!SD.exists(filename))
        return P65_ENOENT;
    if (File f = SD.open(filename))
    {
        if (f.isDirectory())
        {
            f.close();
            return P65_EISDIR;
        }
        f.close();
    }
    if (SD.remove(filename))
        return P65_EOK;
    else
        return P65_EIO;
}



char HandleDeleteDirectory(char* command_buffer)
{
    char* filename = command_buffer + 6;
    if (*filename == 0)
        return P65_EINVAL;
    if (!SD.exists(filename))
        return P65_ENOENT;
    if (File f = SD.open(filename))
    {
        if (!f.isDirectory())
        {
            f.close();
            return P65_ENOTDIR;
        }
        f.rewindDirectory();
        if (File f2 = f.openNextFile())
        {
            f2.close();
            f.close();
            return P65_EEXIST;
        }
        f.close();
    }
    if (SD.rmdir(filename))
        return P65_EOK;
    else
        return P65_EIO;
}



char HandleMkdir(char* command_buffer)
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



char HandleCopyFile(char* command_buffer)
{
    constexpr int buflen = 64;
    char buffer[buflen];
    char* src_filename = command_buffer + 3;
    char* dst_filename = src_filename + strlen(src_filename) + 1;
    File src, dst;

    if ((src_filename[0] == '\0') || (dst_filename[0] == '\0'))
        return P65_EINVAL;
    // SD library doesn't give me any exact way to tell if two files are
    // the same thing, but we'll try to canonicalize as much as possible.
    if (src_filename[0] == '/')
        src_filename += 1;
    if ((dst_filename[0] == '/') && (dst_filename[1] != 0))
        dst_filename += 1;
    if (0 == strcasecmp(src_filename, dst_filename))
        return P65_EINVAL;
    if (!SD.exists(src_filename))
        return P65_ENOENT;

#if 1
    if (src = SD.open(src_filename, FILE_READ | O_WRONLY))
    {
        if (src.isDirectory())
        {
            src.close();
            return P65_EISDIR;
        }
    }
    else
    {
        //src.close();
        return P65_EIO;
    }

    // If dst_filename exists & is a folder, abort.
    if (SD.exists(dst_filename) && (dst = SD.open(dst_filename, FILE_READ)))
    {
        if (dst.isDirectory())
        {
            src.close();
            dst.close();
            return P65_EISDIR;
            //      // make up a new destination name and stick it in buffer.
            //      dst.close();
            //      if (strlen(src_filename) + strlen(dst_filename) + 2 > buflen)
            //      {
            //        return P65_EINVAL; // name too long...
            //      }
            //      sprintf(buffer, "%s/%s",dst_filename, src_filename);
            //      dst_filename = buffer;
        }
        else
        {
            dst.close();
        }
    }

    // so despite the fact that FILE_WRITE is apparently defined with O_APPEND,
    // the actual effect is as if using O_TRUNC. Which begs the question of
    // whether we can make append work at all... or if seek would avail us?
    // or is that us doing that up above?
    dst = SD.open(dst_filename, /*FILE_WRITE*/ O_WRITE | O_CREAT | O_TRUNC);
    if (!dst)
    {
        src.close();
        return P65_EIO;
    }

    // OK, we should have both src and dst open. Start copying?
    int num_read = 0, num_written = 0;
    int total_written = 0;
    int count = src.size();
    for (;;)
    {
        num_read = src.read(buffer, buflen);
        if (num_read == 0)
            break;
        num_written = dst.write(buffer, num_read);
        total_written += num_written;
        if (num_written != num_read)
            break;
    }

    src.close();
    dst.close();

    if (total_written != count)
    {
        //SD.remove(dst_filename);
        return P65_EIO;
    }
    else
        return P65_EOK;
#endif
}



char HandleFileClose(char* command_buffer)
{
    int channel = command_buffer[1] - 48;  // cheap conversion
    if (channel < MIN_CHANNEL || channel > MAX_CHANNEL)
        return P65_EINVAL;
    ClearChannel(channel);
    return P65_EOK;
}



void loop()
{
    char protocol = ReadByte();
    char channel = protocol & 0x0f;
    char command = protocol & 0xf0;

    switch (command)
    {
        case 0x40:  // 6502 sent a multibyte read command
            {
                auto handler = GetIOHandler(channel);
                handler->read();
            }
            break;
        case 0x50:  // 6502 sent a multibyte write command
            {
                auto handler = GetIOHandler(channel);
                handler->write();
            }
            break;
        case 0x30:  // 6502 sending a seek command
            {
                auto handler = GetIOHandler(channel);
                handler->seek();
            }
            break;
        case 0x10:  // 6502 wants to read a byte
            {
                auto handler = GetIOHandler(channel);
                int ch = handler->getChar();
                WriteEscapedChar(ch);
            }
            break;
        case 0x20:  // 6502 wants to write a byte
            {
                char c = ReadByte();
                auto handler = GetIOHandler(channel);
                handler->putChar(c);
            }
            break;
    }
}
