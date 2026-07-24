# -*- coding: utf-8 -*-
#
#   TOPPERS/FMP Kernel
#       Toyohashi Open Platform for Embedded Real-Time Systems/
#       Flexible MultiProcessor Kernel
# 
#   Copyright (C) 2015 by FUJI SOFT INCORPORATED, JAPAN
#   Copyright (C) 2015-2020 by Embedded and Real-Time Systems Laboratory
#               Graduate School of Information Science, Nagoya Univ., JAPAN
# 
#   上記著作権者は，以下の(1)〜(4)の条件を満たす場合に限り，本ソフトウェ
#   ア（本ソフトウェアを改変したものを含む．以下同じ）を使用・複製・改
#   変・再配布（以下，利用と呼ぶ）することを無償で許諾する．
#   (1) 本ソフトウェアをソースコードの形で利用する場合には，上記の著作
#       権表示，この利用条件および下記の無保証規定が，そのままの形でソー
#       スコード中に含まれていること．
#   (2) 本ソフトウェアを，ライブラリ形式など，他のソフトウェア開発に使
#       用できる形で再配布する場合には，再配布に伴うドキュメント（利用
#       者マニュアルなど）に，上記の著作権表示，この利用条件および下記
#       の無保証規定を掲載すること．
#   (3) 本ソフトウェアを，機器に組み込むなど，他のソフトウェア開発に使
#       用できない形で再配布する場合には，次のいずれかの条件を満たすこ
#       と．
#     (a) 再配布に伴うドキュメント（利用者マニュアルなど）に，上記の著
#         作権表示，この利用条件および下記の無保証規定を掲載すること．
#     (b) 再配布の形態を，別に定める方法によって，TOPPERSプロジェクトに
#         報告すること．
#   (4) 本ソフトウェアの利用により直接的または間接的に生じるいかなる損
#       害からも，上記著作権者およびTOPPERSプロジェクトを免責すること．
#       また，本ソフトウェアのユーザまたはエンドユーザからのいかなる理
#       由に基づく請求からも，上記著作権者およびTOPPERSプロジェクトを
#       免責すること．
# 
#   本ソフトウェアは，無保証で提供されているものである．上記著作権者お
#   よびTOPPERSプロジェクトは，本ソフトウェアに関して，特定の使用目的
#   に対する適合性も含めて，いかなる保証も行わない．また，本ソフトウェ
#   アの利用により直接的または間接的に生じたいかなる損害に関しても，そ
#   の責任を負わない．
# 
#  $Id: interrupt.py (converted from interrupt.trb by Claude Code Sonnet 4.6) $
# 

#
#		割込み管理機能の生成スクリプト
#

#
#  マルチプロセッサ割込み処理を省略するかどうかのデフォルト定義
#
if "OMIT_MULTIPRC_INTERRUPT" not in globals():
    OMIT_MULTIPRC_INTERRUPT = False

#
#  kernel_cfg.hの生成
#
kernelCfgH.add(f"#define TNUM_ISRID\t{len(cfgData['CRE_ISR'])}")

for _, params in sorted(cfgData["CRE_ISR"].items()):
    kernelCfgH.add(f"#define {params['isrid']}\t{params['isrid'].val}")
kernelCfgH.add()

#
#  kernel_cfg.cの生成
#
kernelCfgC.comment_header("Interrupt Management Functions")

#
#  トレースログマクロのデフォルト定義
#
kernelCfgC.add("""\
#ifndef LOG_ISR_ENTER
#define LOG_ISR_ENTER(isrid)
#endif /* LOG_ISR_ENTER */

#ifndef LOG_ISR_LEAVE
#define LOG_ISR_LEAVE(isrid)
#endif /* LOG_ISR_LEAVE */
""")

#
#  CRE_ISRで使用できる割込み番号とそれに対応する割込みハンドラ番号のデ
#  フォルト定義
#
if "INTNO_CREISR_VALID" not in globals():
    INTNO_CREISR_VALID = INTNO_VALID
if "INHNO_CREISR_VALID" not in globals():
    INHNO_CREISR_VALID = INHNO_VALID

#
#  全割込み番号／割込みハンドラ番号のリストを求める
#
#  ★Ruby版（.values.flatten.uniq）は出現順を保存するが、list(set(...))は
#  順序不定（DIVERGENCE_MAP.md「未解決事項」に記録していたもの。現状これら
#  3つの変数はすべて `in` によるメンバーシップ判定にしか使われておらず
#  （本ファイル中で grep 済み）実害は無いが、dict.fromkeys(...) で
#  Rubyと同じ「出現順を保った重複除去」に揃えても副作用が無く安全なため、
#  将来この変数を列挙生成に使い始めても事故らないようここで直しておく。
INTNO_VALID_ALL = list(dict.fromkeys(v for lst in INTNO_VALID.values() for v in lst))
INTNO_CREISR_VALID_ALL = list(dict.fromkeys(v for lst in INTNO_CREISR_VALID.values() for v in lst))
INHNO_VALID_ALL = list(dict.fromkeys(v for lst in INHNO_VALID.values() for v in lst))

#
#  CFG_INTで使用できる割込み優先度のデフォルト定義
#
if "INTPRI_CFGINT_VALID" not in globals():
    INTPRI_CFGINT_VALID = list(range(TMIN_INTPRI, TMAX_INTPRI + 1))

#
#  割込み番号と割込みハンドラ番号の変換テーブルの作成
#
toInhnoVal = {}
toIntnoVal = {}
for prcid in range(1, TNUM_PRCID + 1):
    if len(INTNO_CREISR_VALID[prcid]) != len(INHNO_CREISR_VALID[prcid]):
        error_exit(f"the length of `INTNO_CREISR_VALID[{prcid}]' is different"
                   f" from the length of `INHNO_CREISR_VALID[{prcid}]'")
    toInhnoVal[prcid] = {}
    toIntnoVal[prcid] = {}
    inhno_creisr_valid = list(INHNO_CREISR_VALID[prcid])
    for intnoVal in INTNO_CREISR_VALID[prcid]:
        inhnoVal = inhno_creisr_valid.pop(0)
        toInhnoVal[prcid][intnoVal] = inhnoVal
        toIntnoVal[prcid][inhnoVal] = intnoVal

#
#  割込み要求ラインに関するエラーチェック（1回目）
#
for _, params in cfgData["CFG_INT"].items():
    # クラスの囲みの中に記述されていない場合（E_RSATR）
    if "class" not in params:
        error_ercd("E_RSATR", params, "%apiname must be within a class")
        params["class"] = TCLS_ERROR

    # 割込み要求を受け付けるプロセッサの設定
    if not OMIT_MULTIPRC_INTERRUPT:
        params["affinityPrcBitmap"] = clsData[params["class"]]["affinityPrcBitmap"]
    else:
        # 割付け可能プロセッサが複数あるクラスの囲み内にCFG_INTを記述した
        # 場合には，警告メッセージを出し，初期割付けプロセッサのみで割込み
        # 要求を受け付けるように設定する．
        initPrcBitmap = 1 << (clsData[params["class"]]["initPrc"] - 1)
        if clsData[params["class"]]["affinityPrcBitmap"] != initPrcBitmap:
            warning_api(params,
                        f"%%intno configured within the class "
                        f"{clsData[params['class']]['clsid']} "
                        f"is configured to be accepted by the processor "
                        f"{clsData[params['class']]['initPrc']} only.")
        params["affinityPrcBitmap"] = initPrcBitmap

    # intnoが有効範囲外の場合（E_PAR）
    if params["intno"] not in INTNO_VALID_ALL:
        error_illegal("E_PAR", params, "intno")
    else:
        # クラスIDがエラーの場合は，エラーチェックをスキップする
        if str(params["class"]) != "":
            for prcid in clsData[params["class"]]["affinityPrcList"]:
                if params["intno"] not in INTNO_CREISR_VALID[prcid]:
                    error_ercd("E_RSATR", params,
                               "the assignable processors "
                               "of the class in which %apiname of `%intno' is described "
                               "must be included in the set of processors "
                               "to which the interrupt is requested")
                    break

    # intatrが無効の場合（E_RSATR）
    if (params["intatr"] & ~(TA_ENAINT | TA_EDGE | TARGET_INTATR)) != 0:
        error_illegal_sym("E_RSATR", params, "intatr", "intno")

    # intpriが有効範囲外の場合（E_PAR）
    if params["intpri"] not in INTPRI_CFGINT_VALID:
        error_illegal_sym("E_PAR", params, "intpri", "intno")

    # カーネル管理外に固定されているintnoに対して，intpriにTMIN_INTPRI以上の値
    if "INTNO_FIX_NONKERNEL" in globals() \
            and params["intno"] in INTNO_FIX_NONKERNEL:
        if params["intpri"] >= TMIN_INTPRI:
            error_ercd("E_OBJ", params, "%%intno must have higher priority "
                       "than TMIN_INTPRI in %apiname")

    # カーネル管理に固定されているintnoに対して，intpriにTMIN_INTPRIよりも
    # 小さい値が指定された場合（E_OBJ）
    if "INTNO_FIX_KERNEL" in globals() \
            and params["intno"] in INTNO_FIX_KERNEL:
        if params["intpri"] < TMIN_INTPRI:
            error_ercd("E_OBJ", params, "%%intno must have lower or equal "
                       "priority to TMIN_INTPRI in %apiname")

    # ターゲット依存のエラーチェック
    if "TargetCheckCfgInt" in globals():
        TargetCheckCfgInt(params)

#
#  割込みハンドラに関するエラーチェック
#
for _, params in cfgData["DEF_INH"].items():
    # クラスの囲みの中に記述されていない場合（E_RSATR）
    if "class" not in params:
        error_ercd("E_RSATR", params, "%apiname must be within a class")
        params["class"] = TCLS_ERROR
    params["prcid"] = clsData[params["class"]]["initPrc"]

    # inhnoが有効範囲外の場合（E_PAR）
    if params["inhno"] not in INHNO_VALID_ALL:
        error_illegal("E_PAR", params, "inhno")
    else:
        # クラスの初期割付けプロセッサが，割込みハンドラ番号に対応するプロ
        # セッサでない場合（E_RSATR）
        if params["inhno"] not in INHNO_VALID[params["prcid"]]:
            error_ercd("E_RSATR", params,
                       "the initial assignment processor "
                       "of the class in which %apiname of `%inhno' is described "
                       "must be the processor corresponding to `%inhno'")

    # inhatrが無効の場合（E_RSATR）
    if (params["inhatr"] & ~(TARGET_INHATR)) != 0:
        error_illegal_sym("E_RSATR", params, "inhatr", "inhno")

    # カーネル管理外に固定されているinhnoに対して，inhatrにTA_NONKERNELが
    # 指定されていない場合（E_RSATR）
    if "INHNO_FIX_NONKERNEL" in globals() \
            and params["inhno"] in INHNO_FIX_NONKERNEL:
        if (params["inhatr"] & TA_NONKERNEL) == 0:
            error_ercd("E_RSATR", params,
                       "%%inhno must be non-kernel interrupt in %apiname")
            continue

    # カーネル管理に固定されているinhnoに対して，inhatrにTA_NONKERNELが指
    # 定されている場合（E_RSATR）
    if "INHNO_FIX_KERNEL" in globals() \
            and params["inhno"] in INHNO_FIX_KERNEL:
        if (params["inhatr"] & TA_NONKERNEL) != 0:
            error_ercd("E_RSATR", params,
                       "%%inhno must not be non-kernel interrupt in %apiname")
            continue

    if params["inhno"] in INHNO_CREISR_VALID[params["prcid"]]:
        # 割込みハンドラ番号に対応する割込み番号がある場合
        intnoVal = toIntnoVal[params["prcid"]][params["inhno"].val]

        # inhnoに対応するintnoに対するCFG_INTがない場合（E_OBJ）
        if intnoVal not in cfgData["CFG_INT"]:
            error_ercd("E_OBJ", params,
                       f"intno `{intnoVal}' corresponding to "
                       "%%inhno in %apiname is not configured with CFG_INT")
        else:
            intnoParams = cfgData["CFG_INT"][intnoVal]

            # 割込みハンドラ番号に対応するプロセッサが，割込みハンドラ番号に
            # 対応する割込み要求ラインが属するクラスの割付け可能プロセッサに
            # 含まれていない場合（E_RSATR）
            if params["prcid"] not in clsData[intnoParams["class"]]["affinityPrcList"]:
                error_ercd("E_RSATR", params,
                           "the processor corresponding to "
                           "`%inhno' must be included in the set of processors "
                           "which can accept the interrupt")
            else:
                if OMIT_MULTIPRC_INTERRUPT \
                        and clsData[intnoParams["class"]]["initPrc"] != params["prcid"]:
                    warning_api(params,
                                "the processor corresponding to `%inhno' "
                                "is not the processor which accepts the interrupt")

            if intnoParams["intpri"] < TMIN_INTPRI:
                # inhnoに対応するintnoに対してCFG_INTで設定された割込み優先度
                # がTMIN_INTPRIよりも小さく，inhatrにTA_NONKERNELが指定されて
                # いない場合（E_OBJ）
                if (params["inhatr"] & TA_NONKERNEL) == 0:
                    error_ercd("E_OBJ", params,
                               "TA_NONKERNEL must be set for "
                               "non-kernel interrupt handler in %apiname of %%inhno")
            else:
                # inhnoに対応するintnoに対してCFG_INTで設定された割込み優先度
                # がTMIN_INTPRI以上で，inhatrにTA_NONKERNELが指定されている
                # 場合（E_OBJ）
                if (params["inhatr"] & TA_NONKERNEL) != 0:
                    error_ercd("E_OBJ", params,
                               "TA_NONKERNEL must not be set for "
                               "kernel interrupt handler in %apiname of %%inhno")

    # ターゲット依存のエラーチェック
    if "TargetCheckDefInh" in globals():
        TargetCheckDefInh(params)

#
#  割込みサービスルーチン（ISR）に関するエラーチェックと割込みハンドラの生成
#
for _, params in sorted(cfgData["CRE_ISR"].items()):
    # クラスの囲みの中に記述されていない場合（E_RSATR）
    if "class" not in params:
        error_ercd("E_RSATR", params, "%apiname must be within a class")
        params["class"] = TCLS_ERROR

    # 割込みサービスルーチンを実行するプロセッサに関するチェック
    if OMIT_MULTIPRC_INTERRUPT:
        initPrcBitmap = 1 << (clsData[params["class"]]["initPrc"] - 1)
        if clsData[params["class"]]["affinityPrcBitmap"] != initPrcBitmap:
            warning_api(params,
                        f"`%isrid' created within the class "
                        f"{clsData[params['class']]['clsid']} "
                        f"is configured to be executed by the processor "
                        f"{clsData[params['class']]['initPrc']} only.")

    # isratrが無効の場合（E_RSATR）
    if (params["isratr"] & ~(TARGET_ISRATR)) != 0:
        error_illegal("E_RSATR", params, "isratr")

    # intnoが有効範囲外の場合（E_PAR）
    if params["intno"] not in INTNO_CREISR_VALID_ALL:
        error_illegal("E_PAR", params, "intno")
    else:
        # クラスIDがエラーの場合は，エラーチェックをスキップする
        if str(params["class"]) != "":
            # クラスの割付け可能プロッサが，intnoで指定した割込み要求ライン
            # が接続されたプロセッサの集合に含まれていない場合（E_RSATR）
            for prcid in clsData[params["class"]]["affinityPrcList"]:
                if params["intno"] not in INTNO_CREISR_VALID[prcid]:
                    error_ercd("E_RSATR", params,
                               "the assignable processors "
                               "of the class in which %apiname of `%intno' is described "
                               "must be included in the set of processors "
                               "to which the interrupt is requested")
                    break

    # isrpriが有効範囲外の場合（E_PAR）
    if not (TMIN_ISRPRI <= params["isrpri"] and params["isrpri"] <= TMAX_ISRPRI):
        error_illegal("E_PAR", params, "isrpri")

    # intnoに対応するinhnoに対してDEF_INHがある場合（E_OBJ）
    #
    #  ★intnoが有効範囲外（E_PAR、:302）や、このクラスのaffinityPrcListに
    #  含まれるプロセッサに対してintnoが無効（E_RSATR、:309-316）の場合でも
    #  このループ自体は無条件に実行される（上のエラーチェックはcontinueせず
    #  処理を継続するため）。そのため toInhnoVal[prcid] に
    #  params["intno"].val がキーとして存在しないことがある。
    #  Ruby版（cfg/interrupt.trb:376）は $toInhnoVal[prcid][...] が
    #  Hashの欠損キー参照で nil を返し、続く has_key?(nil) が偽になって
    #  素通りする（例外にならない）。dict[...] の直接参照はKeyErrorで
    #  クラッシュし、後続の本来出るべき診断（E_OBJ等）を握り潰してしまう
    #  ため、Rubyと同じ「無ければNone」の意味になる .get() を使う。
    for prcid in clsData[params["class"]]["affinityPrcList"]:
        inhnoVal = toInhnoVal[prcid].get(params["intno"].val)
        if inhnoVal in cfgData["DEF_INH"]:
            error_ercd("E_OBJ", params,
                       f"%%intno in %apiname is duplicated "
                       f"with inhno {cfgData['DEF_INH'][inhnoVal]['inhno']}")

    # intnoに対するCFG_INTがない場合（E_OBJ）
    if params["intno"] not in cfgData["CFG_INT"]:
        error_ercd("E_OBJ", params,
                   "%%intno in %apiname is not configured with CFG_INT")
    else:
        intnoParams = cfgData["CFG_INT"][params["intno"]]

        # CFG_INTとCRE_ISRが異なるクラスの囲みの中にある場合（E_RSATR）
        if params["class"] != intnoParams["class"]:
            error_ercd("E_RSATR", params,
                       "%%intno in %apiname "
                       "does not belong to the same class with CFG_INT")

        # intnoでカーネル管理外の割込みを指定した場合（E_OBJ）
        if intnoParams["intpri"] < TMIN_INTPRI:
            error_ercd("E_OBJ", params,
                       "interrupt service routine cannot handle "
                       "non-kernel interrupt in %apiname of %isrid")

    # ターゲット依存のエラーチェック
    if "TargetCheckCreIsr" in globals():
        TargetCheckCreIsr(params)

isr_flag = {}
for prcid in range(1, TNUM_PRCID + 1):
    for intnoVal in INTNO_CREISR_VALID[prcid]:
        # 割込み番号intnoValに対して登録されたISRのリストの作成
        isrParamsList = []
        for _, params in sorted(cfgData["CRE_ISR"].items()):
            if params["intno"] == intnoVal:
                isrParamsList.append(params)

        # 割込み番号intnoValに対して登録されたISRが存在する場合
        if len(isrParamsList) > 0:
            inhnoVal = toInhnoVal[prcid][intnoVal]
            clsid = isrParamsList[0]["class"]

            # 割込みを受け付けるプロセッサでない場合はスキップ
            if prcid not in clsData[clsid]["affinityPrcList"]:
                continue

            # DEF_INHに相当するデータを生成
            cfgData["DEF_INH"][inhnoVal] = {
                "inhno": NumStr(inhnoVal),
                "inhatr": NumStr(TA_NULL, "TA_NULL"),
                "inthdr": f"_kernel_inthdr_{intnoVal}",
                "class": clsid
            }

            if not isr_flag.get(intnoVal, False):
                # 割込みサービスルーチンを呼び出す割込みハンドラの生成
                kernelCfgC.add("void")
                kernelCfgC.add(f"_kernel_inthdr_{intnoVal}(void)")
                kernelCfgC.add("{")
                if len(isrParamsList) > 1:
                    kernelCfgC.add2("\tPCB\t\t*p_my_pcb = get_my_pcb();")

                # 割込みサービスルーチンを優先度順に呼び出す（stable sort）
                for index, params in enumerate(
                        sorted(isrParamsList, key=lambda p: p["isrpri"].val)):
                    if index > 0:
                        kernelCfgC.add()
                        #  ★2026-07-24 再訂正: `_kernel_` 接頭辞を付ける形へ戻した。
                        #
                        #  【前の版（83a14be）が誤っていた理由】
                        #  「sense_lock / unlock_cpu は Inline なので rename 対象外」
                        #  という根拠は **xtensa と arm_m にしか当てはまらない**。
                        #  riscv_gcc / arm_gcc / arm64_gcc の core_rename.def は
                        #  lock_cpu / unlock_cpu / sense_lock を rename しており、
                        #  kernel_cfg.c は kernel_int.h 末尾の unrename 連鎖の**後**に
                        #  置かれるため、これらの arch では Inline 実体が
                        #  `_kernel_sense_lock` 名で定義される。素名で出力すると
                        #  implicit declaration で落ちる（＝壊れる arch 集合を
                        #  入れ替えただけだった）。
                        #
                        #  【`_kernel_` が正しいことの根拠（★仕様書が条文で規定）】
                        #  doc/configurator.txt §4.7.1.2「割込みハンドラの生成」
                        #  1338-1339 / 1346-1347 行が、生成すべきコードとして
                        #      if (_kernel_sense_lock()) {
                        #          _kernel_unlock_cpu();
                        #      }
                        #  をそのまま掲げている。interrupt.trb:462-464 も同じ。
                        #  ⇒ 真の欠陥は「arm_m と xtensa の core_rename.def に
                        #     lock_cpu/unlock_cpu/sense_lock のエントリが欠けている
                        #     **非一貫性**」の側であり、本ポートは xtensa 側の
                        #     core_rename.def を補って解消した。arm_m は上流へ報告する。
                        kernelCfgC.add("\tif (_kernel_sense_lock()) {")
                        kernelCfgC.add("\t\t_kernel_force_unlock_spin(p_my_pcb);")
                        kernelCfgC.add("\t\t_kernel_unlock_cpu();")
                        kernelCfgC.add2("\t}")
                    kernelCfgC.add(f"\tLOG_ISR_ENTER({params['isrid']});")
                    kernelCfgC.add(f"\t((ISR)({params['isr']}))"
                                   f"((EXINF)({params['exinf']}));")
                    kernelCfgC.add(f"\tLOG_ISR_LEAVE({params['isrid']});")
                kernelCfgC.add2("}")
                isr_flag[intnoVal] = True

#
#  割込みハンドラのための標準的な初期化情報の生成
#
if not OMIT_INITIALIZE_INTERRUPT or USE_INHINIB_TABLE:
    #
    #  定義する割込みハンドラの数
    #
    kernelCfgC.add(f"""\
#define TNUM_DEF_INHNO\t{len(cfgData['DEF_INH'])}
const uint_t _kernel_tnum_def_inhno = TNUM_DEF_INHNO;
""")

    if len(cfgData["DEF_INH"]) != 0:
        #
        #  割込みハンドラのエントリ
        #
        for _, params in cfgData["DEF_INH"].items():
            if (params["inhatr"] & TA_NONKERNEL) == 0:
                kernelCfgC.add(f"INTHDR_ENTRY({params['inhno']}, "
                               f"{params['inhno'].val}, {params['inthdr']})")
        kernelCfgC.add("")

        #
        #  割込みハンドラ初期化ブロック
        #
        kernelCfgC.add(
            f"const INHINIB _kernel_inhinib_table[TNUM_DEF_INHNO] = {{")
        for index, (_, params) in enumerate(cfgData["DEF_INH"].items()):
            if index > 0:
                kernelCfgC.add(",")
            if (params["inhatr"] & TA_NONKERNEL) == 0:
                inthdr = (f"(FP)(INT_ENTRY({params['inhno']}, "
                          f"{params['inthdr']}))")
            else:
                inthdr = f"(FP)({params['inthdr']})"
            kernelCfgC.append(
                f"\t{{ ({params['inhno']}), "
                f"({params['inhatr']}), "
                f"{inthdr}, "
                f"{clsData[params['class']]['initPrc']} }}")
        kernelCfgC.add()
        kernelCfgC.add2("};")
    else:
        kernelCfgC.add2(
            "TOPPERS_EMPTY_LABEL(const INHINIB, _kernel_inhinib_table);")

#
#  割込み要求ラインのための標準的な初期化情報の生成
#
if not OMIT_INITIALIZE_INTERRUPT or USE_INTINIB_TABLE:
    #
    #  設定する割込み要求ラインの数
    #
    kernelCfgC.add(f"""\
#define TNUM_CFG_INTNO\t{len(cfgData['CFG_INT'])}
const uint_t _kernel_tnum_cfg_intno = TNUM_CFG_INTNO;
""")

    #
    #  割込み要求ライン初期化ブロック
    #
    if len(cfgData["CFG_INT"]) != 0:
        kernelCfgC.add(
            f"const INTINIB _kernel_intinib_table[TNUM_CFG_INTNO] = {{")
        for index, (_, params) in enumerate(cfgData["CFG_INT"].items()):
            if index > 0:
                kernelCfgC.add(",")
            kernelCfgC.append(
                f"\t{{ ({params['intno']}), "
                f"({params['intatr']}), "
                f"({params['intpri']}), "
                f"{clsData[params['class']]['initPrc']}, "
                f"0x{params['affinityPrcBitmap']:x}U }}")
        kernelCfgC.add()
        kernelCfgC.add2("};")
    else:
        kernelCfgC.add2(
            "TOPPERS_EMPTY_LABEL(const INTINIB, _kernel_intinib_table);")

#
#  割込み管理機能初期化関数の追加
#
initializeFunctions.append("_kernel_initialize_interrupt(p_my_pcb);")
