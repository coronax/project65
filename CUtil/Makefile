
all: cp.prg date.prg ls.prg rm.prg rmdir.prg stty.prg

cp.prg: cp.c
	cl65 -t p65 cp.c -o cp.prg

date.prg: date.c
	cl65 -t p65 date.c -o date.prg

ls.prg: ls.c
	cl65 -t p65 ls.c -o ls.prg

rm.prg: rm.c
	cl65 -t p65 rm.c -o rm.prg

rmdir.prg: bsd_rmdir.c
	cl65 -t p65 bsd_rmdir.c -o rmdir.prg

stty.prg: stty.c
	cl65 -t p65 stty.c -o stty.prg

clean:
	del *.prg
