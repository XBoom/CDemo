#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <stdlib.h>

#define FATHER_SLEEP_TIMES (5)   
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
        sleep(2);
        printf("pid: %d\t ppid:%d\n", getpid(), getppid());
        printf("child process is exited.\n");
    }
    else
    {
        printf("I am father process.\n");
        sleep(FATHER_SLEEP_TIMES);  //父进程后退出，则子进程称为僵尸进程；父进程先退出，则子进程称为孤儿进程
        system("ps -o pid, ppid, state, tty, command");
        printf("father process is exited.\n");
    }
    
    return 0;

}