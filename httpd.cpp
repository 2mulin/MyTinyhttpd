/**************************************************
 * @author red
 * @date 2020/3/5
 * @brief 服务器端
 * 主要是参考了tinyhttpd的设计
**************************************************/

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/stat.h> // stat()判读文件是否存在

#include <cstring>  //memset()头文件
#include <cstdlib>  //exit()、putenv()头文件
#include <unistd.h> //close()头文件，pipe()
#include <wait.h>   // waitpid()头文件
#include <errno.h>
#include <iostream>
#include <string>
using namespace std;

// errno是全局变量，输出错误消息
void print_err(string msg)
{
    std::cerr << msg << endl;
    std::cerr << "errno = " << errno << "," << strerror(errno) << std::endl;
    exit(-1);
}

/***************************************************************
 * @param port: 端口号
 * @return: server使用的端口号
 * @brief
 * 创建sockfd并绑定server sockaddr，确定端口号，并且返回socketfd
 ***************************************************************/
int startup(unsigned short &port)
{
    // 生成一个socketfd
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd == -1)
        print_err("startup()'s socket() failed");
    struct sockaddr_in server;
    memset(&server, 0, sizeof(server));
    server.sin_family = AF_INET;
    // 设置IPv4地址，这里应该是随机给的
    server.sin_addr.s_addr = htonl(INADDR_ANY); // 本函数将一个32位数从主机字节顺序转换成网络字节顺序。
    server.sin_port = htons(port);              // 设置端口

    // 绑定socketfd和socket address，专用地址sockaddr_in记得转化成通用地址sockaddr
    if (bind(sockfd, (sockaddr *)&server, sizeof(server)) == -1) // 若传进去的端口号为0，则系统会选择本地临时端口
        print_err("startup()'s bind() failed");

    // 若一个套接字与INADDR_ANY捆绑，也就是说该套接字可以用任意主机的地址，
    // 此时除非调用connect()或accept()来连接，否则getsockname()将不会返回主机IP地址的任何信息。
    // getsockname可以返回自动设置的ip和端口到server中，这里返回端口
    socklen_t server_len = sizeof(server);
    if (getsockname(sockfd, (sockaddr *)&server, &server_len) == -1)
        print_err("startup()'s getsockname() failed");
    port = ntohs(server.sin_port); // 改变port，这里的port是引用

    cout << "Server ip=" << inet_ntoa(server.sin_addr) << "\tport=" << port << endl;

    if (listen(sockfd, 5) < 0)
        print_err("startup()'s listen()");
    return sockfd;
}

/**************************************************************
 * @param clientfd 被读取一行的scoketfd（源地址）
 * @param buf 目的地址
 * @return std::size_t 读取到的长度
 * @brief 每次读取一字节(char)，直到读到报文行末"\r\n"
 * 如果读到了'\r'，替换成'\n'。行末的"\r\n"也替换成'\n'
 * linux换行是'\n', windows换行是"\r\n" ,macOS换行是'\r'
***************************************************************/
std::size_t getline_request(int clientfd, string &buf)
{
    // 清空buf
    buf.clear();
    // 设置recv超时时间，如果不设置的话，recv和send都会一直阻塞
    struct timeval timeout = {3, 0}; // 3s
    setsockopt(clientfd, SOL_SOCKET, SO_RCVTIMEO, (char *)&timeout, sizeof(timeout));

    char ch = '\0', next = '\0';
    while (ch != '\n')
    {
        // recv读取后返回实际读取的长度。1是我当前想要读取的长度
        // 返回-1表示错误，返回0表示socket已经关闭
        int len = recv(clientfd, &ch, 1, 0);
        // MSG_PEEK标志表示窥探缓存的数据，但是不会从缓冲中删除这些数据
        int nextlen = recv(clientfd, &next, 1, MSG_PEEK);
        if (len > 0 && nextlen > 0)
        {
            if (ch == '\r' && next == '\n')
            {
                recv(clientfd, &ch, 1, 0); // 行末\r\n,从缓存中读掉末尾的'\n'
                break;                     // 已经确定这是行末\r\n,跳出循环
            }
            buf.push_back(ch);
        }
        else
            ch = '\n'; // 报文错误，退出
    }
    return buf.length();
}

/*******************************************************************
 * @param clientfd 客户端socket
 * @return int类型 返回close()执行结果
 * @brief 500 Internal Server Error
 * 服务器执行请求过程中出错，可能是服务器存在bug。返回500状态给客户端。
 * 并且关闭连接
 *******************************************************************/
int Internal_Server_Error(int clientfd)
{
    char buf[] =
        "HTTP/1.0 500 Internal Server Error\r\n"
        "Server: httpd1.0\r\n"
        "Content-Type: text/html\r\n"
        "\r\n"
        "<html>\r\n"
        "   <head>\r\n"
        "       <title>\r\n"
        "           500 Internal Server Error\r\n"
        "       </title>\r\n"
        "   </head>\r\n"
        "   <body>\r\n"
        "       <p>CGI execute Error</p>\r\n"
        "   </body>\r\n"
        "</html>\r\n";
    if(send(clientfd, buf, sizeof(buf), 0) == -1)
        return -1;
    cout << "500 Internal Server Error 已发送" << endl;
    return 0;
}
/********************************************************************
 * @param client 客户端socket
 * @return int类型 返回close()执行的结果
 * @brief 501 Not Implemented
 * http方法未实现，告知客户端
********************************************************************/
int not_implemented(int clientfd)
{
    char buf[] =
        "HTTP/1.0 501 Not Implemented\r\n"
        "Server: httpd1.0\r\n"
        "Content-Type: text/html\r\n"
        "\r\n"
        "<html>\r\n"
        "   <head>\r\n"
        "       <title>501 not implemented</title>\r\n"
        "   </head>\r\n"
        "   <body>\r\n"
        "       <p>HTTP request method not supported.</p>\r\n"
        "   </body>\r\n"
        "</html>\r\n";
    if(send(clientfd, buf, sizeof(buf), 0) == -1)
        return -1;
    cout << "501 not_implemented 已发送" << endl;
    return 0;
}

/********************************************************************
 * @param clientfd 客户端socket
 * @return int类型 close()执行的结果
 * @brief 404 Not found
 * 请求资源未找到，告知客户端
********************************************************************/
int not_found(int clientfd)
{
    char buf[] =
        "HTTP/1.0 404 NOT FOUND\r\n"
        "Server: httpd1.0\r\n"
        "Content-Type: text/html\r\n"
        "\r\n"
        "<html>\r\n"
        "   <head>\r\n"
        "       <title>Not Found</title>\r\n"
        "   </head>\r\n"
        "   <body>\r\n"
        "       <p>404 Not Found!</p>\r\n"
        "   </body>\r\n"
        "</html>\r\n";
    if(send(clientfd, buf, sizeof(buf), 0) == -1)
        return -1;
    cout << "404 not found 已发送" << endl;
    return 0;
}

/********************************************************************
 * @param clientfd 客户端socket
 * @return int类型 close()执行的结果
 * @brief 400 Bad request
 * 表示客户端发送的请求报文语法存在错误
********************************************************************/
int bad_request(int clientfd)
{
    char buf[] =
        "HTTP/1.0 400 Bad request\r\n"
        "Server: httpd1.0\r\n"
        "Content-Type: text/html\r\n"
        "\r\n"
        "<html>\r\n"
        "   <head>\r\n"
        "       <title>Bad request</title>\r\n"
        "   </head>\r\n"
        "   <body>\r\n"
        "       <p>Your browser sent to a bad request</p>\r\n"
        "       <p>such as a POST without a Content-Length</p>\r\n"
        "   </body>\r\n"
        "</html>\r\n";
    if(send(clientfd, buf, sizeof(buf) - 1, 0) == -1)
        return -1;
    cout << "400 bad request已发送" << endl;
    return 0;
}

/***************************************************************
 * @param clientfd 客户端scoket文件描述符
 * @param path 请求的文件所在路径
 * @param mathod 请求的方法
 * @param query 查询，kye=value的格式。可以作为cgi的参数
 * @brief 执行cgi程序
***************************************************************/
void execute_cgi(int clientfd, const string path, string method, const string query)
{
    string buf;
    int len = 1;
    string content_length;
    if (method == "GET")
    {
        // GET暂时忽略剩余首部和主体的内容。还不会实现
        while (len > 0)
            len = getline_request(clientfd, buf);
    }
    else
    {
        // POST继续读报文内容,因为post一般用来提交数据，比如表单
        // 协议规定POST要提交的数据必须放在主体部分
        // 所以content-length一定不为0
        while (len > 0)
        {
            len = getline_request(clientfd, buf);
            if (buf == "\r\n")
                break;
            // 如果这一行是Conntent-Length
            if (buf.substr(0,16) == "Content-Length: ")
            {
                // 读完header，把body的大小读出来
                for(size_t i = 16; i < buf.length(); i++)
                    content_length.push_back(buf[i]);
            }
        }

        if (content_length.empty())
        {
            bad_request(clientfd);
            return;
        }
    }

    // 子写父读,父写子读
    int childToParent[2], parentToChild[2];
    if (pipe(childToParent) < 0)
    { // 创建失败，直接返回500内部服务器错误
        Internal_Server_Error(clientfd);
        return;
    }
    if (pipe(parentToChild) < 0)
    {
        Internal_Server_Error(clientfd);
        return;
    }

    buf = "HTTP/1.0 200 OK\r\n";
    send(clientfd, buf.data(), buf.length(), 0);
    
    // 创建一个子进程，返回的pid,子进程返回的是0，父进程返回的是子进程的pid
    pid_t pid = fork();

    switch(pid)
    {
        case -1:
        {
            print_err("fork函数出现错误");
            Internal_Server_Error(clientfd);
            break;
        }
        case 0:
        { // 子进程执行cgi
            // 管道写端绑定 子进程的标准输出
            close(1);
            dup2(childToParent[1], 1);
            // 管道读端绑定 子进程的标准输入
            close(0);
            dup2(parentToChild[0], 0);

            close(childToParent[0]);
            close(parentToChild[1]);

            // 在子进程中设置环境变量
            string method_env = "REQUEST_METHOD=" + method;
            putenv(const_cast<char *>(method_env.data()));
            
            if (method == "GET")
            {
                string query_env = "QUERY_STRING=" + query;
                putenv(const_cast<char *>(query_env.data()));
				// 测试一下：环境变量是否配置成功
				printf("%s \n"getenv("REQUEST_METHOD"));
            }
            else
            {
                string length_env = "CONTENT_LENGTH=" + content_length;
                putenv(const_cast<char *>(content_length.data()));
				printf("%s \n"getenv("CONTENT_LENGTH"));
            }
            // execl()用来执行参数path字符串所代表的文件路径，接下来的参数代表执行该文件时
            // 传递过去的argv(0)、argv[1]……，最后一个参数必须用空指针(NULL)作结束。
            if (execl(path.data(), NULL) == -1)
                print_err("cgi执行错误");
            exit(0); //子进程退出
            break;
        }
        default:
        {// 父进程
            close(childToParent[1]);
            close(parentToChild[0]);
            char ch;
            if (method == "POST")
            {// 如果是POST，继续读body的内容,并写到管道让子进程的execl执行的脚本作为输入
                string buf;
                for (int i = 0; i < stoi(content_length); i++)
                {
                    recv(clientfd, &ch, 1, 0);
                    buf += ch;
                }
                write(parentToChild[1], buf.data(), buf.length());
            }
            // 从子进程读出cgi程序的执行结果，发给客户端
            while (read(childToParent[0], &ch, 1) > 0)
                send(clientfd, &ch, sizeof(ch), 0);
            // 关闭两个管道
            close(childToParent[0]);
            close(parentToChild[1]);
            // 等待子进程的退出,第二个参数是记录子进程退出状态，null就不会记录
            waitpid(pid, NULL, 0);
            break;
        }
    }
    return;
}

/********************************************************
 * @param clientfd 客户端socket文件描述符
 * @param paht 文件名路径
 * @brief 把报文头部发送客户端
********************************************************/
void headers(int clientfd)
{
    char buf[] =
        "HTTP/1.0 200 OK\r\n"
        "Server: httpd/1.0\r\n"
        "Content-Type: text/html\r\n"
        "\r\n";
    send(clientfd, buf, sizeof(buf) - 1, 0);
    cout << "200 OK已发送" << endl;
}

/*******************************************************
 * @param clientfd 客户端socket文件描述符
 * @param res 文件指针
 * @brief 把文件内容发送客户端
 ******************************************************/
void cat(int clientfd, FILE *res)
{
    char buf[1024];

    //从文件文件描述符中读取指定内容
    fgets(buf, sizeof(buf), res);
    while (!feof(res))
    {
        send(clientfd, buf, strlen(buf), 0);
        fgets(buf, sizeof(buf), res);
    }
}

/***************************************************************
 * @param clientfd 客户端socket文件描述符
 * @param path 文件路径
 * @brief 向客户端发送常规文件
 **************************************************************/
void server_file(int clientfd, string path)
{
    string buf;
    int len = 1;
    // 读取http报文后面内容，暂时忽略后面所有的内容
    while (len > 0)
        len = getline_request(clientfd, buf);
    FILE *res = fopen(path.c_str(), "r");
    if (res == NULL)
        not_found(clientfd);
    else
    {
        // 文件成功打开后，将这个文件的基本信息封装成response的header发给客户端
        headers(clientfd);
        // 接着把这个文件的内容读出来作为responce的body发送到客户端
        cat(clientfd, res);
    }
    fclose(res);
}

/****************************************************************************************
 * @param clientfd : 客户端socket文件描述符
 * @param URI : 相对路径
 * @brief GET的实现
 * 请求指定的页面信息，并返回实体主体。如果发现URI中有'？'就需要执行cgi
 * 若是需要执行cgi程序的话。GET报文cgi参数在url后面
****************************************************************************************/
void Get(int clientfd, string URI)
{
    cout << "GET " << URI << endl;
    // 所有资源都在htdocs文件夹内
    string URIpath = "htdocs";
    // 读出URI中的path
    size_t i = 0;
    while (i < URI.length())
    {
        if (URI[i] == '?')
            break;
        URIpath.push_back(URI[i++]);
    }
    i++;
    // 如果有的话，读出URI中的query
    string URIquery;
    while (i < URI.length())
        URIquery.push_back(URI[i]);

    // 判断请求的资源文件是否存在
    struct stat st;
    if (stat(URIpath.data(), &st) == -1)
    { // 文件不存在，读空请求报文的剩余部分，返回404 not found
        int len = 1;
        string buf;
        while (len > 1)
            len = getline_request(clientfd, buf);
        not_found(clientfd);
        return;
    }

    // 如果请求的资源存在，但是个目录，默认访问该目录下的index.html
    if ((st.st_mode & __S_IFMT) == __S_IFDIR)
        URIpath += "index.html";

    // 如果有<query>
    if (!URIquery.empty())
        execute_cgi(clientfd, URIpath, "GET", URIquery);
    else
        server_file(clientfd, URIpath);
}

/**************************************************************************
 * @param clientfd 客户端socket描述符
 * @param URI url
 * @brief POST的实现,向指定资源提交数据进行处理请求（例如提交表单）。
 * 数据被包含在请求体中。POST请求可能会导致新的资源的建立和或已有资源的修改。
**************************************************************************/
void Post(int clientfd, string URI)
{
    cout << "POST " << URI << endl;
    //读出URI中filepath,假设没有参数params
    string URIpath = "htdocs";
    unsigned int i = 0;
    while (i < URI.length())
    {
        if (URI[i] == '?')
            break;
        URIpath += URI[i];
        i++;
    }
    i++;
    // 如果有的话，读出URI中的query
    string URIquery;
    while (i < URI.length())
        URIquery.push_back(URI[i]);

    // 判断请求的资源文件是否存在
    struct stat st;
    if (stat(URIpath.data(), &st) == -1)
    { // 请求的文件不存在，读空缓存，返回404 not found
        int len = 1;
        string buf;
        while (len > 1)
            len = getline_request(clientfd, buf);
        not_found(clientfd);
        return;
    }

    // 如果文件资源是个目录。那就默认去访问一下该目录下的index.html，目录没有index.html的话，就是404 not found
    if ((st.st_mode && __S_IFMT) == __S_IFDIR)
        URIpath += "index.html";

    // 执行脚本
    execute_cgi(clientfd, URIpath, "POST", URIquery);
}

/*********************************************************************************************
 * @param client 请求的客户端sockfd
 * @return: 无
 * @brief 处理请求
 * URI可能是绝对路径，也可能是相对路径，相对路径居多，这里假设是相对路径/<path>;<params>?<query>
 * 完整绝对路径的格式：<scheme>://<user>:<password>@<host>:<port>/<path>;<params>?<query>
 * 大部分的url都只有三个部分<scheme>://<host>/<path>
 * 请求头的URI，一般是path?query
**********************************************************************************************/
void accept_request(int clientfd)
{
    string buf;
    size_t size = getline_request(clientfd, buf);
    if(size <= 0)
        return;
    // 把请求报文的http方法记下来
    string http_method;
    size_t i;
    for (i = 0; i < size; i++)
    {
        if (buf[i] == ' ')
            break;
        http_method += buf[i];
    }

    // 暂时实现GET和POST，其他的http方法直接返回未实现
    if (http_method != "GET" && http_method != "POST")
    {
        cout << http_method << "未实现" << endl;
        not_implemented(clientfd);
        return;
    }
    // 读掉空格
    while (buf[i] == ' ')
        i++;
    string URI;
    // 读出URI
    for (; i < size; i++)
    {
        if (buf[i] == ' ')
            break;
        URI += buf[i];
    }
    // 还有最后的http版本号

    // 暂时没有考虑首部字段。
    if (http_method == "GET")
        Get(clientfd, URI);
    if (http_method == "POST")
        Post(clientfd, URI);
    return;
}

int main()
{
    int server_sockfd = -1;   // 服务器端的socket文件描述符
    unsigned short port = 23456; // 端口号 0（自动选择临时可用端口)

    server_sockfd = startup(port);

    cout << "httpd1.0 running on port:" << port << endl;
    while (true)
    {
        int client_sockfd = -1;   // 客户端的socket文件描述符
        struct sockaddr_in client_addr;
        socklen_t client_len = sizeof(client_addr);
        // 接受客户端连接
        client_sockfd = accept(server_sockfd, (sockaddr *)&client_addr, &client_len);
        if (client_sockfd == -1)
            print_err("main()'s accept() failed");
        else
            cout << "tcp connected with ip=" << inet_ntoa(client_addr.sin_addr) << "\tport=" << client_addr.sin_port << endl;
        accept_request(client_sockfd);
        // 关闭客户端连接
        close(client_sockfd);
        cout << endl;
    }


    close(server_sockfd);
    return 0;
}
