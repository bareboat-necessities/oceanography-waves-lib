CC=g++
CPPFLAGS=-O3 -march=native -funroll-loops -fno-finite-math-only -I./../src -I/usr/include/eigen3

.PHONY:	all
all:	fenton_plots

fenton_plots:	fenton_plots.o
	$(CC) -o fenton_plots fenton_plots.o

vessel_rao_response: vessel_rao_response.cpp ../src/VesselRao.h ../src/WaveHarmonic.h
	$(CC) $(CPPFLAGS) -std=c++17 -o $@ $<

.PHONY:	clean
clean:
	rm -f *.o fenton_plots fenton_plots.exe vessel_rao_response
