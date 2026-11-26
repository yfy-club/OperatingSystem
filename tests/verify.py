#!/usr/bin/env python3
"""严格编译、行为/故障回归及可选运行记录采集（Linux）。不运行旧的同名二进制。"""
import argparse
import collections
import ctypes
import datetime
import hashlib
import json
import os
from pathlib import Path
import re
import signal
import subprocess
import sys
import tempfile
import time

LAB = Path(__file__).resolve().parents[1]
FLAGS = ["-std=gnu17", "-Wall", "-Wextra", "-Wpedantic", "-Wconversion", "-Werror", "-O2", "-g"]
RESULTS = []
RECORDS = {}


def passed(name):
    RESULTS.append(name)


def run(args, cwd=None, env=None, timeout=12, ok=True):
    p = subprocess.Popen([str(x) for x in args], cwd=cwd, env=env,
                         stdout=subprocess.PIPE, stderr=subprocess.PIPE,
                         text=True, start_new_session=True)
    try:
        out, err = p.communicate(timeout=timeout)
    except subprocess.TimeoutExpired:
        os.killpg(p.pid, signal.SIGKILL)
        p.communicate()
        raise AssertionError(f"timeout: {args}")
    if ok and p.returncode:
        raise AssertionError(f"{args}: exit {p.returncode}\n{out}\n{err}")
    return p.returncode, out, err


def fault(build, name, mode):
    env = dict(os.environ, LAB_FAULT=mode, LC_ALL="C")
    return run([build / "fault" / name], cwd=build / "lab2", env=env, ok=False)


def ipc_empty():
    # 只在新建 IPC 命名空间调用；不探查/删除用户已有的主机对象。
    for table in ("shm", "msg"):
        assert len(Path(f"/proc/sysvipc/{table}").read_text().splitlines()) == 1


def ipc_suite(build):
    ipc_empty()
    _, out, _ = run([build / "lab3/shm_comm"])
    lines = out.splitlines()
    assert len(lines) == 4 and "512-byte" in lines[0]
    assert lines[1].split("wrote: ")[1] == lines[2].split("read : ")[1]
    ipc_empty()
    RECORDS["lab3_shm.txt"] = "$ ./shm_comm # isolated IPC namespace\n" + out + "IPC check: no shared memory segments remain\n"
    passed("shared memory: write/read agreement and removal")
    _, out, _ = run([build / "lab4/msgq"])
    assert len(out.splitlines()) == 5
    server, child = re.search(r"server (\d+): serving for client (\d+)", out).groups()
    assert f"client {child}: receive reply from {server}" in out
    assert out.index("serving for client") < out.index("receive reply from")
    ipc_empty()
    RECORDS["lab4.txt"] = "$ ./msgq # isolated IPC namespace\n" + out + "IPC check: no message queues remain\n"
    passed("message queue: request/reply PIDs, display order and removal")

    # 预建一个含哨兵消息的 key=75 队列，验证新程序不会碰它。
    libc = ctypes.CDLL(None, use_errno=True)
    class Message(ctypes.Structure):
        _fields_ = [("mtype", ctypes.c_long), ("mtext", ctypes.c_char * 32)]
    qid = libc.msgget(75, 0o1000 | 0o2000 | 0o600)
    assert qid >= 0
    try:
        msg = Message(99, b"keep-existing-queue")
        assert libc.msgsnd(qid, ctypes.byref(msg), 20, 0) == 0
        rc, out, err = run([build / "lab4/msgq"], ok=False)
        assert rc != 0 and "existing queue is left untouched" in err
        assert libc.msgget(75, 0o600) == qid
        msg = Message()
        assert libc.msgrcv(qid, ctypes.byref(msg), 32, 99, 0o4000) == 20
        assert msg.mtext == b"keep-existing-queue"
    finally:
        assert libc.msgctl(qid, 0, None) == 0
    ipc_empty()
    passed("message queue: existing key and sentinel preserved")
    for name, mode in [("shm_comm", "fork"), ("shm_comm", "shmat"),
                       ("shm_comm", "parent_shmat"), ("shm_comm", "child_exit"),
                       ("msgq", "fork"), ("msgq", "msgsnd"), ("msgq", "child_exit")]:
        rc, _, err = fault(build, name, mode)
        assert rc != 0, (name, mode, err)
        ipc_empty()
        passed(f"{name}: {mode} error propagated, IPC cleaned")
    return {"results": RESULTS, "records": RECORDS}


def stop_group(p):
    try:
        os.killpg(p.pid, signal.SIGTERM)
    except ProcessLookupError:
        pass
    p.wait(timeout=3)
    # subreaper 收养的实验子进程，也仅按本次进程组回收。
    deadline = time.monotonic() + 3
    while True:
        try:
            pid, _ = os.waitpid(-p.pid, os.WNOHANG)
        except ChildProcessError:
            return
        if not pid:
            assert time.monotonic() < deadline, "child cleanup timeout"
            time.sleep(0.01)


def observe(build, number):
    p = subprocess.Popen([build / f"lab1/process{number}"], stdout=subprocess.DEVNULL,
                         stderr=subprocess.PIPE, start_new_session=True, text=True)
    try:
        if number == 3:
            assert p.wait(timeout=3) == 0 # 回收原父进程后再观察孤儿
        time.sleep(0.5)
        _, ps, _ = run(["ps", "--sid", p.pid, "-o", "pid,ppid,stat,pcpu,pmem,rss,comm"])
        rows = [line.split() for line in ps.splitlines()[1:]]
        assert len(rows) == (2 if number == 4 else 1), ps
        assert all(row[-1] == f"process{number}" for row in rows)
        if number == 3:
            assert int(rows[0][1]) == os.getpid() # 此测试器显式作为 subreaper
        if number == 4:
            assert any(int(row[1]) == p.pid for row in rows)
        record = (f"$ ./process{number} > /dev/null # harness starts a new session\n"
                  f"started PID = {p.pid}, process group = {p.pid}\n"
                  f"$ ps --sid {p.pid} -o pid,ppid,stat,pcpu,pmem,rss,comm\n" + ps)
        if number == 1:
            _, top, _ = run(["top", "-b", "-n", "1", "-p", p.pid], env=dict(os.environ, LC_ALL="C"))
            record += f"$ LC_ALL=C top -b -n 1 -p {p.pid}\n" + top
        if number == 3:
            record += f"original parent exited; child adopted by test subreaper PID {os.getpid()}\n"
    finally:
        stop_group(p)
        p.stderr.close()
    ps = subprocess.run(["ps", "--sid", str(p.pid), "-o", "pid="], capture_output=True, text=True)
    assert not ps.stdout.strip()
    record += f"$ kill -TERM -- -{p.pid}; wait/reap only this test group\nNo processes remain in this session.\n"
    RECORDS[f"lab1_task{number}.txt"] = record
    passed(f"process{number}: bounded observation, group cleanup")


def allocation_reference(output):
    sequence = [(1, 1, 130), (2, 1, 60), (3, 1, 100), (2, 0, 60),
                (4, 1, 200), (3, 0, 100), (1, 0, 130), (5, 1, 140),
                (6, 1, 60), (7, 1, 50), (6, 0, 60)]
    sections = output.strip().split("\n\n")
    assert len(sections) == 2
    for best, section in enumerate(sections):
        chains = re.findall(r"free chain:(.*)  \(total (\d+)K free\)", section)
        assert len(chains) == 12
        free, jobs = [(0, 640)], {}
        def check(index):
            chain = sorted(free, key=(lambda x: (x[1], x[0])) if best else (lambda x: x[0]))
            actual = [(int(a), int(s)) for a, s in re.findall(r"\[(\d+)K,(\d+)K\]", chains[index][0])]
            assert actual == chain, (best, index, actual, chain)
            assert int(chains[index][1]) == sum(s for _, s in free)
        check(0)
        for step, (job, request, size) in enumerate(sequence, 1):
            if request:
                candidates = [block for block in free if block[1] >= size]
                start, length = min(candidates, key=(lambda x: (x[1], x[0])) if best else (lambda x: x[0]))
                free.remove((start, length))
                if length > size: free.append((start + size, length - size))
                jobs[job] = (start, size)
                assert f"allocated [{start}K,{start + size}K)" in section.split("[step")[step]
            else:
                free.append(jobs.pop(job))
                merged = []
                for start, length in sorted(free):
                    if merged and sum(merged[-1]) == start:
                        previous, previous_size = merged.pop()
                        merged.append((previous, previous_size + length))
                    else: merged.append((start, length))
                free = merged
            check(step)
    return sections


def main(record):
    # PR_SET_CHILD_SUBREAPER：使 process3 的孤儿由测试器接管并回收，避免遗留僵尸。
    assert ctypes.CDLL(None, use_errno=True).prctl(36, 1, 0, 0, 0) == 0
    base = Path("/tmp/opencode")
    with tempfile.TemporaryDirectory(prefix="os-labs-", dir=base if base.is_dir() else None) as tmp:
        build = Path(tmp)
        sources = sorted((LAB / "src").glob("lab*/*.c"))
        for source in sources:
            target = build / source.parent.name / source.stem
            target.parent.mkdir(exist_ok=True)
            run(["gcc", *FLAGS, source, "-o", target])
        passed(f"strict compilation: {len(sources)} programs, zero warnings")
        (build / "fault").mkdir()
        wraps = ["fork", "wait", "read", "write", "shmat", "msgsnd", "malloc"]
        for name in ("process2", "process3", "process4", "fork_children", "fork_exec", "pipe_comm", "shm_comm", "msgq", "alloc"):
            source = next(s for s in sources if s.stem == name)
            run(["gcc", *FLAGS, source, LAB / "tests/faults.c", "-o", build / "fault" / name,
                 *[f"-Wl,--wrap={symbol}" for symbol in wraps]])
        run(["gcc", *FLAGS, LAB / "tests/alloc_test.c", "-o", build / "alloc_test"])
        _, out, _ = run([build / "alloc_test"])
        passed(out.strip())
        _, out, _ = run([build / "lab1/process2"])
        counts = collections.Counter(out.splitlines())
        assert counts == {"This is parent process": 999, "This is child process": 999}
        RECORDS["lab1_task2.txt"] = ("$ ./process2 # capture until all stdout writers close\n" +
                                      "\n".join(out.splitlines()[:12]) +
                                      "\n... (display excerpt only; counting used full output)\n" +
                                      f"parent lines: {counts['This is parent process']}\nchild lines: {counts['This is child process']}\ntotal lines: {len(out.splitlines())}\n")
        passed("process2: parent 999 + child 999, complete pipe capture")
        for number in (1, 3, 4): observe(build, number)
        _, out, _ = run([build / "lab2/fork_children"])
        counts = collections.Counter(out.splitlines())
        assert len(counts) == 3 and set(counts.values()) == {6}
        assert sum(key.startswith("child:") for key in counts) == 2
        assert sum(key.startswith("parent:") for key in counts) == 1
        RECORDS["lab2_task1.txt"] = "$ ./fork_children\n" + out + "verified: 3 distinct PIDs, 6 lines each, 18 total\n"
        passed("fork_children: exactly two children, 6 lines per PID")
        _, out, _ = run([build / "lab2/fork_exec"], cwd=build / "lab2")
        pid = re.search(r"child (\d+): going", out)[1]
        assert f"newprog pid: {pid}" in out and f"child {pid} finished, exit code 0" in out
        RECORDS["lab2_task2.txt"] = "$ ./fork_exec # cwd contains newprog\n" + out
        passed("exec: same PID before/after, wait exit 0")
        rc, _, err = run([build / "lab2/fork_exec"], cwd=build, ok=False)
        assert rc == 127 and "execvp" in err
        passed("exec: missing newprog reports exit 127")
        _, out, _ = run([build / "lab3/pipe_comm"])
        assert re.fullmatch(r"parent \d+ received: \d+ is sending a message to parent !\n", out)
        RECORDS["lab3_pipe.txt"] = "$ ./pipe_comm\n" + out
        passed("pipe: complete message")
        for name, mode, success in [("process2", "fork", False), ("process3", "fork", False),
                                    ("process4", "fork", False), ("fork_children", "fork", False),
                                    ("fork_children", "fork_second", False), ("fork_exec", "fork", False),
                                    ("fork_exec", "wait_eintr", True), ("fork_children", "wait_eintr", True),
                                    ("fork_exec", "child_signal", False),
                                    ("pipe_comm", "fork", False), ("pipe_comm", "short_io", True),
                                    ("pipe_comm", "child_exit", False),
                                    ("alloc", "malloc_pool", False), ("alloc", "malloc_init", False),
                                    ("alloc", "malloc_release", False)]:
            rc, out, err = fault(build, name, mode)
            assert (rc == 0) == success, (name, mode, out, err)
            if mode == "short_io": assert "is sending a message to parent !" in out
            passed(f"{name}: {mode} handled")
        _, out, _ = run([build / "lab5/alloc"])
        sections = allocation_reference(out)
        RECORDS["lab5_alloc.txt"] = "$ ./alloc\n" + out
        for name, section in zip(("first", "best"), sections):
            RECORDS[f"lab5_{name}.txt"] = "$ ./alloc # selected complete algorithm section\n" + section + "\n"
        passed("allocator: all 11 steps of both algorithms match independent reference")
        _, data, _ = run(["unshare", "--user", "--map-root-user", "--ipc", sys.executable,
                          Path(__file__).resolve(), "--ipc-build", build], timeout=25)
        ipc = json.loads(data)
        RESULTS.extend(ipc["results"])
        RECORDS.update(ipc["records"])
        # 有限程序中父进程已回收的子进程不在此处；此处回收 process2 的孤儿。
        while True:
            try:
                pid, _ = os.waitpid(-1, os.WNOHANG)
            except ChildProcessError:
                break
            assert pid > 0, "unexpected live test child"
    stamp = datetime.datetime.now().astimezone().isoformat(timespec="seconds")
    meta = {"collected_at": stamp, "kernel": run(["uname", "-srmo"])[1].strip(),
            "compiler": run(["gcc", "--version"])[1].splitlines()[0],
            "flags": " ".join(FLAGS), "results": RESULTS,
            "source_sha256": {str(p.relative_to(LAB)): hashlib.sha256(p.read_bytes()).hexdigest()
                              for p in sources}}
    if record:
        text = LAB / "reports/text"
        for name, content in RECORDS.items():
            (text / name).write_text(f"# Captured {stamp}; actual run transcript (not a desktop screenshot)\n" + content)
        (text / "verification.json").write_text(json.dumps(meta, ensure_ascii=False, indent=2) + "\n")
    for name in RESULTS: print("PASS", name)
    print(f"{len(RESULTS)} checks passed; record={'updated' if record else 'unchanged'}")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--record", action="store_true", help="refresh reports/text using actual current runs")
    parser.add_argument("--ipc-build", type=Path, help=argparse.SUPPRESS)
    args = parser.parse_args()
    if args.ipc_build:
        print(json.dumps(ipc_suite(args.ipc_build)))
    else:
        main(args.record)
