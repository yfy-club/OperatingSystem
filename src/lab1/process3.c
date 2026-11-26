/* 实验一 任务(2) 变体一：子进程死循环，父进程打印 999 次后退出 */
#include <stdio.h>
#include <sys/types.h>
#include <unistd.h>

int main(void)
{
    int i;
    pid_t pid = fork();
    if (pid < 0)
    {
        perror("fork");
        return 1;
    }
    if (pid == 0)
    {
        for (;;)
            printf("This is child process\n");
    }
    else
    {
        for (i = 1; i < 1000; i++)
            printf("This is parent process\n");
    }
    return 0;
}
