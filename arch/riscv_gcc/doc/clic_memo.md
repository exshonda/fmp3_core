
# CLIC（Core-Local Interrupt Controller）に関するメモ
- 作成者: 本田晋也
- 最終更新: 2026年06月29日

## メモの位置づけ

このメモは，CLIC（Core-Local Interrupt Controller）に関して，FMP3カーネルを
ポーティングするにあたって必要となる事項をまとめたものである．記述は
ESP32-P4（RISC-V HP コア，CLICモード固定）の実装を主たる対象とする．

## 参考文献

[1] https://github.com/riscv/riscv-fast-interrupt （RISC-V CLIC 仕様ドラフト）
[2] ESP32-P4 Technical Reference Manual（CLIC / INTERRUPT MATRIX の章）

## PLICとの違い（概要）

- PLICはプラットフォーム共有の割込みコントローラで，コンテキスト（hart×mode）
  ごとに許可/しきい値レジスタを持つ．CLICは**コアローカル**であり，各コアが
  自分専用のCLICを持つ（コンテキストINDEXは実質不要）．
- CLICはハードウェアのトラップ入口で割込みレベル（mil）を更新し，mret で
  mcause.mpil から復元する（ベクタ/プリエンプションをHWが扱う）．このため
  カーネルは mil の復帰操作が必要だが，これはチップ依存の割込み出口フック
  irc_end_int（chip_support.S）に局所化し，共通部 core_support.S は CLIC 非依存に
  保つ（設計の詳細は clic_design.md「トラップ機構」）．
- CLICはソフトウェアからペンディング（IP）を操作でき，エッジ線では ras_int で
  割込みを生成できる（PLICでは不可）．

## 割込みレベル（mil）と mcause.mpil

- mil（machine interrupt level）は in-service の割込みレベルで，
  mintstatus CSR（0x346）の [31:24] に現れる．割込み入口でHWが上昇させる．
- mret は mcause.mpil（mcause[23:16]）から mil を復元する．mret は mcause 全体を
  復元しないため，ネスト復帰時に mpil が外側levelの値に書き換わっている可能性が
  ある．そこで全割込み出口で必ず通る irc_end_int で **mcause.mpil=0 を毎回強制**し，
  後続の mret（通常復帰/ディスパッチ/アイドル復帰）が一律に mil=0 へ戻るようにする
  （mil の固着＝lost wakeup / livelock を防ぐ）．ネスト復帰で外側ハンドラも mil=0 に
  なるが，preemption は threshold で守られるため安全（clic_design.md 参照）．
- 注（ESP32-P4 実機）: 一部のCLIC関連CSRは未実装で，アクセスすると不正命令例外と
  なる．mintstatus は **0x346** を用いること（0xFB1 は不正）．mintthresh（0x347）も
  本チップでは不正で，割込み優先度マスクはメモリマップド CLIC_INT_THRESH を使う．

## レジスタ（ESP32-P4）

ベースアドレス CLIC_BASE = 0x2080_0000．

- CLIC_INT_THRESH（CLIC_BASE+0x0008）
	- 割込み優先度マスク（しきい値）．[31:24] に8bit値．自コアのレジスタ．
	- **read-back が不安定**で，書いた値をそのまま読み戻せない（同一命令の連続読みで
	  異なる値＝ライブ状態を返す）．このため真値はソフトウェアシャドウ
	  clic_ipm_shadow[] で管理する（clic_kernel_impl.c）．
- CLIC_INT_CTRL(i)（CLIC_BASE+0x1000 + i*4）
	- 線 i ごとの制御レジスタ．以下のフィールドを持つ．
		- IE  : 割込み許可
		- IP  : ペンディング（ソフトウェアからセット/クリア可）
		- TRIG: トリガ種別（エッジ/レベル）．ras_int で立てる線はエッジに設定する．
		- CTL[31:24]: 割込み優先度/レベル（8bit，内部表現をそのまま格納）
	- 有効線は 0 .. CLIC_TNUM_INTNO-1（ESP32-P4 は CLIC_TNUM_INTNO=48，線 0..47）．
	  初期化等で範囲外の線番号（48）にアクセスしないこと（OOB MMIO）．

## NLBITS（レベル/優先度の割付）

ESP32-P4 は NLBITS=3．CTL[31:24] の上位 NLBITS ビットがレベル，残りが優先度として
解釈される．カーネルの割込み優先度（TMIN_INTPRI=-7 .. TMAX_INTPRI=-1）は内部表現
（8bit, [31:24]）へ写像して CTL / THRESH に格納する．

## 割込み線の割付（ESP32-P4）

- 内部割込み: msip（CLINT 由来）= 線3，mtimer（CLINT 由来）= 線7．
- 外部割込み（INTERRUPT MATRIX 由来）は +16 オフセットで線16〜47に現れる．
  線16は USB-Serial/JTAG が使用するため，アプリ用には線17以降を用いる．

## プロセッサ間割込み（IPI）と storm livelock

- IPI（ディスパッチ要求）は CLINT の MSI（msip）で発生させる．CLINT はコア
  ローカルで，自コアは CLINT_BASE（0x2000_0000），他コアは +CLINT_CORE_STRIDE．
- 他コアからの高レートな msip により，割込み対象コアが割込み入口/出口処理に飽和し，
  割込まれたタスクが割込みの合間に1命令も retire できない**位相共振 livelock**が
  起こり得る（実機 test_mtrans2 で実証．割込み1回コストが低いPLIC構成では非発生）．
- 対策: msi_handler 入口（clear_msip の前）の関数フック irc_begin_ipi で連続入口の
  mcycle 間隔を測り，閾値未満（連続）なら storm とみなして連続数に比例した有界
  backoff を入れ，受信位相を他コアの送出 cadence からずらす（irc_begin_ipi の実体は
  チップ依存部が提供．ESP32-P4 は chip_kernel_impl.c）．固定 backoff は timing 敏感で
  脆い（計装の数十cycが消えるだけで効かなくなる Heisenbug）ため，連続数比例の
  self-tuning とし，COUNT_CAP で最悪レイテンシを bound する．スケジューラ状態も
  msipのマスク論理も変えず，msip は level-pending のまま保持されるため lost-wakeup
  は生じない．

## 割込みペンディング・許可

- ペンディング（IP）はソフトウェアからセット/クリア可能（CLIC_INT_CTRL）．
  エッジトリガ線では IP のセットで割込みを生成できる（ras_int）．
- 許可（IE）も CLIC_INT_CTRL のビットで線ごとに操作する．

以上
