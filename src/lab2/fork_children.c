/* 实验二 任务一：fork 创建两个子进程，父子进程重复显示自己的标识数 */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <errno.h>

#define LOOP 6 /* 每个进程重复显示的次数 */

/* wait 返回任意已结束的子进程，并不保证创建顺序。 */
static int reap(int count)
{
    int failed = 0;
    while (count-- > 0)
    {
        int status;
        pid_t done;
        do 
        { 
            done = wait(&status); 
        } 
        while (done < 0 && errno == EINTR);

        if (done < 0)
        {
            perror("wait");
            return 1;
        }
        if (!WIFEXITED(status) || WEXITSTATUS(status) != 0)
            failed = 1;
    }
    return failed;
}

int main(void)
{
    pid_t pid;

    /* 把 stdout 设为行缓冲：输出重定向到管道/文件时也逐行刷新，
       便于观察三个进程并发执行时输出交错出现的现象 */
    setvbuf(stdout, NULL, _IOLBF, 0);

    pid = fork(); /* 创建第一个子进程 */
    if (pid < 0)
    {
        perror("fork");
        return 1;
    }
    if (pid == 0)
    {
        for (int i = 0; i < LOOP; i++)
        {
            printf("child: %d\n", getpid());
            usleep(100000); /* 睡 100ms，便于观察交错，不保证轮流执行 */
        }
        exit(0);
    }

    pid = fork(); /* 创建第二个子进程 */
    if (pid < 0)
    {
        perror("fork");
        (void)reap(1); /* 第二次创建失败时仍回收已有子进程 */
        return 1;
    }
    if (pid == 0)
    {
        for (int i = 0; i < LOOP; i++)
        {
            printf("child: %d\n", getpid());
            usleep(150000); /* 睡 150ms，与另一子进程错开节奏 */
        }
        exit(0);
    }

    /* 父进程重复显示 "parent:" 与自己的标识数 */
    for (int i = 0; i < LOOP; i++)
    {
        printf("parent: %d\n", getpid());
        usleep(120000);
    }

    return reap(2); /* 回收两个子进程并检查状态 */
}
