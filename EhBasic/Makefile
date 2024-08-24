all: basic

basic.o: basic.asm
	ca65 -l basic.lst basic.asm

basic: basic.o
	ld65 -t none basic.o -o basic

clean:
	del *.o *.lst *.prg basic


