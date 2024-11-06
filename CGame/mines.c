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

// Field dimensions here are for the maximum grid. We're gonna try drawing 
// cells as 3x1.
#define MAX_FIELD_X 30
#define MAX_FIELD_Y 16

// these values depend on game mode
int field_x = 16;
int field_y = 16;
int total_mines = 40;

// references the logical playfield, not screen coords.
int player_x = 0;
int player_y = 0;

// Field flags:
enum FLAGS { MINED = 0x10, CONCEALED = 0x20, FLAGGED = 0x40 };

// The field here is just the playfield, without borders
// Each field value is the # of neighboring mines in the
// low nybble, with flags in the high nybble.
// Play begins with all squares CONCEALED, some with MINEs
unsigned char field[MAX_FIELD_X][MAX_FIELD_Y];

int score = 0;
int mines_flagged = 0;
int flags_left = 0;

// Dir is 1-8, clockwise from UP

// Gets the neighboring x coord for the given direction. Returns -1 if the 
// value is out of bounds.
int GetNeighborX (int x, int dir)
{
    int xp;
    switch (dir)
    {
    case 2:
    case 3:
    case 4:
        xp = x+1;
        break;
    case 6:
    case 7:
    case 8:
        xp = x-1;
        break;
    default:
        xp = x;
        break;
    }
    if ((xp < 0) || (xp >= field_x))
        xp = -1;
    return xp;
}

int GetNeighborY (int y, int dir)
{
    int yp;
    switch (dir)
    {
    case 8:
    case 1:
    case 2:
        yp = y-1;
        break;
    case 4:
    case 5:
    case 6:
        yp = y+1;
        break;
    default:
        yp = y;
        break;
    }
    if ((yp < 0) || (yp >= field_y))
        yp = -1;
    return yp;
}

unsigned char* GetNeighbor (int x, int y, int dir)
{
    x = GetNeighborX (x, dir);
    y = GetNeighborY (y, dir);
    if ((x == -1) || (y == -1))
        return NULL;
    else
        return &field[x][y];
}


unsigned char* GetHex (int x, int y)
{
    return &field[x][y];
}


void PlaceMines(int count)
{
    int target_x, target_y;
    int i;
    while (count > 0)
    {
        target_x = ((int)rand())%(field_x);
        target_y = ((int)rand())%(field_y);

        if ((field[target_x][target_y] & MINED) == 0) // shouldn't == have a much lower priority than &?
        {
            field[target_x][target_y] |= MINED;
            --count;

            // We should add to the count of each neighbor
            for (i = 1; i <= 8; ++i)
            {
                unsigned char* c;
                c = GetNeighbor(target_x, target_y, i);
                if (c)
                    ++(*c);
            }
        }
    }
}



// Adds amount to score & updates the display
void DisplayFlagsCount ()
{
    int count = flags_left;
    if (count < 0)
        count = 0;
    gotoxy (67, 12);
    textcolor(15);
    bgcolor(0);
    cprintf ("%3d", count);
}

// colors:
// 0-7          8-15
// black        
// red
// green
// yellow
// blue 
// magenta
// cyan
// white

void DrawCell (int x, int y, char highlight)
{
    int screen_x = 2 + 3*x;
    int screen_y = 2 + y;
    unsigned char content = field[x][y];
    gotoxy (screen_x, screen_y);
    if (highlight)
    {
        if (content & FLAGGED)
        {
            textcolor(15);
            bgcolor(1);
            cputs ("\xb1 \xb1");
            gotoxy (0,0);
        }
        else if (content & CONCEALED)
        {
            textcolor(15);
            bgcolor(15);
            cputs ("\xdb\xdb\xdb");
            gotoxy (0,0);
            //cputs ("\xb1\xb1\xb1");            
        }
        else if (content & MINED)
        {
            bgcolor(8);
            cputs ("---");
        }
        else if ((content & 0x0f) == 0)
        {
            textcolor(15);
            bgcolor(15);
            cputs ("\xdb\xdb\xdb");
            gotoxy (0,0);
            //bgcolor(15);
            //cputs ("..."); // empty black
        }
        else
        {
            textcolor(15);
            bgcolor(0);
            cprintf ("\xdb%d\xdb", content & 0x0f);
            gotoxy (0,0);
            //bgcolor(7);
            //textcolor(0);
            //cprintf (" %d ", content & 0x0f);
        }
    }
    else
    {
        if (content & FLAGGED)
        {
            textcolor(7);
            bgcolor(1);
            cputs ("\xb1 \xb1");
        }
        else if (content & CONCEALED)
        {
            textcolor(7);
            bgcolor(7);
            cputs ("   ");
            //cputs ("\xb1\xb1\xb1");            
        }
        else if (content & MINED)
        {
            bgcolor(1);
            cputs ("   ");
        }
        else if ((content & 0x0f) == 0)
        {
            bgcolor(0);
            cputs ("   "); // empty black
        }
        else
        {
            bgcolor(0);
            textcolor(15);
            cprintf (" %d ", content & 0x0f);
        }
    }
}



void InitPlayfield()
{
    int i, j;
    
    for (i = 0; i < field_x; ++i)
        for (j = 0; j < field_y; ++j)
            field[i][j] = CONCEALED;

    PlaceMines(total_mines);

    clrscr();
    cursor(0);

    textcolor(10);
    //gotoxy (66,8);
    //cputs ("M I N E S");
    cputsxy (60,8, "M I N E S");
    textcolor(7);
    cputsxy (59,12, "Mines:");

    // Draw boundaries of the field
    textcolor(4);
    bgcolor(4);
    for (i = 1; i <= 2 + 3 * field_x; ++i)
    {
        cputcxy (i,1,' ');
        cputcxy (i,2 + field_y,'\xdb');
    }
    for (i = 2; i <= 2 + field_y; ++i)
    {
        cputcxy (1,i,' ');
        cputcxy (2 + 3 * field_x, i, '\xdb');
    }
    bgcolor(0);

    textcolor(7);
    bgcolor(7);
    for (j = 2; j < field_y + 2; ++j)
    {
        gotoxy (2,j);
        for (i = 0; i < field_x; ++i)
            cputs ("   ");
    }
    // The slowest way to draw the field:
    //for (i = 0; i < field_x; ++i)
    //    for (j = 0; j < field_y; ++j)
    //        DrawCell (i, j, 0);

    DisplayFlagsCount();
}



void RevealCell (int x, int y)
{
    int i;
    if (field[x][y] & CONCEALED)
    {
        field[x][y] &= ~CONCEALED;
        field[x][y] &= ~FLAGGED;
        DrawCell (x,y,0);
        if ((field[x][y] & (MINED | 0x0f)) == 0)
        {
            // reveal the neighbors too
            // Now, If I recurse will it blow up in my face?
            for (i = 1; i < 9; ++i)
            {
                int xp = GetNeighborX (x,i);
                int yp = GetNeighborY (y,i);
                if ((xp != -1) && (yp != -1))
                    RevealCell (xp, yp);
            }
        }
    }
}



// Read & Interpret keyboard commands.
// The result value is one of:
// 0   - no valid input
// 1-8 - a direction, clockwise from 1 at top
// 'q' - quit
// 'r' - reveal the current square
// 'f' - flag the current square
char GetCommand()
{
    int ch = 0;
    int command = 0;
        
    // If we queue instructions, we can't recover from mistakes. If we 
    // ingest all characters, we can make up for some fumble-fingeredness,
    // but inserting multiple instructions quickly can be very timing 
    // tricky (for the player). This version chooses the latter.
    ch = cgetc();
    switch (ch)
    {
        case 3: // ctrl-c
            command = 'q';
            break;
        case 'Q': 
            command = 'q';
            break;
        case 'w':
            command = 1;
            break;
        case 'a':
            command = 7;
            break;
        case 's':
            command = 5;
            break;
        case 'd':
            command = 3;
            break;
        case 'q':
            command = 'r'; // reveal
            break;
        case 'e':
            command = 'f'; // flag
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
                command = 1;
                break;
            case 'B':
                command = 5;
                break;
            case 'C':
                command = 3;
                break;
            case 'D':
                command = 7;
                break;
            default:
                break;
            }
            break;
        default:
            break;
    }
    return command;
}


// Runs a single game of Mines.
// Returns 1 if the user quits, 2 if the user dies, 3 for victory.
int Game()
{
    int command;
    char explosion = 0;
    char quit = 0;
    score = 0;
    mines_flagged = 0;
    flags_left = total_mines;

    InitPlayfield();

    player_x = field_x / 2;
    player_y = field_y / 2;

    // highlight initial player pos
    DrawCell (player_x, player_y, 1);

    while (!explosion && !quit)
    {
        command = GetCommand();

        if ((command >= 1) && (command <= 8)) // move cursor
        {
            int xp = GetNeighborX (player_x, command);
            int yp = GetNeighborY (player_y, command);
            if ((xp != -1) && (yp != -1))
            {
                DrawCell (player_x, player_y, 0);
                player_x = xp;
                player_y = yp;
                DrawCell (player_x, player_y, 1);
            }
        }
        else if (command == 'r') // reveal
        {
            RevealCell (player_x, player_y);
            if (field[player_x][player_y] & MINED)
                explosion = 1;
            else
                DrawCell (player_x, player_y, 1);
        }
        else if (command == 'f') // flag
        {
            unsigned char* c = GetHex (player_x, player_y);

            if (*c & CONCEALED)
            {
                if (*c & FLAGGED)
                {
                    // unflag
                    *c ^= FLAGGED;
                    if (*c & MINED)
                        --mines_flagged;
                    ++flags_left;
                }
                else
                {
                    // flag
                    *c |= FLAGGED;
                    if (*c & MINED)
                        ++mines_flagged;
                    --flags_left;
                }
                DisplayFlagsCount();
                DrawCell (player_x, player_y, 1);
            }
        }
        else if (command == 'q') // quit
        {
            return 1;
        }

        // check for victory, return 3
        if (mines_flagged == total_mines)
        {
            // we ought to reveal the whole map
            int i,j;
            for (i = 0; i < field_x; ++i)
                for (j = 0; j < field_y; ++j)
                {
                    unsigned char* c = GetHex (i,j);
                    if (*c & CONCEALED)
                    {
                        *c ^= CONCEALED;
                        DrawCell(i,j,0);
                    }
                }
            return 3;
        }

    }

    if (explosion)
    {
        textcolor(11);
        cputsxy (player_x * 3, player_y + 2, " BOOM! ");
        msleep(2000);
        return 2;
    }

    return 1; // quit
}



int main ()
{
    char ch;
    int done = 0;
    int game_result = 0;

    // Init random number generator
    srand(time(NULL));

    // cgetc will only work in raw mode
    ioctl(0, IO_TTY_RAW_MODE);
    ioctl(0, IO_TTY_ECHO_OFF);
    
    while (!done)
    {
        game_result = Game();

        if (game_result == 1)
            break; // user quit
        //if (Game())
        //    break; // user quit
        textcolor(11);
        bgcolor(0);
        cputsxy (16,  7, "                   ");
        if (game_result == 2)
            cputsxy (16,  8, " G A M E   O V E R ");
        else
            cputsxy (16,  8, "  CONGRATULATIONS! ");
        textcolor(7);
        cputsxy (16,  9, "                   ");
        cputsxy (16, 10, " Play again (y/n)? ");
        cputsxy (16, 11, "                   ");
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
    gotoxy (0,20); // bottom of screen.
    cputs ("\r\n");
    return 0;
}