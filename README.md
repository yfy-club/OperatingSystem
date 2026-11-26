# 操作系统原理实验

Linux 下操作系统课程实验的完整实现：进程观测与控制、进程间通信（管道、共享内存、消息队列）以及动态分区分配模拟。仓库包含实验讲义、源码、严格编译与故障注入测试，以及由真实运行记录生成的实验报告。

## 实验一览

| 实验 | 主题 | 源码 | 讲义 | 报告 |
| --- | --- | --- | --- | --- |
| 实验一 | 进程观测 | [`src/lab1`](src/lab1) | [`docs/lab1-process-observation.md`](docs/lab1-process-observation.md) | [`reports/lab1-report.md`](reports/lab1-report.md) |
| 实验二 | 进程控制 | [`src/lab2`](src/lab2) | [`docs/lab2-process-control.md`](docs/lab2-process-control.md) | [`reports/lab2-report.md`](reports/lab2-report.md) |
| 实验三 | 进程通信（一）：管道与共享内存 | [`src/lab3`](src/lab3) | [`docs/lab3-ipc-pipe-shared-memory.md`](docs/lab3-ipc-pipe-shared-memory.md) | [`reports/lab3-report.md`](reports/lab3-report.md) |
| 实验四 | 进程通信（二）：消息队列 | [`src/lab4`](src/lab4) | [`docs/lab4-ipc-message-queue.md`](docs/lab4-ipc-message-queue.md) | [`reports/lab4-report.md`](reports/lab4-report.md) |
| 实验五 | 动态分区分配方式的模拟 | [`src/lab5`](src/lab5) | [`docs/lab5-dynamic-partition-allocation.md`](docs/lab5-dynamic-partition-allocation.md) | [`reports/lab5-report.md`](reports/lab5-report.md) |

## 目录结构

```text
.
├── docs/                  # 实验讲义（课程题目）
├── src/                   # 实验源码
│   ├── lab1/              # 进程观测
│   ├── lab2/              # 进程控制
│   ├── lab3/              # 管道与共享内存
│   ├── lab4/              # 消息队列
│   └── lab5/              # 动态分区分配
├── tests/                 # 严格编译、行为回归与故障注入测试
├── reports/
│   ├── templates/         # 报告模板（文字从这里修改）
│   ├── text/              # 实际运行记录与 verification.json
│   ├── shots/             # 由运行记录排版生成的 PNG
│   └── lab*-report.md     # 生成后的五份报告
├── build/                 # 编译输出（不提交）
└── Makefile
```

## 环境要求

Linux、GCC、GNU Make、Python 3、procps 和 util-linux。完整测试需要创建独立的 IPC namespace：普通用户依赖可用的无特权 user namespace，root 则直接使用 `unshare --ipc`；两者都不可用时测试明确失败。

## 快速开始

在仓库根目录执行：

```sh
make all       # 编译 11 个程序，输出到 build/lab1 ... build/lab5
make test      # 严格编译、正常/故障回归，并检查报告是否与源码同步
```

运行程序，例如：

```sh
./build/lab1/process1
(cd build/lab2 && ./fork_exec)   # fork_exec 需要同目录下的 newprog
```

## 报告流水线

```sh
make record    # 重新运行测试，更新 reports/text/ 的实际记录与环境信息
make reports   # 从模板、当前源码和本次运行记录生成五份完整报告
make images    # 将实际记录排版为 reports/shots/ 中的 11 张 PNG
```

修改源码后按 `make record && make reports && make images && make test` 更新和检查。`make test` 不刷新运行记录；记录、报告与代码不一致时检查会失败。

报告文字请编辑 `reports/templates/` 中对应模板，再执行 `make reports`。程序清单由生成器自动读取当前源码，避免复制后过时。

### 图片依赖

`make images` 需要 PyCairo、PyGObject（Pango/PangoCairo）、Pillow 和可用的等宽/CJK 字体。可以自建虚拟环境：

```sh
python3 -m venv reports/text/.venv
reports/text/.venv/bin/pip install pycairo PyGObject pillow
make images
```

Makefile 检测到 `reports/text/.venv/bin/python` 时会优先使用，否则回退到 `python3`，也可以通过 `make images RENDER_PYTHON=/path/to/python` 指定解释器。`reports/text/shot.sh` 是同一排版器的单张入口。

## 测试与安全范围

- 以 `-std=gnu17 -Wall -Wextra -Wpedantic -Wconversion -Werror -O2 -g` 编译全部 11 个程序。
- 验证父子输出次数、双子进程数量、exec 前后 PID、缺失可执行文件、信号退出、wait 被中断、管道短读/短写和 EINTR。
- 在**独立 IPC 命名空间**运行共享内存与固定 key=75 的消息队列，检查正常及注入失败后的清理；预建含哨兵消息的队列，确认程序拒绝冲突且不删除原对象。无特权用户/IPC 命名空间不可用时测试明确失败，不冒充通过，也不回退到删除主机队列。
- 比对首次适应和最佳适应两种算法所有 11 步与独立参考结果；检查链序、完整分区回收、边界、重复释放、申请失败、结点分配/释放计数，以及 2000 次固定种子的操作。
- 无限循环程序只短时采样，测试器作为 subreaper 回收孤儿进程，只清理自身创建的进程组；不广泛 `pkill` 同名进程，也不删除不明归属的 IPC 对象。
- 测试在临时构建目录运行，结束自动清理。没有宣称通过 ASan 或通用泄漏检测器；结点计数与不变量检查仅覆盖所运行的路径。IPC 清理无法应对 SIGKILL、崩溃或断电等不可处理的中断。

## 报告与记录说明

- `reports/text/*.txt` 来自真实运行；无限循环输出丢弃到 `/dev/null`，不写巨型日志。记录中的 `$` 是等价命令展示，`verified`、`IPC check` 等行是测试器核验说明，不是程序本身输出。
- `reports/text/verification.json` 保存复测时间、内核、编译器、编译参数、通过的检查和源码 SHA-256。生成报告前会核对源码，避免用旧记录说明新代码。
- `reports/shots/*.png` 是运行记录的**排版图，不是原始终端截图**。内存分布条由当前输出自动推导。按课程要求正式提交前，请自行运行程序并补充本人终端原始截图，填写姓名、学号，确认日期与个人体会。

## 许可

[MIT](LICENSE)
