CXX ?= g++
TARGET ?= tests
BASE_CPPFLAGS ?= -I./../src -I/usr/include/eigen3
BASE_CXXFLAGS ?= -std=c++20 -O2 -Wall -Wextra -Wpedantic -funroll-loops -fno-finite-math-only
CPPFLAGS += $(BASE_CPPFLAGS)
CXXFLAGS += $(BASE_CXXFLAGS)
LDFLAGS ?=

.PHONY: all check clean

all: $(TARGET)

check: $(TARGET)
	./$(TARGET)

$(TARGET): tests.o
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LDFLAGS)

reference_acceleration_audit: reference_acceleration_audit.cpp $(wildcard ../src/*.h)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -o $@ $< $(LDFLAGS)

tests.o: tests.cpp jonswap_physics.h $(wildcard ../src/*.h)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -c -o $@ $<

clean:
	rm -f *.o $(TARGET) $(TARGET).exe reference_acceleration_audit *.csv
