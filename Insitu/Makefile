#Compile with: cl65 -t none --config p65.cfg ctest.c p65.lib

insitu: insitu.c insitu_io.asm p65.cfg
	cl65 -t p65 --config p65.cfg insitu.c insitu_io.asm
	
clean:
	del insitu insitu.o insitu_io.o
