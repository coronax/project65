
os3.prg: *.asm
	cl65 -l os3.lst -t p65 main.asm cmdline.asm devtab.asm filesystem.asm io.asm sd.asm tty.asm xmodem.asm util.asm vectors.asm -C p65.cfg -o $@
	
clean:
	del *.o *.lst *.prg

