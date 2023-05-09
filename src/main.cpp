/**
 * @author 2mu
 * @date 2020/3/5
 * @brief 简单的http server实现
 */
#include <cstring>
#include <cstdlib>
#include <string>
#include <unordered_set>
#include <unordered_map>
#include <algorithm>

#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <wait.h>
#include <cerrno>

#include "httpstat.h"
#include "unit.h"

static pthread_mutex_t gSetMutex = PTHREAD_MUTEX_INITIALIZER;
static std::unordered_set<int*> gSetClientSocks;


/**
 * @param port 端口号
 * @return server使用的端口号
 * @brief 创建sockfd并绑定server sockaddr，确定端口号，并且返回socketfd
 */
int startup(unsigned short &port)
{
    // 生成一个socketfd
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd == -1)
    {
        LOG_FATAL << "socket: " << strerror(errno);
        return -1;
    }
    struct sockaddr_in server;
    memset(&server, 0, sizeof(server));
    server.sin_family = AF_INET;
    server.sin_addr.s_addr = htonl(INADDR_ANY); // 任意可用地址，转化成网络字节序
    server.sin_port = htons(port);              // 设置端口
    int opt = 1;
    int ret = setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    if (ret == -1)
    {
        LOG_FATAL << "setsockopt: " << strerror(errno);
        return -1;
    }

    // 绑定socketfd和socket address，专用地址sockaddr_in记得转化成通用地址sockaddr
    if (bind(sockfd, (sockaddr *)&server, sizeof(server)) == -1)
    {
        LOG_FATAL << "bind: " << strerror(errno);
        return -1;
    }

    // 开启监听
    if (listen(sockfd, 5) < 0)
    {
        LOG_FATAL << "listen: " << strerror(errno);
        return -1;
    }

    // getsockname可以返回socfd绑定的ip和端口到 server结构体 中
    socklen_t serverLen = sizeof(server);
    if (getsockname(sockfd, (sockaddr *)&server, &serverLen) == -1)
    {
        LOG_FATAL << "getsockname: " << strerror(errno);
        return -1;
    }
    port = ntohs(server.sin_port);
    LOG_INFO_FMT("server listen: %s:%d", inet_ntoa(server.sin_addr), port);
    return sockfd;
}

/**
 * @brief 执行cgi程序
 * @param clientSock 客户端scoket文件描述符
 * @param strPath cgi脚本的路径
 * @param strMethod 请求的方法
 * @param strQuery 查询，kye=value的格式。可以作为cgi的参数
 * @return bool true成功, false失败
 */
bool executeCgi(int clientSock, const string& strPath, const string& strMethod, const string& strQuery)
{
    int iContentLength = -1;
    if (strMethod == "GET")
    {
        // 忽略GET请求的 header, 暂时没实现
        std::string buf;
        do
        {
            buf = getLine(clientSock);
        }while(buf != "\r\n");
        // 忽略GET请求的 body 内容, RFC规范似乎没有强制规定 GET 请求不允许有 body
        // 不过一般不会存在body; 
    }
    else if(strMethod == "POST")
    {
        // 读header, 因为post一般用来提交数据，比如表单, http协议规定POST要提交的数据必须放在body
        // 所以content-length头部字段值一定不为0
        std::unordered_map<std::string, std::string> mapHeaderField;
        std::string buf;
        do
        {
            buf = getLine(clientSock);
            if(buf == "\r\n")
                break;  // 头部字段读取完毕, body后续再读取
            // 否则就是 字段值, 去除末尾的 "\r\n"
            buf = strip(buf);
            // 第一个':'  出现的位置, 不能用splie函数, 因为可能有多个':' 如Host字段
            std::string::size_type pos = buf.find(':');
            std::string key = buf.substr(0, pos);
            std::transform(key.begin(), key.end(),
                key.begin(), [](unsigned char ch)
                {
                    return std::tolower(ch);
                });
            mapHeaderField[key] = buf.substr(pos + 1);
        }while(true);

        if(mapHeaderField.find("content-length") != mapHeaderField.end())
        {
            iContentLength = std::stoi(mapHeaderField["content-length"]);
        }
        else
        {
            write(clientSock, gStrBadRequest.c_str(), gStrBadRequest.length());
            LOG_ERR << "POST request format error! content-length=0";
            return false;
        }
    }

    // 管道是单向, 父子进程通信需要两个管道;
    int childToParent[2], parentToChild[2];
    if (pipe(childToParent) < 0 || pipe(parentToChild) < 0)
    {
        write(clientSock, gStrInternalServerError.c_str(), gStrInternalServerError.length());
        LOG_ERR << "executeCgi: create pipe failed!";
        return false;
    }

    bool bSuccess = true;
    // 创建一个子进程执行cgi
    pid_t pid = fork();
    switch (pid)
    {
    case -1:
    {
        write(clientSock, gStrInternalServerError.c_str(), gStrInternalServerError.length());
        LOG_ERR << "executeCgi: create child process failed!";
        return false;
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
        char methodEnv[256];
        char queryEnv[256];
        char lengthEnv[256];
        sprintf(methodEnv, "REQUEST_METHOD=%s", strMethod.c_str());
        putenv(methodEnv);
        if (strMethod == "GET")
        {
            sprintf(queryEnv, "QUERY_STRING=%s", strQuery.c_str());
            putenv(queryEnv);
        }
        else
        {
            sprintf(lengthEnv,"CONTENT_LENGTH=%d", iContentLength);
            putenv(lengthEnv);
        }
        if (execl(strPath.c_str(), strPath.c_str(), NULL) == -1)
        {
            /**
             * 只有调用execl出错时, 才会执行以下代码; 如果execl正常执行, 是不会有返回值的;
             * 因为子进程已经被完全重置, 代码段什么都被重置了, 所以不会再运行这些代码
             */
            LOG_ERR_FMT("target: %s, execl: %s", strPath.c_str(), strerror(errno));
            exit(EXIT_FAILURE);
        }
        break;
    }
    default:
    { // 父进程
        close(childToParent[1]);
        close(parentToChild[0]);
        if (strMethod == "POST")
        { // 如果是POST, 读body的内容, 并写到管道让子进程的execl执行的脚本作为输入
            char* buf = new char[iContentLength + 1];
            memset(buf, 0, iContentLength + 1);
            read(clientSock, buf, iContentLength);
            write(parentToChild[1], buf, strlen(buf));
            delete[] buf;
        }
        // 先发送header给客户端
        write(clientSock, gStrResponseHeader.c_str(), gStrResponseHeader.length());
        // 从子进程读出cgi程序的执行结果，发给客户端
        char buf[4096] = {0};
        while (read(childToParent[0], buf, 4095) > 0)
            write(clientSock, buf, strlen(buf));
        // 关闭两个管道
        close(childToParent[0]);
        close(parentToChild[1]);
        // 等待子进程的退出,第二个参数是记录子进程退出状态, 
        int *childStat = new int(0);
        waitpid(pid, childStat, 0);
        if(!WIFEXITED(*childStat))
            bSuccess = false;   // 进程没有正常结束
        if(WIFEXITED(*childStat) && WEXITSTATUS(*childStat))
            bSuccess = false;   // 进程正常终止, 但是返回错误码
        break;
    }
    }
    return bSuccess;
}

/**
 * @param clientSock 客户端socket
 * @param strPath 访问的文件路径
 * @brief 向客户端发送文件内容
 * @return bool true成功, false失败
 */
bool sendServerFile(int clientSock, const string& strPath)
{
    // 读取http请求的header字段和body, 但实际上不进行处理
    string requestHttpHead;
    // 忽略GET请求的 header, 暂时没实现
    std::string tmpStr;
    do
    {
        tmpStr = getLine(clientSock);
        requestHttpHead += tmpStr;
    }while(tmpStr != "\r\n");
    // 一般GET请求不会有body, 这里不处理;

    int fd = open(strPath.c_str(), O_RDONLY);
    if (fd == -1)
    {
        LOG_ERR << strPath << " file open failed! " << strerror(errno);
        write(clientSock, gStrNotFound.c_str(), gStrNotFound.length());
        return false;
    }
    // 先发送header给客户端
    write(clientSock, gStrResponseHeader.c_str(), gStrResponseHeader.length());

    // 将文件所有内容读出来, 作为responce的body发送到客户端
    char buf[4096] = {0};
    ssize_t rtn = 1;
    while(rtn > 0)
    {
        rtn = read(fd, buf, sizeof(buf) - 1);
        write(clientSock, buf, strlen(buf));
        memset(buf, 0, sizeof(buf));
    }
    return true;
}

/**
 * @param clientSock 客户端socket
 * @param URI 请求URI
 * @brief 处理GET请求; 请求指定的页面信息，并返回实体主体。如果发现URI中有'？'就需要执行cgi
 * 若是需要执行cgi程序的话。GET报文cgi参数在url后面
 */
void processGet(int clientSock, const string& strURI)
{
    // 所有资源都在htdocs文件夹内, htdocs对外, 相当于是 整个httpd 的根目录;
    string strPath = "htdocs";
    string strURIQuery;
    // 读出URI中的path
    std::string::size_type pos = strURI.find('?');
    if(pos == std::string::npos)
        strPath += strURI;
    else
    {
        strPath += strURI.substr(0, pos);
        strURIQuery = strURI.substr(pos + 1);
    }

    // 判断请求的资源文件是否存在
    int rtn = access(strPath.c_str(), F_OK);
    if(rtn != 0)
    {
        write(clientSock, gStrNotFound.c_str(), gStrNotFound.length());
        LOG_ERR << "not found: " << strPath;
        return;
    }

    struct stat statTarget;
    stat(strPath.c_str(), &statTarget);
    // 如果请求的资源存在，是目录，默认访问该目录下的index.html
    if ((statTarget.st_mode & S_IFMT) == S_IFDIR)
        strPath += "/index.html";

    bool isSuccess = false;
    // 如果有<query>, 执行cgi
    if (!strURIQuery.empty())
    {
        if(strPath.find("htdocs/cgi-bin") == std::string::npos)
            isSuccess = sendServerFile(clientSock, strPath);// 不是cig-bin目录下的文件, 带query也没用....
        else
            isSuccess = executeCgi(clientSock, strPath, "GET", strURIQuery);
    }
    else
        isSuccess = sendServerFile(clientSock, strPath);
    if(isSuccess)
        LOG_INFO << "GET " << strURI << " success!";
    else
        LOG_ERR << "GET " << strURI << " failed!";
}

/**
 * @brief POST的实现, 向指定资源提交数据进行处理请求(例如提交表单)
 * 数据被包含在请求体中, POST请求可能会导致新的资源的建立和或已有资源的修改;
 * @param clientSock 客户端socket
 * @param URI 请求URI
 */
void processPost(int clientSock, const string& strURI)
{
    string strPath = "htdocs/cgi-bin";
    string strQuery;
    std::string::size_type pos = strURI.find('?');
    if(pos == std::string::npos)
        strPath += strURI;
    else
    {
        strPath += strURI.substr(0, pos);
        strQuery = strURI.substr(pos + 1);
    }

    // 判断请求的cgi脚本是否存在
    int rtn = access(strPath.c_str(), F_OK);
    if(rtn != 0)
    {
        write(clientSock, gStrNotFound.c_str(), gStrNotFound.length());
        return;
    }
    executeCgi(clientSock, strPath, "POST", strQuery);
}

/**
 * @brief 等待客户端完全关闭
 * @param clientSock 等待的socket
 */
void waitSockClose(int clientSock)
{
    shutdown(clientSock, SHUT_WR);
    ssize_t rtn = 1;
    char* buf = new char[4096];
    // 直到read返回0, 才表示对端关闭;
    while(rtn != 0)
    {
        rtn = read(clientSock, buf, 4095);
    }
    delete[] buf;
    shutdown(clientSock, SHUT_RD);
}


/**
 * @brief 处理客户端的请求 URI完整的格式：<scheme>://<user>:<password>@<host>:<port>/<path>;<params>?<query>
 * 大部分的url都只有三个部分<scheme>://<host>/<path>
 * @param pClientSock 请求的客户端sockfd
 */
static void* processRequest(void* pClientSock)
{
    int clientSock = *((int*)pClientSock);
    // 读取request请求行: http方法 URI http版本号
    string buf = getLine(clientSock);
    buf = strip(buf);
    if (buf.size() > 0)
    {
        std::vector<std::string> vctWords = split(buf, ' ');
        std::string strHttpMethod, strURI, strHttpVersion;
        if(vctWords.size() == 3)
        {
            strHttpMethod = vctWords[0];
            strURI = vctWords[1];
            strHttpVersion = vctWords[2];
        }
        if (strHttpMethod == "GET")
        {
            LOG_INFO_FMT("GET %s %s", strURI.c_str(), strHttpVersion.c_str());
            processGet(clientSock, strURI);
        }
        else if (strHttpMethod == "POST")
        {
            LOG_INFO_FMT("POST %s %s", strURI.c_str(), strHttpVersion.c_str());
            processPost(clientSock, strURI);
        }
        else
        {
            LOG_INFO_FMT("%s", buf.c_str());
            write(clientSock, gStrNotImplemented.c_str(), gStrNotImplemented.length());
        }
        waitSockClose(clientSock);
    }
    else
    {
        close(clientSock);
    }

    pthread_mutex_lock(&gSetMutex);
    gSetClientSocks.erase((int*)pClientSock);
    pthread_mutex_unlock(&gSetMutex);
    return NULL;
}


int main()
{
    signal(SIGPIPE, SIG_IGN);

    unsigned short port = 23456;
    int serverSock = startup(port);

    while (true)
    {
        struct sockaddr_in clientAddr;
        socklen_t sockLen = sizeof(clientAddr);
        // 接受客户端连接
        int clientSock = accept(serverSock, (sockaddr *)&clientAddr, &sockLen);
        if (clientSock == -1)
        {
            LOG_ERR << "accept: " << strerror(errno);
            continue;
        }
        // 创建线程处理请求... (tinyhttpd源码也是多线程)
        pthread_t newThread;
        int* pSock = new int(clientSock);
        
        pthread_mutex_lock(&gSetMutex);
        int rtn = pthread_create(&newThread, nullptr, processRequest, pSock);
        if(rtn != 0)
        {
            LOG_ERR << "pthread_create:" << strerror(errno);
            delete pSock;
        }
        else
            gSetClientSocks.insert(pSock);
        pthread_mutex_unlock(&gSetMutex);

        rtn = pthread_detach(newThread);
        if(rtn != 0)
            LOG_ERR << "pthread_detach!" << strerror(errno);
    }
    close(serverSock);
    return 0;
}
