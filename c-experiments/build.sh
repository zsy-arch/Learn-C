#!/usr/bin/env bash
# build.sh —— 统一的编译/运行辅助脚本
# 用法:
#   ./build.sh strict  <src.c> [more.c ...]   # 严格模式：-Werror
#   ./build.sh warn    <src.c>                # 只看警告，不 -Werror
#   ./build.sh san     <src.c>                # ASan + UBSan
#   ./build.sh opt     <src.c> <-O级别>       # 指定优化级别
set -u

CC=${CC:-cc}
BASE=(-std=c17 -Wall -Wextra -Wpedantic -O0 -g)
STRICT=(-Werror)
SAN=(-fsanitize=address,undefined -fno-omit-frame-pointer)

mode=$1; shift

case "$mode" in
  strict) "$CC" "${BASE[@]}" "${STRICT[@]}" "$@" ;;
  warn)   "$CC" "${BASE[@]}" "$@" ;;
  san)    "$CC" "${BASE[@]}" "${SAN[@]}" "$@" ;;
  opt)    "$CC" -std=c17 -Wall -Wextra -Wpedantic -g "$@" ;;
  *) echo "unknown mode: $mode" >&2; exit 2 ;;
esac
