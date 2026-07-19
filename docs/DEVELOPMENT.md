# DEVELOPMENT.md — 別マシンで開発を継続するための環境メモ

**この文書の目的は「環境と再現」だけ**である。プロジェクトの規約・運用は
[AGENTS.md](../AGENTS.md)（正本）と [CLAUDE.md](../CLAUDE.md) を見ること。ここには書かない。

**この文書が解こうとしている問題**：このリポジトリは現在、
**このマシンにしか存在しない絶対パスと特定バージョンの道具に依存している**。
別マシンで clone すると、**環境の欠落がコードの不具合に見える形で**失敗する。
特に QEMU は「ビルドは通るのに実行時だけ壊れ、しかもカーネルのバグに見える」という
最悪の壊れ方をする（§1.2）。

記載はすべて 2026-07-19 時点の**このマシンでの実測値**である。
食い違ったら記録ではなく現環境を正とし、この文書を直すこと。

---

## 1. 必要な道具（実測値）

### 1.1 クロスツールチェーン

`cmake/toolchain-*.cmake` が各々 `FMP3_EXPECTED_TOOLCHAIN_MACHINE` を宣言し、
`cmake/toolchain_check.cmake:55` が `${CMAKE_C_COMPILER} -dumpmachine` の出力と
**`MATCHES`（正規表現の部分一致）**で照合する。完全一致ではないので
`riscv64` という期待値に `riscv64-unknown-elf` が合致する。

| ツールチェーン | 対象 preset | 期待パターン | 実測 `-dumpmachine` | 版 | 現在地 |
|---|---|---|---|---|---|
| `aarch64-none-elf-` | `kria_arm64`, `kria_arm64-1core` | `aarch64-none-elf`<br>(`cmake/toolchain-aarch64-none-elf.cmake:34`) | `aarch64-none-elf` | 14.3.1 | `/usr/local/tools/arm-gnu-toolchain-14.3.rel1-x86_64-aarch64-none-elf/bin/` |
| `arm-none-eabi-` | `musca_b1*`, `rp2350_pico2`, `kria_r5*` | `arm-none-eabi`<br>(`cmake/toolchain-arm-none-eabi.cmake:28`) | `arm-none-eabi` | 13.2.1 | `/usr/bin`（ディストリ） |
| `riscv64-unknown-elf-` | `polarfire_soc_kit*` | `riscv64`<br>(`cmake/toolchain-riscv64.cmake:30`) | `riscv64-unknown-elf` | 13.2.0 | `/usr/bin`（ディストリ） |

**3つとも接頭辞の上書き口がある**（後述 §2）。aarch64 のみ `/usr/local/tools/` 配下の
手動導入で、PATH に入っていることが前提。他2つはディストリのパッケージ。

### 1.2 QEMU — ★最大の罠

| preset | 必要なバイナリ | 最低版 | 理由 |
|---|---|---|---|
| `polarfire_soc_kit-qemu` | `qemu-system-riscv64` | 8.2.2 で可 | `microchip-icicle-kit` は 8.2.2 にある |
| `musca_b1`, `musca_b1-2core` | `qemu-system-arm` | **11.0.1** | 2プロセッサの MHU/CPUWAIT。`musca-b1` マシン自体は 8.2.2 にもある（`target/musca_b1_gcc/target.cmake:103`） |
| `kria_arm64*` | `qemu-system-aarch64` | **11.0.1** | APU の RVBAR／CRF リセット制御 |
| `kria_r5*` | **`qemu-system-aarch64`**（`-arm` ではない） | **11.0.1** | `xlnx-zcu102` の RPU クラスタ。zcu102 は aarch64 の機械なので R5 ターゲットでも `-aarch64` を使う |

**このマシンでの実測：**

- `/usr/bin/qemu-system-{arm,aarch64,riscv64}` … **8.2.2**（Debian 1:8.2.2+ds-0ubuntu1.16）
- `/home/honda/qemu-build/install/bin/qemu-system-arm` … **11.0.1**
- `/home/honda/qemu-build/qemu-11.0.1/build-a64/qemu-system-aarch64` … **11.0.1**
  （※ `install/bin/` に aarch64 は**入っていない**。ビルドツリー配下のまま）

**★なぜこれが最大の罠か。**
各 `target.cmake` は「決め打ちパスが存在すればそれを、無ければ PATH 上の裸の名前」に
**静かにフォールバックする**（例：`target/kria_r5_gcc/target.cmake:104-109`,
`target/musca_b1_gcc/target.cmake:105-110`）。したがって別マシンでは
**configure も build も何事もなく成功し、システムの 8.2.2 が使われる**。
版検査は `WARNING` 止まりで `FATAL_ERROR` にしない（`target/kria_arm64_gcc/target.cmake:160-166`）。

そして 8.2.2 で `kria_arm64` を走らせると、**単に警告が出るのではなく
CPU 例外を取って死ぬ**。実際の出力：

```
CPU exception handler (p_excinf = 00060600).
...
Sample program ends with exception.
```

**これはカーネルのバグに見える。** 実際には環境の欠落である。
本セッションでも制御側が一度この誤認をしている。
**QEMU で異常が出たら、コードを疑う前に `--version` を見ること。**

### 1.3 Ruby — ビルドには不要、検証には必須

```
ruby 3.2.3 (2024-01-18 revision 52bb2ac0a6) [x86_64-linux-gnu]
```

Ruby は**製品のビルドには一切使わない**（cfg は `cfg_py/` の Python 実装）。
しかし pristine の `cfg/cfg.rb` は**差分等価性検査のオラクル**であり、
本プロジェクトの**主たる正しさの根拠**である `tools/cfg_equivalence.sh` が
これを走らせる。Ruby が無い環境では検査が動かず、
**「ビルドは通るが正しさを確認する手段が無い」**状態になる。

### 1.4 その他

```
Python 3.12.3
cmake version 3.30.0
ninja 1.12.1
```

---

## 2. マシン固有パスの上書き方法

### ツールチェーン接頭辞

| 変数 | 既定値 | 定義箇所 |
|---|---|---|
| `AARCH64_NONE_ELF_TOOLCHAIN_PREFIX` | `aarch64-none-elf-` | `cmake/toolchain-aarch64-none-elf.cmake:37-38` |
| `ARM_NONE_EABI_TOOLCHAIN_PREFIX` | `arm-none-eabi-` | `cmake/toolchain-arm-none-eabi.cmake:31-32` |
| `RISCV64_TOOLCHAIN_PREFIX` | `riscv64-unknown-elf-` | `cmake/toolchain-riscv64.cmake:33-34` |
| `FMP3_EXPECTED_TOOLCHAIN_MACHINE` | 各ファイル参照 | 別ツールチェーンを使うとき照合を合わせる |

### QEMU

| 変数 | 使う preset | 定義箇所 |
|---|---|---|
| `QEMU_SYSTEM_RISCV64` | `polarfire_soc_kit-qemu` | `target/polarfire_soc_kit_gcc/target.cmake:184` |
| `QEMU_SYSTEM_ARM_MUSCA_B1` | `musca_b1*` | `target/musca_b1_gcc/target.cmake:111` |
| `QEMU_SYSTEM_AARCH64_KRIA` | `kria_arm64*` | `target/kria_arm64_gcc/target.cmake:145` |
| `QEMU_SYSTEM_AARCH64_KRIA_R5` | `kria_r5*` | `target/kria_r5_gcc/target.cmake:110` |

### 具体例

```bash
# PATH 上の道具だけで済む環境
cmake --preset polarfire_soc_kit-qemu

# 別マシンで QEMU を自前ビルドした場合
cmake --preset kria_arm64 \
      -DQEMU_SYSTEM_AARCH64_KRIA=/opt/qemu-11.0.1/bin/qemu-system-aarch64

# ツールチェーンを別の場所に置いた場合（接頭辞なので末尾の - を忘れない）
cmake --preset kria_arm64 \
      -DAARCH64_NONE_ELF_TOOLCHAIN_PREFIX=/opt/arm-gnu-14.3/bin/aarch64-none-elf-
```

### 移植性の欠陥（隠さず記録する）

- **決め打ち絶対パスが `target.cmake` に直書きされている。**
  `target/kria_r5_gcc/target.cmake:104`、`target/musca_b1_gcc/target.cmake:105`、
  `target/kria_arm64_gcc/target.cmake:135-136` に `/home/honda/qemu-build/...` が残る。
  上書き口はあるのでブロッカーではないが、**他人のマシンのパスがソースに入っている**のは
  綺麗ではない。環境変数か `cmake/local.cmake`（gitignore）へ追い出すのが本来。
- **版検査が `WARNING` 止まり。** §1.2 の通り、これが「環境の欠落がカーネルのバグに見える」
  現象の直接の原因になっている。`FATAL_ERROR` にするか、少なくとも `run` ターゲット側で
  弾くのが望ましい（未着手）。

---

## 3. ブランチ構成 — 新規 clone の罠

**`main` と `upstream` の両方が要る。** `upstream` は pristine のみを載せた
vendor ブランチで、上流追従の `git merge upstream` はこれを追う。
**素の `git clone` は既定ブランチしか取らない**ので、明示的に取ること：

```bash
git clone git@github.com:exshonda/fmp3_core.git
cd fmp3_core
git fetch origin upstream:upstream        # ← これを忘れると git merge upstream が失敗する
git branch -a                             # main と upstream があることを確認
```

- `tools/import_upstream.sh` を使う（＝新しい上流を取り込む）には
  **`fmp3_archive` が手元に要る**。このマシンでは `../fmp3_archive`。
- `UPSTREAM_PRISTINE.txt` が現在の `upstream` の元 archive commit SHA を固定している。
  取り込みをやり直すときはここと `UPSTREAM_VERSION` を必ず更新する（AGENTS.md §4）。

---

## 4. ビルドと検証

### preset 一覧（`cmake --list-presets` の実出力）

| preset | 中身 | QEMU |
|---|---|---|
| `polarfire_soc_kit` | PolarFire SoC Kit (U54/RV64, **実機**) | — |
| `polarfire_soc_kit-qemu` | 同 / QEMU (microchip-icicle-kit) | 4コア ✓ |
| `musca_b1` | ARM Musca-B1 (dual Cortex-M33), 1プロセッサ | ✓ |
| `musca_b1-2core` | 同, 2プロセッサ SMP | ✓ |
| `rp2350_pico2` | RaspberryPi Pico 2 (RP2350) | **ビルドのみ** |
| `kria_arm64` | KRIA Cortex-A53 (AArch64), 4プロセッサ | ✓ |
| `kria_arm64-1core` | 同, 1プロセッサ | ✓ |
| `kria_r5` | KRIA Cortex-R5F, 1プロセッサ / lockstep | ✓ |
| `kria_r5-2core` | 同, 2プロセッサ split mode | ✓ |

**計9個**（QEMU 起動可 7 / 実機向け 1 / ビルドのみ 1）。
`rp2350_pico2` がビルド専用なのは、**RP2350 を模擬する QEMU の機械モデルが存在しない**ため
（8.2.2 にも 11.0.1 にも無い。実測済み）。

### ビルドと実行

```bash
cmake --preset kria_r5-2core
cmake --build build/kria_r5-2core

# QEMU 実行（成功の証拠は「Processor N start.」が プロセッサ数だけ出ること）
cmake --build build/kria_r5-2core --target run
```

**★`rc=124` は成功でも失敗でもない。** `run` は QEMU が終了しないので
`timeout` を掛けると必ず 124 が返る。健全な実行でも 124、
**出力ゼロの無言ハングでも 124** である（Task 12 で実際に踏んだ）。
**戻り値ではなく期待出力の有無で判定すること。**

```bash
# 良い判定のしかた
timeout -k5 25 cmake --build build/kria_r5-2core --target run > /tmp/run.log 2>&1
grep -cE 'Processor [12] start\.' /tmp/run.log     # 2 が出れば成功
pkill -f qemu-system                                # 孤児プロセスの掃除（下記）
```

`timeout N cmake --build ... --target run` は **QEMU を孤児化しうる**
（ninja の `USES_TERMINAL` ジョブが `sh -c` 経由で起動するため素の SIGTERM が届かない）。
`timeout -k` を使い、終わったら `pgrep -c qemu-system` で残存を確認すること。

### 検証（★これが本プロジェクトの正しさの根拠）

```bash
# 差分等価性検査：pristine の Ruby cfg と Python cfg を独立に走らせバイト比較
tools/cfg_equivalence.sh <build-preset-dir>
#   exit 0 = 一致 / 1 = 不一致 / 2 = 実行前提が満たされていない
```

**★`exit 2` は PASS ではない。** 「前提が揃っていないので検査していない」の意味である。
Ruby が無い、`cfg1_out` がビルドされていない等で出る。**通ったと誤読しないこと。**

```bash
# エラー経路の回帰
tools/cfg_error_tests/run.sh <build-preset-dir> <error.cfg> [expected-substring] [extra-cflags]
```

ケースは `tools/cfg_error_tests/*.cfg`。**全ケースが全 preset に当たるわけではない**：
`musca_b1_*` / `rp2350_*` / `kria_r5_*` は名前どおり対象限定で、
`CLS_PRC2` を使うケースは**2コアのビルドディレクトリ**でないと
`CLS_PRC2 undeclared` になる。ケースと preset の対応を合わせること。

---

## 5. 引き継ぐべき知見

**本セッションで実際に事故った型だけを書く。一般論ではない。**

### ★壊れた検証は、成功と同じ顔をする

**positive / negative control の対が無い検証は、検証ではない。**
「差が出るはずのケースで実際に差が出る」ことを実演していなければ、
その検査は always-pass かもしれない。実例：

- **空ファイル同士は `diff -q` で「一致」になる。** 両パイプラインが同一に失敗しただけなのに
  「バイト一致」と読める。
- **死んだ分岐への変異は何も証明しない。** `TNUM_PRCID==1` のガード下に仕込んだ変異は、
  4コアビルドでは実行されないので検査が落ちない。「落ちなかった＝正しい」ではない。
- **「置換0件なら FATAL」というガードは「2件中1件だけ置換」を見逃す。**
  実際に `tools/cfg_error_tests/run.sh` がこれで壊れていた（`start.S.obj` は置換され
  `chip_support.S.obj` は残った）。現在の `run.sh:211` は
  **完全性検査**（相対パスが1つでも残っていれば FATAL）に直してある。
- **QEMU の `-serial` の数が足りないと、コンソール出力が無言で消える。**
  `hw/arm/xlnx-zynqmp.c` は *n* 番目の `-serial` を UART*n* に割り当てる。
  UART1 を使うターゲットで `-serial mon:stdio` 1個だけ渡すと、
  エラーも警告も無く出力だけが消え、ハングやクラッシュに見える。

参考までに、本セッションでは**制御側の検証が11回間違い、そのすべてで
サブエージェント側が正しかった**。誰の報告よりも、成果物と対照実験を信じること。

### `pwd` と `pwd -P`

symlink 経由で到達できるツリーでは、論理パス（`pwd`）を使うスクリプトが
**古いファイルを黙ってコンパイルする**。実際に `cfg_error_tests/run.sh` が
これで stale な `cfg1_out.c` を検証していた。**症状が「不一致」ではなく
「古いものが一致する」方向に出る**ので発見が遅れる。
`cd "$d" && pwd -P` で物理パスへ正規化すること。CMake では `REALPATH`。

### `KERNEL_FCSRCS` の突き合わせ（AGENTS.md §4）

`CMakeLists.txt` は `kernel/*.c` の22個を**手書きで列挙**している。
上流が `kernel/Makefile.kernel` の `KERNEL_FCSRCS` にソースを追加・削除しても
**CMake 側は追従せず、静かに古いまま**になる。
`git merge upstream` の後は必ず両者を突き合わせること（2026-07-19 時点で差分ゼロ）。

### 参照先

- [DIVERGENCE_MAP.md](../DIVERGENCE_MAP.md) — pristine への乖離台帳と**未解決事項の一覧**。
  マージ衝突を解決するときの唯一の根拠。
- [docs/handover/2026-07-19-esp32s3-migration-brief.md](handover/2026-07-19-esp32s3-migration-brief.md)
  — **外部リポジトリから `fmp3_core` を submodule として使う際の受け入れ契約**。
  `FMP3_TARGET_DIR` で arch/chip/target の3層まるごとを外から供給できること、
  および既知の穴（呼び出し側スコープ依存2件）を記載。
