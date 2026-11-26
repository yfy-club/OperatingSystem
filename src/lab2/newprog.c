/* 实验二 任务二：新的执行代码，由子进程通过 execvp 装入 */
#include <stdio.h>
#include <unistd.h>

int main(void)
{
    printf("new program.\n");
    printf("newprog pid: %d\n", getpid()); /* 与 exec 前的子进程 PID 对照 */
    return 0;
}
