/* 实验二 任务二：子进程 execvp 更换执行代码，父进程 wait 等待并显示子进程标识符 */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <errno.h>

int main(void)
{
    pid_t pid;
    int status;

    pid = fork(); /* 创建子进程 */
    if (pid < 0)
    {
        perror("fork");
        return 1;
    }
    if (pid == 0)
    {
        /* 子进程：打印一行后用 execvp 把当前进程映像替换为 ./newprog。
           execvp 成功后原代码不再执行，只有失败才会返回 */
        printf("child %d: going to execvp a new program\n", getpid());
        /* exec 前必须刷新 stdio 缓冲：execvp 会整个替换进程映像，
           尚未写到管道/文件的缓冲数据会被直接丢弃 */
        if (fflush(stdout) == EOF)
        {
            perror("fflush");
            _exit(1);
        }
        char *argv[] = {"newprog", NULL};
        execvp("./newprog", argv);
        perror("execvp"); /* 只有 execvp 出错才会执行到这里 */
        _exit(127);
    }

    /* 父进程：wait 阻塞等待子进程结束，结束后显示子进程标识符 */
    pid_t cpid;
    do { cpid = wait(&status); } while (cpid < 0 && errno == EINTR);
    if (cpid < 0)
    {
        perror("wait");
        return 1;
    }
    if (WIFEXITED(status))
    {
        printf("parent %d: child %d finished, exit code %d\n",
               getpid(), cpid, WEXITSTATUS(status));
        return WEXITSTATUS(status);
    }
    if (WIFSIGNALED(status))
        fprintf(stderr, "child %d terminated by signal %d\n", cpid, WTERMSIG(status));
    return 1;
}
