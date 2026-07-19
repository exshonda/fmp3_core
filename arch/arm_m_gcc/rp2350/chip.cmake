#
#		チップ依存部の CMake 定義（RP2350 / Cortex-M33 用）
#
#  target.cmake から include される（上流 Makefile.chip に相当）．
#
#  外部（SDK）ターゲットのパス解決規約（fmp3_esp_idf / fmp3_pico_sdk 等の
#  統合リポジトリから fmp3_core が submodule として使われる場合に備える。
#  arch/riscv_gcc/polarfire_soc/chip.cmake / arch/arm_m_gcc/musca_b1/chip.cmake
#  と同じ作法）：
#   - 共通 arch（arch/arm_m_gcc/common）は fmp3_core 側＝ARCHDIR
#   - チップ依存部（rp2350）・target 依存部は，これを include する
#     target.cmake 側で解決される＝CHIPDIR／TARGETDIR
#  ARCHDIR／CHIPDIR／TARGETDIR は呼び出し元の target.cmake が設定済み
#  （本ファイルではハードコードしない）．
#
set(COREDIR ${ARCHDIR}/common)

#
#  コンパイルオプション（Makefile.chip:13-19）
#
#  Cortex-M33（ARMv8-M Mainline）．RP2350 の M33 は FPU を搭載するが，
#  Phase D1 ではソフトウェア浮動小数点 ABI を用いる（musca_b1 と同様．
#  FPU 対応は後続フェーズ）．
#
list(APPEND FMP3_COMPILE_OPTIONS
    -mcpu=cortex-m33
    -mthumb
    -mfloat-abi=soft
)
list(APPEND FMP3_LINK_OPTIONS
    -mcpu=cortex-m33
    -mthumb
    -mfloat-abi=soft
)
list(APPEND FMP3_COMPILE_DEFS TOPPERS_CORTEX_M33)

#
#  ARM アーキテクチャの世代（ARMv8-M Mainline，Makefile.chip:21-27）
#
#  __TARGET_ARCH_THUMB は旧 ARM 純正コンパイラの組込みマクロであり，
#  GNU 開発環境では定義されないため，明示的に定義する（5 = ARMv8-M）．
#
list(APPEND FMP3_COMPILE_DEFS __TARGET_ARCH_THUMB=5)

#
#  TrustZone 対応コアでのセキュア単独動作（Makefile.chip:29-38）
#
#  RP2350 の bootROM は ARM イメージをセキュア状態で起動する．TrustZone
#  搭載コアでは例外リターン値 EXC_RETURN にセキュア状態を表すビットが
#  必要なため，TOPPERS_ENABLE_TRUSTZONE を定義してセキュア用の
#  EXC_RETURN（0xfffffffd）を選択する．これを定義しないと非セキュア用の
#  0xffffffbc が用いられ，最初の例外リターンで整合性チェックに失敗する
#  （INVPC UsageFault）．
#
list(APPEND FMP3_COMPILE_DEFS TOPPERS_ENABLE_TRUSTZONE)

list(APPEND FMP3_INCLUDE_DIRS ${CHIPDIR})

#
#  カーネルに含めるチップ依存ソース（Makefile.chip:45-46）
#
list(APPEND FMP3_ARCH_C_FILES
    ${CHIPDIR}/chip_kernel_impl.c
    ${CHIPDIR}/chip_ipi.c
)

#
#  非TECS版 SIO ドライバ（Makefile.chip:51-52）
#
list(APPEND FMP3_SYSSVC_TARGET_C_FILES
    ${CHIPDIR}/chip_serial.c
)

#
#  スタートアップモジュール（Makefile.chip:54-63）
#
#  START_OBJS を start.o（COREDIR/start.S）に設定し，LDFLAGS に -nostdlib
#  を追加する（musca_b1/chip.cmake と同一パターン．cfg1_out / fmp どちらも
#  FMP3_START_FILES を消費するため，ここを省くとリンクが失敗する）．
#
list(APPEND FMP3_START_FILES
    ${COREDIR}/start.S
)
list(APPEND FMP3_LINK_OPTIONS -nostdlib)

#
#  コア依存部（Makefile.chip:68）
#
include(${COREDIR}/arch.cmake)
