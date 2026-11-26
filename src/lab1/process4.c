/* 实验一 任务(2) 变体二：父子进程都是死循环 */
#include <stdio.h>
#include <sys/types.h>
#include <unistd.h>

int main(void)
{
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
        for (;;)
            printf("This is parent process\n");
    }
    return 0;
}
