#ifndef HTTPSTAT_H
#define HTTPSTAT_H

#include <string>

/**
 * @brief 返回给client响应报文的头部数据
 */
std::string gStrResponseHeader =
    "HTTP/1.0 200 OK\r\n"
    "Server: httpd/1.0\r\n"
    "Content-Type: text/html\r\n"
    "\r\n";

/**
 * @brief 500 Internal Server Error
 */
std::string gStrInternalServerError = 
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

/**
 * @brief 501 Not Implemented 暂不支持的http请求
 */
std::string gStrNotImplemented = 
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

/**
 * @brief 404 Not found 请求资源未找到
 */
std::string gStrNotFound = 
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

/**
 * @brief 400 Bad request 表示客户端发送的请求报文语法存在错误
 */
std::string gStrBadRequest = 
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

#endif //HTTPSTAT_H