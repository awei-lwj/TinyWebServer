CC=g++  
CXXFLAGS = -std=c++0x
CFLAGS=-I
skiplist: skiplist_test.o 
	$(CC) -o ../bin/skiplist_test skiplist_test.o --std=c++11 -pthread 
	rm -f ./*.o

clean: 
	rm -f ./*.o
