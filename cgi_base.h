#pragma once                                                                    

#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include<unistd.h>

//分 GET/POST两种情况读取计算的参数
//1.对于 GET 请求从 query_string 读取
//2.对 POST 从 body 中读取
//读取的结果就放在buf这个输出缓冲区中
static int GetQueryString(char buf[]){
    //1.从环境变量中获取到的方法是什么
    char* method = getenv("REQUEST_METHOD");
    if(method == NULL){
        //由于当前的CGI程序对应的标准输出以及被重定向到管道上了.
        //而这部分数据又会被返回客户端.
        //避免让程序内部的错误暴露给普通用户,通过 stderr
        //来作为输出日志的手段
        fprintf(stderr, "[CGI] method == NULL\n");
        return -1;    
        }     
     //2.判定方法时 GET，还是 POST
     //如果是 GET ，就从环境变量里面读取 QUERY_STRING
     //如果是 POST，就需要从环境变量里读取 CONTENT_LENGTH
     if(strcasecmp(method, "GET") == 0){
         char* query_string = getenv("QUERY_STRING");
         if(query_string == NULL){
            fprintf(stderr, "[CGI] query_string is NULL\n");
            return -1;
        }
        // 拷贝完成之后，buf 里面的内容形如 
        // a=10&b=10
        strcpy(buf, query_string);  
     }else{
        char* content_length_str = getenv("CONTENT_LENGTH");
        if(content_length_str == NULL){                                         
            fprintf(stderr, "[CGI] content_length is NULL\n");
            return -1;
        }
        int content_length = atoi(content_length_str);
        int i = 0;
        for(; i < content_length; ++i){
             //此处由于父进程把 body 已经写入管道.
             //子进程又已经把0号文件描述符重定向到了管道
             //此时从标准输入来读数据，也就读到了管道中的数据
             read(0, &buf[i], 1);                          
        }   
        // 此循环读完之后，buf 里面的内容形如
        // a=10&b=20
        buf[i] = '\0';
     }
     return 0;
}          
