/* 实验三：匿名管道是字节流。本例以所有写端关闭后的 EOF 作为消息结束。 */
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>

int main(void)
{
    int fd[2]; /* fd[0] 读，fd[1] 写 */
    if (pipe(fd) < 0)
    {
        perror("pipe");
        return 1;
    }
    pid_t pid = fork();
    if (pid < 0)
    {
        perror("fork");
        close(fd[0]);
        close(fd[1]);
        return 1;
    }
    if (pid == 0)
    {
        close(fd[0]);
        char msg[128];
        int len = snprintf(msg, sizeof(msg), "%d is sending a message to parent !", getpid());
        if (len < 0 || (size_t)len >= sizeof(msg))
            _exit(1);
        size_t sent = 0;
        while (sent < (size_t)len)
        {
            ssize_t n = write(fd[1], msg + sent, (size_t)len - sent);
            if (n < 0 && errno == EINTR)
                continue;
            if (n <= 0)
            {
                perror("write");
                _exit(1);
            }
            sent += (size_t)n;
        }
        close(fd[1]); /* 父进程会读到 EOF，无需传递字符串结束符 */
        _exit(0);
    }

    close(fd[1]); /* 否则自己的写端会使 read 永远等不到 EOF */
    char buf[256], part[128];
    size_t used = 0;
    int failed = 0;
    for (;;)
    {
        ssize_t n = read(fd[0], part, sizeof(part));
        if (n < 0 && errno == EINTR)
            continue;
        if (n < 0)
        {
            perror("read");
            failed = 1;
            break;
        }
        if (n == 0)
            break;
        if ((size_t)n > sizeof(buf) - 1 - used)
            failed = 1; /* 继续排空管道，防止写端因管道满而无法退出 */
        else if (!failed)
        {
            memcpy(buf + used, part, (size_t)n);
            used += (size_t)n;
        }
    }
    close(fd[0]);
    int status;
    pid_t done;
    do { done = waitpid(pid, &status, 0); } while (done < 0 && errno == EINTR);
    if (done < 0)
    {
        perror("waitpid");
        return 1;
    }
    if (failed || used == 0 || !WIFEXITED(status) || WEXITSTATUS(status) != 0)
    {
        fprintf(stderr, "pipe transfer failed or message too long\n");
        return 1;
    }
    buf[used] = '\0'; /* 无论分几次 read，都由接收方补上结束符 */
    printf("parent %d received: %s\n", getpid(), buf);
    return 0;
}
