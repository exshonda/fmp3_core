#!/usr/bin/env python3
#
#               cfg_py/cfg.py -- 計画A限りの Ruby 委譲シム
#
#  ★これは計画A（本計画）の間だけ存在するシムである。計画Bで
#    asp3_core 1.7.1 の本物の Python cfg エンジンにこのファイルごと
#    差し替える。
#
#  pristine の cfg/cfg.rb をそのまま呼び出す薄いラッパ。cfg.rb と
#  同じコマンドライン引数を受け取り、解釈せずに ruby へそのまま渡し、
#  終了コードを透過する。
#
#  ★引数を解釈しない。解釈すると Ruby 版との差異が生まれる。
#
#  作業ディレクトリと環境変数は呼び出し元から引き継ぐ（cd しない、
#  env を作り直さない）。cfg は cfg1_out.db / cfg1_out.syms /
#  cfg1_out.srec / cfg2_out.db を裸の相対名で読み書きするため、
#  cwd が load-bearing である。
#
import shutil
import subprocess
import sys
from pathlib import Path


def main() -> int:
    ruby = shutil.which("ruby")
    if ruby is None:
        print(
            "cfg_py/cfg.py: 'ruby' not found on PATH. "
            "cfg_py/cfg.py is a Plan-A shim that delegates to the pristine "
            "cfg/cfg.rb and requires a ruby interpreter.",
            file=sys.stderr,
        )
        return 127

    cfg_rb = Path(__file__).resolve().parent.parent / "cfg" / "cfg.rb"
    if not cfg_rb.is_file():
        print(f"cfg_py/cfg.py: {cfg_rb} not found.", file=sys.stderr)
        return 1

    #  sys.argv[1:] をそのまま渡す。解釈しない。
    result = subprocess.run([ruby, str(cfg_rb), *sys.argv[1:]])
    return result.returncode


if __name__ == "__main__":
    sys.exit(main())
