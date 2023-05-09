#ifndef UNIT_H
#define UNIT_H

#include <string>
#include <vector>
#include <sstream>

using namespace std;

/**
 * @brief 从客户端socket中读取一行数据; **效率非常低**, 因为每次读取1 byte;
 * @param clientFd 客户端socket, 不能是非阻塞IO, 非阻塞IO不能读, 会当作错误;
 * @return std::string 读到的一行数据
 */
std::string getLine(int clientFd);

/**
 * @brief 去除字符串 尾部的空白字符; 如"\r\n", "\n", "\r"字符, 以及空格
 * @return std::string
 */
std::string strip(std::string str);

/**
 * @brief 字符串分割, 将一个字符串按照指定字符分割为多个字符串
 * @param str 需要分割的字符串
 * @param sep 用于分割字符串的字符
 * @return std::vector<std::string>
 */
std::vector<std::string> split(std::string str, char sep);

// 日志等级
enum class ELogLevel
{
    INFO,
    ERR,
    FATAL
};

// 日志等级 转化为 字符串
std::string LogLevelToStr(ELogLevel level);

class LogEvent
{
public:
    LogEvent(ELogLevel level);
    ~LogEvent();

    void format(const char *fmt, ...);
    std::stringstream &getSS();

private:
    ELogLevel m_level;
    std::stringstream m_ss;
};

#define LOG_INFO LogEvent(ELogLevel::INFO).getSS()
#define LOG_ERR LogEvent(ELogLevel::ERR).getSS()
#define LOG_FATAL LogEvent(ELogLevel::FATAL).getSS()

#define LOG_INFO_FMT(fmt, ...) LogEvent(ELogLevel::INFO).format(fmt, __VA_ARGS__)
#define LOG_ERR_FMT(fmt, ...) LogEvent(ELogLevel::ERR).format(fmt, __VA_ARGS__)
#define LOG_FATAL_FMT(fmt, ...) LogEvent(ELogLevel::FATAL).format(fmt, __VA_ARGS__)

#endif // UNIT_H
