all: httpd

httpd: httpd.cpp
	g++ -std=c++11 -W -Wall -o httpd httpd.cpp -lpthread

clean:
	rm *.o
	rm httpd
