include<stdio.h>                                                               
#include<string.h>
#include<stdlib.h>
#include<unistd.h>
#include<pthread.h>
#include<sys/socket.h>
#include<netinet/in.h>
#include<arpa/inet.h>
#include<sys/stat.h>
#include<sys/sendfile.h>
#include<sys/wait.h>
#include<fcntl.h>

#include "http_server.h"

typedef struct sockaddr sockaddr;
typedef struct sockaddr_in sockaddr_in;

//一次从 socket 中读取一行数据  
//把数据放到 buf 缓冲区中  
//如果读取失败， 返回值就是-1
// \n \r \r\n (浏览器发送的换行可能性)
int ReadLine(int sock, char buf[], ssize_t size){
    //1.从 socket 中一次读取一个字符   
    char c = '\0';
    ssize_t i = 0;  //当前读了多少个字符
    //结束条件：   
    //a）读的长度太长，达到缓冲区长度的上限   
    //b)读到了 \n （此处我们要兼容 \r \r\n 的情况，如果遇到 
    //  \r 或者 \r\n 想办法转换成 \n）
    while(i < size -1 && c != '\n'){
        ssize_t read_size =recv(sock, &c, 1, 0);
        if(read_size <= 0){
            //预期是要读到 \n 这样的换行符.结果还没读到呢   
            //就先读到了 EOF ，这种情况我们也暂时认为是失败
            return -1;
        }                                                                       
        if(c == '\r'){
            //当遇到了 \r ，但是还需要确定下一个字符是不是 \n      
            //MSG_PEEK 选项从内核的缓冲区中读取数据.    
            //但是读到的数据不会从缓冲区中删除.
            recv(sock, &c, 1, MSG_PEEK);
            if(c == '\n'){
                //此时整个的分隔符就是 \r\n 
                recv(sock, &c, 1, 0); 
            }else{
                //当前分隔符确定就是 \r ，此时把分隔符转换成 \n   
                c = '\n';
            }   
        }
        //只要上面c读到的是 \r，那么 if 结束后，c 都变成了 \n   
        //这种方式就是把前面的  \r 和 \r\n 两种情况都统一变成了  \n   
        
        buf[i++] = c;
        }   
        buf[i] = '\0';
        return i;//真正往缓冲区中放置的字符的个数
}                                                                               

int Split(char input[], const char* split_char, char* output[],
                int output_size){
        //使用 strtok
        int i = 0;
        char * tmp = NULL;
        char * pch;
        //使用线程安全的 strtok_r 代替 strtok
        //这一点是以后非常容易出错的一点.
        pch = strtok_r(input, split_char, &tmp);
        while(pch != NULL){
            if(i >= output_size){
                return i;
            }     
            output[i++] = pch;
            pch = strtok_r(NULL, split_char, &tmp);
        }   
        return i;
}

int ParseFirstLine(char first_line[], char** p_url,                             
        char** p_method){
    //把首行按照空格进行字符串切分
    char* tok[10];
    //把 first_line 按照空格进行字符串切分.    
    //切分得到的每一个部分，就放到 tok数组之中  
    //返回值，就是 tok 数组中包含几个元素.   
    //最后一个参数 10 表示 tok 数组最多能放几个元素.
    int tok_size = Split(first_line, " ", tok, 10);
    if(tok_size != 3){
        printf("Split failed! tok_size=%d\n", tok_size);
        return -1;
    }
    *p_method = tok[0];
    *p_url = tok[1];
    return 0;
}

int ParseQueryString(char * url, char** p_url_path,
                char** p_query_string){
    *p_url_path = url;
    char* p = url;                                                              
    for(; *p != '\0';++p){
        if(*p == '?'){
            *p = '\0';
            *p_query_string = p + 1;
            return 0;
        }   
    }   
    //如果循环结束都没有找到 ？ ，说明这个请求不带  query_string
    *p_query_string = NULL;
    return 0;
}

int ParseHeader(int sock, int * content_length){
        char buf[SIZE] = {0};
        while(1){
            //1.循环从 socket 中读取一行.   
            ssize_t read_size = ReadLine(sock, buf, sizeof(buf));
            //处理读失败的情况
            if(read_size <= 0){ 
                return -1; 
            }                                                                   
            //5.读到空行循环结束
            if(strcmp(buf, "\n") == 0){
               return 0;
            }
            //2.判定当前行是不是Content_Length   
            //  如果是Content_Length 就直接把 value 读取出来   
            //  如果不是就直接丢弃 
            const char* content_length_str = "Content-Length: ";
            if(content_length != NULL
                    && strncmp(buf, "Content-Length: ",                         
                        strlen(content_length_str)) == 0){ 
                *content_length = atoi(buf + strlen(content_length_str));
            }   
        }   
        return 0;
}

void Handler404(int sock){
    //构造一个完整的 HTTP 响应
    //状态码就是404
    //body 部分应该也是一个 404 相关的错误页面
    const char* first_line = "HTTP/1.1 404 NOT Found\n";
    const char* type_line = "Content-Type: text/html;charset=utf-8\n";//防止出现
乱码
    const char* blank_line = "\n";
    const char* html = "<head><meta http-equiv=\"content-type\" content=\"text/html;charset=utf-8\"></head>"
        "<h1>您的页面被喵星人吃掉了</h1>";
    send(sock, first_line, strlen(first_line), 0);
    send(sock, type_line, strlen(type_line), 0);
    send(sock, blank_line, strlen(blank_line), 0); 
    send(sock, html, strlen(html), 0); 
    return;
}

void PrintRequest(Request* req){
    printf("method: %s\n", req->method);
    printf("url-path: %s\n", req->url_path);
    printf("query_string: %s\n", req->query_string);
    printf("content_length: %d\n", req->content_length);
    return;                                                                     
}

int IsDir(const char* file_path){
    struct stat st;
    int ret = stat(file_path, &st);
    if(ret < 0){
        return 0;
    }
    if(S_ISDIR(st.st_mode)){
        return 1;
    }
    return 0;
}

void HandlerFilePath(const char* url_path, char file_path[]){
    //a) 给 url_path 加上前缀（HTTP服务器的根目录）
    // url_path => /index.html
    // file_path => ./wwwroot/index.html
    sprintf(file_path, "./wwwroot%s", url_path);
    //b）例如 url_path => /,此时 url_path 其实是一个目录.
    //如果是目录的话，就给这个目录之中，追加上一个 index.html                   
    // url_path / 或者 /image/
    if(file_path[strlen(file_path) - 1] == '/'){
        strcat(file_path, "index.html");
    }
    //c)例如 url_path => /image
    if(file_path[strlen(file_path) - 1] == '/'){
        strcat(file_path, "index.html");
    }
    //c)例如 url_path => /image
    if(IsDir(file_path)){
        strcat(file_path, "/index.html");
    }
    return;
}

ssize_t GetFileSize(const char* file_path){
    struct stat st;
    int ret = stat(file_path, &st);
    if(ret < 0){
        //打开文件失败，很可能是文件不存在
        //此时直接返回文件长度为0                                               
        return 0;
    }
    return st.st_size;
}

int WriteStaticFile(int sock, const char* file_path ){
    //1.打开文件
    //什么情况下会打开失败？ 1.文件描述符不够用 2.文件不存在  
    int fd = open(file_path, O_RDONLY);
    if(fd < 0){
        perror("open");
        return 404;
    }
    //2.把把构造出来的HTTP响应写入到 socket 之中  
    //  a)写入首行
    const char* first_line = "HTTP/1.1 200 ok\n";
    send(sock, first_line, strlen(first_line), 0);
    //  b)写入header
    //const char* type_line = "Content-Type: text/html;charset=utf-8\n";
    //const char* type_line = "Content-Type: image/jpg;charset=utf-8\n";
    //send(sock, type_line, strlen(type_line), 0);让浏览器自己自动识别文件类型  
    //  c)写入空行
    const char* blank_line = "\n";
    send(sock, blank_line, strlen(blank_line), 0);
    //  d)写入body（文件内容）
    /*
    ssize_t file_size = GetFileSize(file_path);
    ssize_t i = 0;
    for(; i < file_size; ++i){
        char c;
        read(fd, &c, 1);
        send(sock, &c, 1, 0);
    }
    */
    sendfile(sock, fd, NULL, GetFileSize(file_path));
    //3.关闭文件
    close(fd);
    return 200;
}

int HandlerStaticFile(int sock, Request* req){
    //1.根据 url_path 获取到文件在服务器上的真实路jing.                         
    char file_path[SIZE] = {0};
    HandlerFilePath(req->url_path, file_path);
    //2.读取文件，把文件的内容直接写到 socket 之中
    int err_code = WriteStaticFile(sock, file_path);
    return err_code;
}

int HandlerCGIFather(int new_sock, int father_read, 
        int father_write, int child_pid, Request* req){
    //1.如果是 POST 请求，就把 body 写入到管道中
    if(strcasecmp(req->method, "POST") == 0){
      int i = 0;
      char c = '\0';
      for(; i < req->content_length; ++i){
          read(new_sock, &c, 1);
          write(father_write, &c, 1);
      }
    }
    //2.构造 HTTP 响应
    const char* first_line = "HTTP/1.1 200 OK\n";
    send(new_sock, first_line, strlen(first_line), 0);                          
    const char* type_line = "Content-Type: image/jpg;charset=utf-8\n";
    send(new_sock, type_line, strlen(type_line), 0);
    const char* blank_line = "\n";
    send(new_sock, blank_line, strlen(blank_line), 0);
    //3.循环的从管道中读取数据并写入到 socket
    char c = '\0';
    while(read(father_read, &c , 1) > 0){
        send(new_sock, &c, 1, 0);
    }
    //4.回收子进程的资源
    waitpid(child_pid, NULL, 0);
    return 200;
}

int HandlerCGIChild(int child_read, int child_write, 
        Request* req){
    //1.设置必要的环境变量
    char method_env[SIZE] = {0};
    sprintf(method_env, "REQUEST_METHOD=%s", req->method);
    putenv(method_env);
    //还需要设置 QUERY_STRING 或者 Content_LENGTH                               
    if(strcasecmp(req->method, "GET") == 0){
        char query_string_env[SIZE] = {0};
        sprintf(query_string_env, "QUERY_STRING=%s",
                req->query_string);
    }else{
        char content_length_env[SIZE] = {0};
        sprintf(content_length_env, "CONTENT_LENGTH=%d", 
                req->content_length);
    }
    //2.把标准输入输出重定向到管道中
    dup2(child_read, 0);
    dup2(child_write, 1);                                                       
    //3.对子进程进行程序替换
    // url_path: /cgi-bin/test
    // file_path: ./wwwroot/cgi-bin/test
    char file_path[SIZE] = {0};
    HandlerFilePath(req->url_path, file_path);
    //l
    //lp
    //le
    //v
    //vp
    //ve
    execl(file_path, file_path, NULL);//程序替换
    exit(1);
    return 200;
}
int HandlerCGI(int new_sock, Request* req){
    int err_code = 200;
    //1.创建一对匿名管道
    int fd1[2],fd2[2];
    int ret = pipe(fd1);
    if(ret < 0){
        return 404;
    }
    ret = pipe(fd2);
    if(ret < 0){
        close(fd1[0]);
        close(fd1[1]);
        return 404;
    }                                                                           
    // fd1 fd2 这样的变量名描述性太差，后面直接使用的话 
    // 是非常容易弄混的.所以直接在此处定义几个  
    // 更加明确的变量名来描述该文件描述符的用途.
    int father_read = fd1[0];
    int child_write = fd1[1];
    int father_write = fd2[1];
    int child_read = fd2[0];
    //2.创建子进程
    ret = fork();
    //3.父子进程各自执行不同的逻辑
    if(ret > 0){
        // father
        // 此处父进程优先关闭这两个管道的文件描述符， 
        // 是为了后续父进程从子进程这里读数据的时候，能够读到 EOF . 
        // 对于管道来说，所有的写端关闭，继续读，才有 EOF.  
        // 而此处的所有写端，一方面是父进程需要关闭，
        // 另一方面子进程也需要关闭. 
        // 所以此处父进程先关闭不必要的写端之后，
        // 后续子进程用完了直接关闭，父进程也就读到了 EOF
        close(child_read);
        close(child_write);                                                     
        err_code = HandlerCGIFather(new_sock, father_read,
                father_write, ret, req);
    }else if(ret == 0){
        // child
        close(father_read);
        close(father_write);
        err_code = HandlerCGIChild(child_read, child_write, req);
    }else{
        perror("fork");
        err_code = 404;
        goto END;                                                               
    }
    //4.收尾工作和错误处理
END:
    close(fd1[0]);
    close(fd1[1]);
    close(fd2[0]);
    close(fd2[1]);
    return err_code;
}

void HandlerRequest(int new_sock){
    int err_code = 200;
    //1.读取并解析请求（反序列化）
    Request req;
    memset(&req, 0, sizeof(req));
    // a)从 socket 中读取首行
    if(ReadLine(new_sock, req.first_line, 
                sizeof(req.first_line)) < 0){ 
        err_code = 404;
        goto END; 
    }   
    // b)解析首行，从首行中解析出 url 和 method 
    if(ParseFirstLine(req.first_line, &req.url, &req.method)){
        err_code = 404;
        goto END;//TODO失败处理 
    } 
    // c)解析url，从url之中解析出url_path, query_string
    if(ParseQueryString(req.url, &req.url_path, 
                &req.query_string)){
        err_code = 404;
        goto END;//TODO失败处理                                                 
    }
    // d)处理 Header. 丢弃了大部分 header, 只读取Content_Length
    if(ParseHeader(new_sock, &req.content_length)){
        err_code = 404;
        goto END;//TODO失败处理
    }
    PrintRequest(&req); 
    //2.静态/动态方式生成页面,把生成结果写回到客户端.
    //假如浏览器发送的请求方法叫做 “get” /“Get”    
    //不区分大小写的比较
    if(strcasecmp(req.method, "GET") == 0 
            && req.query_string == NULL){
        //a) 如果请求是GET请求，并且没有 query_string,  
        //那么就返回静态页面  
        err_code = HandlerStaticFile(new_sock, &req);
    }else if(strcasecmp(req.method, "GET") == 0 
            && req.query_string != NULL){
        //b)如果请求是GET请求， 并且有 query_dtring   
        //那么就返回动态页面  
        err_code = HandlerCGI(new_sock, &req);
    }else if(strcasecmp(req.method, "POST") == 0){                              
        //c）如果请求是POST请求（一定是带参数的，参数是通过 body 来传给服务器）>，
        //那么也是返回动态页面.
        err_code = HandlerCGI(new_sock, &req);
    }else{
        err_code = 404;
        goto END;//TODO失败处理
    }   
    //错误处理:直接返回一个 404 的HTTP响应
END:
    if(err_code != 200){
        Handler404(new_sock);
    }   
    close(new_sock);
    return;
}

void* ThreadEntry(void* arg){
        int32_t new_sock = (int32_t)arg;
        //此处使用HandlerRequest函数进行完成具体的处理请求过程  
        //这个过程单独提取出来是为了解耦合                                      
        //一旦需要把服务器改成多进程或者IO多路复用的形式，
        //整体代码的改动都是比较小的
        HandlerRequest(new_sock);
        return NULL;
}
//服务器启动
void HttpServerStart(const char* ip, short port){
    int listen_sock = socket(AF_INET, SOCK_STREAM, 0); 
    if(listen_sock < 0){ 
        perror("socket");
        return;
    }   

    //加上这个选项就能够重用 TIME_WAIT 链接
    int opt = 1;
    setsockopt(listen_sock, SOL_SOCKET, SO_REUSEADDR, 
            &opt, sizeof(opt));
    
    sockaddr_in addr;
    addr.sin_family = AF_INET;                                                  
    addr.sin_addr.s_addr = inet_addr(ip);
    addr.sin_port = htons(port);
    int ret = bind(listen_sock, (sockaddr*)&addr, sizeof(addr));
    if(ret < 0){
        perror("bind");
        return;
    }   
    ret = listen(listen_sock, 5); 
    if(ret < 0){ 
        perror("listen");
        return;
    }   
    printf("ServerInit OK\n");
    while(1){
        sockaddr_in peer;
        socklen_t len = sizeof(peer);
        int32_t new_sock = accept(listen_sock, (sockaddr*)&peer, &len);
        if(new_sock < 0){
            perror("accept");
            continue;
        }                                                                       
        //使用多线程的方式来实现TCP服务器
        pthread_t tid;
        pthread_create(&tid, NULL, ThreadEntry, (void*)new_sock);
        pthread_detach(tid);
    }
}

// ./http_server [ip] [port]
int main(int argc, char* argv[]){
    if(argc != 3){ 
        printf("Usage ./heep_server [ip] [port]\n");
        return 1;
    }   
    HttpServerStart(argv[1], atoi(argv[2]));
    return 0;
}
