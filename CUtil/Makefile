
all: cp.prg ls.prg rm.prg rmdir.prg

cp.prg: cp.c
	cl65 -t p65 cp.c -o cp.prg

ls.prg: ls.c
	cl65 -t p65 ls.c -o ls.prg

rm.prg: rm.c
	cl65 -t p65 rm.c -o rm.prg

rmdir.prg: bsd_rmdir.c
	cl65 -t p65 bsd_rmdir.c -o rmdir.prg

clean:
	del *.prg
