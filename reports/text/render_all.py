#!/usr/bin/env python3
"""
OS Lab Report Transcript Renderer (not a desktop screenshot)
Uses PyCairo + PangoCairo for vector typography and Pillow for drop shadows.
"""

import os
import re
import html
from pathlib import Path
import cairo
import gi

gi.require_version('Pango', '1.0')
gi.require_version('PangoCairo', '1.0')
from gi.repository import Pango, PangoCairo
from PIL import Image, ImageFilter

TEXT_DIR = Path(__file__).resolve().parent
SHOTS_DIR = TEXT_DIR.parent / "shots"

def highlight_line(line):
    esc = html.escape(line)
    if not esc.strip():
        return " "
        
    # Check if pure comment line or inline comment
    if '#' in esc and not esc.startswith('$'):
        parts = esc.split('#', 1)
        return highlight_line_content(parts[0]) + f'<span foreground="#565f89" style="italic">#{parts[1]}</span>'
    return highlight_line_content(esc)

def highlight_line_content(esc):
    # Shell prompt
    if esc.startswith('$ '):
        cmd_part = esc[2:]
        tokens = cmd_part.split(' ')
        res = ['<span foreground="#7aa2f7" weight="bold">$</span>']
        first = True
        for tok in tokens:
            if not tok:
                continue
            if first:
                res.append(f'<span foreground="#7dcfff" weight="bold">{tok}</span>')
                first = False
            elif tok.startswith('-'):
                res.append(f'<span foreground="#bb9af7">{tok}</span>')
            elif tok in ['|', '>', '&', ';']:
                res.append(f'<span foreground="#ff9e64" weight="bold">{tok}</span>')
            elif tok.startswith('#'):
                res.append(f'<span foreground="#565f89" style="italic">{tok}</span>')
            else:
                res.append(f'<span foreground="#c0caf5">{tok}</span>')
        return ' '.join(res)
    elif esc == '$':
        return '<span foreground="#7aa2f7" weight="bold">$</span>'

    # Step markings
    if esc.startswith('['):
        esc = re.sub(r'(\[step \d+\])', r'<span foreground="#e0af68" weight="bold">\1</span>', esc)
        esc = re.sub(r'(\[\d+\]\+?\s+Terminated)', r'<span foreground="#f7768e" weight="bold">\1</span>', esc)
        esc = re.sub(r'(\[\d+\]\s+\d+)', r'<span foreground="#e0af68">\1</span>', esc)

    # Process / Top headers
    if 'PID' in esc and ('COMMAND' in esc or 'CMD' in esc):
        return f'<span foreground="#7dcfff" weight="bold">{esc}</span>'
    if 'key' in esc and 'msqid' in esc:
        return f'<span foreground="#7dcfff" weight="bold">{esc}</span>'

    # Top summary headers
    if esc.startswith('top -') or esc.startswith('Tasks:') or esc.startswith('%Cpu(s):') or esc.startswith('MiB '):
        esc = re.sub(r'([0-9]+\.?[0-9]*)', r'<span foreground="#73daca">\1</span>', esc)
        return f'<span foreground="#9aa5ce">{esc}</span>'

    # Section titles
    if esc.startswith('====='):
        return f'<span foreground="#7aa2f7" weight="bold">{esc}</span>'
    if esc.startswith('initial state:'):
        return f'<span foreground="#73daca" weight="bold">{esc}</span>'

    # Specific patterns
    esc = re.sub(r'\b(parent)\b', r'<span foreground="#7aa2f7" weight="bold">\1</span>', esc)
    esc = re.sub(r'\b(child)\b', r'<span foreground="#9ece6a" weight="bold">\1</span>', esc)
    esc = re.sub(r'\b(server)\b', r'<span foreground="#7aa2f7" weight="bold">\1</span>', esc)
    esc = re.sub(r'\b(client)\b', r'<span foreground="#9ece6a" weight="bold">\1</span>', esc)
    esc = re.sub(r'(-&gt; allocated)', r'<span foreground="#9ece6a" weight="bold">\1</span>', esc)
    esc = re.sub(r'(-&gt; released)', r'<span foreground="#f7768e" weight="bold">\1</span>', esc)
    esc = re.sub(r'\b(free chain:)', r'<span foreground="#7aa2f7" weight="bold">\1</span>', esc)
    esc = re.sub(r'(\[\d+K,\s*\d+K\))', r'<span foreground="#bb9af7">\1</span>', esc)
    esc = re.sub(r'(\[\d+K,\d+K\])', r'<span foreground="#bb9af7">\1</span>', esc)
    esc = re.sub(r'(\(total \d+K free\))', r'<span foreground="#73daca">\1</span>', esc)
    esc = re.sub(r'\b(new program\.)\b', r'<span foreground="#e0af68" weight="bold">\1</span>', esc)
    esc = re.sub(r'\b(exit code 0)\b', r'<span foreground="#9ece6a" weight="bold">\1</span>', esc)
    esc = re.sub(r'\b(message queue created)\b', r'<span foreground="#73daca" weight="bold">\1</span>', esc)
    esc = re.sub(r'\b(shared memory removed|message queue removed)\b', r'<span foreground="#73daca">\1</span>', esc)

    return f'<span foreground="#c0caf5">{esc}</span>'

def draw_memory_bar(ctx, x, y, width, height, blocks, title_text, total_mem=640):
    """Draw a vector memory partition bar below terminal output for Lab 5."""
    # Divider
    ctx.set_source_rgba(0.25, 0.28, 0.38, 0.6)
    ctx.set_line_width(1.0)
    ctx.move_to(x - 10, y - 40)
    ctx.line_to(x + width + 10, y - 40)
    ctx.stroke()

    # Section Title
    tlayout = PangoCairo.create_layout(ctx)
    tlayout.set_font_description(Pango.FontDescription("JetBrains Mono, Noto Sans CJK SC 10 Bold"))
    tlayout.set_markup(f'<span foreground="#7dcfff">{title_text}</span>')
    ctx.move_to(x, y - 26)
    PangoCairo.show_layout(ctx, tlayout)

    # Base track
    ctx.set_source_rgb(0.11, 0.12, 0.17)
    ctx.rectangle(x, y, width, height)
    ctx.fill()

    desc_reg = Pango.FontDescription("JetBrains Mono, Noto Sans CJK SC 9 Bold")
    desc_small = Pango.FontDescription("JetBrains Mono, Noto Sans CJK SC 8 Bold")
    blayout = PangoCairo.create_layout(ctx)

    tick_desc = Pango.FontDescription("JetBrains Mono 8")
    tick_layout = PangoCairo.create_layout(ctx)
    tick_layout.set_font_description(tick_desc)

    frag_callout = None

    for start, size, label, is_free, is_frag in blocks:
        bx = x + (start / total_mem) * width
        bw = (size / total_mem) * width

        if is_frag:
            ctx.set_source_rgb(0.97, 0.46, 0.56)  # Red warning
            text_color = "#151620"
            frag_callout = (bx + bw / 2, y)
        elif is_free:
            ctx.set_source_rgb(0.18, 0.32, 0.28)  # Green/Teal
            text_color = "#9ece6a"
        else:
            ctx.set_source_rgb(0.22, 0.30, 0.48)  # Slate blue
            text_color = "#c0caf5"

        ctx.rectangle(bx + 1, y, max(bw - 2, 2), height)
        ctx.fill()

        # Outline
        ctx.set_source_rgba(0.4, 0.5, 0.7, 0.35)
        ctx.set_line_width(1.0)
        ctx.rectangle(bx + 1, y, max(bw - 2, 2), height)
        ctx.stroke()

        # Adaptive text label: try full, then abbreviated, then size only
        candidates = [
            (desc_reg, label),
            (desc_small, label.replace("作业", "J").replace("空闲 ", "空")),
            (desc_small, f"{size}K")
        ]
        
        placed = False
        for fdesc, text in candidates:
            blayout.set_font_description(fdesc)
            blayout.set_markup(f'<span foreground="{text_color}">{text}</span>')
            tw, th = blayout.get_pixel_size()
            if tw <= bw - 2:
                ctx.move_to(bx + (bw - tw) / 2, y + (height - th) / 2)
                PangoCairo.show_layout(ctx, blayout)
                placed = True
                break

        # Start address tick (avoid collision near 640K)
        if (total_mem - start) / total_mem * width >= 28:
            tick_layout.set_markup(f'<span foreground="#565f89">{start}K</span>')
            ctx.move_to(bx - 3, y + height + 4)
            PangoCairo.show_layout(ctx, tick_layout)

    # 640K tick
    tick_layout.set_markup('<span foreground="#565f89">640K</span>')
    ctx.move_to(x + width - 18, y + height + 4)
    PangoCairo.show_layout(ctx, tick_layout)

    # If fragment, draw arrow marker above pointing rightwards
    if frag_callout:
        fx, fy = frag_callout
        flayout = PangoCairo.create_layout(ctx)
        flayout.set_font_description(Pango.FontDescription("JetBrains Mono, Noto Sans CJK SC 8 Bold"))
        flayout.set_markup('<span foreground="#f7768e">外部碎片 (10K) →</span>')
        tw, th = flayout.get_pixel_size()
        ctx.move_to(fx - tw - 6, fy - 16)
        PangoCairo.show_layout(ctx, flayout)


def render_card(title, text_lines, output_png, mem_blocks=None, mem_title=""):
    text_lines = ["运行记录排版图（非原始终端截图）", ""] + text_lines
    PAD = 24
    WIN_PAD = 20
    TITLE_H = 38
    FONT_SIZE = 13
    LINE_H = 22
    EXTRA_MEM_H = 110 if mem_blocks else 0

    # Measure width
    dummy_surf = cairo.ImageSurface(cairo.FORMAT_ARGB32, 10, 10)
    dctx = cairo.Context(dummy_surf)
    layout = PangoCairo.create_layout(dctx)
    desc = Pango.FontDescription(f"JetBrains Mono {FONT_SIZE}")
    layout.set_font_description(desc)

    max_w = 720
    for l in text_lines:
        layout.set_text(l, -1)
        w, _ = layout.get_pixel_size()
        if w > max_w:
            max_w = w

    term_w = max_w + WIN_PAD * 2
    term_h = TITLE_H + len(text_lines) * LINE_H + WIN_PAD * 2 + EXTRA_MEM_H

    canvas_w = term_w + PAD * 2
    canvas_h = term_h + PAD * 2

    # Draw window
    win_surf = cairo.ImageSurface(cairo.FORMAT_ARGB32, term_w, term_h)
    wctx = cairo.Context(win_surf)

    # Window body
    R = 10
    wctx.new_sub_path()
    wctx.arc(term_w - R, R, R, -1.5707963, 0)
    wctx.arc(term_w - R, term_h - R, R, 0, 1.5707963)
    wctx.arc(R, term_h - R, R, 1.5707963, 3.1415926)
    wctx.arc(R, R, R, 3.1415926, 4.7123889)
    wctx.close_path()
    wctx.set_source_rgb(0.094, 0.102, 0.149)  # #181a26
    wctx.fill_preserve()
    wctx.set_source_rgba(0.25, 0.28, 0.38, 0.7)
    wctx.set_line_width(1.0)
    wctx.stroke()

    # Window title bar
    wctx.save()
    wctx.new_sub_path()
    wctx.arc(term_w - R, R, R, -1.5707963, 0)
    wctx.line_to(term_w, TITLE_H)
    wctx.line_to(0, TITLE_H)
    wctx.arc(R, R, R, 3.1415926, 4.7123889)
    wctx.close_path()
    wctx.set_source_rgb(0.082, 0.086, 0.125)  # #151620
    wctx.fill()
    wctx.restore()

    # Divider line
    wctx.set_source_rgba(0.2, 0.22, 0.32, 0.8)
    wctx.set_line_width(1.0)
    wctx.move_to(0, TITLE_H)
    wctx.line_to(term_w, TITLE_H)
    wctx.stroke()

    # Traffic lights
    colors = [(1.0, 0.33, 0.33), (1.0, 0.72, 0.42), (0.31, 0.88, 0.48)]
    for i, col in enumerate(colors):
        wctx.arc(20 + i * 18, TITLE_H / 2, 5.5, 0, 6.2831853)
        wctx.set_source_rgb(*col)
        wctx.fill()

    # Title text
    tlayout = PangoCairo.create_layout(wctx)
    tdesc = Pango.FontDescription("JetBrains Mono, Noto Sans CJK SC 10 Bold")
    tlayout.set_font_description(tdesc)
    tlayout.set_markup(f'<span foreground="#9aa5ce">{html.escape(title)}</span>')
    tw, th = tlayout.get_pixel_size()
    wctx.move_to((term_w - tw) / 2, (TITLE_H - th) / 2)
    PangoCairo.show_layout(wctx, tlayout)

    # Render terminal lines
    llayout = PangoCairo.create_layout(wctx)
    ldesc = Pango.FontDescription(f"JetBrains Mono {FONT_SIZE}")
    llayout.set_font_description(ldesc)

    curr_y = TITLE_H + WIN_PAD
    for l in text_lines:
        markup = highlight_line(l)
        try:
            llayout.set_markup(markup)
        except Exception:
            llayout.set_text(l, -1)
        wctx.move_to(WIN_PAD, curr_y)
        PangoCairo.show_layout(wctx, llayout)
        curr_y += LINE_H

    # Render Memory Bar if requested
    if mem_blocks:
        mem_y = curr_y + 40
        draw_memory_bar(wctx, WIN_PAD, mem_y, term_w - WIN_PAD * 2, 34, mem_blocks, mem_title)

    # Pillow drop shadow composition
    buf = win_surf.get_data()
    win_img = Image.frombuffer("RGBA", (term_w, term_h), buf, "raw", "BGRA", 0, 1)

    canvas = Image.new("RGBA", (canvas_w, canvas_h), (18, 20, 29, 255))
    shadow_mask = Image.new("RGBA", (canvas_w, canvas_h), (0, 0, 0, 0))
    s_box = Image.new("RGBA", (term_w, term_h), (0, 0, 0, 160))
    shadow_mask.paste(s_box, (PAD, PAD + 5), win_img.split()[3])
    shadow_blur = shadow_mask.filter(ImageFilter.GaussianBlur(radius=12))

    canvas.paste(shadow_blur, (0, 0), shadow_blur)
    canvas.paste(win_img, (PAD, PAD), win_img)

    canvas.save(output_png)
    print(f"Rendered: {os.path.basename(output_png)} ({canvas_w}x{canvas_h})")

def main():
    os.makedirs(SHOTS_DIR, exist_ok=True)

    tasks = [
        ("lab1_task1.txt", "lab1_task1.png", "Terminal - 实验一 任务 (1)：进程观测与终止 (process1)", None, ""),
        ("lab1_task2.txt", "lab1_task2.png", "Terminal - 实验一 任务 (2)：父子进程并发交错输出 (process2)", None, ""),
        ("lab1_task3.txt", "lab1_task3.png", "Terminal - 实验一 变体一：父进程退出后孤儿进程观测 (process3)", None, ""),
        ("lab1_task4.txt", "lab1_task4.png", "Terminal - 实验一 变体二：父子双死循环与 CPU 核心占用 (process4)", None, ""),
        ("lab2_task1.txt", "lab2_task1.png", "Terminal - 实验二 任务一：fork 创建双子进程并发执行 (fork_children)", None, ""),
        ("lab2_task2.txt", "lab2_task2.png", "Terminal - 实验二 任务二：execvp 更换执行代码与 wait 同步 (fork_exec)", None, ""),
        ("lab3_pipe.txt", "lab3_pipe.png", "Terminal - 实验三 任务一：匿名管道父子通信与同步 (pipe_comm)", None, ""),
        ("lab3_shm.txt", "lab3_shm.png", "Terminal - 实验三 任务二：共享存储区通信与生命周期 (shm_comm)", None, ""),
        ("lab4.txt", "lab4.png", "Terminal - 实验四：System V 消息缓冲队列客户/服务器通信 (msgq)", None, ""),
    ]

    # 从实际记录推导最终分布，不再手写另一套分区数据。
    for name, title in (("first", "First Fit"), ("best", "Best Fit")):
        transcript = (TEXT_DIR / f"lab5_{name}.txt").read_text(encoding="utf-8")
        jobs = {}
        for line in transcript.splitlines():
            match = re.search(r"job (\d+) requests .*allocated \[(\d+)K,(\d+)K\)", line)
            if match:
                job, start, end = map(int, match.groups())
                jobs[job] = (start, end - start)
            match = re.search(r"job (\d+) releases", line)
            if match:
                del jobs[int(match[1])]
        final_chain = [line for line in transcript.splitlines() if "free chain:" in line][-1]
        blocks = [(int(start), int(size), f"空闲 {size}K", True, False)
                  for start, size in re.findall(r"\[(\d+)K,(\d+)K\]", final_chain)]
        blocks += [(start, size, f"作业{job} {size}K", False, False)
                   for job, (start, size) in jobs.items()]
        blocks.sort()
        assert sum(block[1] for block in blocks) == 640
        assert all(blocks[i][0] + blocks[i][1] == blocks[i + 1][0]
                   for i in range(len(blocks) - 1))
        tasks.append((f"lab5_{name}.txt", f"lab5_{name}.png", f"实验五运行记录排版 - {title}",
                      blocks, f"根据本次记录绘制的最终分布 ({title})"))

    for txt_file, png_file, title, blocks, mem_title in tasks:
        txt_path = os.path.join(TEXT_DIR, txt_file)
        png_path = os.path.join(SHOTS_DIR, png_file)
        if not os.path.exists(txt_path):
            print(f"Skipping {txt_file}: not found")
            continue
        lines = open(txt_path, "r", encoding="utf-8").read().splitlines()
        render_card(title, lines, png_path, blocks, mem_title)

if __name__ == "__main__":
    main()
