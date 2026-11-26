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
