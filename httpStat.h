#ifndef HTTPSTAT__H
#define HTTPSTAT__H

#include <sys/types.h>
#include <sys/socket.h>
#include <stdio.h>


// 把报文头部发送客户端
void headers(int clientfd)
{
    char buf[] =
        "HTTP/1.0 200 OK\r\n"
        "Server: httpd/1.0\r\n"
        "Content-Type: \r\n"
        "\r\n";
    send(clientfd, buf, sizeof(buf) - 1, 0);
    printf("200 OK已发送\n");
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
    printf("500 Internal Server Error 已发送\n");
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
    printf("501 not_implemented 已发送\n");
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
    printf("404 not found 已发送\n");
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
    printf("400 bad request 已发送\n");
    return 0;
}


#endif //HTTPSTAT__H