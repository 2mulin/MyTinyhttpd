#include "unit.h"

#include <cstdarg>
#include <cstdio>
#include <cstring>

#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <pthread.h>

// 暂定写死 日志文件名
static std::string gLogFileName = "general.log";
static pthread_mutex_t gLogMtx = PTHREAD_MUTEX_INITIALIZER;

std::string getLine(int clientfd)
{
    std::string buf;
    char ch = 0;
    while (ch != '\n')
    {
        int ret = read(clientfd, &ch, 1);
        if (ret == -1)
        {
            if (errno == EINTR)
                continue; // 读取过程中, 被信号中断, 重新读取即可
            else
                return buf; // 出现了错误, 或者非阻塞IO, 当前不能读
        }
        else if (ret == 0)
            break; // 读到了EOF, 表示对端socket已经关闭
        else
        {
            buf.push_back(ch);
        }
    }
    return buf;
}

std::string strip(std::string str)
{
    if (str.back() == '\n')
        str.pop_back();
    if (str.back() == '\r')
        str.pop_back();
    while (str.back() == ' ')
        str.pop_back();
    return str;
}

std::vector<std::string> split(std::string str, char sep)
{
    std::string tmpStr;
    std::vector<std::string> vctStrs;
    for (auto ch : str)
    {
        if (ch == sep && !tmpStr.empty())
        {
            vctStrs.push_back(tmpStr);
            tmpStr.clear();
        }
        else
            tmpStr.push_back(ch);
    }
    if (!tmpStr.empty())
        vctStrs.push_back(tmpStr);
    return vctStrs;
}

std::string LogLevelToStr(ELogLevel level)
{
    if (level == ELogLevel::INFO)
        return "INFO";
    else if (level == ELogLevel::ERR)
        return "ERR";
    else if (level == ELogLevel::FATAL)
        return "FATAL";
    return "NODEFINE";
}

LogEvent::LogEvent(ELogLevel level)
    : m_level(level)
{
    m_ss << "[" << LogLevelToStr(m_level) << "] ";
}

LogEvent::~LogEvent()
{
    // 要加锁, 保证多线程之间写日志读写安全;
    pthread_mutex_lock(&gLogMtx);
    int fd = open(gLogFileName.c_str(), O_CREAT | O_WRONLY | O_APPEND, 0644);
    if (fd == -1)
    {
        printf("open: %s\n", strerror(errno));
        exit(EXIT_FAILURE);
    }
    std::string logContent = m_ss.str();
    logContent.push_back('\n');
    int rtn = write(fd, logContent.c_str(), logContent.length());
    if (rtn == -1)
    {
        printf("write log failed! %s\n", strerror(errno));
    }
    pthread_mutex_unlock(&gLogMtx);
}

void LogEvent::format(const char *fmt, ...)
{
    va_list al;
    va_start(al, fmt);
    char *buf = nullptr;
    int len = vasprintf(&buf, fmt, al);
    if (len != -1)
    {
        m_ss << std::string(buf, len);
        free(buf);
    }
    va_end(al);
}

std::stringstream &LogEvent::getSS()
{
    return m_ss;
}
