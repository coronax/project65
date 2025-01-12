
#include <errno.h>
#include <time.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>

int write_test = 0;
int read_test = 0;
int test_size = 10 * 1024;


void PerformWriteTest ()
{
    char* buffer;
    int i;
    int fd;
    int output_size;
    struct timespec t1, t2;

    buffer = malloc (test_size);
    if (buffer == NULL)
    { 
        printf ("Unable to allocate %d bytes.\r\n", test_size);
        exit(1);
    }
    for (i = 0; i < test_size; ++i)
        buffer[i] = (char)(i%256);
    
    fd = open ("ds_test.dat", O_WRONLY | O_CREAT | O_TRUNC);
    if (fd == -1)
    {
        printf ("Unable to open test file ds_test.dat: %s.\r\n", strerror(errno));
        exit(1);
    }

    clock_gettime(CLOCK_REALTIME, &t1);
    output_size = write (fd, buffer, test_size);
    clock_gettime(CLOCK_REALTIME, &t2);

    close(fd);

    if (output_size == -1)
    {
        printf ("Error during write: %s.\r\n", strerror(errno));
    }
    else if (test_size != output_size)
    {
        printf ("Error: only wrote %d bytes.\r\n", output_size);
    }
    else
    {
        if (t2.tv_nsec < t1.tv_nsec)
        {
            t2.tv_nsec += 1000000000;
            t2.tv_sec -= 1;
        }
        printf ("Write Completed: %d kb in %ld.%02ld s\r\n", test_size / 1024, (t2.tv_sec - t1.tv_sec), (t2.tv_nsec - t1.tv_nsec) / 10000000);
    }
}



void PerformReadTest ()
{
    char* buffer;
    int fd;
    int output_size;
    struct timespec t1, t2;
    int i; 
    int mismatches = 0;

    buffer = malloc (test_size);
    if (buffer == NULL)
    { 
        printf ("Unable to allocate %d bytes.\r\n", test_size);
        exit(1);
    }
    
    fd = open ("ds_test.dat", O_RDONLY);
    if (fd == -1)
    {
        printf ("Unable to open test file ds_test.dat.\r\n");
        exit(1);
    }

    clock_gettime(CLOCK_REALTIME, &t1);
    output_size = read (fd, buffer, test_size);
    clock_gettime(CLOCK_REALTIME, &t2);

    if (test_size != output_size)
    {
        printf ("Error: only read %d bytes.", output_size);
        close (fd);
        exit(1);
    }

    close(fd);

    for (i = 0; i < test_size; ++i)
        if (buffer[i] != (char)(i%256))
            ++mismatches;
    if (mismatches != 0)
        printf ("Error: %d mismatches in read data.", mismatches);

    if (t2.tv_nsec < t1.tv_nsec)
    {
        // borrow a billion nanoseconds from tv_sec
        t2.tv_nsec += 1000000000;
        t2.tv_sec -= 1;
    }
    printf ("Read Completed: %d kb in %ld.%02ld s\r\n", test_size / 1024, (t2.tv_sec - t1.tv_sec), (t2.tv_nsec - t1.tv_nsec) / 10000000);
}



void usage (void)
{
    fprintf(stderr, "usage: diskspeed -r\r\n");
    fprintf(stderr, "   or: diskspeed -w [-s size_kb]\r\n");
    exit(2);
}


int main (int argc, char** argv)
{
    int ch;

    while ((ch = getopt(argc, argv, "rws:")) != -1)
    {
        switch(ch)
        {
        case 'r':
            read_test = 1;
            break;
        case 'w':
            write_test = 1;
            break;
        case 's':
            test_size = 1024 * atoi (optarg);
            break;
        case '?':
        default:
            usage();
        }
    }

    if (test_size <= 0)
        usage();

    if (read_test == 0 && write_test == 0)
        read_test = 1;

    if (write_test)
        PerformWriteTest();

    if (read_test)
        PerformReadTest();

    return 0;
}
