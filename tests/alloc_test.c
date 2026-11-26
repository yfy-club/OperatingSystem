/* 在同一翻译单元内测试教学程序的 static 分配接口，不增加正式程序入口。 */
#include <assert.h>
#include <stdlib.h>
static int live_nodes;
static int fail_next;
static void *test_malloc(size_t size)
{
    if (fail_next)
    {
        fail_next = 0;
        return NULL;
    }
    void *ptr = malloc(size);
    if (ptr) live_nodes++;
    return ptr;
}
static void test_free(void *ptr)
{
    if (ptr) live_nodes--;
    free(ptr);
}
#define malloc test_malloc
#define free test_free
#define main lab5_main
#include "../src/lab5/alloc.c"
#undef main
#undef malloc
#undef free

static void check(ALLOC *a)
{
    unsigned char state[MEM_SIZE] = {0};
    FB *previous = NULL;
    for (FB *p = a->head; p; p = p->next)
    {
        assert(p->addr >= 0 && p->size > 0 && p->size <= MEM_SIZE - p->addr);
        if (previous)
        {
            if (a->best)
                assert(previous->size < p->size ||
                       (previous->size == p->size && previous->addr < p->addr));
            else
                assert(previous->addr + previous->size < p->addr);
        }
        for (int k = p->addr; k < p->addr + p->size; k++)
        {
            assert(state[k] == 0);
            state[k] = 1;
        }
        /* 任意两个空闲块均不重叠且不相邻，即合并已经完成。 */
        for (FB *q = p->next; q; q = q->next)
            assert(p->addr + p->size < q->addr || q->addr + q->size < p->addr);
        previous = p;
    }
    for (int i = 0; i < MEM_SIZE; i++)
    {
        int size = a->allocated[i];
        if (!size)
            continue;
        assert(size > 0 && size <= MEM_SIZE - i);
        for (int k = i; k < i + size; k++)
        {
            assert(state[k] == 0);
            state[k] = 2;
        }
    }
    for (int k = 0; k < MEM_SIZE; k++)
    {
        assert(state[k] != 0); /* 空闲量 + 已分配量 = 640 KB */
        for (int byte = 0; byte < 1024; byte++)
            assert(a->pool[(size_t)k * 1024 + (size_t)byte] == (state[k] == 2));
    }
}

static unsigned int seed = 20261028;
static unsigned int next_random(void)
{
    seed = seed * 1664525U + 1013904223U;
    return seed;
}

int main(void)
{
    unsigned char *pool = calloc(MEM_SIZE, 1024);
    assert(pool);
    ALLOC a;
    fail_next = 1;
    assert(myinit(&a, 1, pool) == -1 && live_nodes == 0);
    /* 原缺陷回归：分配后 100K、40K 的链必须重排成 40K、100K。 */
    assert(myinit(&a, 1, pool) == 0);
    assert(mymalloc(&a, 100) == 0);
    assert(mymalloc(&a, 100) == 100);
    assert(mymalloc(&a, 100) == 200);
    assert(myfree(&a, 0, 100) == 0);
    assert(mymalloc(&a, 300) == 300);
    FB *unchanged = a.head;
    fail_next = 1;
    assert(myfree(&a, 100, 100) == -1 && a.head == unchanged);
    assert(a.allocated[100] == 100); /* 结点申请失败不应改变账目 */
    assert(a.head->addr == 600 && a.head->size == 40);
    assert(a.head->next->addr == 0 && a.head->next->size == 100);
    check(&a);
    mydestroy(&a);
    assert(live_nodes == 0);
    for (int best = 0; best <= 1; best++)
    {
        memset(pool, 0, (size_t)MEM_SIZE * 1024);
        assert(myinit(&a, best, pool) == 0);
        assert(mymalloc(&a, 0) == -1);
        assert(mymalloc(&a, -1) == -1);
        assert(mymalloc(&a, MEM_SIZE + 1) == -1);
        assert(myfree(&a, -1, 1) == -1);
        assert(myfree(&a, MEM_SIZE, 1) == -1);
        assert(myfree(&a, 0, MEM_SIZE + 1) == -1);
        assert(mymalloc(&a, MEM_SIZE) == 0);
        assert(a.head == NULL);
        assert(myfree(&a, 0, 0) == -1);
        assert(myfree(&a, 1, MEM_SIZE - 1) == -1);
        assert(mymalloc(&a, 1) == -1);
        assert(myfree(&a, 0, MEM_SIZE - 1) == -1);
        assert(myfree(&a, 0, MEM_SIZE) == 0);
        assert(myfree(&a, 0, MEM_SIZE) == -1);
        check(&a);
        for (int iteration = 0; iteration < 1000; iteration++)
        {
            int addr = (int)(next_random() % MEM_SIZE);
            if ((next_random() % 3U) == 0 && a.allocated[addr])
                assert(myfree(&a, addr, a.allocated[addr]) == 0);
            else
            {
                int size = (int)(next_random() % 80U) + 1;
                int expected = -1, smallest = MEM_SIZE + 1;
                /* 独立按每 KB 占用图推导候选，不复用正式链查找逻辑。 */
                unsigned char busy[MEM_SIZE] = {0};
                for (int i = 0; i < MEM_SIZE; i++)
                    for (int j = 0; j < a.allocated[i]; j++)
                        busy[i + j] = 1;
                for (int i = 0; i < MEM_SIZE; )
                {
                    if (busy[i]) { i++; continue; }
                    int start = i;
                    while (i < MEM_SIZE && !busy[i]) i++;
                    int length = i - start;
                    if (length >= size && (expected < 0 || (best && length < smallest)))
                    {
                        expected = start;
                        smallest = length;
                    }
                }
                assert(mymalloc(&a, size) == expected);
            }
            check(&a);
        }
        for (int i = 0; i < MEM_SIZE; i++)
            if (a.allocated[i])
                assert(myfree(&a, i, a.allocated[i]) == 0);
        assert(a.head && a.head->addr == 0 && a.head->size == MEM_SIZE && !a.head->next);
        check(&a);
        mydestroy(&a);
        assert(live_nodes == 0);
    }
    free(pool);
    puts("allocator: ordering, bounds, exact/double free, full merge, malloc failure, node balance, 2000 seeded operations PASS");
    return 0;
}
