CXX=g++
CXXFLAGS=-O3 -march=native -msse3 -msse4.2 -fopenmp -std=c++17 -s `sdl2-config --cflags --libs` -lSDL2_mixer

all: tracer

tracer: main.cpp
	$(CXX) main.cpp -o tracer $(CXXFLAGS)

clean:
	rm -f *.o
	rm tracer

.PHONY: all tracer clean