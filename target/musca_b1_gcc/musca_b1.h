/*
 *  TOPPERS/ASP Kernel
 *      Toyohashi Open Platform for Embedded Real-Time Systems/
 *      Advanced Standard Profile Kernel
 *
 *  Copyright (C) 2026 by Embedded and Real-Time Systems Laboratory
 *              Graduate School of Information Science, Nagoya Univ., JAPAN
 *
 *  上記著作権者は，以下の(1)〜(4)の条件を満たす場合に限り，本ソフトウェ
 *  ア（本ソフトウェアを改変したものを含む．以下同じ）を使用・複製・改
 *  変・再配布（以下，利用と呼ぶ）することを無償で許諾する．
 *  (1) 本ソフトウェアをソースコードの形で利用する場合には，上記の著作
 *      権表示，この利用条件および下記の無保証規定が，そのままの形でソー
 *      スコード中に含まれていること．
 *  (2) 本ソフトウェアを，ライブラリ形式など，他のソフトウェア開発に使
 *      用できる形で再配布する場合には，再配布に伴うドキュメント（利用
 *      者マニュアルなど）に，上記の著作権表示，この利用条件および下記
 *      の無保証規定を掲載すること．
 *  (3) 本ソフトウェアを，機器に組み込むなど，他のソフトウェア開発に使
 *      用できない形で再配布する場合には，次のいずれかの条件を満たすこと．
 *    (a) 再配布に伴うドキュメントに，上記の著作権表示，この利用条件およ
 *        び下記の無保証規定を掲載すること．
 *    (b) 再配布の形態を，別に定める方法によって，TOPPERSプロジェクトに
 *        報告すること．
 *  (4) 本ソフトウェアの利用により直接的または間接的に生じるいかなる損
 *      害からも，上記著作権者およびTOPPERSプロジェクトを免責すること．
 *      また，本ソフトウェアのユーザまたはエンドユーザからのいかなる理
 *      由に基づく請求からも，上記著作権者およびTOPPERSプロジェクトを免
 *      責すること．
 *
 *  本ソフトウェアは，無保証で提供されているものである．上記著作権者お
 *  よびTOPPERSプロジェクトは，本ソフトウェアに関して，特定の使用目的に
 *  対する適合性も含めて，いかなる保証も行わない．また，本ソフトウェア
 *  の利用により直接的または間接的に生じたいかなる損害に関しても，その
 *  責任を負わない．
 *
 */

/*
 *  ARM Musca-B1 (SSE-200, dual Cortex-M33) サポートモジュール
 *
 *  本ファイルは，QEMU の "musca-b1" マシンを対象とする．ハードウェア資源
 *  のアドレス・割込み番号は QEMU の hw/arm/musca.c および hw/arm/armsse.c
 *  に基づく．Musca-B1 は SSE-200 を中核とするが，UART は ARM
 *  PrimeCell PL011 であり，SYSCLK が 40MHz である点が異なる．
 */

#ifndef TOPPERS_MUSCA_B1_H
#define TOPPERS_MUSCA_B1_H

/*
 *  コアのクロック周波数
 *
 *  QEMU の musca-b1 マシンの SYSCLK は 40MHz（musca.c の
 *  SYSCLK_FRQ = 40000000）．SysTick はプロセッサクロックで駆動される．
 */
#define CPU_CLOCK_HZ    40000000

/*
 *  割込み番号の最大値
 *
 *  Musca-B1 の SSE-200 は外部割込み EXP_NUMIRQ=96 を持つ（musca.c の
 *  num_irqs = 96）．内部 32 ＋ 拡張 96 で 0〜127．例外番号はこれに 16 を
 *  加えたもの（最大 127 + 16 = 143）．
 */
#define TMAX_INTNO      (127 + 16)

/*
 *  微少時間待ち（sil_dly_nse）のための定義（本来は SIL のターゲット依存部）
 *
 *  QEMU はサイクル精度のエミュレーションを行わないため，値は目安である．
 */
#define SIL_DLY_TIM1    10
#define SIL_DLY_TIM2    10

/*
 *  ARM PrimeCell PL011 UART
 *
 *  Musca-B1 では uart0/uart1 が PPC ポート（0x40100000）の背後に配置される
 *  （musca.c の make_uart 参照）．低レベル出力・コンソールには uart0 を用い
 *  る．PL011 の割込みは複数あるが，QEMU では combined（任意の要因で発火）
 *  を含めて 6 本ある．SIO ドライバには combined 割込みを用いる．
 *
 *  uart0 の SSE 入力 IRQ 配線（make_uart：irqbase = 7 + i*6, i=0）：
 *    RX       = irqbase+0 =  7
 *    TX       = irqbase+1 =  8
 *    RT       = irqbase+2 =  9
 *    MS       = irqbase+3 = 10
 *    E        = irqbase+4 = 11
 *    combined = irqbase+5 = 12
 */
#define MUSCA_B1_UART0_BASE             0x40105000
#define MUSCA_B1_UART1_BASE             0x40106000

/*
 *  SSE-200 システム制御レジスタブロック（IoT Kit System Control）
 *
 *  QEMU の hw/misc/iotkit-sysctl.c / hw/arm/armsse.c に基づく．
 *    ベース       = 0x50021000（armsse.c の armsse-sysctl 配置アドレス）
 *    CPUWAIT      = ベース + 0x118
 *
 *  CPUWAIT レジスタの各ビットが 1 の間，対応する CPU はリセット保持される．
 *  SSE-200 のリセット値は 0x2（CPUWAIT_RST=2）であり，CPU1（bit1）が保持，
 *  CPU0（bit0）が起動する．CPU0 が bit1 を 1→0 に書き込むと，QEMU 内部で
 *  arm_set_cpu_on_and_reset(1) が呼ばれ，CPU1 が init_svtor（0x10000000）
 *  からリセット起動する．
 */
#define MUSCA_B1_SYSCTL_BASE            0x50021000
#define MUSCA_B1_SYSCTL_CPUWAIT         (MUSCA_B1_SYSCTL_BASE + 0x118)
#define MUSCA_B1_CPUWAIT_CPU0           0x1U
#define MUSCA_B1_CPUWAIT_CPU1           0x2U

#define MUSCA_B1_UART0_RX_IRQn          7
#define MUSCA_B1_UART0_TX_IRQn          8
#define MUSCA_B1_UART0_COMBINED_IRQn    12
#define MUSCA_B1_UART1_RX_IRQn          13
#define MUSCA_B1_UART1_TX_IRQn          14
#define MUSCA_B1_UART1_COMBINED_IRQn    18

/*
 *  SSE-200 Message Handling Unit (MHU)
 *
 *  クロスコア・プロセッサ間割込み（IPI）に用いる．QEMU の hw/arm/armsse.c
 *  および hw/misc/armsse-mhu.c に基づき，番地・割込み番号を確定した．
 *
 *  ［番地］（APB PPC0 背後にマップ；armsse.c の memory_region_add_subregion）
 *    MHU0 = 0x40003000
 *    MHU1 = 0x40004000
 *
 *  ［割込み］（armsse.c：MHU i の irq line cpunum → CPU(cpunum) の IRQ(6+i)）
 *    MHU0 → 各コアの NVIC IRQ 6（例外番号 22）
 *    MHU1 → 各コアの NVIC IRQ 7（例外番号 23）
 *  本ターゲットの割込み番号（intno）＝例外番号＝16 + IRQn なので，
 *    MHU0 = intno 22, MHU1 = intno 23．
 *
 *  ［レジスタ］（armsse-mhu.c；下位 4bit が有効＝INTR_MASK 0xf，レベルトリガ）
 *    CPU0INTR_STAT = +0x00（RO）  CPU0INTR_SET = +0x04（WO）  CPU0INTR_CLR = +0x08（WO）
 *    CPU1INTR_STAT = +0x10（RO）  CPU1INTR_SET = +0x14（WO）  CPU1INTR_CLR = +0x18（WO）
 *  STAT が非ゼロの間，対応する CPU の IRQ がアサートされ続ける（レベル）ため，
 *  受信ハンドラ先頭で必ず CLR すること．
 *
 *  CPUn は「コア番号 n（0/1）宛て」を意味する（prcid 1=CPU0, prcid 2=CPU1）．
 */
#define MUSCA_B1_MHU0_BASE              0x40003000
#define MUSCA_B1_MHU1_BASE              0x40004000

#define MUSCA_B1_MHU_CPU0INTR_STAT(base)  ((uint32_t)(base) + 0x00U)
#define MUSCA_B1_MHU_CPU0INTR_SET(base)   ((uint32_t)(base) + 0x04U)
#define MUSCA_B1_MHU_CPU0INTR_CLR(base)   ((uint32_t)(base) + 0x08U)
#define MUSCA_B1_MHU_CPU1INTR_STAT(base)  ((uint32_t)(base) + 0x10U)
#define MUSCA_B1_MHU_CPU1INTR_SET(base)   ((uint32_t)(base) + 0x14U)
#define MUSCA_B1_MHU_CPU1INTR_CLR(base)   ((uint32_t)(base) + 0x18U)

/*  MHU0 の割込み番号（例外番号）：NVIC IRQ6 → 16 + 6 = 22  */
#define MUSCA_B1_MHU0_IRQn              6
#define MUSCA_B1_MHU0_INTNO            (16 + MUSCA_B1_MHU0_IRQn)

/*
 *  PL011 レジスタ（ベースアドレスからのオフセット）
 */
#define PL011_UARTDR(base)      ((uint32_t)(base) + 0x000U)  /* データ */
#define PL011_UARTFR(base)      ((uint32_t)(base) + 0x018U)  /* フラグ */
#define PL011_UARTIBRD(base)    ((uint32_t)(base) + 0x024U)  /* 整数ボーレート分周 */
#define PL011_UARTFBRD(base)    ((uint32_t)(base) + 0x028U)  /* 小数ボーレート分周 */
#define PL011_UARTLCR_H(base)   ((uint32_t)(base) + 0x02CU)  /* ライン制御 */
#define PL011_UARTCR(base)      ((uint32_t)(base) + 0x030U)  /* 制御 */
#define PL011_UARTIFLS(base)    ((uint32_t)(base) + 0x034U)  /* 割込みFIFOレベル */
#define PL011_UARTIMSC(base)    ((uint32_t)(base) + 0x038U)  /* 割込みマスク設定/クリア */
#define PL011_UARTRIS(base)     ((uint32_t)(base) + 0x03CU)  /* 生割込みステータス */
#define PL011_UARTMIS(base)     ((uint32_t)(base) + 0x040U)  /* マスク後割込みステータス */
#define PL011_UARTICR(base)     ((uint32_t)(base) + 0x044U)  /* 割込みクリア */

/*
 *  UARTFR（フラグ）レジスタのビット
 */
#define PL011_FR_TXFE           0x080U   /* 送信FIFO空 */
#define PL011_FR_RXFF           0x040U   /* 受信FIFOフル */
#define PL011_FR_TXFF           0x020U   /* 送信FIFOフル */
#define PL011_FR_RXFE           0x010U   /* 受信FIFO空 */
#define PL011_FR_BUSY           0x008U   /* UARTビジー */

/*
 *  UARTLCR_H（ライン制御）レジスタのビット
 */
#define PL011_LCRH_SPS          0x080U   /* スティックパリティ */
#define PL011_LCRH_WLEN_8       0x060U   /* ワード長8ビット */
#define PL011_LCRH_FEN          0x010U   /* FIFO許可 */
#define PL011_LCRH_STP2         0x008U   /* ストップビット2 */
#define PL011_LCRH_EPS          0x004U   /* 偶数パリティ */
#define PL011_LCRH_PEN          0x002U   /* パリティ許可 */
#define PL011_LCRH_BRK          0x001U   /* ブレーク送信 */

/*
 *  UARTCR（制御）レジスタのビット
 */
#define PL011_CR_CTSEN          0x8000U  /* CTSハードウェアフロー制御許可 */
#define PL011_CR_RTSEN          0x4000U  /* RTSハードウェアフロー制御許可 */
#define PL011_CR_RXE            0x0200U  /* 受信許可 */
#define PL011_CR_TXE            0x0100U  /* 送信許可 */
#define PL011_CR_UARTEN         0x0001U  /* UART許可 */

/*
 *  UARTIMSC／UARTRIS／UARTMIS／UARTICR の割込みビット
 */
#define PL011_INT_OE            0x400U   /* オーバラン */
#define PL011_INT_BE            0x200U   /* ブレーク */
#define PL011_INT_PE            0x100U   /* パリティエラー */
#define PL011_INT_FE            0x080U   /* フレーミングエラー */
#define PL011_INT_RT            0x040U   /* 受信タイムアウト */
#define PL011_INT_TX            0x020U   /* 送信 */
#define PL011_INT_RX            0x010U   /* 受信 */

#endif /* TOPPERS_MUSCA_B1_H */
