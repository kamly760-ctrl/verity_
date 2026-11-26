# Build the console program:  make   (then ./verity)
CXX ?= g++
verity: src/truth_table.cpp src/splash.h
	$(CXX) -std=c++11 -Wall -O2 -o verity src/truth_table.cpp
clean:
	rm -f verity verity.exe
