# fmp3_core CMake（雛形）。pristine（kernel/ arch/ target/ ...）を参照し、
# configurator/（Python cfg）で生成物を作ってからビルドする。実体は後で拡充。
cmake_minimum_required(VERSION 3.20)
project(fmp3_core C ASM)
# add_subdirectory(kernel) 等はターゲット構成に合わせて記述
message(STATUS "fmp3_core: pristine at repo root; Python cfg under configurator/")
