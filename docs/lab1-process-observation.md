# 操作系统原理-作业

* **实验名称**：实验一：进程观测

---

## 一、实验内容及要求（共1题，0.0分）

### 1. 实验目的

1. 了解在 Linux 操作系统中进程的特点和表现形式。
2. 掌握 Linux 查看进程的方式与方法。
3. 掌握在一个进程中创建另一个进程的方法。
4. 掌握父进程和子进程的关系及 `fork` 的用法。

### 2. 实验内容

1. 编写一个简单的程序，使用 `ps` 或 `top` 工具观察该进程的 ID 号，并使用 `kill` 工具终止进程运行。
2. 编写一个程序，使用 `fork` 函数生成一个子进程，并使用相关工具观察进程状态。

### 3. 实验步骤

#### 任务 (1)

编写代码如下：

```c
#include <stdio.h>
int main()
{
    //设计一个循环，使其反复运行，方便观察
    while(1)
    {
        printf("I am the first process!\n");
    }
    return 0;
}

```

**实验要求与问题：**

* 文件名命名为 `process1.c`，使用 `gcc process1.c -o process` 编译该程序。
* 运行该程序，打开另一个终端窗口，输入命令 `top`，观察名称为 `process1` 的进程，记录各项数据（包括进程号）。
* 使用 `kill 进程号` 直接杀死该进程。观察进程是否消失？
* **需要记录的数据**：进程状态中的 id，内存使用和 CPU 占有率。
* **思考题**：由于该进程一直处于循环中，思考 id、内存使用和 CPU 占有率哪一个因素和循环关系密切？如何避免，请给出合理的建议。这是否说明 CPU 也是操作系统中的一个重要资源？

#### 任务 (2)

编写代码如下：

```c
#include <stdio.h>
#include <sys/types.h>    
#include <unistd.h>
int main()
{
    int i;
    if ( fork() == 0 )
    {
       /*子进程程序*/
       for ( i = 1; i <1000; i ++ )
          printf("This is child process\n");
    }
    else
    {
       /*父进程程序*/
       for ( i = 1; i <1000; i ++ )
          printf("This is parent process\n");
    }
    return 0;
}

```

**实验要求与问题：**

* 保存为 `process2.c`，编译运行。
* 写出你观察到的输出结果，能否对输出的现象做一个合理的解释？

#### 任务 (2) 变体一

若将上述实例 (2) 改写为：

```c
#include <stdio.h>
#include <sys/types.h>    
#include <unistd.h>
int main()
{
    int i;
    if ( fork() == 0 )
    {
       /*子进程程序*/
       for ( ;; )
          printf("This is child process\n");
    }
    else
    {
       /*父进程程序*/
       for ( i = 1; i <1000; i ++ )
          printf("This is parent process\n");
    }
    return 0;
}

```

**实验要求与问题：**

* 保存为 `process3.c`，编译运行。
* 写出你观察到的输出结果，能否对输出的现象做一个合理的解释？

#### 任务 (2) 变体二

若将实例 (2) 改写为：

```c
#include <stdio.h>
#include <sys/types.h>    
#include <unistd.h>
int main()
{
    int i;
    if ( fork() == 0 )
    {
       /*子进程程序*/
       for ( ;; )
          printf("This is child process\n");
    }
    else
    {
       /*父进程程序*/
       for (;;)
          printf("This is parent process\n");
    }
    return 0;
}

```

**实验要求与问题：**

* 保存为 `process4.c`，编译运行。
* 写出你观察到的输出结果，能否对输出的现象做一个合理的解释？

---

## 二、实验报告作答项（共4题，100.0分）

### 1. 实验过程（20.0分）

* 请用文字描述本次实验内容、步骤与调试过程等。

### 2. 实验详细操作步骤与程序清单（30.0分）

* 完整列出本次实验操作过程、程序代码，注释说明详尽。

### 3. 实验结果及分析（30.0分）

* 上传实验运行结果截图，并用文字对实验结果进行简单分析。

### 4. 疑难小结（20.0分）

* 总结个人在实验中遇到的问题、解决方法、心得体会等。