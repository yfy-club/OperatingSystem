/* 实验四：key=75，类型 1 请求；类型=子 PID 应答。仅删除本次创建的队列。 */
#define _POSIX_C_SOURCE 200809L
#include <errno.h>      /* errno 和 EINTR、ESRCH 等错误码 */
#include <signal.h>     /* sigaction、SIGALRM、SIGTERM */
#include <stdio.h>      /* printf、perror、snprintf */
#include <stdlib.h>
#include <string.h>     /* memcmp */
#include <unistd.h>     /* fork、getpid、alarm */
#include <sys/wait.h>   /* waitpid 以及 WIFEXITED 等宏 */
#include <sys/msg.h>    /* System V 消息队列相关接口 */

#define MSGKEY 75 /* 消息队列的唯一键值，用于创建和定位该队列 */
#define TIMEOUT 5 /* 对端故障时不永久阻塞；不是消息队列自带的超时机制 */

struct msgform
{
    long mtype; /* 表示消息类型，用于接收方筛选消息。 */
    char mtext[1024]; /* 表示消息内容。 */
};

static volatile sig_atomic_t expired; /* 表示是否超时  0表示未超时，1表示已超时 */
static void on_alarm(int sig)
{
    /* 信号处理函数只修改标志，不在信号处理期间执行复杂操作。 */
    (void)sig;
    expired = 1;
    alarm(1); /* 再次唤醒，避免信号恰在进入阻塞调用前送达而丢失唤醒 */
}

/* qid：消息队列id type：消息类型 pid：要发送的进程ID */
static int send_pid(int qid, long type, pid_t pid)
{
    struct msgform msg = {.mtype = type};
    /* 消息正文使用字符串保存 PID，便于发送端和接收端进行严格校验。 */
    int len = snprintf(msg.mtext, sizeof(msg.mtext), "%ld", (long)pid);
    /* snprintf 会自动写入字符串结束符，len 不包含最后的 '\0'。 */
    if (len < 0 || (size_t)len >= sizeof(msg.mtext))
        return -1;
    int rc;
    /* msgsnd 的长度只计算 mtext，不包括消息结构开头的 mtype。 */
    do { rc = msgsnd(qid, &msg, (size_t)len + 1, 0); }
    /* 队列暂时不可用时可能被 SIGALRM 打断；未超时则继续尝试。 */
    while (rc < 0 && errno == EINTR && !expired);
    if (rc < 0)
        perror("msgsnd");
    return rc;
}

/* 本例仅有一对父子进程，严格核对正文长度、结束符及对端 PID。 */
static int receive_pid(int qid, long type, pid_t expected)
{
    struct msgform msg;
    ssize_t n;
    /* msgrcv 只接收指定类型的消息；消息类型必须是正数。 */
    do { n = msgrcv(qid, &msg, sizeof(msg.mtext), type, 0); }
    /* 没有匹配消息时会阻塞；被普通信号打断且未超时则重新等待。 */
    while (n < 0 && errno == EINTR && !expired);
    if (n < 0)
    {
        perror("msgrcv");
        return -1;
    }
    /* 接收内容应当是 expected 对应的十进制 PID。 */
    char text[32];
    int len = snprintf(text, sizeof(text), "%ld", (long)expected);
    /* len + 1 包含字符串结束符，必须与发送时的长度完全一致。 */
    /* 同时检查长度和正文，避免收到不属于当前通信双方的消息。 */
    if (len < 0 || (size_t)len >= sizeof(text) || n != len + 1 ||
        memcmp(msg.mtext, text, (size_t)len + 1) != 0)
    {
        fprintf(stderr, "invalid PID message\n");
        return -1;
    }
    return 0;
}

int main(void)
{
    /*
     * 先注册 SIGALRM 处理函数。
     * 后续的消息收发和 waitpid 可能阻塞，alarm 用于避免无限等待。
     */
    struct sigaction sa = {0};
    sa.sa_handler = on_alarm;
    /* 不设置 SA_RESTART，使 SIGALRM 能够打断阻塞系统调用。 */
    sigemptyset(&sa.sa_mask);
    if (sigaction(SIGALRM, &sa, NULL) < 0)
    {
        perror("sigaction");
        return 1;
    }
    /* 使用行缓冲，确保父子进程的状态信息及时输出。 */
    setvbuf(stdout, NULL, _IOLBF, 0);

    /*
     * 创建本次运行专用的消息队列。
     * IPC_EXCL 防止误用同 key 的旧队列，0600 只允许当前用户读写。
     */
    int qid = msgget(MSGKEY, IPC_CREAT | IPC_EXCL | 0600);
    if (qid < 0)
    {
        perror("msgget (existing queue is left untouched)");
        return 1;
    }
    printf("message queue created, key = %d, qid = %d\n", MSGKEY, qid);
    /* 保存父进程 PID，子进程会用它验证应答消息的发送者。 */
    pid_t parent = getpid();
    /* fork 后父子进程共享该消息队列，但各自拥有独立的执行流。 */
    pid_t pid = fork();
    int rc = 1;
    if (pid < 0)
    {
        /* fork 失败时没有子进程，但已创建的消息队列仍需删除。 */
        perror("fork");
        goto cleanup;
    }

    if (pid == 0)
    {
        /*
         * 子进程作为客户端：
         * 发送 type=1、正文为自身 PID 的请求，
         * 再接收 type=自身 PID 的服务器应答。
         */
        alarm(TIMEOUT);
        if (send_pid(qid, 1L, getpid()) < 0)
            _exit(1);
        printf("client %d: request sent, waiting for reply...\n", getpid());
        if (receive_pid(qid, (long)getpid(), parent) < 0)
            _exit(1);
        printf("client %d: receive reply from %d\n", getpid(), parent);
        _exit(0);
    }

    /*
     * fork 返回子进程 PID，说明当前处于父进程分支。
     * 父进程作为服务器，接收 type=1 的请求，
     * 再向对应的子进程 PID 发送应答。
     */
    alarm(TIMEOUT);
    if (receive_pid(qid, 1L, pid) < 0)
        goto cleanup;
    printf("server %d: serving for client %d\n", getpid(), pid);
    if (send_pid(qid, (long)pid, getpid()) < 0)
        goto cleanup;

    int status;
    pid_t done;
    /*
     * 等待子进程结束并保存退出状态。
     * 如果只是被信号打断且尚未超时，则继续等待。
     */
    do { done = waitpid(pid, &status, 0); }
    while (done < 0 && errno == EINTR && !expired);
    if (done < 0)
    {
        perror("waitpid");
        goto cleanup;
    }
    /* 子进程已被回收，清理阶段不应再次向它发送信号。 */
    pid = -1;
    /* 只有子进程正常退出且返回 0，父进程才报告通信成功。 */
    rc = !WIFEXITED(status) || WEXITSTATUS(status) != 0;

cleanup:
    /*
     * 统一清理入口：通信成功或任意一步失败，
     * 都必须取消定时器并删除本次创建的消息队列。
     */
    alarm(0);
    if (msgctl(qid, IPC_RMID, NULL) < 0)
    {
        perror("msgctl IPC_RMID");
        rc = 1;
    }
    else
        printf("server %d: message queue removed\n", getpid());
    if (pid > 0)
    {
        /* 出错时子进程可能仍在阻塞，先终止它，再回收其进程资源。 */
        if (kill(pid, SIGTERM) < 0 && errno != ESRCH)
            perror("kill child");
        /* waitpid 被信号中断时继续等待，避免留下僵尸进程。 */
        while (waitpid(pid, NULL, 0) < 0)
        {
            if (errno != EINTR)
            {
                perror("waitpid cleanup");
                break;
            }
        }
    }
    return rc;
}
