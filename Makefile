all: bin bin/httpd
	rm -f general.log

bin:
	mkdir -p bin

bin/unit.o: src/unit.cpp
	g++ -ggdb -c src/unit.cpp -o bin/unit.o

bin/main.o : src/main.cpp src/unit.h
	g++ -ggdb -c src/main.cpp -o bin/main.o -Isrc/

bin/httpd: bin/main.o bin/unit.o
	g++ -ggdb -std=c++11 -Wall -o bin/httpd bin/main.o bin/unit.o -lpthread

.PHONY: clean
clean:
	rm -rf bin/*
