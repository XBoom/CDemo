#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <stdlib.h>

int main()
{
    pid_t pid;
    pid = fork();
    if (pid < 0)
    {
        perror("fork error:");
        exit(1);   //stdlib.h
    }

    if (pid == 0)
    {
        printf("I am the child process\n");
        printf("pid: %d\t ppid:%d\n",getpid(), getppid());
        printf("I wile sleep five seconds.\n");
        sleep(5);
        printf("pid: %d\t ppid:%d\n", getpid(), getppid());
        printf("child process is exited.\n");
    }
    else
    {
        printf("I am father process.\n");
        sleep(1);   //父进程先退出，子进程成为孤儿进程
        printf("father process is exited.\n");
    }
    
    return 0;

}