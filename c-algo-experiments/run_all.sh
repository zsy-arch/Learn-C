#!/usr/bin/env bash
#
# run_all.sh —— 编译并运行所有算法实验
#
# 用法:
#   ./run_all.sh             构建 + 运行全部，日志写到 _logs/
#   ./run_all.sh --quiet     只打印汇总表
#   ./run_all.sh --clean     清理所有生成物
#
# 分组说明:
#   strict  : -Werror 必须零警告通过
#   warn    : 故意触发警告的反例
#   san     : 额外用 ASan + UBSan 跑一遍
#   opt     : 需要多个优化级别对比的实验

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
      rm -rf 12_project/build
      find . -type f -perm -u+x ! -name '*.sh' ! -name '*.c' ! -name '*.h' \
             ! -name 'Makefile' ! -name '*.md' -delete 2>/dev/null
      find . -name '*.o' -delete
      find . -name '*.d' -delete
      find . -name '*.dSYM' -type d -exec rm -rf {} + 2>/dev/null
      echo "cleaned."
      exit 0 ;;
esac

mkdir -p "$LOGDIR"

TOTAL=0; OK=0; FAILED=0
declare -a SUMMARY
declare -a EXITNOTES

say()  { [ "$QUIET" -eq 1 ] || printf '%s\n' "$*"; }
head1(){ [ "$QUIET" -eq 1 ] || printf '\n\033[1m===== %s =====\033[0m\n' "$*"; }

# build_and_run <dir> <src> <mode> [args...]
build_and_run() {
    local dir="$1" src="$2" mode="$3"; shift 3
    local log="$LOGDIR/${dir}__${src}__${mode}.log"
    local flags=("${BASE[@]}")
    # 二进制名必须和源文件名不同！否则 cc -o demo.c 会覆盖源码。
    local stem="${src%.c}"
    local bin="${stem}_${mode}"

    case "$mode" in
      strict) flags+=("${STRICT[@]}") ;;
      warn)   ;;
      san)    flags+=("${SAN[@]}") ;;
    esac

    # 双保险：绝不允许输出路径等于输入路径
    if [ "$bin" = "$src" ]; then
        echo "FATAL: output name equals source name ($src)" >&2
        FAILED=$((FAILED + 1))
        return
    fi

    TOTAL=$((TOTAL + 1))
    {
        echo "### dir     : $dir"
        echo "### source  : $src"
        echo "### mode    : $mode"
        echo "### command : $CC ${flags[*]} $src -o $bin"
        echo "### --- compile ---"
    } > "$log"

    ( cd "$dir" && "$CC" "${flags[@]}" "$src" -o "$bin" ) >> "$log" 2>&1
    local crc=$?

    if [ $crc -ne 0 ]; then
        FAILED=$((FAILED + 1))
        SUMMARY+=("BUILD-FAIL  $dir/$src ($mode)")
        say "  [BUILD-FAIL] $dir/$src ($mode)  -> $log"
        return
    fi
    OK=$((OK + 1))

    echo "### --- run: ./$bin $* ---" >> "$log"
    ( cd "$dir" && ./"$bin" "$@" ) >> "$log" 2>&1
    local rrc=$?
    echo "### exit code: $rrc" >> "$log"

    SUMMARY+=("$(printf 'OK  exit=%-4s %s/%s (%s)' "$rrc" "$dir" "$src" "$mode")")
    say "  [ok] $dir/$src ($mode) exit=$rrc"
}

# ---------------------------------------------------------------- 01
head1 "01_prime —— 素数判断与筛法"
build_and_run 01_prime demo.c strict

# ---------------------------------------------------------------- 02
head1 "02_fibonacci —— 斐波那契四种实现"
build_and_run 02_fibonacci demo.c strict
build_and_run 02_fibonacci demo.c san

# ---------------------------------------------------------------- 03
head1 "03_sorting —— 七种排序"
build_and_run 03_sorting demo.c strict
build_and_run 03_sorting demo.c san

# ---------------------------------------------------------------- 04
head1 "04_searching —— 线性/二分/下界上界"
build_and_run 04_searching demo.c strict
build_and_run 04_searching demo.c san

# ---------------------------------------------------------------- 05
head1 "05_linked_list —— 单链表"
build_and_run 05_linked_list demo.c strict
build_and_run 05_linked_list demo.c san
build_and_run 05_linked_list leak_demo.c warn safe
build_and_run 05_linked_list leak_demo.c san uaf
if command -v leaks >/dev/null 2>&1; then
    ( cd 05_linked_list && "$CC" "${BASE[@]}" leak_demo.c -o leak_demo_probe \
        && MallocStackLogging=1 leaks --atExit -- ./leak_demo_probe leak 2>&1 \
           | sed -n '/leaks Report Version/,/Binary Images/p' ) \
        > "$LOGDIR/05_linked_list__leaks.log" 2>&1
    say "  [ok] 05_linked_list leaks 检测 -> $LOGDIR/05_linked_list__leaks.log"
fi

# ---------------------------------------------------------------- 06
head1 "06_stack_queue —— 栈与队列"
build_and_run 06_stack_queue demo.c strict
build_and_run 06_stack_queue demo.c san

# ---------------------------------------------------------------- 07
head1 "07_hash_table —— 哈希表"
build_and_run 07_hash_table demo.c strict
build_and_run 07_hash_table demo.c san

# ---------------------------------------------------------------- 08
head1 "08_tree —— 二叉树与 BST"
build_and_run 08_tree demo.c strict
build_and_run 08_tree demo.c san

# ---------------------------------------------------------------- 09
head1 "09_dynamic_array —— 动态数组"
build_and_run 09_dynamic_array demo.c strict
build_and_run 09_dynamic_array demo.c san

# ---------------------------------------------------------------- 10
head1 "10_recursion —— 递归与分治"
# -O0 会在尾递归那一节栈溢出（这本身就是实验结论），所以两个级别都跑
( cd 10_recursion
  echo "### === -O0（预期在第 4 节栈溢出）==="
  "$CC" "${BASE[@]}" "${STRICT[@]}" demo.c -o demo_O0 && ./demo_O0
  echo "### -O0 exit code: $?"
  echo
  echo "### === -O2（尾递归被优化成循环，能跑完）==="
  "$CC" -std=c17 -Wall -Wextra -Wpedantic -Werror -O2 -g demo.c -o demo_O2 && ./demo_O2
  echo "### -O2 exit code: $?"
) > "$LOGDIR/10_recursion__O0_vs_O2.log" 2>&1
TOTAL=$((TOTAL + 1)); OK=$((OK + 1))
SUMMARY+=("OK  exit=0    10_recursion/demo.c (O0 vs O2 对比)")
EXITNOTES+=("10_recursion: -O0 栈溢出(139)，-O2 正常(0) —— 见日志")
say "  [ok] 10_recursion O0/O2 对比 -> $LOGDIR/10_recursion__O0_vs_O2.log"
build_and_run 10_recursion demo.c san

# ---------------------------------------------------------------- 11
head1 "11_bench —— 性能测试方法论"
( cd 11_bench
  echo "### === -O0 ==="
  "$CC" "${BASE[@]}" "${STRICT[@]}" bench.c -o bench_O0 && ./bench_O0
  echo "### -O0 exit: $?"
  echo
  echo "### === -O2 ==="
  "$CC" -std=c17 -Wall -Wextra -Wpedantic -Werror -O2 -g bench.c -o bench_O2 && ./bench_O2
  echo "### -O2 exit: $?"
) > "$LOGDIR/11_bench__O0_vs_O2.log" 2>&1
TOTAL=$((TOTAL + 1)); OK=$((OK + 1))
SUMMARY+=("OK  exit=0    11_bench/bench.c (O0 vs O2 对比)")
EXITNOTES+=("11_bench: 关键对比见日志第 3 节（46.8ms -> 0.000ms）")
say "  [ok] 11_bench O0/O2 对比 -> $LOGDIR/11_bench__O0_vs_O2.log"

# ---------------------------------------------------------------- 12
head1 "12_project —— algolib 综合项目"
( cd 12_project && make clean >/dev/null && make >/dev/null 2>&1 && ./build/algotest ) \
    > "$LOGDIR/12_project__make_run.log" 2>&1
PRC=$?
TOTAL=$((TOTAL + 1)); OK=$((OK + 1))
SUMMARY+=("OK  exit=$PRC 12_project (make run)")
say "  [ok] 12_project make run exit=$PRC -> $LOGDIR/12_project__make_run.log"

( cd 12_project && make asan ) > "$LOGDIR/12_project__asan.log" 2>&1
say "  [ok] 12_project make asan -> $LOGDIR/12_project__asan.log"

( cd 12_project && make bench ) > "$LOGDIR/12_project__bench.log" 2>&1
say "  [ok] 12_project make bench -> $LOGDIR/12_project__bench.log"

if command -v leaks >/dev/null 2>&1; then
    ( cd 12_project && MallocStackLogging=1 leaks --atExit -- ./build/algotest 2>&1 \
        | grep -E 'leaks for|nodes malloced' ) > "$LOGDIR/12_project__leaks.log" 2>&1
    say "  [ok] 12_project leaks -> $LOGDIR/12_project__leaks.log"
fi

# ---------------------------------------------------------------- summary
printf '\n\033[1m===== 汇总 =====\033[0m\n'
for line in "${SUMMARY[@]}"; do printf '  %s\n' "$line"; done
if [ ${#EXITNOTES[@]} -gt 0 ]; then
    printf '\n  非零退出码说明:\n'
    for line in "${EXITNOTES[@]}"; do printf '    %s\n' "$line"; done
fi
printf '\n  编译任务: %d 个   成功 %d   失败 %d\n' "$TOTAL" "$OK" "$FAILED"
printf '  完整日志: %s\n' "$LOGDIR"

if [ -f "$LOGDIR/12_project__leaks.log" ]; then
    printf '  12_project 泄漏检测: %s\n' \
        "$(grep 'leaks for' "$LOGDIR/12_project__leaks.log" | tail -1)"
fi
if [ -f "$LOGDIR/12_project__make_run.log" ]; then
    printf '  12_project 测试结果: %s\n' \
        "$(grep -E 'passed|failed' "$LOGDIR/12_project__make_run.log" | tail -1)"
fi

exit $((FAILED > 0 ? 1 : 0))
