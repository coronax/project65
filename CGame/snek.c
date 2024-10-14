
#include <stdio.h>
#include <unistd.h>
#include <conio.h>
#include <ctype.h>
#include <stdlib.h>
#include <time.h>

// note: screen positions are 1-based. sigh.
// also my terminal screen is 80x24 default.
// Rather than messing about with translating indices, I think I'm okay with
// just allocating an extra unused column & row 0.

// Annotations in the field:
//  $00 - clear
//  $FF - wall segments
//  'X' - food
//  $01 to $04 - directional map of the snake.
unsigned char field[61][25];

int score = 0;

void updatepos (int* x, int* y, char dir)
{
    switch (dir)
    {
    case 1:
        *y -= 1;
        break;
    case 2:
        *x += 1;
        break;
    case 3:
        *y += 1;
        break;
    case 4:
        *x -= 1;
        break;
    default:
        printf ("invalid dir %d\r\n",(int)dir);
        break;
    }
}


void PlaceTarget()
{
    int target_x, target_y;
    do
    {
        target_x = 2 + ((int)rand())%58;
        target_y = 2 + ((int)rand())%22;
    }
    while (field[target_x][target_y] != 0);
    textcolor(11);
    cputcxy (target_x, target_y, 'X');
    textcolor(2);
    field[target_x][target_y] = 'X';
}

void __fastcall__ bzero (void* ptr, size_t n);

void InitPlayfield()
{
    int i;
    bzero (field, 61*25);
    clrscr();
    cursor(0);
    //revers(1);

    textcolor(10);
    cputsxy (64,8, "S N E K");
    textcolor(7);
    cputsxy (62,12, "Score:   0");

    bgcolor(7);
    for (i = 1; i < 61; ++i)
    {
        field[i][1] = field[i][24] = 0xFF; 
        //textcolor (i%16);
        //bgcolor (7);
        cputcxy (i,1,'\xb1');
        cputcxy (i,24,'\xb1');
    }
    for (i = 2; i < 24; ++i)
    {
        field[1][i] = field[60][i] = 0xFF;
        //textcolor (i%16);
        //bgcolor (7);
        cputcxy (1,i,'\xb1');
        cputcxy (60, i, '\xb1');
    }
    bgcolor(0);
    //revers(0);
}

// Adds amount to score & updates the display
void AddToScore (int amount)
{
    score += amount;
    gotoxy (69, 12);
    cprintf ("%3d", score);
}


int main ()
{
    char dir = 2; // 1 = up, then cw
    char newdir;
    int x = 30;
    int y = 12;
    int tailx = x;
    int taily = y;
    //int turn = 0;
    //int target_x, target_y;
    char t;
    char bumplen = 0;
    score = 0;
    // reset conole: treat with caution
    // printf ("\x1b\x63");
    //sleep(2);

    srand(time(NULL));

    // cgetc will only work in raw mode
    ioctl(0, IO_TTY_RAW_MODE);
    ioctl(0, IO_TTY_ECHO_OFF);
    

    InitPlayfield();

    PlaceTarget();

    // draw initial snake
    cputcxy (x,y,'*');

    for (;;)
    {
        int ch = 0;
        newdir = dir;

        // If we queue instructions, we can't recover from mistakes. If we ingest
        // all characters, we can make up for some fumble-fingeredness, but inserting
        // multiple instructions quickly can be very timing tricky.
        while (kbhit())
        {
            ch = cgetc();
            switch (ch)
            {
                case ' ': // for testing tail bug
                    bumplen = 1;
                    break;
                case 'q': 
                    goto quit;
                    break;
                case 'w':
                    newdir = 1;
                    break;
                case 'a':
                    newdir = 4;
                    break;
                case 's':
                    newdir = 3;
                    break;
                case 'd':
                    newdir = 2;
                    break;
                case 27: // esc
                    // read a cursor key sequence. Will it break things to just call
                    // cgetc here? What if the user actually presses ESC? Will we block?
                    // would it work to call kbhit?
                    // The cursor key sequence is like esc[1A, where the 1 is optional.
                    ch = cgetc();
                    if (ch != '[')
                        break;
                    do {ch = cgetc();}
                        while (isdigit(ch));
                    switch (ch)
                    {
                    case 'A': 
                        newdir = 1;
                        break;
                    case 'B':
                        newdir = 3;
                        break;
                    case 'C':
                        newdir = 2;
                        break;
                    case 'D':
                        newdir = 4;
                        break;
                    default:
                        break;
                    }
                    break;
                default:
                    break;
            }
        }
        
        // let's not blow ourselves up by accidentally hitting the exact
        // opposite of our current direction:
        if (dir != (newdir+2)%4)
            dir = newdir;

        field[x][y] = dir;
        
        updatepos (&x,&y,dir);

        t = field[x][y];
        if (t == 0)
        {
            cputcxy (x,y,'*');
        }
        else if (t == 'X')
        {
            cputcxy (x,y, '*');
            //score += 10;
            AddToScore (10);

            PlaceTarget();
            bumplen = 1;
        }
        else
        {
            printf ("BOOM!\r\n");
            //printf ("xy == %d, %d\r\n", x, y);
            //printf ("t == %d", (int)t);
            //printf ("field        = %p\r\n", field);
            //printf ("&field[x][y] = %p\r\n", &field[x][y]);
            goto quit;
        }

        if (bumplen)
            bumplen = 0;
        else
        {
            // Clear last tail element
            unsigned char taildir = field[tailx][taily];
            field[tailx][taily] = 0;
            cputcxy (tailx, taily, ' ');
            updatepos (&tailx, &taily, taildir);
        }

        msleep (100);
    }


quit:
    // eat any remaining input so the console doesn't get it.
    while (kbhit())
        cgetc();
    cursor(1);
    textcolor(7);
    bgcolor(0);
    gotoxy (0,24);
    return 0;
}