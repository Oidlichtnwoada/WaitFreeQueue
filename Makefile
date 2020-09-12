all: Benchmark

Benchmark: Benchmark.o
	g++ -O3 --std=c++2a -lpthread -o Benchmark Benchmark.o

Benchmark.o: src/Benchmark.cpp
	g++ -O3 --std=c++2a -c -o Benchmark.o src/Benchmark.cpp

clean:
	rm -rf Benchmark Benchmark.o
