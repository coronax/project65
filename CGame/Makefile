
all: mines.prg snek.prg

mines.prg: mines.c 
	cl65 -t p65 mines.c -o mines.prg

snek.prg: snek.c 
	cl65 -t p65 snek.c -o snek.prg

clean:
	del *.prg *.o
