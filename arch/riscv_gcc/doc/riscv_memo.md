# RISC-Vのアーキテクチャに関するメモ
- 作成者: 本田晋也
- 最終更新: 2024年04月12日

# メモの位置づけ

このメモは，RISC-Vのアーキテクチャに関して，TOPPERSカーネルをポーティングするにあたって必要となる事項をまとめたものである．
なお，このメモで対象とするARMアーキテクチャは以下のものである．
- RV32I
- RV64I

# 参考文献

- [RISC-V_ISA_P_1.1] The RISC-V Instruction Set Manual Volume II: Privileged Architecture Document Version 1.10
    - U54-MCで参照しているバージョン．
 - [RISC-V_ISA_U_2.2] The RISC-V Instruction Set Manual Volume I: Unprivileged ISA Document Version 2.2
    - U54-MCで参照しているバージョン．
- [CC] https://riscv.org/wp-content/uploads/2015/01/riscv-calling.pdf
- [RISC-V-C] https://github.com/riscv-non-isa/riscv-c-api-doc/blob/master/riscv-c-api.md
- [PLIC] https://github.com/riscv/riscv-plic-spec/blob/master/riscv-plic.adoc

# 用語集

- WPRI : Reserved Writes Preserve Values, Reads Ignore Values
- WLRL : Write/Read Only Legal Values 
- WARL : Write Any Values, Reads Legal Values 
- CSR  : Control and Status Register
- XLEN : 汎用レジスタの幅

# RISC-V ISA
- 基本的には以下の4種類
    - RV32I     : XLEN=32
    - RV64I     : XLEN=64
    - RV32E     : RV31Iから汎用レジスタ数を半分の16本に変更
    - RV128I    : XLEN=128

- RV32IとRV64Iの違いは次の通りである．
    - レジスタ幅が異なり，レジスタ全体のロード・ストア命令が異なる
        - RV32I : lw/sw
        - RV64I : ld/sd
    - 32bitロード命令lwの意味が異なる
        - RV32I : 符号拡張を行わない（正確には必要ない）
        - RV64I : 符号拡張を行う．符号拡張が必要ない場合はlwuを使用する．

## レジスタ
- x0 : zero : ゼロレジスタ
- x1 : ra : 戻りアドレス
- x2 : sp : スタックポインタ
- x3 : gp : グローバルポインタ
- x4 : tp : スレッドポインター
- x5-7 : t0-2
- x8   : s0/fp
- x9   : s1/fp
- x10-11 : a0-1 : 引数・戻り値
- x12-17 : a2-7 : 引数
- x18-27 : s2-11
- x28-31 : t3-6

## CSR
- プロセッサの振る舞いを変更したり状態を取得するためのレジスタ．
- 専用の命令で読み書きする．
- 各モード毎に用意されているものもある．
- weakly ordered memory-mapped I/O region  として扱われる．
- 代表的なCSR
    - mhartid : hartのIDを保持
    - misa : Machine ISA Register.
    - mstatus : ステータスレジスタ

## 命令

###  RV64I Base Integer Instruction Set [RISC-V_ISA_U]
- Wが付くと32bit板となる．
    - ADDIW
    - SLLIW/SRLIW/SRAIW
    - ADDW/SLLW/SRLW/SUBW/SRAW
    - LD : 64bitロード
    - LW : 32bitロード，符号拡張
    - LWU : 32bitロード，0拡張

### G : RV32/64G Instruction Set Listings 
- 以下のオプションが有効になっている．
- misa bit 8: RV32I/RV64I/RV128I base integer instruction set
- misa bit 12: extension M (integer multiply/divide instructions)
- misa bit 0: extension A (atomic instructions)
- misa bit 5: extension F (single-precision floating point)
- misa bit 3: extension D (double-precision floating point)
- “Zicsr”, Control and Status Register (CSR) Instructions 
- “Zifencei” Instruction-Fetch Fence

### C : misa bit 2: extension C (compressed instructions)    
- 16bit命令のサポート

### “A” Standard Extension for Atomic Instructions [RISC-V_ISA_U]
- 大きく次の2種類が存在
- メモリの特性によって使用可能な命令が異なる．
- どちらの命令も- aq/rlのsuffixを付与することができる．
    - aq(acquire access) : この命令を実行が完了して外部に見えた後に後続の命令を実行する．
    - rl(release access) ：この命令の前のメモリアクセスが完了してからこの命令を実行する．

- Load-Reserved/Store-Conditional Instructions
    - LR.W/D : Dは64bit板
    - SC.W/D : Dは64bit板
    - **予約セット（reservation set）**：LRは対象アドレスを含む「予約セット」に予約を
      作り、SCはその予約が有効な場合のみ成功する。**予約セットの大きさと整列はISA上
      「実装依存」**で、規定されているのは「対象のワード／ダブルワードを含むこと」だけ
      である。実装ではキャッシュライン粒度（あるいはそれより粗い粒度）とするのが一般的
      で、**予約セット内のいずれかのアドレスへの他ハートのストアで予約が失効する**
      （対象アドレス自身への書込みに限らない）。
    - **前進保証（forward progress）**：ISAが eventual success を保証するのは
      「constrained LR/SC sequence」に対してであり、かつ **他のハートが予約セットへ
      ストアしないこと**が条件である。したがって、**別々のロックや無関係な変数が同一の
      予約セット（＝実質的に同一キャッシュライン）に同居していると、それらへの書込みが
      互いの予約を潰し合い、SCの失敗が繰り返される**（前進保証の枠外となる）。
      → ロック変数はキャッシュラインを専有させる必要がある。設計上の対処は
      `riscv_design.md`「ロック変数のキャッシュラインの専有」を参照。
- Atomic Memory Operations
    - AMOSWAP.W/D 
    - AMOADD.W/D
    - AMOAND.W/D
    - AMOOR.W/D
    - AMOXOR.W/D
    - AMOMAX[U].W/D
    - AMOMIN[U].W/D

### “Zicsr”, Control and Status Register (CSR) Instructions [RISC-V_ISA_U]
- CSRRW/I : atomic read/write
- CSRRS/I : atomic read/set bit
    - 下位5ビットのみビットマスクで指定する．
- CSRRC/I : atoic read/clear bit
    - 下位5ビットのみビットマスクで指定する．
- CSRへのアクセスオーダー

### Counters [RISC-V_ISA_U]
- 最大32個の64bitカウンタを持つ
- RDCYCLE : - cycleカウンタを読む
- RDTIME : timeカウンタを読む
- RDINSTRET : instretカウンタを読む

### “F” Standard Extension for Single-Precision Floating-Point/“D” Standard Extension for Double-Precision Floating-Point
- レジスタ
    - f0-f31
        - F拡張 : FLEN=32
        - D拡張 : FLEN=64
    - fcsr : Floating-Point Control and Status Register
        - 32bit
        - ビット4-0はfflagsとしてアクセス可能
            - 例外発生時の状況がセットされる
        - ビット7-5はfrmとしてアクセス可能
            - 丸めモードを設定
- mstatus.FS
    - Intial : リセット時の値
    - Clean : Initialと値が異なるがcontext swap時の値
    - Dirty : context switch から値が変わっている．
 
## RVWMO(RISC-V Weak Memory Ordering) Memory Consistency Model [RISC-V_ISA_U]
- RISV-V のメモリモデル
- コア内でのメモリアクセス順は保証される
- 他のコアから別の順序でメモリアクセスしているように観測される
- マルチスレッドのコードは明示的な同期が必要
- FENCE
    - メモリやIOアクセスが他のhartsから見えるように保証する命令．
    - fence iorw, iorw
        - 第1オペランド : 先行処理 この命令の前の処理
        - 第2オペランド : 後行処理 この命令の後の処理
        - 先行処理 が 後行処理の前に終了することを保証する．
        - オペランドに何も指定しないと，fence iorw, iorw と同等になる．

- FENCE.I : [RISC-V_ISA_U] “Zifencei” Instruction-Fetch Fence
    - 命令メモリの書き込みと読み込みを保証


# 実行モード

## 特権レベル

RISC-Vは，以下の特権レベルを持つ．
- User-mode         ユーザプログラムを実行するモード，非特権レベル
- Supervisor-mode   OSを実行するレベル（Linux等を想定），特権レベル
- Machine-mode      BIOSを実行するレベル

## mstatus
- プロセッサの実行モードに関連するCSR
- 仮想化のため，現在のモードは取得できない仕様としている．
    - https://forums.sifive.com/t/how-to-determine-the-current-execution-privilege-mode/2823
- MIE : 1 << 3 : M interrupt enable
- SIE : 1 << 1 : S interrupt enable
- MPIE : 1 << 7 : 割り込み発生時のMIE
- SPIE : 1 << 5 : 割り込み発生時のSIE
- SPP  : 1 << 8  : 割り込み前のモード
- MPP  : 3 << 11 : 割り込み前のモード
- SXL  : 3 << 34  : S-modeの32/64切り替え(オプション)
- UXL  : 3 << 32  : U-modeの32/64切り替え(オプション)
- MPRV : 1 << 17  : MPPのモードの権限でロード・ストアする．
- MXR : 1 << 19  : 1ならR=1のテーブルを実行できる．
- SUM : 1 << 18  : 1ならu-mode用のメモリにアクセス可能
- MBE/SBE/UBE : 
- TVM : 1 <<  : S-modeのvirtual-memoryの操作をトラップする
- TW :  1 <<  : WFIでトラップを起こす
- TSR : 1 <<  : s-modeでのSRETをトラップする．
- FS : 3 << 13 : RW FLOATの状況 : 0:Off/1:Iniial/2:clean/3:dirty
- VS : 3 << 9  : RW Vectorの状況
- XS : 3 << 9  : RO ユーザー拡張
- SD : 1 << 63 : FS/VS/XSのいずれかがdirtyなのかを示す．

### FS/VS の使い方 [RISC-V_ISA_P]
- コンテキスト保存時
    - Dirty なら保存してCleanに設定する．
    - 誰がDirtyにする -> 命令によって変わりそう P.41[RISC-V_ISA_P]
- コンテキストの復帰時
    - Cleanなら復帰する? ToDo 理由を明らかに
- ビット
- Intial : リセット時の値
- Clean : Initialと値が異なるがcontext swap時の値
- Dirty : context switch から値が変わっている．


# 割込み
- 明確な定義はないが，割込み及び例外をtrapと読んでいる．
- コア直接受け付けるローカル割込みと外部割込みコントローラ経由で受け付ける外部割込みがある（明確な定義はない）．

## ローカル割込み
- Software Interrupt(SI)， Machine Timer(MI)が定義されている．
- 実装毎にローカル割込みを追加する場合がある
- 外部割込みより優先度が高い
- 番号でも優先度が異なり，番号が大きいほど優先度が高い．
    - m-external/m-software/m-timer の順

### Software Interrupt(SI)
- [RISC-V_ISA_P] で出てくる．正確には定義されていない．
- Superviesor と Machine の両バージョンがある．
- Superviesor-mode
    - mipのSSIPのセットまたは外部の割込みコントローラにより発生可能
- Machine-mode
    - 外部回路により発生可能
    - mipはソフトウェアにより設定できない
    - サポートしない場合もある(シングルプロセッサや外部割込みを使う場合)
    - hart間の割込みに使用することもある．
- Software Interrupt を実装しない例もある．
    - 例) ルネサス R9A02G021

### Machine Timer Interrupt(MI)
- [RISC-V_ISA_P] で出てくる．正確には定義されていない．
- 定義されている内容は
    - 割込みが発生する
    - メモリマップレジスタとして，以下のレジスタをプラットホームとして用意することが規定されている．
        - mtime     : カウンタレジスタ
        - mtimecmp  : 比較レジスタ
    - mtimecmpの値がmtime以下であるとMTI (Machine Timer Interrupt) が発生する．
- Superviesor と Machine の両バージョンがある．
- Machine-mode
    - 外部のタイマにより設定される
    - mipはソフトウェアにより設定できない
- Superviesor-mode
    - mipのSSIPのセットにより発生可能.   
    - Machine-mode のソフトからセットする．
- Machine Timer を実装しない例もある．
    - 例) ルネサス R9A02G021

## 外部割込み(EI)
- 外部の割込みコントローラから入力される割込み
- コアとしては1要因のみ存在．
- Superviesor と Machine の両バージョンがある．
- Machine-mode
    - 外部割込みコントローラにより設定される
    - mipはソフトウェアにより設定できない
- Superviesor-mode
    - mipのSSIPのセットにより発生可能.   
    - Machine-mode のソフトからセットする．

## 割込み関連のレジスタ
### mie
指定した割込みを許可するレジスタ
- SSIE : 1 << 1  : S-moe software interrupt enable
- MSIE : 1 << 3  : M-moe software interrupt enable
- STIE : 1 << 5  : S-moe timer interrupt enable
- MTIE : 1 << 7  : M-moe timer interrupt enable
- SEIE : 1 << 9  : S-moe external interrupt enable
- MEIE : 1 << 11 : M-moe external interrupt enable

### mip
指定した割込みのペンディング状態を示す．一部レジスアは書き込みが可能．
- SSIP : 1 << 1  : RW : S-moe software interrupt pending
- MSIP : 1 << 3  : RO : M-moe software interrupt pending
- STIP : 1 << 5  : RW : S-moe timer interrupt pending
- MTIP : 1 << 7  : RO : M-moe timer interrupt pending   : コンペアレジスタに書き込むとクリアされる
- SEIP : 1 << 9  : S-moe external interrupt pending
- MEIP : 1 << 11 : M-moe external interrupt pending

### mcause
受け付けたtrapの要因を取得するレジスタ．
- 内容
    - [62:0] : 受け付けた割込み番号
    - [63] : 1 : 割込み，0 : 割込み以外
- 割込みコード
    - 1 : S-mode software interrupt
    - 3 : M-mode software interrupt
    - 5 : S-mode timer interrupt
    - 7 : M-mode timer interrupt
    - 9 : S-mode external interrupt
    - 11 : M-mode external interrupt
- 例外コード
    - 0 Instruction address misaligned
    - 1 Instruction access fault
    - 2 Illegal instruction
    - 3 Breakpoint
    - 4 Load address misaligned
    - 5 Load access fault
    - 6 Store/AMO address misaligned
    - 7 Store/AMO access fault
    - 8 Environment call from U-mode
    - 9 Environment call from S-mode
    - 10 Reserved
    - 11 Environment call from M-mode
    - 12 Instruction page fault
    - 13 Load page fault
    - 14 Reserved
    - 15 Store/AMO page fault
    - 16-23 Reserved
    - 24-31 Designated for custom use
    - 32-47 Reserved
    - 48-63 Designated for custom use
    - 64- Reserve

### mtvec
- 0:1 : モード,  0 : ダイレクト，1 : 個別ベクター
- 63:2 : アドレス
- ダイレクト
    - 割込み発生時，指定したアドレスを実行する
- 個別ベクター
    - ｀ベクターテーブルは4-XLEN byteのアライメント制約がある[RISC-V_ISA_P_1.1]．
    - 割込み発生時，mtvecに指定したアドレス + 例外コード x 4 のアドレスの命令を実行する

## 割込み受け付けとリターン

- 割込み受付時の振る舞い
    - mstatus.MIE が mstatus.MPIEにコピーされる
    - pcがmepcにコピーされる
    - pcにmtvecの内容がセット
        - vectorが有効の場合
            - 割込みは，mtvec.BASE + 4 x ecption code がセットされる
            - 例外は0ベクタのアドレスを実行
    - 割込み前の状態が mstatus.MPP に設定される．

- 割込みリターンの振る舞い
    - MRETの実行
    - mstatus.MPPのモードにモードが設定される
    - mstatus.MPIEがmstatus.MIEにコピーされる
    - mepcがpcにコピーされる．

## Supervisor Mode Interrupt
- すべてのtrapはS-modeに遷移するよう設定することが可能．
- ローカル割込みはS-modeに遷移するよう設定できない．
- mideleg
    - soft/timer/external をS-modeにする．
- medeleg
    - 例外をS-modeにする
- sstatus/sie/sip/scause/stvec

# 割込みコントローラ
- 割込みコントローラはコア内部に持つローカル割込みコントローラと，外部割込みとしてコアに接続する外部割込みコントローラがある．
- 実装としては，ローカル割込みコントローラのみの場合と，ローカル割込みコントローラと外部割込みコントローラを組み合わせる場合がある．

## ローカル割込みコントローラ
- CLINT (Core-local Interrupt Controller)
    - 元々は各HARTに割込みを発生させるコントローラ
        - HART間割込み（IPI），タイマー
    - [RISC-V_ISA_P]ではコアローカルの割込みコントローラは存在しなかったが，CLICのドキュメントで，Original RISC-V basic local Interrupts (CLINT mode) と名付けられている．
        - Core Local Interrupt Controller (CLIC) RISC-V Privileged Architecture Extension Version 0.9-draft, 5/10/2022
    - Software InterruptとTimerはRISC-V仕様[RISC-V_ISA_P]で定義されている．
    - そのため現状では次の2種類を示す用語となっている
        - コアのローカルの割込み仕様    
            - mip/mie/mstatus
        - 各HARTにSoftware InterruptとMachine Timer Interrupt を発生させる機能．
    - ターゲット毎に拡張されている場合がある
        - U54-MCではローカル割込みが追加されている

- CLIC (Core Local Interrupt Controller)
    - CLINTの後にリリースされた仕様．
    - シングルコアのマイコンで使用されている．
    - 更に拡張したベンダー独自仕様のECLICもある．

## 外部割込みコントローラ
- PLIC (Platform-Level Interrupt Controller)
    - CLINTと組み合わせる外部割込みコントローラ．

- AIA ( Advanced Interrupt Architecture)
    - PLIC に置き換わる新しい割込みコントローラ仕様．
    - https://github.com/riscv/riscv-aia

## PLIC
- 割込みID
    - 1から511
    - 0は外部に接続されていない．
- 優先度レジスタ : nレベル優先度
- ペンディングレジスタ : RO : 下位ビットから0から順に割り付けられている
- イネーブルレジスタ : RW  : 下位ビットから0から順に割り付けられている
- 優先度スレッシュホールド(優先度マスク) : 設定した優先度までの割込みをマスクする
- claim/complete : RW : 
    - Read  : ペンディングしている最高優先度の割込みのIDを渡す．
    - Write : 割込み処理終了時にそのIDを書き込む．

# C言語

## ABI
- RV32Eはx16-x31がない．
- 汎用レジスタの機能
    - x1 : ra : 戻りアドレス
    - x2 : sp : スタックポインタ
    - x3 : gp : グローバルポインタ
    - x4 : tp : スレッドポインター
    - x5-7 : t0-2
    - x8   : s0/fp
    - x9   : s1/fp
    - x10-11 : a0-1 : 引数・戻り値
    - x12-17 : a2-7 : 引数
    - x18-27 : s2-11
    - x28-31 : t3-6
    - f0-7 : ft0-7
    - f8-9 : fs0-1
    - f10-11 : fa0-1 : 引数・戻り値
    - f12-17 : fa2-7 : 引数
    - f18-27 : fs2-11 : 引数
    - f28-31 : ft8-11 : 引数
- Caller save
    - x1, x5-x7, x10-11, x12-17, x28-31
    - f0-7, f10-11,f12-17,f28-31
- Callee save
    - X2(sp), x8, x9, x18-27
    - f8-9,f18-27

- スタックアライメント
    - RV32I/RV64I : 16byte
    - RV32E : 4byte


# アセンブリ言語
- アセンブリ言語でプログラムする際には，xで始まる名前ではなく，役割の名前で記載する
- オプション https://sourceware.org/binutils/docs-2.31/as/RISC_002dV_002dDirectives.html#RISC_002dV_002dDirectives
    - .option norelax : リンカ最適化を無効に(gp相対を無効に)
    - .option norvc   : 圧縮命令を無効に

以上
