#!/usr/bin/env bash
# 兼容旧的单张排版图入口：不声称生成真实终端截图。
set -euo pipefail
if [[ $# -ne 3 ]]; then
    echo 'usage: shot.sh <title> <textfile> <out.png>' >&2
    exit 2
fi
script_dir=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)
python=${RENDER_PYTHON:-python3}
exec "$python" - "$script_dir" "$@" <<'PY'
import sys
from pathlib import Path
sys.path.insert(0, sys.argv[1])
from render_all import render_card
_, _, title, body, out = sys.argv
render_card(title, Path(body).read_text(encoding="utf-8").splitlines(), out)
PY
