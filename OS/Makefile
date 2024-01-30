
os3.prg: *.asm
	cl65 -l os3.lst -t p65 main.asm devtab.asm io.asm sd.asm xmodem.asm vectors.asm -C p65.cfg -o $@
	
clean:
	del *.o *.lst *.prg

