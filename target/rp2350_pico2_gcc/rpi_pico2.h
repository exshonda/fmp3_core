/*
 *  TOPPERS/FMP Kernel
 *      Toyohashi Open Platform for Embedded Real-Time Systems/
 *      Flexible MultiProcessor Kernel
 *
 *  Copyright (C) 2020 by Embedded and Real-Time Systems Laboratory
 *              Graduate School of Information Science, Nagoya Univ., JAPAN
 *
 *  上記著作権者は，以下の(1)〜(4)の条件を満たす場合に限り，本ソフトウェ
 *  ア（本ソフトウェアを改変したものを含む．以下同じ）を使用・複製・改
 *  変・再配布（以下，利用と呼ぶ）することを無償で許諾する．（条件略）
 *
 *  本ソフトウェアは，無保証で提供されているものである．
 */

/*
 *  RaspberryPi Pico 2（RP2350）サポートモジュール
 */

#ifndef TOPPERS_RPI_PICO2_H
#define TOPPERS_RPI_PICO2_H

/*
 *  コアのクロック周波数
 *
 *  XOSC 12MHz × 125（VCO 1500MHz）÷5 ÷2 = 150MHz（hardware_init_hook）．
 */
#define CPU_CLOCK_HZ    150000000

/*
 *  割込み番号の最大値
 *
 *  RP2350 の NVIC 外部割込みは 52 本（IRQ 0〜51）．例外番号はこれに 16 を
 *  加えたもの（最大 51 + 16 = 67）．ベクタ表は 0〜TMAX_INTNO の TMAX_INTNO+1
 *  エントリを持つ．
 */
#define TMAX_INTNO      (52 + 16)

/*
 *  微少時間待ち（sil_dly_nse）のための定義（本来は SIL のターゲット依存部）
 *
 *  150MHz 実機での実測校正値（ASP3 移植 dlynse 校正より：初回 46ns・
 *  ループ毎 33ns）．
 */
#define SIL_DLY_TIM1    46
#define SIL_DLY_TIM2    33

#endif /* TOPPERS_RPI_PICO2_H */
