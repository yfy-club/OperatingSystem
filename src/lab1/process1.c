/* 实验一 任务(1)：观察进程的 ID、CPU 占用与内存使用 */
#include <stdio.h>
#include <sys/types.h>
#include <unistd.h>

int main(void)
{
    /* 设计一个死循环，使进程持续运行，便于用 top/ps 观察其状态 */
    while (1)
    {
           // pid_t pid ;
            
        printf("I am the first process %d\n",getpid());
        sleep(1);
    }
    return 0;
}
