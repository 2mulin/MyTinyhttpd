# 尝试网络编程

依赖: 

```shell
apt-get install libcgi-ajax-perl
apt-get install libcgi-application-perl
```

## 一. 第一次尝试

Tinyhttpd 是`J. David Blackstone`在1999年写的一个不到 500 行的超轻量型 Http Server, 可以帮助我们真正理解服务器程序的本质。代码地址: http://tinyhttpd.sourceforge.net

主要参考Tinyhttpd的实现, 学习网络编程。

perl写的cgi脚本，我没太看懂，不然cgi脚本可以修改为C++, C去写。主要逻辑是父进程读取脚本输出，若是POST，父进程也会输入body的内容，在子进程使用putenv函数设置的环境变量是为了给cgi脚本执行时使用的，相当于通过环境变量实现一个进程间通信。通过`execl()`调用cgi脚本，环境变量被继承到cgi程序中; 然后cgi脚本返回正确的html文本给父进程, 再发送给对端;

无需客户端程序, 可以直接使用chrome浏览器测试能否访问;

知识点:

* HTTP的GET请求和POST请求在提交表单时的不同:
    1. GET请求的表单内容都是放在url的`path?`后面以`key=value&key=value`的形式表示
    2. POST请求则把表单内容存放在请求报文的body内部。
* fork()的应用, 创建子进程，根据返回值的不同判断是父进程还是子进程。exit销毁进程
* 利用管道实现进程间通信。

## 二. 修改

三年之后查看自己的代码真的是一团糟, 当初想要使用这种项目作为面试项目, 怪不得败得一塌涂地;

还是重新整理下:

1. 对于execl函数族的错误理解, execl是会重新初始化当前进程, 一旦执行成功, 是不会返回的, 并且当前进程的代码段都会被修改, 所以execl之后的代码没有意义(除检测execl错误之外)
2. `if ((st.st_mode && __S_IFMT) == __S_IFDIR)`这代码也是bug, 是按位`&`, 否则这个判断结果永远是false, 因为`(st.st_mode && __S_IFMT)`结果是1, `__S_IFDIR`是另一个数, 永远不相等;
3. 代码文件名命名全部用小写; 代码格式修改: tab键全部改成 4个空格, 还有宏定义和全局变量也需要修改下, 函数命名也是, 有些是驼峰命名, 有些是下划线, 需要统一风格;
4. 简单的加个日志文件吧, 现在全都是printf作为日志, 无法持久化, low的一批;
5. 增加多线程的支持, 原本tinyhttpd在Linux上不支持多线程, 作者使用的pthread_create接口都不一样;
6. 忽略SIGPIPE信号, 该信号默认处理是进程退出; 但是往一个已经关闭的socket中write(有概率出现这种情况), 就会触发该信号;

## 三. 后续计划

1. 根据UNP中的说法, write可能会被信号中断, 导致写入不完整, 但是这里暂时不处理这种情况, 假设write一定成功;
