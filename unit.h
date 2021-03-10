#ifndef UNIT__H
#define UNIT__H

#include <unistd.h>
#include <string>
#include <cstdio>
using namespace std;

/**************************************************************
 * @param clientfd scoketfd（源地址）
 * @param buf 目的地址
 * @return 读取到的长度
 * @brief 读取一行。
***************************************************************/
std::size_t get_line(int clientfd, string &buf)
{
    buf.clear();
    char ch = 0;
	size_t total = 0;
    while (ch != '\n')
    {
		int ret = read(clientfd, &ch, 1);
		if(ret == -1)
		{
			if(errno == EINTR)
				continue;
			else
				return -1;
		}
		else if(ret == 0)
			break;// 读不到了, 结尾EOF
		else 
	    {
			buf.push_back(ch);
			++total;
		}
    }
    return total;
}

#endif
