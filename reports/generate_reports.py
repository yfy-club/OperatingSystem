#!/usr/bin/env python3
"""由模板、当前源码和已采集环境生成报告；--check 只检查、不写文件。"""
import argparse
import hashlib
import json
from pathlib import Path
import re

REPORTS = Path(__file__).resolve().parent
LAB = REPORTS.parent


def main(check):
    meta = json.loads((REPORTS / "text/verification.json").read_text(encoding="utf-8"))
    for path, expected in meta["source_sha256"].items():
        if hashlib.sha256((LAB / path).read_bytes()).hexdigest() != expected:
            raise SystemExit(f"源码在采集后变化：{path}；请先 make record")
    values = dict(meta, check_count=str(len(meta["results"])))
    def expand(match):
        key = match[1]
        if key.startswith("code:"):
            path = key[5:]
            return f"### `{path}`\n\n```c\n{(LAB / path).read_text(encoding='utf-8').rstrip()}\n```"
        if key.startswith("text:"):
            return "```text\n" + (REPORTS / "text" / key[5:]).read_text(encoding="utf-8").rstrip() + "\n```"
        return str(values[key])
    for template in sorted((REPORTS / "templates").glob("*.md")):
        content = re.sub(r"\{\{([^{}]+)\}\}", expand, template.read_text(encoding="utf-8"))
        output = REPORTS / template.name
        if check:
            if not output.exists() or output.read_text(encoding="utf-8") != content:
                raise SystemExit(f"报告未同步：{output.name}；请运行 make reports")
        else:
            output.write_text(content, encoding="utf-8")
        print(("checked: " if check else "generated: ") + output.name)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--check", action="store_true")
    main(parser.parse_args().check)
