all: httpd

# -lpthread是编译使用了POSIX thread的程序通常加的额外的选项
debug: httpd.cpp
	g++ -std=c++11 -W -Wall -lpthread -o httpd httpd.cpp -g

httpd: httpd.cpp
	g++ -std=c++11 -W -Wall -lpthread -o httpd httpd.cpp 


clean:
	rm httpd