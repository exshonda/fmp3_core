#
#		チップ依存部の CMake 定義（KRIA / ZynqMP用）
#
#  target.cmake から include される（上流 Makefile.chip に相当）．
#
#  ARCHDIR／CHIPDIR／TARGETDIR は呼び出し元の target.cmake が設定済み
#  （本ファイルではハードコードしない．riscv_gcc/polarfire_soc/chip.cmake・
#  arm_m_gcc/musca_b1/chip.cmake と同じ規約）．
#
set(COREDIR ${ARCHDIR}/common)

#  Makefile.chip:12-14
list(APPEND FMP3_COMPILE_OPTIONS -mcpu=cortex-a53)
list(APPEND FMP3_LINK_OPTIONS -mcpu=cortex-a53)
list(APPEND FMP3_COMPILE_DEFS TOPPERS_CORTEX_A53)

list(APPEND FMP3_INCLUDE_DIRS ${CHIPDIR})

#  Makefile.chip:18  LDFLAGS := $(LDFLAGS) -N
#
#  ★現物は "-N"（-Wl, プレフィックス無し）．$(LINK) は $(CC)（gcc を
#    リンカドライバとして使う）なので，一見 -Wl,-N が要るように見えるが，
#    実測（aarch64-none-elf-gcc 14.3.1）で確認した通り，gcc ドライバは
#    ld へ素の "-N" をそのまま転送する（-Wl,-N を付けた場合と
#    readelf -l の LOAD セグメント（RWE 結合・非ページアライン＝NMAGIC）が
#    完全一致することを確認済み）．上流の綴りに忠実に "-N" のまま積む．
#
list(APPEND FMP3_LINK_OPTIONS -N)

#
#  カーネルに含めるチップ依存ソース（Makefile.chip:22-24）
#
list(APPEND FMP3_ARCH_C_FILES
    ${CHIPDIR}/chip_kernel_impl.c
)

#
#  非TECS版 SIO ドライバ（Makefile.chip:29、Cadence UART）
#
list(APPEND FMP3_SYSSVC_TARGET_C_FILES
    ${CHIPDIR}/chip_serial.c
    ${CHIPDIR}/xuartps.c
)

#
#  スタートアップモジュール（Makefile.chip:33-43）
#
#  ★このターゲットでは start.S / -nostdlib はコア層(arch.cmake)ではなく
#    チップ層(ここ)が積む（上流 Makefile.chip の構造どおり）．
#
#  ★start.S の所在について（現物確認）：arm64_gcc には start.S が
#    common/ と zynqmp/ の両方にあり，内容が異なる（zynqmp/start.S は
#    my_core_index という，このリポジトリのどこにも定義が無いマクロを
#    使っている未定義参照＝上流の潜在バグと判断．common/start.S は
#    my_prcidx（target_asm.inc が定義）を使っており，こちらが実際に
#    アセンブルできる版）．上流 Makefile の vpath 解決順（KERNEL_DIRS は
#    CHIPDIR が COREDIR より先に積まれる）を素直に辿ると zynqmp/start.S
#    （壊れている版）が選ばれてしまうことを GNU make で実際に再現して
#    確認した．本 CMake 層は「ビルドが通る」common/start.S を明示的に
#    選ぶ（Step 7 のビルドで実際にアセンブルできることを確認する）．
#
list(APPEND FMP3_START_FILES
    ${COREDIR}/start.S
)
list(APPEND FMP3_LINK_OPTIONS -nostdlib)

#
#  コア依存部（Makefile.chip:48）
#
include(${COREDIR}/arch.cmake)
