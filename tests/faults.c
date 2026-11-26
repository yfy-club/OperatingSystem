/* 仅用于测试：链接器 --wrap 故障注入，不影响正式实验二进制。 */
#define _GNU_SOURCE
#include <errno.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/shm.h>
#include <sys/msg.h>
#include <sys/wait.h>

static int mode(const char *name)
{
    const char *value = getenv("LAB_FAULT");
    return value && strcmp(value, name) == 0;
}

pid_t __real_fork(void);
pid_t __wrap_fork(void)
{
    static int calls;
    calls++;
    if (mode("fork") || (mode("fork_second") && calls == 2))
    {
        errno = EAGAIN;
        return -1;
    }
    pid_t pid = __real_fork();
    if (pid == 0 && mode("child_exit"))
        _exit(2);
    if (pid == 0 && mode("child_signal"))
    {
        raise(SIGTERM);
        _exit(2);
    }
    return pid;
}

pid_t __real_wait(int *status);
pid_t __wrap_wait(int *status)
{
    static int calls;
    if (mode("wait_eintr") && calls++ == 0)
    {
        errno = EINTR;
        return -1;
    }
    return __real_wait(status);
}

ssize_t __real_read(int fd, void *buf, size_t count);
ssize_t __wrap_read(int fd, void *buf, size_t count)
{
    static int calls;
    if (mode("short_io") && calls++ == 0)
    {
        errno = EINTR;
        return -1;
    }
    if (mode("short_io") && count > 3) count = 3;
    return __real_read(fd, buf, count);
}

ssize_t __real_write(int fd, const void *buf, size_t count);
ssize_t __wrap_write(int fd, const void *buf, size_t count)
{
    static int calls;
    if (mode("short_io") && calls++ == 0)
    {
        errno = EINTR;
        return -1;
    }
    if (mode("short_io") && count > 2) count = 2;
    return __real_write(fd, buf, count);
}

void *__real_shmat(int id, const void *addr, int flags);
void *__wrap_shmat(int id, const void *addr, int flags)
{
    if (mode("shmat") || (mode("parent_shmat") && (flags & SHM_RDONLY)))
    {
        errno = ENOMEM;
        return (void *)-1;
    }
    return __real_shmat(id, addr, flags);
}

int __real_msgsnd(int id, const void *msg, size_t size, int flags);
int __wrap_msgsnd(int id, const void *msg, size_t size, int flags)
{
    if (mode("msgsnd"))
    {
        errno = EACCES;
        return -1;
    }
    return __real_msgsnd(id, msg, size, flags);
}

void *__real_malloc(size_t size);
void *__wrap_malloc(size_t size)
{
    static int calls;
    calls++;
    if ((mode("malloc_pool") && calls == 1) ||
        (mode("malloc_init") && calls == 2) ||
        (mode("malloc_release") && calls == 3))
    {
        errno = ENOMEM;
        return NULL;
    }
    return __real_malloc(size);
}
