
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

// Field dimensions here include the borders, which are in columns 1 and 60
// and rows 1 and 24.
#define FIELD_X 60
#define FIELD_Y 24

// Annotations in the field:
//  $00 - clear
//  $FF - wall segments
//  'X' - food
//  $01 to $04 - directional map of the snake.
unsigned char field[FIELD_X + 1][FIELD_Y + 1];

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
        target_x = 2 + ((int)rand())%(FIELD_X-2);
        target_y = 2 + ((int)rand())%(FIELD_Y-2);
    }
    while (field[target_x][target_y] != 0);
    textcolor(11);
    cputcxy (target_x, target_y, 'X');
    textcolor(2);
    field[target_x][target_y] = 'X';
}

void __fastcall__ bzero (void* ptr, size_t n);


// Adds amount to score & updates the display
void DisplayScore ()
{
    gotoxy (73, 12);
    textcolor(15);
    cprintf ("%3d", score);
}



void InitPlayfield()
{
    int i;
    bzero (field, 61*25);
    clrscr();
    cursor(0);

    textcolor(10);
    cputsxy (66,8, "S N E K");
    textcolor(7);
    cputsxy (65,12, "Score:");

    // Draw boundaries of the field
    bgcolor(7);
    for (i = 1; i <= FIELD_X; ++i)
    {
        field[i][1] = field[i][FIELD_Y] = 0xFF; 
        //textcolor (i%16);
        //bgcolor (7);
        cputcxy (i,1,'\xb1');
        cputcxy (i,FIELD_Y,'\xb1');
    }
    for (i = 2; i < FIELD_Y; ++i)
    {
        field[1][i] = field[FIELD_X][i] = 0xFF;
        //textcolor (i%16);
        //bgcolor (7);
        cputcxy (1,i,'\xb1');
        cputcxy (FIELD_X, i, '\xb1');
    }
    bgcolor(0);

    DisplayScore();
}



char OppositeDir (char d)
{
    switch (d)
    {
    case 1:
        return 3;
    case 2: 
        return 4;
    case 3:
        return 1;
    case 4:
        return 2;
    default:
        return 1; // Never return an invalid direction.
    }
}


// Runs a single game of Snek.
// Returns 0 if game ends normally, 1 if user aborts with 'q'.
int Game()
{
    char dir = 2; // 1 = up, then cw
    char newdir;
    int x = 30;
    int y = 12;
    int tailx = x;
    int taily = y;
    char t;
    char bumplen = 0;
    score = 0;


    InitPlayfield();

    PlaceTarget();

    // draw initial snake
    cputcxy (x,y,'*');

    for (;;)
    {
        int ch = 0;
        newdir = dir;

        // If we queue instructions, we can't recover from mistakes. If we 
        // ingest all characters, we can make up for some fumble-fingeredness,
        // but inserting multiple instructions quickly can be very timing 
        // tricky (for the player). This version chooses the latter.
        while (kbhit())
        {
            ch = cgetc();
            switch (ch)
            {
                case ' ': // for testing tail bug
                    bumplen = 1;
                    break;
                case 'q': 
                    //goto quit;
                    return 1;
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
        if (newdir != OppositeDir(dir))
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
            score += 10;
            DisplayScore ();

            PlaceTarget();
            bumplen = 2; // controls how many spaces the tail grows by

            textcolor(2);
        }
        else
        {
            textcolor(9);
            cputsxy (x-2, y, "BOOM!");
            return 0;
        }

        if (bumplen > 0)
            --bumplen;
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

}



int main ()
{
    char ch;
    int done = 0;

    // Init random number generator
    srand(time(NULL));

    // cgetc will only work in raw mode
    ioctl(0, IO_TTY_RAW_MODE);
    ioctl(0, IO_TTY_ECHO_OFF);
    
    while (!done)
    {
        if (Game())
            break; // user quit
        textcolor(11);
        cputsxy (22, 12, "G A M E   O V E R");
        textcolor(7);
        cputsxy (22, 14, "Play again (y/n)?");
        for (;;)
        {
            ch = cgetc();
            if (tolower(ch) == 'y')
                break;
            if (tolower(ch) == 'n')
            {
                done = 1;
                break;
            }
        }
    }


    // eat any remaining input so the console doesn't get it.
    while (kbhit())
        cgetc();

    // Put console back in reasonable state. Kernel will take care of 
    // resetting raw mode & echo.
    cursor(1);
    textcolor(7);
    bgcolor(0);
    gotoxy (0,FIELD_Y);
    cputs ("\r\n");
    return 0;
}