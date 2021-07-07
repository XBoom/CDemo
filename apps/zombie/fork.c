#include <unistd.h>
#include <stdio.h>

//test
int main()
{
    pid_t fpid;      //fpid表示fork函数返回的值
    int count = 0;  
    /*
        这里不添加换行符,“fork!”仅仅被放到了缓冲里,程序运行到fork时缓冲里面的“fork!”  被子进程复制过去了。
        因此在子进程度stdout缓冲里面就也有了fork! 
    */printf("fork!"); 
    fpid = fork();   //创建新进程成功后，进程没有固定的先后顺序，根据进程调度策略(既然一样，调度策略怎么决定的)
    if (fpid < 0)   
    {
        /*
            fork出错的可能原因
            1）当前的进程数已经达到了系统规定的上限(ulimit -a -> max user processes)，这时errno的值被设置为EAGAIN()
            2）系统内存不足，这时errno的值被设置为ENOMEM
        */
        printf("error in fork!");
    }
    else if (fpid == 0)   
    {
        printf("i am the child process, my process id is %d/n", getpid());
        printf("我是爹的儿子/n");
        count ++;
    }
    else
    {
        printf("i am the parent process, my process id is %d/n", getpid());
        printf("我是孩子他爹/n");
        count ++;
    }
    printf("统计结果是: %d/n", count);

    return 0;
}
/*
加上main进程，一共有20个进程
#include <stdio.h>  
int main(int argc, char* argv[])  
{  
   fork();  
   fork() && fork() || fork();  
   fork();  
   printf("+/n");  
} 
*/