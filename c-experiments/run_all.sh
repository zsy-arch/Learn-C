#!/usr/bin/env bash
#
# run_all.sh —— 一键编译并运行所有实验
#
# 用法:
#   ./run_all.sh            构建 + 运行所有实验，输出写到 _logs/
#   ./run_all.sh --quiet    只打印汇总表
#   ./run_all.sh --clean    删除所有生成的可执行文件和日志
#
# 说明:
#   - "strict" 组：-Werror 必须零警告通过
#   - "warn"   组：故意触发警告的反例，不加 -Werror
#   - "ub"     组：故意触发 UB 的反例，额外跑一次 sanitizer 版本
#

set -u

HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$HERE"

CC=${CC:-cc}
BASE=(-std=c17 -Wall -Wextra -Wpedantic -O0 -g)
STRICT=(-Werror)
SAN=(-fsanitize=address,undefined -fno-omit-frame-pointer)
LOGDIR="$HERE/_logs"

QUIET=0
case "${1:-}" in
  --quiet) QUIET=1 ;;
  --clean)
      rm -rf "$LOGDIR"
      find . -type f -perm -u+x ! -name '*.sh' ! -name '*.c' ! -name '*.h' \
             ! -name 'Makefile' ! -name '*.md' -delete 2>/dev/null
      rm -rf 10_project/build
      find . -name '*.o' -delete
      find . -name '*.d' -delete
      find . -name '*.i' -delete
      find . -name '*.s' -delete
      find . -name '*.dSYM' -type d -exec rm -rf {} + 2>/dev/null
      echo "cleaned."
      exit 0 ;;
esac

mkdir -p "$LOGDIR"

TOTAL=0; BUILD_OK=0; BUILD_FAIL=0
declare -a SUMMARY

say()  { [ "$QUIET" -eq 1 ] || printf '%s\n' "$*"; }
head1(){ [ "$QUIET" -eq 1 ] || printf '\n\033[1m===== %s =====\033[0m\n' "$*"; }

# build_and_run <dir> <src-basename> <mode> [args...]
#   mode: strict | warn | san
build_and_run() {
    local dir="$1" src="$2" mode="$3"; shift 3
    local out="$dir/$src"
    local log="$LOGDIR/${dir}__${src}__${mode}.log"
    local flags=("${BASE[@]}")
    local bin="$src"

    case "$mode" in
      strict) flags+=("${STRICT[@]}") ;;
      warn)   ;;
      san)    flags+=("${SAN[@]}"); bin="${src}_san" ;;
    esac

    TOTAL=$((TOTAL + 1))
    {
        echo "### dir     : $dir"
        echo "### source  : $src.c"
        echo "### mode    : $mode"
        echo "### command : $CC ${flags[*]} $src.c -o $bin"
        echo "### --- compile ---"
    } > "$log"

    ( cd "$dir" && "$CC" "${flags[@]}" "$src.c" -o "$bin" ) >> "$log" 2>&1
    local crc=$?

    if [ $crc -ne 0 ]; then
        BUILD_FAIL=$((BUILD_FAIL + 1))
        SUMMARY+=("BUILD-FAIL  $dir/$src ($mode)")
        say "  [BUILD-FAIL] $dir/$src ($mode)  -> $log"
        return
    fi
    BUILD_OK=$((BUILD_OK + 1))

    echo "### --- run: ./$bin $* ---" >> "$log"
    ( cd "$dir" && ./"$bin" "$@" ) >> "$log" 2>&1
    local rrc=$?
    echo "### exit code: $rrc" >> "$log"

    SUMMARY+=("$(printf 'OK  exit=%-4s %s/%s (%s)' "$rrc" "$dir" "$src" "$mode")")
    say "  [ok] $dir/$src ($mode) exit=$rrc"
}

# ---------------------------------------------------------------- 00
head1 "00_toolchain —— 编译四阶段"
( cd 00_toolchain
  "$CC" -std=c17 -E hello.c -o hello.i
  "$CC" -std=c17 -S -O0 hello.c -o hello.s
  "$CC" -std=c17 -c hello.c -o hello.o
  "$CC" hello.o -o hello_full
  "$CC" "${BASE[@]}" "${STRICT[@]}" -c main.c  -o main.o
  "$CC" "${BASE[@]}" "${STRICT[@]}" -c greet.c -o greet.o
  "$CC" main.o greet.o -o app
) > "$LOGDIR/00_toolchain__stages.log" 2>&1
{
  echo "### hello.i 行数: $(wc -l < 00_toolchain/hello.i)"
  echo "### nm hello.o";  nm 00_toolchain/hello.o
  echo "### ./hello_full"; ( cd 00_toolchain && ./hello_full )
  echo "### ./app";        ( cd 00_toolchain && ./app )
  echo "### 反例: link_error（应当链接失败）"
  ( cd 00_toolchain && "$CC" "${BASE[@]}" link_error.c -o link_error ) 2>&1
  echo "### link exit: $?"
} >> "$LOGDIR/00_toolchain__stages.log" 2>&1
say "  [ok] 00_toolchain 四阶段 -> $LOGDIR/00_toolchain__stages.log"

# ---------------------------------------------------------------- 01
head1 "01_variables"
build_and_run 01_variables demo    strict
build_and_run 01_variables signcmp warn

# ---------------------------------------------------------------- 02
head1 "02_branch"
build_and_run 02_branch demo    strict
build_and_run 02_branch pitfall warn

# ---------------------------------------------------------------- 03
head1 "03_loop"
build_and_run 03_loop demo          strict
build_and_run 03_loop unsigned_loop warn

# ---------------------------------------------------------------- 04
head1 "04_function"
build_and_run 04_function demo strict
( cd 04_function
  "$CC" "${BASE[@]}" "${STRICT[@]}" -c mathutil.c -o mathutil.o
  "$CC" "${BASE[@]}" "${STRICT[@]}" -c app.c      -o app.o
  "$CC" mathutil.o app.o -o app && ./app
) > "$LOGDIR/04_function__module.log" 2>&1
say "  [ok] 04_function 模块化 -> $LOGDIR/04_function__module.log"
build_and_run 04_function dangling_return warn
build_and_run 04_function dangling_return san

# ---------------------------------------------------------------- 05
head1 "05_pointer"
for f in p01_basics p03_strings p04_swap p05_multilevel p06_funcptr \
         p07_const p08_void_null p09_arith p10_aliasing; do
    build_and_run 05_pointer "$f" strict
done
build_and_run 05_pointer p02_decay warn
# 严格别名：-O0 vs -O2
( cd 05_pointer
  echo "### -O0"; "$CC" -std=c17 -Wall -Wextra -Wpedantic -g -O0 p10_aliasing.c -o p10_O0 && ./p10_O0 | head -6
  echo "### -O2"; "$CC" -std=c17 -Wall -Wextra -Wpedantic -g -O2 p10_aliasing.c -o p10_O2 && ./p10_O2 | head -6
) > "$LOGDIR/05_pointer__aliasing_O0_vs_O2.log" 2>&1
say "  [ok] 05_pointer 严格别名 O0/O2 对比 -> $LOGDIR/05_pointer__aliasing_O0_vs_O2.log"

# ---------------------------------------------------------------- 06
head1 "06_struct"
for f in s01_basics s02_layout s03_deepcopy s04_bitfield_union s05_flexarray s06_list; do
    build_and_run 06_struct "$f" strict
done
build_and_run 06_struct s03_doublefree san

# ---------------------------------------------------------------- 07
head1 "07_memory"
for f in m01_regions m02_alloc m03_realloc_bug m04_leak; do
    build_and_run 07_memory "$f" strict
done
if command -v leaks >/dev/null 2>&1; then
    ( cd 07_memory && MallocStackLogging=1 leaks --atExit -- ./m04_leak 2>&1 \
        | sed -n '/leaks Report Version/,/Binary Images/p' ) \
        > "$LOGDIR/07_memory__leaks.log" 2>&1
    say "  [ok] 07_memory leaks 检测 -> $LOGDIR/07_memory__leaks.log"
fi

# ---------------------------------------------------------------- 08
head1 "08_ub （全部是错误示例）"
build_and_run 08_ub u01_signed_overflow warn
build_and_run 08_ub u01_signed_overflow san
build_and_run 08_ub u02_oob san stack
build_and_run 08_ub u03_uaf san read
build_and_run 08_ub u04_uninit warn
build_and_run 08_ub u05_nullderef san safe
build_and_run 08_ub u06_literal_write warn safe
build_and_run 08_ub u07_misc san shift
( cd 08_ub
  echo "### -O0"; "$CC" -std=c17 -Wall -Wextra -Wpedantic -Werror -g -O0 u08_optimizer.c -o u08_O0 && ./u08_O0 2000000000 2000000000
  echo; echo "### -O2"; "$CC" -std=c17 -Wall -Wextra -Wpedantic -Werror -g -O2 u08_optimizer.c -o u08_O2 && ./u08_O2 2000000000 2000000000
) > "$LOGDIR/08_ub__optimizer_O0_vs_O2.log" 2>&1
say "  [ok] 08_ub 优化器对比 -> $LOGDIR/08_ub__optimizer_O0_vs_O2.log"

# ---------------------------------------------------------------- 09
head1 "09_array_string"
build_and_run 09_array_string a01_arrays  strict
build_and_run 09_array_string a02_strings strict
build_and_run 09_array_string a03_overflow strict safe "0123456789ABCDEF"
build_and_run 09_array_string a03_overflow san unsafe "0123456789ABCDEF"

# ---------------------------------------------------------------- 10
head1 "10_project"
( cd 10_project && make clean >/dev/null && make && ./build/demo )   \
    > "$LOGDIR/10_project__make_run.log" 2>&1
PRC=$?
say "  [ok] 10_project make run exit=$PRC -> $LOGDIR/10_project__make_run.log"
( cd 10_project && make asan ) > "$LOGDIR/10_project__asan.log" 2>&1
say "  [ok] 10_project make asan -> $LOGDIR/10_project__asan.log"
if command -v leaks >/dev/null 2>&1; then
    ( cd 10_project && MallocStackLogging=1 leaks --atExit -- ./build/demo 2>&1 \
        | grep -E 'leaks for|nodes malloced' ) > "$LOGDIR/10_project__leaks.log" 2>&1
    say "  [ok] 10_project leaks -> $LOGDIR/10_project__leaks.log"
fi

# ---------------------------------------------------------------- summary
printf '\n\033[1m===== 汇总 =====\033[0m\n'
for line in "${SUMMARY[@]}"; do printf '  %s\n' "$line"; done
printf '\n  编译任务: %d 个   成功 %d   失败 %d\n' "$TOTAL" "$BUILD_OK" "$BUILD_FAIL"
printf '  完整日志: %s\n' "$LOGDIR"

if command -v leaks >/dev/null 2>&1 && [ -f "$LOGDIR/10_project__leaks.log" ]; then
    printf '  10_project 泄漏检测: %s\n' "$(grep 'leaks for' "$LOGDIR/10_project__leaks.log" | tail -1)"
fi

exit $((BUILD_FAIL > 0 ? 1 : 0))
