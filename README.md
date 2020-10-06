## 第一个网络编程项目 ##

运行环境 CentOS7.7 g++4.8.5 gdb9.2
perl perl-cgi

## About ##

Tinyhttpd 是J. David Blackstone在1999年写的一个不到 500 行的超轻量型 Http Server，用来学习非常不错，可以帮助我们真正理解服务器程序的本质。官网:http://tinyhttpd.sourceforge.net

主要参考Tinyhttpd，但不是完全一样的，有部分修改。比如char \*指针大部分被我换成了string（虽然这么做没有什么实际意义），主要是char \*字符串用的少，C语言string.h里那些字符串函数也用的比较少，不太熟悉，换成string，便于自己理解。

perl写的cgi脚本，我没太看懂，不然cgi脚本可以修改为C++、C去写。父进程读取脚本输出，若是POST，父进程也会输入body的内容，在子进程使用putenv函数设置的环境变量是为了给cgi脚本执行时使用的，相当于通过环境变量实现一个进程间通信。通过execl()调用cgi脚本，环境变量被继承到了了cgi程序中。

客户端程序没写，直接使用chrome和edge连接阿里云服务器测试的。

总体来说：程序代码挺简单的，我也学到了很多,对C++网络编程有了初步的认识。

比如get和post在提交表单时的不同：get的表单内容都是放在url的path？后面以key=value&key=value的形式表示，而post则把表单内容存放在请求报文的body内部。

fork()创建子进程，根据返回值的不同判断是父进程还是子进程。特别是利用命名管道实现进程间通信。
