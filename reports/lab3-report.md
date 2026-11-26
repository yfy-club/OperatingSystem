# 操作系统原理 实验报告

* **实验名称**：实验三：进程通信（一）
* **姓名**：（填写姓名）　**学号**：（填写学号）　**日期**：2026-10-25
* **维护复测时间**：2026-11-26T19:44:23+08:00
* **复测环境**：Linux 7.3.5-2-cachyos x86_64 GNU/Linux；gcc (GCC) 16.2.1 20260810

> 图片为实际运行记录的排版图，非原始终端截图。提交时需补本人终端原始截图、个人信息；本文程序清单由源码同步生成。

## 一、实验过程（20 分）

管道程序先创建管道再 fork。子进程发送 PID 和 `is sending a message to parent !`，父进程读出显示。两方关闭不用的一端，接收方循环读取至 EOF，并自行补字符串结束符。发送方处理短写，接收方处理短读，两方对 EINTR 重试，避免把字节流误当作一次 read 必然完整的一条消息。

共享内存程序由父进程创建 512 字节的私有 System V 段，子进程附加、写入并脱离；父进程确认子进程成功结束后，以只读方式附加、显示数据，再脱离并删除段。本次补齐 fork、父子 shmat、等待状态和 IPC 撤销的错误处理。可处理的失败路径都会尝试清理本次创建的段；SIGKILL、断电等不可处理的中断不作自动清理保证。

## 二、实验详细操作步骤与程序清单（30 分）

在仓库根目录中执行：

```sh
make all
./build/lab3/pipe_comm
unshare --user --map-root-user --ipc sh -c './build/lab3/shm_comm; ipcs -m'
make record
make reports
```

`unshare` 创建独立 IPC 命名空间，使复测不会影响主机现有的实验对象。系统若禁用无特权用户命名空间，应先确认权限配置，不能把启动失败当作实验通过。一般直接运行本程序也可创建独立的 IPC_PRIVATE 段，但仍需正确清理自己创建的对象。

### `src/lab3/pipe_comm.c`

```c
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
```

### `src/lab3/shm_comm.c`

```c
/* 实验三：512 字节 System V 共享内存；用子进程退出事件保证先写后读。 */
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/shm.h>

#define SHM_SIZE 512

int main(void)
{
    int rc = 1;
    int shmid = shmget(IPC_PRIVATE, SHM_SIZE, IPC_CREAT | 0600);
    if (shmid < 0)
    {
        perror("shmget");
        return 1;
    }
    printf("parent %d: created %d-byte shared memory, shmid = %d\n",
           getpid(), SHM_SIZE, shmid);
    if (fflush(stdout) == EOF)
    {
        perror("fflush");
        goto cleanup;
    }
    pid_t pid = fork();
    if (pid < 0)
    {
        perror("fork");
        goto cleanup;
    }
    if (pid == 0)
    {
        char *addr = shmat(shmid, NULL, 0);
        if (addr == (void *)-1) /* 注意不是 NULL */
        {
            perror("child shmat");
            _exit(1);
        }
        int n = snprintf(addr, SHM_SIZE, "child %d is writing data into the shared memory", getpid());
        int failed = n < 0 || n >= SHM_SIZE;
        if (!failed)
            printf("child %d wrote: %s\n", getpid(), addr);
        if (shmdt(addr) < 0)
        {
            perror("child shmdt");
            failed = 1;
        }
        exit(failed); /* 刷新该子进程自己的输出缓冲 */
    }

    int status;
    pid_t done;
    do { done = waitpid(pid, &status, 0); } while (done < 0 && errno == EINTR);
    if (done < 0)
    {
        perror("waitpid");
        goto cleanup;
    }
    if (!WIFEXITED(status) || WEXITSTATUS(status) != 0)
    {
        fprintf(stderr, "child failed; shared data not read\n");
        goto cleanup;
    }
    char *addr = shmat(shmid, NULL, SHM_RDONLY);
    if (addr == (void *)-1)
    {
        perror("parent shmat");
        goto cleanup;
    }
    printf("parent %d read : %.*s\n", getpid(), SHM_SIZE, addr);
    rc = 0;
    if (shmdt(addr) < 0)
    {
        perror("parent shmdt");
        rc = 1;
    }

cleanup: /* 只删除本次创建的段；所有可处理的失败路径均到这里 */
    if (shmctl(shmid, IPC_RMID, NULL) < 0)
    {
        perror("shmctl IPC_RMID");
        rc = 1;
    }
    else
        printf("parent %d: shared memory removed\n", getpid());
    return rc;
}
```

## 三、实验结果及分析（30 分）

### 1. 管道通信

![管道运行记录排版图，非原始终端截图](shots/lab3_pipe.png)

```text
# Captured 2026-11-26T19:44:23+08:00; actual run transcript (not a desktop screenshot)
$ ./pipe_comm
parent 79607 received: 79608 is sending a message to parent !
```

父进程显示收到的子 PID 与要求的字符串。父子 PID 不保证相邻；仅凭“父 PID + 1”不是可靠验证方法。回归测试还将每次 read 限制为最多 3 字节、write 限制为最多 2 字节，并分别注入一次 EINTR，仍得到完整消息。父进程必须先读再等待，避免消息超过管道容量时“子进程等写入、父进程等退出”的死锁。

### 2. 共享存储区通信

![共享内存运行记录排版图，非原始终端截图](shots/lab3_shm.png)

```text
# Captured 2026-11-26T19:44:23+08:00; actual run transcript (not a desktop screenshot)
$ ./shm_comm # isolated IPC namespace
parent 79652: created 512-byte shared memory, shmid = 0
child 79653 wrote: child 79653 is writing data into the shared memory
parent 79652 read : child 79653 is writing data into the shared memory
parent 79652: shared memory removed
IPC check: no shared memory segments remain
```

输出依次显示创建 512 字节段、子进程写入、父进程读出同样内容、撤销段。测试在独立 IPC 命名空间内检查段表为空，同时实际覆盖 fork 失败、子附加失败、父附加失败和子异常状态的清理分支。父读取前确认子进程正常成功结束，不会在子写入失败后仍把空数据当作成功。

### 3. 思考题

**（1）哪种机制提供同步，如何实现？**

阻塞模式管道具有缓冲层面的同步：空管道且仍有写端存在时，read 等待；空管道且全部写端已关闭时，read 返回 0；有数据时可先读出数据，读尽后才是 EOF。管道满且仍有读端时，阻塞 write 等待空间；所有读端关闭时，写入触发 SIGPIPE，忽略/处理该信号后可观察到 EPIPE。非阻塞模式的行为不同。管道容量与系统配置有关，不能固定写成 64 KB，也不能认为一次 read 一定与一次 write 对应。

共享内存本身不规定读写顺序或提供互斥。本实验仅一次传递，故使用 `waitpid()` 等子退出后再读；长期交互应采用合适的进程共享信号量、锁等同步协议，不能用普通变量忙等代替所有同步。

**（2）两种机制的特点及适用场合？**

匿名管道通常用于通过 fork 继承描述符的亲缘进程；也可以通过其他机制传递描述符，因此“只能亲缘进程使用”过于绝对。它是有限容量的字节流，适合命令流水线和顺序数据传输。内核管道对象的存续取决于相关描述符是否全部关闭，不是任意一个进程退出就消失。

共享内存映射后可直接访问同一段存储，避免每次传递都经内核消息缓冲搬运大块数据，适合大量、频繁的数据交换；但映射、缺页和同步仍有开销，不能无条件称为所有场景最快。满足权限条件的非亲缘进程也可共享。`shmdt()` 只脱离映射，`shmctl(IPC_RMID)` 标记删除，段在最后一个附加者脱离后真正销毁；未删除的 System V 段可在创建者退出后继续存在。

## 四、疑难小结（20 分）

1. 原管道程序只 read 一次且未显式补 NUL，本次改为读至 EOF、边界检查并补结束符；超过接收缓冲区时报告错误而不越界。
2. `shmat()` 失败返回 `(void *)-1`，不能按 NULL 判断。父、子附加失败均已注入测试，并确认没有段残留。
3. fork 前刷新共享内存创建提示，避免用户态缓冲副本重复输出。共享内存可见性与终端输出缓冲是两个不同问题。
4. 同步策略需与任务一致：本题用进程退出事件已足够；若父子持续共同修改数据，再引入更适合的同步对象。本节仅记录实际维护与测试，不补写未经核实的历史故障。
