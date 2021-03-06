//实现可执行的 CGI程序

//////////////////////////////////////////////////////////////////////          
/////////此处实现的可执行程序就是CGI程序
//////////////////////////////////////////////////////////////////////
#include "cgi_base.h"

int main(){
    //1.调用封装好的函数，获取的参数
    char buf[1024 * 4] = {0};
    int ret = GetQueryString(buf);
    if(ret < 0){
        fprintf(stderr, "[CGI] GetQueryString failed\n");
        return 1;
    }
    //此时获取到的buf中的内容格式为 a=10&b=20
    int a, b;
    sscanf(buf, "a=%d&b=%d", &a, &b);
    int sum = a + b;
    //printf 输出的结果就会被返回到客户端上
    //作为HTTP服务器，每次给客户端返回的字符串
    //必须符合HTTP协议格式
    //由于父进程已经把 首行，header ，空行都已经写回给客户端
    printf("<h1>sum=%d</h1>", sum);
    return 0;
}              
