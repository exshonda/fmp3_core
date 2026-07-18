/*
 *  TOPPERS/FMP Kernel
 *      Toyohashi Open Platform for Embedded Real-Time Systems/
 *      Flexible MultiProcessor Kernel
 *
 *  Copyright (C) 2020-2024 by Embedded and Real-Time Systems Laboratory
 *              Graduate School of Information Science, Nagoya Univ., JAPAN
 *
 *  上記著作権者は，以下の(1)～(4)の条件を満たす場合に限り，本ソフトウェ
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
 *  @(#) $Id: $
 */

/*
 *		KRIAのハードウェア資源の定義
 */

#ifndef TOPPERS_KRIA_H
#define TOPPERS_KRIA_H

/*
 *  コアクロック
 */
#define CORE_CLOCK_MHZ  50U

/*
 *  UARTの設定値の定義（115.2Kbpsで動作させる場合）
 */
#define XUARTPS_BAUDGEN_115K	0x8fU
#define XUARTPS_BAUDDIV_115K	0x05U

/*
 *  ワールド間割込みの定義
 */
#define GENINT_GP1_D0_BASE 0xA1210000
#define GENINT_GP1_D1_BASE 0xA1210008 /* CA53 では8byte毎に設定 */
#define GENINT_GP2_D0_BASE 0xA1220000
#define GENINT_GP2_D1_BASE 0xA1220008 /* CA53 では8byte毎に設定 */
#define GENINT_GP3_D0_BASE 0xA1230000
#define GENINT_GP3_D1_BASE 0xA1230008 /* CA53 では8byte毎に設定 */
#define GENINT_GP4_D0_BASE 0xA1240000
#define GENINT_GP4_D1_BASE 0xA1240008 /* CA53 では8byte毎に設定 */

#define GENINT_GP3_D0_XS_XNS_BIT    0x01U
#define GENINT_GP3_D0_XS_AS_BIT     0x02U
#define GENINT_GP3_D0_XS_ANS_BIT    0x04U

#define GENINT_GP3_D1_XNS_XS_BIT    0x01U
#define GENINT_GP3_D1_XNS_ANS_BIT   0x02U
#define GENINT_GP3_D1_XNS_AS_BIT    0x04U

#define GENINT_GP4_D0_AS_XS_BIT     0x01U
#define GENINT_GP4_D0_AS_XNS_BIT    0x02U

#define GENINT_GP4_D1_ANS_XS_BIT    0x01U
#define GENINT_GP4_D1_ANS_XNS_BIT   0x02U

#define INTNO_XS_AS      UINT_C(121)
#define INTNO_XNS_AS     UINT_C(122)
#define INTNO_XS_ANS     UINT_C(136)
#define INTNO_XNS_ANS    UINT_C(137)


#endif /* TOPPERS_KRIA_H */
