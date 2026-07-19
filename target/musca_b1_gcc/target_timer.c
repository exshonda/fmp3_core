/*
 *  TOPPERS/ASP Kernel
 *      Toyohashi Open Platform for Embedded Real-Time Systems/
 *      Advanced Standard Profile Kernel
 *
 *  Copyright (C) 2026 by Embedded and Real-Time Systems Laboratory
 *              Graduate School of Information Science, Nagoya Univ., JAPAN
 *
 *  上記著作権者は，本ソフトウェアを TOPPERS ライセンス（条件は他のソー
 *  スファイルの先頭コメントを参照）の下で利用することを許諾する．本ソフ
 *  トウェアは無保証で提供される．
 *
 */

/*
 *  タイマドライバ（ARM Musca-B1 用，SysTick によるイベント駆動 HRT）
 *
 *  SysTick は 24bit ダウンカウンタで，区間の終わり（0 への到達）で割込みを
 *  発生して自動的に RVR にリロードする．本ドライバは次の方式で HRT を実現
 *  する（係数 HRT_CLOCKS_PER_US は musca-b1 では 40MHz/1MHz = 40）．
 *
 *    hrt_base    現在の区間の開始時刻 [us]（単調増加）
 *    hrt_reload  現在の区間長 [tick]（常に HRT_CLOCKS_PER_US の倍数）
 *
 *    get_current = hrt_base + （区間内の経過 tick） / HRT_CLOCKS_PER_US
 *
 *  区間内の経過は (RVR - SYST_CVR) で求める．区間がまるごと 1 回経過した
 *  ことは「割込みが発生した」ことそのものが表すため，割込みハンドラで
 *  hrt_base に 1 区間分（hrt_reload/HRT_CLOCKS_PER_US）を加える．これにより，
 *  区間中にカウント値の読出しが一度も行われなくても（dly_tsk 等）経過時間を
 *  正しく計上できる（ダウンカウンタが 1 周して元の値に戻るため，端点だけの
 *  観測では経過が見えない問題を回避する）．
 *
 *  hrt_reload は常に HRT_CLOCKS_PER_US の倍数なので除算に誤差は生じない．区間
 *  内オフセットの除算の切捨ては，区間境界で hrt_base が割込みにより正確に再
 *  設定されるため累積しない．
 */

#include "kernel_impl.h"
#include "time_event.h"
#include "target_timer.h"
#include <sil.h>

/*
 *  HRT の状態はプロセッサ毎に持つ
 *
 *  SysTick は Cortex-M のコアごとに独立したハードウェアであり，各コアは自分
 *  の SysTick のみを参照・更新する（FMP3 では他プロセッサの HRT 操作は
 *  TOPPERS_SUPPORT_CONTROL_OTHER_HRT が未定義のため request_set_hrt_event に
 *  よる IPI 経由で行われ，target_hrt_* は常に自プロセッサに対して呼ばれる）．
 *  したがって各配列要素は所有コアからのみアクセスされ，コア間の排他は不要
 *  である．添字は get_my_prcidx()（0 始まり）を用いる．
 */

/*
 *  現在の区間の開始時刻 [us]（単調増加）
 */
static volatile HRTCNT   hrt_base[TNUM_PRCID];

/*
 *  現在の区間長 [tick]（= SYST_RVR + 1）
 */
static volatile uint32_t hrt_reload[TNUM_PRCID];

/*
 *  前回 get_current が返した値（単調性保証のための下限）
 */
static volatile HRTCNT   hrt_last[TNUM_PRCID];

/*
 *  区間設定直後（SYST_CVR を 0 にクリアし，まだ RVR にリロードしていない）
 *  かどうか．QEMU では CVR 書込み直後にカウンタが 0 を返すため，この間の
 *  経過を 0 として扱い，区間長分の見かけの経過（スパイク）を防ぐ．
 */
static volatile uint8_t  hrt_fresh[TNUM_PRCID];

/*
 *  SysTick の制御レジスタ設定値（プロセッサクロック，割込み許可，動作）
 */
#define HRT_CTRL_RUN    (SYSTIC_ENABLE | SYSTIC_CLKSOURCE | SYSTIC_TICINT)

/*
 *  raise_event で用いる最小区間 [tick]（≒ 1us）
 */
#define HRT_MIN_TICKS   HRT_CLOCKS_PER_US

/*
 *  区間内の経過 tick 数を求める
 */
static uint32_t
hrt_offset_ticks(void)
{
    uint_t   idx    = get_my_prcidx();
    uint32_t reload = hrt_reload[idx];
    uint32_t cur;
    uint32_t off;

    /* 割込み保留中（区間終端に到達済）なら 1 区間分が経過している */
    if ((sil_rew_mem((void *) NVIC_ICSR) & NVIC_PENDSTSET) != 0U) {
        return reload;
    }

    cur = sil_rew_mem((void *) SYSTIC_CURRENT_VALUE);

    if (hrt_fresh[idx] != 0U) {
        if (cur == 0U) {
            return 0U;          /* まだ RVR にリロードしていない */
        }
        hrt_fresh[idx] = 0U;    /* リロードされ，カウント開始した */
    }

    /* SYST_RVR = reload - 1 から cur まで減ったので経過は (reload-1) - cur */
    off = (reload - 1U) - cur;
    if (off > reload) {
        off = reload;           /* 念のためのクランプ */
    }
    return off;
}

/*
 *  SysTick を指定のリロード値（tick）で（再）起動する
 */
static void
hrt_program(uint32_t ticks)
{
    uint_t idx = get_my_prcidx();

    if (ticks < 2U) {
        ticks = 2U;
    }
    if (ticks > HRT_MAX_TICKS) {
        ticks = HRT_MAX_TICKS;
    }
    hrt_reload[idx] = ticks;
    hrt_fresh[idx] = 1U;

    sil_wrw_mem((void *) SYSTIC_CONTROL_STATUS, 0U);
    sil_wrw_mem((void *) SYSTIC_RELOAD_VALUE, ticks - 1U);
    sil_wrw_mem((void *) SYSTIC_CURRENT_VALUE, 0U);
    sil_wrw_mem((void *) NVIC_ICSR, NVIC_PENDSTCLR);
    /* COUNTFLAG をクリアするために一度読む */
    (void) sil_rew_mem((void *) SYSTIC_CONTROL_STATUS);
    sil_wrw_mem((void *) SYSTIC_CONTROL_STATUS, HRT_CTRL_RUN);
}

/*
 *  タイマの起動処理
 */
void
target_hrt_initialize(EXINF exinf)
{
    uint_t idx = get_my_prcidx();

    hrt_base[idx] = 0U;
    hrt_last[idx] = 0U;
    /* 最初の set_event までは最大区間で空回しする */
    hrt_program(HRT_MAX_TICKS);
}

/*
 *  タイマの停止処理
 */
void
target_hrt_terminate(EXINF exinf)
{
    sil_wrw_mem((void *) SYSTIC_CONTROL_STATUS, 0U);
    sil_wrw_mem((void *) NVIC_ICSR, NVIC_PENDSTCLR);
}

/*
 *  高分解能タイマの現在のカウント値の読出し（本体）
 */
HRTCNT
hrt_get_current_body(void)
{
    uint_t  idx = get_my_prcidx();
    HRTCNT  v = (HRTCNT)(hrt_base[idx] + hrt_offset_ticks() / HRT_CLOCKS_PER_US);

    /*
     *  単調性の保証．区間境界での再設定やカウンタ読出しの僅かな前後により
     *  瞬間的に値が戻ることがある（時間が実際に戻るわけではない）．前回値
     *  より小さくなった場合は前回値を返す．2^32 us での正規の折返しと区別
     *  するため，差が範囲の半分以上のときだけ「戻った」と判定する．
     */
    if ((HRTCNT)(v - hrt_last[idx]) >= 0x80000000U) {
        v = hrt_last[idx];
    }
    else {
        hrt_last[idx] = v;
    }
    return v;
}

/*
 *  高分解能タイマへの割込みタイミングの設定（本体）
 */
void
hrt_set_event_body(HRTCNT hrtcnt)
{
    /* これまでの経過を基準時刻に畳み込んでから区間を張り直す */
    hrt_base[get_my_prcidx()] += hrt_offset_ticks() / HRT_CLOCKS_PER_US;

    if (hrtcnt == 0U) {
        hrtcnt = 1U;
    }
    hrt_program((uint32_t) hrtcnt * HRT_CLOCKS_PER_US);
}

/*
 *  高分解能タイマ割込みの要求（本体）
 *
 *  手動で割込み保留にすると「区間終端到達」と区別できないため，最小区間で
 *  張り直して直ちに（≒1us 後に）割込みを発生させる．
 */
void
hrt_raise_event_body(void)
{
    hrt_base[get_my_prcidx()] += hrt_offset_ticks() / HRT_CLOCKS_PER_US;
    hrt_program(HRT_MIN_TICKS);
}

/*
 *  高分解能タイマ割込みのクリア（本体）
 *
 *  当面割込みを発生させたくない場合に，最大区間で張り直す（FMP3 の
 *  target_hrt_clear_event に対応）．これまでの経過は基準時刻に畳み込む．
 */
void
hrt_clear_event_body(void)
{
    hrt_base[get_my_prcidx()] += hrt_offset_ticks() / HRT_CLOCKS_PER_US;
    hrt_program(HRT_MAX_TICKS);
}

/*
 *  タイマ割込みハンドラ
 *
 *  区間終端に到達して発生した割込みなので，1 区間分を基準時刻に加える．
 *  カウンタは自動リロードされ次区間の計測を続けるが，signal_time() の最後で
 *  set_hrt_event() が区間を張り直す．
 */
void
target_hrt_handler(void)
{
    uint_t   idx = get_my_prcidx();
    uint32_t csr = sil_rew_mem((void *) SYSTIC_CONTROL_STATUS);

    if ((csr & SYSTIC_COUNTFLAG) != 0U) {
        /* 区間終端に到達：1 区間分が経過した */
        hrt_base[idx] += hrt_reload[idx] / HRT_CLOCKS_PER_US;
        hrt_fresh[idx] = 0U;
    }
    else {
        /* 通常は起こらない（防御的に経過分だけ計上） */
        hrt_base[idx] += hrt_offset_ticks() / HRT_CLOCKS_PER_US;
    }
    sil_wrw_mem((void *) NVIC_ICSR, NVIC_PENDSTCLR);

    signal_time();
}
