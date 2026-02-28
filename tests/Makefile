CC=g++
CPPFLAGS=-O3 -funroll-loops -fno-finite-math-only -I./../src -I/usr/include/eigen3

.PHONY: all
all:	tests

tests:	tests.o
	$(CC) $(LDFLAGS) -o $@ $^

%.o: %.cpp
	$(CC) $(CPPFLAGS) $(CXXFLAGS) -c -o $@ $<

.PHONY:	clean
clean:
	rm -f *.o *.csv tests tests.exe