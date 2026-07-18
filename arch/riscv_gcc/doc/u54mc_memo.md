# U54-MC に関するメモ
- 作成者: 本田晋也
- 最終更新: 2024年04月12日


# メモの位置づけ

このメモは，Polafire SoC に対してTOPPERSカーネルをポーティングするにあたって，U54-MCのアーキテクチャの関連事項をまとめたものである．


# 参考文献

[U54-MC] SiFive U54-MC Core Complex Manual v1p0


# U54-MC のバージョン
- U54-MCのマニュアルは以下の2種類が見つかる
    - SiFive U54-MC Core Complex Manual v1p0 2017
    - SiFive U54-MC Core Complex Manual 21G2.01.00 2021
- 2021年度版はタイマとソフトウエア以外のローカル割込みがなくなっている．
    - 残っている図がある..
- Polafireがどちらの版なのかは明記されていないが，おそらく2017年度版をベースとしている．


# コア構成

## U54 x 4 : RV64GC : RV64IMAFDC
    - 32KB I/D cache
    - Hardware Perfomance Monitor

## E51 x 1 : RV64IMAC
    - 4KB I cache


# 割込み

## ローカル割込み
- ローカル割込みとして 16-63番に48個の割込みが接続可能となっている
- mie/mip/ベクターテーブルが拡張されている．

## 割込みコントローラ
- CLIC
    - Software Interrupt の発生と Machine Timer 機能を持つコントローラ
- PLIC
    - グローバル割込みを各hartに送る割込みコントローラ
    - 優先度 : 7 レベル
        - 0が最低レベル値が大きいほど高優先度．

## hartid と PLICのコンテキストINDEXの関係

hartid と PLICのM-mode割込みのコンテキストINDEXの関係は次の通りである．

| CPU | hartid | コンテキストINDEX |
| ---- | ---- | ---- |
| E51   | 0 | 0|
| U54_1 | 1 | 1|
| U54_2 | 2 | 3|
| U54_3 | 3 | 5|
| U54_4 | 4 | 7|


# その他

## 排他制御命令
- ll/sc はキャッシュ可能な領域のみで実行可能．

以上．
