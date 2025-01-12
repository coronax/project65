all: dskspeed.prg

ctest.prg: ctest.c ctest_io.asm 
	cl65 -t p65 ctest.c ctest_io.asm -o ctest.prg

dskspeed.prg: dskspeed.c
	cl65 -t p65 dskspeed.c -o dskspeed.prg

clean:
	del *.prg ctest.o ctest_io.o
