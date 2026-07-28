/*
 *  TOPPERS Software
 *      Toyohashi Open Platform for Embedded Real-Time Systems
 *
 *  Copyright (C) 2024 by Embedded and Real-Time Systems Laboratory
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
 *      用できない形で再配布する場合には，次のいずれかの条件を満たすこ
 *      と．
 *    (a) 再配布に伴うドキュメント（利用者マニュアルなど）に，上記の著
 *        作権表示，この利用条件および下記の無保証規定を掲載すること．
 *    (b) 再配布の形態を，別に定める方法によって，TOPPERSプロジェクトに
 *        報告すること．
 *  (4) 本ソフトウェアの利用により直接的または間接的に生じるいかなる損
 *      害からも，上記著作権者およびTOPPERSプロジェクトを免責すること．
 *      また，本ソフトウェアのユーザまたはエンドユーザからのいかなる理
 *      由に基づく請求からも，上記著作権者およびTOPPERSプロジェクトを
 *      免責すること．
 *
 *  本ソフトウェアは，無保証で提供されているものである．上記著作権者お
 *  よびTOPPERSプロジェクトは，本ソフトウェアに関して，特定の使用目的
 *  に対する適合性も含めて，いかなる保証も行わない．また，本ソフトウェ
 *  アの利用により直接的または間接的に生じたいかなる損害に関しても，そ
 *  の責任を負わない．
 *
 *  $Id: zybo_z7.h 335 2023-04-18 10:50:40Z ertl-honda $
 */

/*
 *    ボード毎のハードウェア資源の定義
 */
#ifndef POLARFIRE_SOC_KIT_H
#define POLARFIRE_SOC_KIT_H

#ifdef MPFS_DISCOVERY_KIT
/*
 *  起動メッセージのターゲットシステム名
 */
#define TARGET_NAME  "PolarFire SoC Discovery Kit <U54, RISC-V>"

/*
 *  コアの動作周波数
 */
#define CORE_CLK_MHZ  600

/*
 *  Machine タイマの駆動周波数
 */
#define MTIMER_FREQ_MHZ  1

/*
 *  使用するUART
 */
#define USE_UART1

/*
 *  微少時間待ちのための定義（本来はSILのターゲット依存部）
 */
#define SIL_DLY_TIM1    6
#define SIL_DLY_TIM2    3

#endif /* MPFS_DISCOVERY_KIT */

#ifdef MPFS_ICICLE_KIT
/*
 *  起動メッセージのターゲットシステム名
 */
#define TARGET_NAME  "PolarFire SoC FPGA Icicle Kit <U54, RISC-V>"

/*
 *  コアの動作周波数
 */
#define CORE_CLK_MHZ  600

/*
 *  Machine タイマの駆動周波数
 */
#define MTIMER_FREQ_MHZ  1

/*
 *  使用するUART
 */
#define USE_UART0

/*
 *  微少時間待ちのための定義（本来はSILのターゲット依存部）
 *  2026-07-20: test_dlynse 実機校正（U54 600MHz）。旧値 70/44 は zybo 流用の
 *  未校正値で約8倍 under-deliver（要求2270ns→実測273ns）していた。Discovery Kit と
 *  同一の U54@600MHz のため Discovery と同じ 6/3 に統一（conservative: delivered>=requested）。
 */
#define SIL_DLY_TIM1    6
#define SIL_DLY_TIM2    3

#endif /* MPFS_ICICLE_KIT */

#endif /* POLARFIRE_SOC_KIT_H */
