## 第一个网络编程项目 ##

运行环境 CentOS7.7 g++4.8.5 gdb 7.9

## About ##

Tinyhttpd 是J. David Blackstone在1999年写的一个不到 500 行的超轻量型 Http Server，用来学习非常不错，可以帮助我们真正理解服务器程序的本质。官网:http://tinyhttpd.sourceforge.net

主要参考Tinyhttpd，但是不是完全一样的，有部分修改。比如char \*指针大部分被我换成了string（虽然这么做没有什么实际意义），主要是为了换成我的代码风格，便于自己理解。

客户端没写，直接使用chrome连接测试的。http://39.99.232.213:12345
get已经浏览器测试过了，正常打开没有问题，但是POST会调用CGI脚本，需要安装perl和perl-cgi。
