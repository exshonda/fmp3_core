# cfg_py — Python cfg

pristine の `cfg/`（旧コンフィギュレータ）の代替。CMake から呼ばれ、システムコンフィギュレーション
ファイルを処理して生成物を出力する。pristine の `cfg/` は参照しない（DIVERGENCE_MAP 参照）。

エンジン本体（`cfg.py`／`pass1.py`／`pass2.py`／`gen_file.py`／`srecord.py`）は asp3_core 1.7.1 の
Python cfg エンジンを移植したもの（計画B、2026-07-19）。以前（計画A）は pristine の `cfg/cfg.rb`
へ委譲する薄いシムだったが、`kernel/`・`arch/`・`target/` 配下の `.trb`（Ruby）テンプレート
30ファイル・3963行を `.py` へ全数移植したうえで、pristine の Ruby cfg との差分等価性検査
（`tools/cfg_equivalence.sh`）で生成物のバイト一致を確認して切り替えた（AGENTS.md §2 規則3を
文言・精神の両方で満たす。詳細は `DIVERGENCE_MAP.md` の「解消済み事項」を参照）。Ruby は
`tools/cfg_equivalence.sh`（CMake 外）からのみ呼ばれるオラクルとして残る。
