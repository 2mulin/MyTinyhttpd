all: httpd

debug: httpd.cpp
	g++ -std=c++11 -W -Wall -lpthread -o httpd httpd.cpp -g

httpd: httpd.cpp
	g++ -std=c++11 -W -Wall -lpthread -o httpd httpd.cpp 
# -lpthread是编译使用了POSIX thread的程序通常加的额外的选项

client: client.cpp
	g++ -std=c++11 -W -Wall -lpthread -o client client.cpp

clean:
	rm httpd