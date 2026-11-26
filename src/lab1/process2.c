/* 实验一 任务(2)：fork 创建子进程，父子进程各打印 999 次 */
#include <stdio.h>
#include <sys/types.h>
#include <unistd.h>

int main(void)
{
    int i;
    /* 为便于按行统计，重定向时也使用行缓冲；不规定父子执行次序。 */
    setvbuf(stdout, NULL, _IOLBF, 0);
    pid_t pid = fork();
    if (pid < 0)
    {
        perror("fork");
        return 1;
    }
    if (pid == 0)
    {
        for (i = 1; i < 1000; i++)
            printf("This is child process\n");
    }
    else
    {
        for (i = 1; i < 1000; i++)
            printf("This is parent process\n");
    }
    return 0;
}
