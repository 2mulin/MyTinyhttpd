/**************************************************
 * @author red
 * @date 2020/3/5
 * @brief 服务器端
 * 主要是参考了tinyhttpd的设计
**************************************************/
#include <sys/stat.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <wait.h>
#include <errno.h>
#include <pthread.h>

#include <cstring>
#include <cstdlib>
#include <cstdio>
#include <string>

#include "httpStat.h"
#include "unit.h"

using std::string;

#define ERROR -1        // 错误
#define NOT_IMPL -2     // http方法未实现
#define NOT_FOD -3      // 文件未找到

// 统计服务器处理请求次数
static int count = 1;

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
    {
        perror("socket()");
        return -1;
    }
    struct sockaddr_in server;
    memset(&server, 0, sizeof(server));
    server.sin_family = AF_INET;
    server.sin_addr.s_addr = htonl(INADDR_ANY); // 任意可用地址，转化成网络字节序
    server.sin_port = htons(port);              // 设置端口
	int opt = 1;
	int ret = setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
	if(ret == -1)
		perror("setsockopt");

    // 绑定socketfd和socket address，专用地址sockaddr_in记得转化成通用地址sockaddr
    if (bind(sockfd, (sockaddr *)&server, sizeof(server)) == -1)
    {
        perror("bind()");
        return -1;
    }

    // getsockname可以返回自动设置的ip和端口到server中，这里返回端口
    socklen_t server_len = sizeof(server);
    if (getsockname(sockfd, (sockaddr *)&server, &server_len) == -1)
    {
        perror("getsocknam()");
        return -1;
    }
    port = ntohs(server.sin_port); // 改变port
    printf("服务器IP：%s\t端口号：%d\n", inet_ntoa(server.sin_addr), port);
	// 开启监听
    if (listen(sockfd, 5) < 0)
    {
        perror("listen()");
        return -1;
    }
    return sockfd;
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
        // GET暂时忽略剩余首部。没有实现
        while (len > 0)
            len = get_line(clientfd, buf);
    }
    else
    {
        // POST继续读报文内容,因为post一般用来提交数据，比如表单
        // 协议规定POST要提交的数据必须放在主体部分
        // 所以content-length一定不为0
        while (len > 0)
        {
            len = get_line(clientfd, buf);
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
            perror("fork函数出现错误: ");
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

            // 设置环境变量
            string method_env = "REQUEST_METHOD=" + method;
            putenv(const_cast<char *>(method_env.data()));
			// 测试一下：环境变量是否配置成功
			// printf("%s \n", getenv("REQUEST_METHOD"));
            
            if (method == "GET")
            {
                string query_env = "QUERY_STRING=" + query;
                putenv(const_cast<char *>(query_env.data()));
            }
            else
            {
                string length_env = "CONTENT_LENGTH=" + content_length;
                putenv(const_cast<char *>(length_env.data()));
            }
            // 参数path可执行文件路径，后面的参数代表执行该文件时传递过去的argv(0)、argv[1]……，最后一个参数必须用空指针(NULL)作结束。
            execl(path.data(), path.data(), NULL);
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
                int len = stoi(content_length);
                for (int i = 0; i < len; i++)
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
    // 读取http报文头部, 不处理
    while (len > 0 && buf != "\r\n")
        len = get_line(clientfd, buf);
    FILE *res = fopen(path.c_str(), "rb");
    if(res == NULL)
    {
        printf("%s文件打开失败！\n", path.data());
        exit(0);// 直接退出
    }
    // 文件成功打开后，将这个文件的基本信息封装成response header先发给客户端
    headers(clientfd);
    // 接着把这个文件的内容读出来作为responce的body发送到客户端
    cat(clientfd, res);
    fclose(res);
}

/****************************************************************************************
 * @param clientfd : 客户端socket文件描述符
 * @param URI : 相对路径
 * @brief GET的实现
 * 请求指定的页面信息，并返回实体主体。如果发现URI中有'？'就需要执行cgi
 * 若是需要执行cgi程序的话。GET报文cgi参数在url后面
****************************************************************************************/
void  Get(int clientfd, string URI)
{
    // 所有资源都在htdocs文件夹内
    string URIpath = "htdocs";
    // 读出URI中的path
    size_t i = 0;
    while (i < URI.length())
    {
        if (URI[i] == '?')
        {
            ++i;
            break;
        }
        URIpath.push_back(URI[i++]);
    }
    // 如果有的话，读出URI中的query
    string URIquery;
    while (i < URI.length())
		URIquery.push_back(URI[i++]);

    // 判断请求的资源文件是否存在
    struct stat statbuf;
    if (stat(URIpath.data(), &statbuf) == -1)
    { // 文件不存在，读空请求报文的剩余部分，返回404 not found
        perror("stat");
        string buf;
        while (get_line(clientfd, buf) > 0 && buf != "\r\n")
            ;
        not_found(clientfd);
        return;
    }

    // 如果请求的资源存在，但是个目录，默认访问该目录下的index.html
    if ((statbuf.st_mode & S_IFMT) == S_IFDIR)
        URIpath += "index.html";

    // 如果有<query>,执行cgi
    if (!URIquery.empty())
        execute_cgi(clientfd, URIpath, "GET", URIquery);
    else
        server_file(clientfd, URIpath);
}

/**************************************************************************
 * @param clientfd 客户端socket描述符
 * @param URI 请求URI
 * @brief POST的实现,向指定资源提交数据进行处理请求（例如提交表单）。
 * 数据被包含在请求体中。POST请求可能会导致新的资源的建立和或已有资源的修改。
**************************************************************************/
void Post(int clientfd, string URI)
{
    string URIpath = "htdocs/cgi-bin";
    size_t i = 0;
    while (i < URI.length())
    {
        if (URI[i] == '?')
            break;
        URIpath += URI[i++];
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
        perror("stat()错误：");
        string buf;
        while (get_line(clientfd, buf) > 0)
            ;
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
 * @return: int 请求处理结果
 * @brief 处理客户端的请求
 * URI完整的格式：<scheme>://<user>:<password>@<host>:<port>/<path>;<params>?<query>
 * 大部分的url都只有三个部分<scheme>://<host>/<path>
**********************************************************************************************/
static void* accept_request(void* arg)
{
    int clientfd = (intptr_t)arg;
    if(clientfd <= 0)
        printf("arg参数错误!\n");
    string buf;
    size_t size = get_line(clientfd, buf);
    if(size == 0)
        return NULL;
    // 请求行的http method
    string http_method;
    string::iterator it = buf.begin();
    while(it != buf.end())
    {
        if (*it == ' ')
        {
            ++it; // 去掉空格
            break;
        }
        http_method.push_back(*it++);
    }
    // 暂时实现GET和POST，其他的http方法直接返回not implemented
    if (http_method != "GET" && http_method != "POST")
    {
        not_implemented(clientfd);
        return NULL;
    }
    string URI;
    // 请求URI
    while(it != buf.end())
    {
        if (*it == ' ')
        {
            ++it;
            break;
        }
        URI.push_back(*it++);
    }
    // http版本号
    string httpVersion(it, buf.end());
    if (http_method == "GET")
    {
        printf("GET %s %s\n", URI.data(), httpVersion.data());
        Get(clientfd, URI);
    }
    if (http_method == "POST")
    {
        printf("POST %s %s\n", URI.data(), httpVersion.data());
        Post(clientfd, URI);
    }

    if(close(clientfd) == -1)
        perror("close");
}

int main()
{
    unsigned short port = 23456;
    int server_sock = startup(port);
    while (true)
    {
        printf("等待第%d个请求：\n", count++);
        struct sockaddr_in client_addr;
        socklen_t client_len = sizeof(client_addr);
        // 接受客户端连接
        int client = accept(server_sock, (sockaddr *)&client_addr, &client_len);
        if (client == -1)
        {
            perror("accept()");
            return -1;
        }
        // 创建一个线程处理请求
        pthread_t tid;
        pthread_attr_t attr;
        pthread_attr_init(&attr);
        pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
        if(-1 == pthread_create(&tid, NULL, accept_request, (void *)(intptr_t)client));
            perror("pthread_create");
    }
    close(server_sock);
    return 0;
}
