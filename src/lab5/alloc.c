/* 实验五：动态分区分配方式的模拟。
   用 malloc() 一次性申请 640KB 空间作为被管理的内存（此处用下标模拟地址，
   只做占用/空闲标记，并不真正向 OS 申请释放）。
   空闲分区用链表管理：
     - 首次适应(first fit)：空闲链按地址从小到大，分配时取第一个够用的块；
     - 最佳适应(best fit) ：空闲链按块大小从小到大，分配时取最小的够用块。
   分配时优先使用空闲块低端空间。回收时按地址插入并合并相邻空闲块。 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MEM_SIZE 640 /* 被管理内存总量 640KB */

/* 空闲分区结点 */
typedef struct fb
{
    int addr;        /* 分区起始地址(KB) */
    int size;        /* 分区大小(KB) */
    struct fb *next; /* 指向下一个空闲分区 */
} FB;

/* 分配器：head 指向空闲分区链，best=0 首次适应，best=1 最佳适应 */
typedef struct
{
    FB *head;
    int best;
    unsigned char *pool; /* main 一次申请；地址 addr 对应 pool + addr * 1024 */
    int allocated[MEM_SIZE]; /* 起址处记录已分配长度，用于拒绝错误/重复释放 */
} ALLOC;

static void resort(ALLOC *a, int by_size);

/* 初始化：整块 640KB 内存作为一个空闲分区 */
static int myinit(ALLOC *a, int best, unsigned char *pool)
{
    memset(a, 0, sizeof(*a));
    a->best = best;
    a->pool = pool;
    a->head = (FB *)malloc(sizeof(FB));
    if (a->head == NULL)
        return -1;
    a->head->addr = 0;
    a->head->size = MEM_SIZE;
    a->head->next = NULL;
    return 0;
}

/* 销毁管理结点；pool 的所有权在 main，最后只 free 一次。 */
static void mydestroy(ALLOC *a)
{
    while (a->head)
    {
        FB *next = a->head->next;
        free(a->head);
        a->head = next;
    }
}

/* 分配 size KB：成功返回起始地址，失败返回 -1。
   首次适应：取地址序中第一个 size 够用的块；
   最佳适应：在所有够用的块中取最小的。
   两种算法都从选中空闲块的低端切分。 */
static int mymalloc(ALLOC *a, int size)
{
    if (size <= 0 || size > MEM_SIZE)
        return -1;
    FB *prev = NULL, *cur = a->head;
    FB *pick = NULL, *pick_prev = NULL;

    while (cur)
    {
        if (cur->size >= size)
        {
            if (pick == NULL ||                       /* 第一个候选 */
                (a->best && cur->size < pick->size))  /* 最佳适应取更小者 */
            {
                pick = cur;
                pick_prev = prev;
                if (!a->best)
                    break; /* 首次适应：找到即止 */
            }
        }
        prev = cur;
        cur = cur->next;
    }
    if (pick == NULL)
        return -1; /* 没有足够大的空闲分区 */

    int addr = pick->addr; /* 从空闲块低端分配 */
    pick->addr += size;
    pick->size -= size;
    if (pick->size == 0) /* 整块用完则从链上摘除 */
    {
        if (pick_prev)
            pick_prev->next = pick->next;
        else
            a->head = pick->next;
        free(pick);
    }
    a->allocated[addr] = size;
    /* 真正的分区位于已申请的池中，不向 OS 再申请作业空间。 */
    memset(a->pool + (size_t)addr * 1024, 1, (size_t)size * 1024);
    if (a->best)
        resort(a, 1); /* 切分会改变大小，分配后也必须恢复最佳适应链序 */
    return addr;
}

/* 按地址从小到大把结点插入空闲链 */
static void insert_by_addr(ALLOC *a, FB *n)
{
    FB *prev = NULL, *cur = a->head;
    while (cur && cur->addr < n->addr)
    {
        prev = cur;
        cur = cur->next;
    }
    n->next = cur;
    if (prev)
        prev->next = n;
    else
        a->head = n;
}

/* 空闲链按指定关键字重排：by_size=1 按大小(最佳适应)，否则按地址 */
static void resort(ALLOC *a, int by_size)
{
    FB *sorted = NULL;
    while (a->head)
    {
        FB *n = a->head;
        a->head = n->next;
        FB *prev = NULL, *cur = sorted;
        while (cur && (by_size ?
               (cur->size < n->size || (cur->size == n->size && cur->addr < n->addr)) :
               cur->addr < n->addr))
        {
            prev = cur;
            cur = cur->next;
        }
        n->next = cur;
        if (prev)
            prev->next = n;
        else
            sorted = n;
    }
    a->head = sorted;
}

/* 回收起始地址 addr、大小 size 的分区：按地址插入，合并前后相邻空闲块 */
static int myfree(ALLOC *a, int addr, int size)
{
    if (addr < 0 || addr >= MEM_SIZE || size <= 0 || size > MEM_SIZE - addr ||
        a->allocated[addr] != size)
        return -1;
    /* 先申请结点，失败时保持原有分配状态和链序不变。 */
    FB *n = (FB *)malloc(sizeof(FB));
    if (n == NULL)
        return -1;
    /* 最佳适应的链平时按大小排序，先恢复地址序，插入合并才正确 */
    if (a->best)
        resort(a, 0);

    n->addr = addr;
    n->size = size;
    insert_by_addr(a, n);

    /* 合并地址相邻的空闲分区 */
    for (FB *cur = a->head; cur; cur = cur->next)
    {
        while (cur->next && cur->addr + cur->size == cur->next->addr)
        {
            FB *nb = cur->next;
            cur->size += nb->size;
            cur->next = nb->next;
            free(nb);
        }
    }
    if (a->best)
        resort(a, 1); /* 最佳适应：回收后重新按大小排序 */
    a->allocated[addr] = 0;
    memset(a->pool + (size_t)addr * 1024, 0, (size_t)size * 1024);
    return 0;
}

/* 打印当前空闲分区链 */
static void dump(ALLOC *a)
{
    int total = 0;
    printf("    free chain:");
    for (FB *p = a->head; p; p = p->next)
    {
        printf(" [%dK,%dK]", p->addr, p->size);
        total += p->size;
    }
    printf("  (total %dK free)\n", total);
}

/* 作业请求序列：op=1 申请，op=0 释放 */
struct step
{
    int job, op, size;
};
static struct step seq[] = {
    {1, 1, 130}, /* 作业 1 申请 130KB */
    {2, 1, 60},  /* 作业 2 申请 60KB  */
    {3, 1, 100}, /* 作业 3 申请 100KB */
    {2, 0, 60},  /* 作业 2 释放 60KB  */
    {4, 1, 200}, /* 作业 4 申请 200KB */
    {3, 0, 100}, /* 作业 3 释放 100KB */
    {1, 0, 130}, /* 作业 1 释放 130KB */
    {5, 1, 140}, /* 作业 5 申请 140KB */
    {6, 1, 60},  /* 作业 6 申请 60KB  */
    {7, 1, 50},  /* 作业 7 申请 50KB  */
    {6, 0, 60},  /* 作业 6 释放 60KB  */
};

/* 用指定算法跑一遍完整作业序列，每步打印空闲链 */
static int run(int best, unsigned char *pool)
{
    ALLOC a;
    int addr[8] = {0}, size[8] = {0}; /* 记录每个作业的分区(作业号 1..7) */

    printf("===== %s =====\n", best ? "Best Fit (smallest adequate block)" : "First Fit (lowest address first)");
    if (myinit(&a, best, pool) < 0)
    {
        perror("malloc free-list node");
        return 1;
    }
    memset(pool, 0, (size_t)MEM_SIZE * 1024);
    printf("initial state: %d KB memory\n", MEM_SIZE);
    dump(&a);

    size_t steps = sizeof(seq) / sizeof(seq[0]);
    for (size_t k = 0; k < steps; k++)
    {
        struct step *s = &seq[k];
        printf("[step %02zu] job %d %s %d KB", k + 1, s->job,
               s->op ? "requests" : "releases", s->size);
        if (s->op) /* 申请 */
        {
            int ad = mymalloc(&a, s->size);
            if (ad < 0)
            {
                printf("  -> FAILED: no adequate free block\n");
                mydestroy(&a);
                return 1;
            }
            else
            {
                addr[s->job] = ad;
                size[s->job] = s->size;
                printf("  -> allocated [%dK,%dK)\n", ad, ad + s->size);
            }
        }
        else /* 释放 */
        {
            if (myfree(&a, addr[s->job], size[s->job]) < 0)
            {
                fprintf(stderr, "invalid release or node allocation failed\n");
                mydestroy(&a);
                return 1;
            }
            printf("  -> released  [%dK,%dK)\n",
                   addr[s->job], addr[s->job] + size[s->job]);
            size[s->job] = 0;
        }
        dump(&a);
    }
    printf("\n");
    mydestroy(&a); /* 作业 4、5、7 仍占用；结束模拟时统一销毁管理账目 */
    return 0;
}

int main(void)
{
    unsigned char *pool = malloc((size_t)MEM_SIZE * 1024);
    if (pool == NULL)
    {
        perror("malloc 640KB pool");
        return 1;
    }
    int rc = run(0, pool); /* 首次适应算法 */
    if (rc == 0)
        rc = run(1, pool); /* 重置同一块内存，独立模拟最佳适应 */
    free(pool);
    return rc;
}
