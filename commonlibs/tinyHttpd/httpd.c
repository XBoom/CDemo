/* J. David's webserver */
/* This is a simple webserver.
 * Created November 1999 by J. David Blackstone.
 * CSE 4344 (Network concepts), Prof. Zeigler
 * University of Texas at Arlington
 */
/* This program compiles for Sparc Solaris 2.6.
 * To compile for Linux:
 *  1) Comment out the #include <pthread.h> line.
 *  2) Comment out the line that defines the variable newthread.
 *  3) Comment out the two lines that run pthread_create().
 *  4) Uncomment the line that runs accept_request().
 *  5) Remove -lsocket from the Makefile.
 */
#include <stdio.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <ctype.h>
#include <strings.h>
#include <string.h>
#include <sys/stat.h>
#include <pthread.h>
#include <sys/wait.h>
#include <stdlib.h>
#include <stdint.h>

#define ISspace(x) isspace((int)(x))  // isspace 返回非0表示x是空格

#define SERVER_STRING "Server: jdbhttpd/0.1.0\r\n"
#define STDIN   0
#define STDOUT  1
#define STDERR  2

//处理http请求
void accept_request(void *);

//返回给客户端这是个错误请求
void bad_request(int);

//读取文件
void cat(int, FILE *);

//无法执行
void cannot_execute(int);

//打印错误信息写到perror并退出
void error_die(const char *);

//执行cgi脚本
void execute_cgi(int, const char *, const char *, const char *);

//读取套接字的一行，把回车换行等情况都统一为换行符结束
int get_line(int, char *, int);

//返回http头
void headers(int, const char *);

//找不到文件
void not_found(int);

//如果不是CGI文件，直接读取文件返回给请求的http客户端
void serve_file(int, const char *);

//开启tcp链接，绑定端口等操作
int startup(u_short *);

//返回给浏览器表明收到的 HTTP 请求所用的 method 不被支持 (get/post)
// Http请求，后续主要是处理这个头
//
// GET / HTTP/1.1
// Host: 192.168.0.23:47310
// Connection: keep-alive
// Upgrade-Insecure-Requests: 1
// User-Agent: Mozilla/5.0 (Windows NT 6.1; WOW64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/55.0.2883.87 Safari/537.36
// Accept: text/html,application/xhtml+xml,application/xml;q=0.9,image/webp,*/*; q = 0.8
// Accept - Encoding: gzip, deflate, sdch
// Accept - Language : zh - CN, zh; q = 0.8
// Cookie: __guid = 179317988.1576506943281708800.1510107225903.8862; monitor_count = 5
//

// POST / color1.cgi HTTP / 1.1
// Host: 192.168.0.23 : 47310
// Connection : keep - alive
// Content - Length : 10
// Cache - Control : max - age = 0
// Origin : http ://192.168.0.23:40786
// Upgrade - Insecure - Requests : 1
// User - Agent : Mozilla / 5.0 (Windows NT 6.1; WOW64) AppleWebKit / 537.36 (KHTML, like Gecko) Chrome / 55.0.2883.87 Safari / 537.36
// Content - Type : application / x - www - form - urlencoded
// Accept : text / html, application / xhtml + xml, application / xml; q = 0.9, image / webp, */*;q=0.8
// Referer: http://192.168.0.23:47310/
// Accept-Encoding: gzip, deflate
// Accept-Language: zh-CN,zh;q=0.8
// Cookie: __guid=179317988.1576506943281708800.1510107225903.8862; monitor_count=281
// Form Data
// color=gray
void unimplemented(int);


/*
阅读顺序：main -> startup -> accept_request -> execute_cgi
工作原理：
1） 服务器启动，在指定端口或随机选取端口绑定 httpd 服务。
2） 收到一个 HTTP 请求时（其实就是 listen 的端口 accpet 的时候），派生一个线程运行 accept_request 函数。
3） 取出 HTTP 请求中的 method (GET 或 POST) 和 url,。对于 GET 方法，如果有携带参数，则 query_string 指针指向 url 中 ？ 后面的 GET 参数。
4） 格式化 url 到 path 数组，表示浏览器请求的服务器文件路径，在 tinyhttpd 中服务器文件是在 htdocs 文件夹下。当 url 以 / 结尾，或 url 是个目录，则默认在 path 中加上 index.html，表示访问主页。
5） 如果文件路径合法，对于无参数的 GET 请求，直接输出服务器文件到浏览器，即用 HTTP 格式写到套接字上，跳到（10）。其他情况（带参数 GET，POST 方式，url 为可执行文件），则调用 excute_cgi 函数执行 cgi 脚本。
6） 读取整个 HTTP 请求并丢弃，如果是 POST 则找出 Content-Length. 把 HTTP 200  状态码写到套接字。
7） 建立两个管道，cgi_input 和 cgi_output, 并 fork 一个进程。
8） 在子进程中，把 STDOUT 重定向到 cgi_outputt 的写入端，把 STDIN 重定向到 cgi_input 的读取端，关闭 cgi_input 的写入端 和 cgi_output 的读取端，设置 request_method 的环境变量，GET 的话设置 query_string 的环境变量，POST 的话设置 content_length 的环境变量，这些环境变量都是为了给 cgi 脚本调用，接着用 execl 运行 cgi 程序。
9） 在父进程中，关闭 cgi_input 的读取端 和 cgi_output 的写入端，如果 POST 的话，把 POST 数据写入 cgi_input，已被重定向到 STDIN，读取 cgi_output 的管道输出到客户端，该管道输入是 STDOUT。接着关闭所有管道，等待子进程结束。这一部分比较乱，见下图说明：
10）关闭与浏览器的连接，完成了一次 HTTP 请求与回应，因为 HTTP 是无连接的
*/

/**********************************************************************/
/* A request has caused a call to accept() on the server port to
 * return.  Process the request appropriately.
 * Parameters: the socket connected to the client */
/**********************************************************************/
//处理请求
void accept_request(void *arg)
{
    int client = (intptr_t)arg;
    char buf[1024];
    size_t numchars;
    char method[255];
    char url[255];
    char path[512];
    size_t i, j;
    struct stat st;
    int cgi = 0;      /* becomes true if server decides this is a CGI
                       * program */
    char *query_string = NULL;
    printf("accept_request\n");
    numchars = get_line(client, buf, sizeof(buf));  //读取一行，并返回长度
    i = 0; j = 0;
    while (!ISspace(buf[i]) && (i < sizeof(method) - 1))
    {
        method[i] = buf[i];
        i++;
    }
    j=i;
    method[i] = '\0';

    /*
        strcasecmp(a,b) 忽略大小写，比较前n个字符 若参数s1和s2字符串相等则返回0 s1大于s2则返回大于0 的值，s1 小于s2 则返回小于0的值
        int strncasecmp(const char *s1, const char *s2, size_t n) 指定前n
        int strcmp(const char *s1, const char *s2);   区分大小写
    */
    printf("unimplemented1\n");
    if (strcasecmp(method, "GET") && strcasecmp(method, "POST"))
    {
        unimplemented(client);
        printf("unimplemented1\n");
        return;
    }
    printf("unimplemented1\n");
    if (strcasecmp(method, "POST") == 0)
        cgi = 1;

    i = 0;
    while (ISspace(buf[j]) && (j < numchars))  //跳过空格
        j++;

    //得到 "/"   注意：如果你的http的网址为http://192.168.0.23:47310/index.html
    //               那么你得到的第一条http信息为GET /index.html HTTP/1.1，那么
    //               解析得到的就是/index.html
    while (!ISspace(buf[j]) && (i < sizeof(url) - 1) && (j < numchars))
    {
        url[i] = buf[j];
        i++; j++;
    }
    url[i] = '\0';

    if (strcasecmp(method, "GET") == 0)
    {
        query_string = url;
        while ((*query_string != '?') && (*query_string != '\0'))
            query_string++;
        if (*query_string == '?')
        {
            cgi = 1;
            *query_string = '\0';
            query_string++;
        }
    }

    sprintf(path, "htdocs%s", url);   //指定路径 htdocs/*
    if (path[strlen(path) - 1] == '/')
        strcat(path, "index.html");
    /*
        获取文件信息
        int stat(const char *path, struct stat *buf)
    */
    if (stat(path, &st) == -1) {      //如果文件没有找到则将消息都读出并丢弃
        while ((numchars > 0) && strcmp("\n", buf))  /* read & discard headers */
            numchars = get_line(client, buf, sizeof(buf));
        not_found(client);
    }
    else
    {
        /*
            S_IFMT     0170000   bitmask for the file type bitfields
            S_IFSOCK   0140000   socket
            S_IFLNK    0120000   symbolic link
            S_IFREG    0100000   regular file
            S_IFBLK    0060000   block device
            S_IFDIR    0040000   directory
            S_IFCHR    0020000   character device
            S_IFIFO    0010000   fifo
            S_ISUID    0004000   set UID bit
            S_ISGID    0002000   set GID bit (see below)
            S_ISVTX    0001000   sticky bit (see below)
            S_IRWXU    00700     mask for file owner permissions
            S_IRUSR    00400     owner has read permission
            S_IWUSR    00200     owner has write permission
            S_IXUSR    00100     owner has execute permission
            S_IRWXG    00070     mask for group permissions
            S_IRGRP    00040     group has read permission
            S_IWGRP    00020     group has write permission
            S_IXGRP    00010     group has execute permission
            S_IRWXO    00007     mask for permissions for others (not in group)
            S_IROTH    00004     others have read permission
            S_IWOTH    00002     others have write permisson
            S_IXOTH    00001     others have execute permission
        */
        if ((st.st_mode & S_IFMT) == S_IFDIR)
            strcat(path, "/index.html");
        //如果你的文件默认是有执行权限的，自动解析成cgi程序，如果有执行权限但是不能执行，会接受到报错信号
        if ((st.st_mode & S_IXUSR) ||
                (st.st_mode & S_IXGRP) ||
                (st.st_mode & S_IXOTH)    )
            cgi = 1;
        if (!cgi)
            serve_file(client, path);                       //接读取文件返回给请求的http客户端
        else
            execute_cgi(client, path, method, query_string);//执行cgi文件
    }
    //执行完毕关闭socket
    close(client);
}

/**********************************************************************/
/* Inform the client that a request it has made has a problem.
 * Parameters: client socket */
/**********************************************************************/
void bad_request(int client)
{
    char buf[1024];

    sprintf(buf, "HTTP/1.0 400 BAD REQUEST\r\n");
    send(client, buf, sizeof(buf), 0);
    sprintf(buf, "Content-type: text/html\r\n");
    send(client, buf, sizeof(buf), 0);
    sprintf(buf, "\r\n");
    send(client, buf, sizeof(buf), 0);
    sprintf(buf, "<P>Your browser sent a bad request, ");
    send(client, buf, sizeof(buf), 0);
    sprintf(buf, "such as a POST without a Content-Length.\r\n");
    send(client, buf, sizeof(buf), 0);
}

/**********************************************************************/
/* Put the entire contents of a file out on a socket.  This function
 * is named after the UNIX "cat" command, because it might have been
 * easier just to do something like pipe, fork, and exec("cat").
 * Parameters: the client socket descriptor
 *             FILE pointer for the file to cat */
/**********************************************************************/
void cat(int client, FILE *resource)
{
    char buf[1024];

    fgets(buf, sizeof(buf), resource);
    while (!feof(resource))
    {
        send(client, buf, strlen(buf), 0);
        fgets(buf, sizeof(buf), resource);
    }
}

/**********************************************************************/
/* Inform the client that a CGI script could not be executed.
 * Parameter: the client socket descriptor. */
/**********************************************************************/
void cannot_execute(int client)
{
    char buf[1024];

    sprintf(buf, "HTTP/1.0 500 Internal Server Error\r\n");
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "Content-type: text/html\r\n");
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "\r\n");
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "<P>Error prohibited CGI execution.\r\n");
    send(client, buf, strlen(buf), 0);
}

/**********************************************************************/
/* Print out an error message with perror() (for system errors; based
 * on value of errno, which indicates system call errors) and exit the
 * program indicating an error. */
/**********************************************************************/
void error_die(const char *sc)
{
    perror(sc);   //用来将上一个函数发生错误的原因输出到标准设备(stderr)
    exit(1);
}

/**********************************************************************/
/* Execute a CGI script.  Will need to set environment variables as
 * appropriate.
 * Parameters: client socket descriptor
 *             path to the CGI script */
/**********************************************************************/
void execute_cgi(int client, const char *path,
        const char *method, const char *query_string)
{
    //缓冲区
    char buf[1024];

    //输入输出管道
    int cgi_output[2];
    int cgi_input[2];

    //进程pid和状态
    pid_t pid;
    int status;

    int i;
    char c;

    int numchars = 1;           //读取的字节数
    int content_length = -1;

    buf[0] = 'A'; buf[1] = '\0';    //默认字符
    if (strcasecmp(method, "GET") == 0)     //读取数据，把整个header都读掉，因为Get写死了直接读取index.html，没有必要分析余下的http信息了
        while ((numchars > 0) && strcmp("\n", buf))  /* read & discard headers */
            numchars = get_line(client, buf, sizeof(buf));
    else if (strcasecmp(method, "POST") == 0) /*POST*/
    {
        numchars = get_line(client, buf, sizeof(buf));
        while ((numchars > 0) && strcmp("\n", buf))
        {
            //POST请求，读取得到Content-Length，Content-Length：这个字符串一共长为15位，所以
            //取出头部一句后，将第16位设置结束符，进行比较
            //第16位置为结束
            buf[15] = '\0';
            if (strcasecmp(buf, "Content-Length:") == 0)
                //内存从第17位开始就是长度，将17位开始的所有字符串转成整数就是content_length
                content_length = atoi(&(buf[16]));
            numchars = get_line(client, buf, sizeof(buf));
        }
        if (content_length == -1) {
            bad_request(client);
            return;
        }
    }
    else/*HEAD or other*/
    {
    }

    //建立output管道
    if (pipe(cgi_output) < 0) {
        cannot_execute(client);
        return;
    }
    //建立input管道
    if (pipe(cgi_input) < 0) {
        cannot_execute(client);
        return;
    }
    //       fork后管道都复制了一份，都是一样的
    //       子进程关闭2个无用的端口，避免浪费             
    //       ×<------------------------->1    output
    //       0<-------------------------->×   input 

    //       父进程关闭2个无用的端口，避免浪费             
    //       0<-------------------------->×   output
    //       ×<------------------------->1    input
    //       此时父子进程已经可以通信

    //fork进程，子进程用于执行CGI
    //父进程用于收数据以及发送子进程处理的回复数据
    if ( (pid = fork()) < 0 ) {
        cannot_execute(client);
        return;
    }
    sprintf(buf, "HTTP/1.0 200 OK\r\n");
    send(client, buf, strlen(buf), 0);
    if (pid == 0)  /* child: CGI script */
    {
        char meth_env[255];
        char query_env[255];
        char length_env[255];
        /*
            int dup(int oldfd); 
            //当复制成功是，返回最小的尚未被使用过的文件描述符，若有错误则返回-1.错误代码存入errno中
            //返回的新文件描述符和参数oldfd指向同一个文件，这两个描述符共享同一个数据结构，共享所有的锁定，读写指针和各项全现或标志位
             
            int dup2(int oldfd, int newfd);
            //dup2与dup区别是dup2可以用参数newfd指定新文件描述符的数值。
            //若参数newfd已经被程序使用，则系统就会将newfd所指的文件关闭，
            //若newfd等于oldfd，则返回newfd,而不关闭newfd所指的文件。
            //dup2所复制的文件描述符与原来的文件描述符共享各种文件状态。共享所有的锁定，读写位置和各项权限或flags等
        */
        dup2(cgi_output[1], STDOUT);        //子进程输出重定向到output管道的1端
        dup2(cgi_input[0], STDIN);          //子进程输入重定向到input管道的0端
        close(cgi_output[0]);   
        close(cgi_input[1]);    
        sprintf(meth_env, "REQUEST_METHOD=%s", method);
        putenv(meth_env);       //putenv()用来改变或增加环境变量的内容
        if (strcasecmp(method, "GET") == 0) {
            sprintf(query_env, "QUERY_STRING=%s", query_string);
            putenv(query_env);          
        }
        else {   /* POST */
            sprintf(length_env, "CONTENT_LENGTH=%d", content_length);
            putenv(length_env);
        }
        execl(path, NULL);  //execl() 第一参数path字符指针所指向要执行的文件路径， 接下来的参数代表执行该文件时传递的参数列表
        exit(0);            //退出子进程，管道被破坏，但是父进程还在往里面写东西，触发Program received signal SIGPIPE, Broken pipe.
    } else {    /* parent */
        close(cgi_output[1]);
        close(cgi_input[0]);
        if (strcasecmp(method, "POST") == 0)
            for (i = 0; i < content_length; i++) {
                //得到post请求数据，写到input管道中，供子进程使用
                recv(client, &c, 1, 0);
                write(cgi_input[1], &c, 1);   //
            }
        //从output管道读到子进程处理后的信息，然后send出去
        while (read(cgi_output[0], &c, 1) > 0)
            send(client, &c, 1, 0);

        //完成操作后关闭管道
        close(cgi_output[0]);
        close(cgi_input[1]);
        //等待子进程返回
        waitpid(pid, &status, 0);
    }
}

/**********************************************************************/
/* Get a line from a socket, whether the line ends in a newline,
 * carriage return, or a CRLF combination.  Terminates the string read
 * with a null character.  If no newline indicator is found before the
 * end of the buffer, the string is terminated with a null.  If any of
 * the above three line terminators is read, the last character of the
 * string will be a linefeed and the string will be terminated with a
 * null character.
 * Parameters: the socket descriptor
 *             the buffer to save the data in
 *             the size of the buffer
 * Returns: the number of bytes stored (excluding null) */
/**********************************************************************/
//得到一行数据,只要发现c为\n,就认为是一行结束，如果读到\r,再用MSG_PEEK的方式读入一个字符，如果是\n，从socket用读出
//如果是下个字符则不处理，将c置为\n，结束。如果读到的数据为0中断，或者小于0，也视为结束，c置为\n
int get_line(int sock, char *buf, int size)
{
    int i = 0;
    char c = '\0';
    int n;

    while ((i < size - 1) && (c != '\n'))
    {
        /*
            int recv(int new_fd, void *buf, int len, unsigned int flags）;
            recv函数读取tcp buffer中的数据到buf中，并从tcp buffer中移除已读取的数据。
        */
        n = recv(sock, &c, 1, 0);
        /* DEBUG printf("%02X\n", c); */
        if (n > 0)
        {
            if (c == '\r')
            {
                /*
                    把flags设置为MSG_PEEK，仅把tcp buffer中的数据读取到buf中，
                    并不把已读取的数据从tcp buffer中移除，再次调用recv仍然可以读到刚才读到的数据 
                */
                n = recv(sock, &c, 1, MSG_PEEK);
                /*
                    读到\r 下一个是\n 则继续读取
                    读到\r 下一个不是\n或没有读到 则完成一行读取
                */
                if ((n > 0) && (c == '\n'))
                    recv(sock, &c, 1, 0);
                else
                    c = '\n';
            }
            buf[i] = c;
            i++;
        }
        else
            c = '\n';
    }
    buf[i] = '\0';

    return(i);
}

/**********************************************************************/
/* Return the informational HTTP headers about a file. */
/* Parameters: the socket to print the headers on
 *             the name of the file */
/**********************************************************************/
//返回头文件
void headers(int client, const char *filename)
{
    char buf[1024];
    (void)filename;  /* could use filename to determine file type */

    strcpy(buf, "HTTP/1.0 200 OK\r\n");
    send(client, buf, strlen(buf), 0);
    strcpy(buf, SERVER_STRING);
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "Content-Type: text/html\r\n");
    send(client, buf, strlen(buf), 0);
    strcpy(buf, "\r\n");
    send(client, buf, strlen(buf), 0);
}

/**********************************************************************/
/* Give a client a 404 not found status message. */
/**********************************************************************/
//返回404页面
void not_found(int client)
{
    char buf[1024];

    sprintf(buf, "HTTP/1.0 404 NOT FOUND\r\n");
    send(client, buf, strlen(buf), 0);
    sprintf(buf, SERVER_STRING);
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "Content-Type: text/html\r\n");
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "\r\n");
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "<HTML><TITLE>Not Found</TITLE>\r\n");
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "<BODY><P>The server could not fulfill\r\n");
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "your request because the resource specified\r\n");
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "is unavailable or nonexistent.\r\n");
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "</BODY></HTML>\r\n");
    send(client, buf, strlen(buf), 0);
}

/**********************************************************************/
/* Send a regular file to the client.  Use headers, and report
 * errors to client if they occur.
 * Parameters: a pointer to a file structure produced from the socket
 *              file descriptor
 *             the name of the file to serve */
/**********************************************************************/
//如果不是CGI文件，直接读取文件返回给请求的http客户端
void serve_file(int client, const char *filename)
{
    FILE *resource = NULL;
    int numchars = 1;
    char buf[1024];

    buf[0] = 'A'; buf[1] = '\0';
    while ((numchars > 0) && strcmp("\n", buf))  /* read & discard headers */
        numchars = get_line(client, buf, sizeof(buf));

    resource = fopen(filename, "r");
    if (resource == NULL)
        not_found(client);
    else
    {
        headers(client, filename);
        cat(client, resource);     
    }
    fclose(resource);
}

/**********************************************************************/
/* This function starts the process of listening for web connections
 * on a specified port.  If the port is 0, then dynamically allocate a
 * port and modify the original port variable to reflect the actual
 * port.
 * Parameters: pointer to variable containing the port to connect on
 * Returns: the socket */
/**********************************************************************/
int startup(u_short *port)
{
    int httpd = 0;
    int on = 1;
    struct sockaddr_in name;

    /*
        原型：
        #include<sys/types.h>
        #include<sys/socket.h>
        int socket(int domain, int type, int protocol);
        domain:网络通信域(PF_UNIX,PF_LOCAL/ AF_INET,PF_INET:ipv4 / PF_NET6:ipv6 / PF_IPX / PF_NETLINK)
        type:套接字通信类型(SOCK_STREAM/SOCK_DGRAM/SOCK_SEQPACKET/SOCK_RAW/SOCK_RDM/SOCK_PACKET)
        protocol:指定协议特定类型，如果只有一种则只填0
    */
    //分配一个TCP套接字
    httpd = socket(PF_INET, SOCK_STREAM, 0);
    if (httpd == -1)
        error_die("socket");

    /*
        网络字节顺序(Network Byte Order)和主机字节顺序(Host Byte Order)转换：因为不同体系结构的机器之间无法通信
        网络字节顺序：从高到低的顺序存储，网络上统一的网络字节顺序，避免兼容性

        htonl()--"Host to Network Long"
        ntohl()--"Network to Host Long"
        htons()--"Host to Network Short"
        ntohs()--"Network to Host Short"  
    */
    //初始化
    memset(&name, 0, sizeof(name));
    name.sin_family = AF_INET;
    name.sin_port = htons(*port);
    name.sin_addr.s_addr = htonl(INADDR_ANY);

    /*
        int setsockopt(int sock, int level, int optname, const void *optval, socklen_t optlen)
        sock:将要被设置的套接字
        level:选项所在的协议层 (SOL_SOCKET:通用套接字选项 / IPPROTO_IP:IP选项 /IPPROTO_TCP:TCP选项)
        optname:需要访问的选项名 (SO_REUSEADDR:允许重用本地地址和端口)
    */
    if ((setsockopt(httpd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on))) < 0)  
    {  
        error_die("setsockopt failed");
    }

    if (bind(httpd, (struct sockaddr *)&name, sizeof(name)) < 0)
        error_die("bind");
    if (*port == 0)  /* 如果端口没有设置，提供个随机端口 */
    {
        socklen_t namelen = sizeof(name);
        if (getsockname(httpd, (struct sockaddr *)&name, &namelen) == -1)
            error_die("getsockname");
        *port = ntohs(name.sin_port);
    }
    if (listen(httpd, 5) < 0)
        error_die("listen");
    return(httpd);
}

/**********************************************************************/
/* Inform the client that the requested web method has not been
 * implemented.
 * Parameter: the client socket */
/**********************************************************************/
//不是get或者post 返回此消息告诉客户端
void unimplemented(int client)
{
    char buf[1024];

    sprintf(buf, "HTTP/1.0 501 Method Not Implemented\r\n");
    send(client, buf, strlen(buf), 0);
    sprintf(buf, SERVER_STRING);
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "Content-Type: text/html\r\n");
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "\r\n");
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "<HTML><HEAD><TITLE>Method Not Implemented\r\n");
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "</TITLE></HEAD>\r\n");
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "<BODY><P>HTTP request method not supported.\r\n");
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "</BODY></HTML>\r\n");
    send(client, buf, strlen(buf), 0);
}

/**********************************************************************/

int main(void)
{
    int server_sock = -1;
    u_short port = 4000;
    int client_sock = -1;
    struct sockaddr_in client_name;
    socklen_t  client_name_len = sizeof(client_name);
    pthread_t newthread;

    server_sock = startup(&port);  //初始化http服务(建立套接字，绑定端口，监听端口)
    printf("httpd running on port %d\n", port);

    while (1)
    {
        /*
            接收请求
            int accept(int sockfd, struct sockaddr *addr, socklen_t *addrlen);
            addr:用于存放客户端的地址
            addrlen:在调用函数时被设置为addr指向区域的长度
            返回：返回值是一个新的套接字描述符，它代表的是和客户端的新的连接，含有客户端和服务器的ip，port
        */
        client_sock = accept(server_sock,
                (struct sockaddr *)&client_name,
                &client_name_len);
        if (client_sock == -1)
            error_die("accept");
        /* accept_request(&client_sock); */
        if (pthread_create(&newthread , NULL, (void *)accept_request, (void *)(intptr_t)client_sock) != 0)
            perror("pthread_create");
    }

    close(server_sock);

    return(0);
}
