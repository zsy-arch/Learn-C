#!/usr/bin/env bash
# run_all_tests.sh —— 一次性编译并运行 c-algo-advanced-experiments 下六个专题的
# 全部测试套件（常规模式 + sanitizer 模式），汇总真实的通过/失败数字。
#
# 用法：
#   ./run_all_tests.sh          跑常规模式 + sanitizer 模式
#   ./run_all_tests.sh --quick  只跑常规模式，跳过 sanitizer（更快）

set -u

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
EXPERIMENTS_DIR="$SCRIPT_DIR/c-algo-advanced-experiments"

DIRS=(
    01_binary_tree_traversal
    02_graph_traversal
    03_trie
    04_b_tree
    05_b_plus_tree
    06_red_black_tree
)

QUICK=0
if [[ "${1:-}" == "--quick" ]]; then
    QUICK=1
fi

TOTAL_PASS=0
TOTAL_FAIL=0
FAILED_DIRS=()

echo "========================================"
echo " c-algo-advanced-experiments 测试汇总"
echo "========================================"

for d in "${DIRS[@]}"; do
    dir="$EXPERIMENTS_DIR/${d}"
    if [[ ! -d "$dir" ]]; then
        echo "[跳过] ${d} 目录不存在"
        continue
    fi

    echo ""
    echo "---- ${d}：常规模式 ----"
    ( cd "$dir" && make clean >/dev/null 2>&1 && make tests >/dev/null 2>&1 )
    if [[ $? -ne 0 ]]; then
        echo "[编译失败] ${d}（常规模式）"
        FAILED_DIRS+=("${d} (编译失败-常规)")
        continue
    fi

    out="$(cd "$dir" && ./tests 2>&1)"
    echo "$out" | tail -5

    # 汇总行的两种真实格式：
    #   "通过: N, 失败: M, 总计: K"
    #   "PASS: N, FAIL: M, TOTAL: K"
    line="$(echo "$out" | grep -E "(通过|PASS):" | tail -1)"
    pass_n="$(echo "$line" | grep -oE '(通过|PASS): *[0-9]+' | grep -oE '[0-9]+')"
    fail_n="$(echo "$line" | grep -oE '(失败|FAIL): *[0-9]+' | grep -oE '[0-9]+')"

    if [[ -z "$pass_n" ]]; then
        echo "[无法解析测试汇总] ${d}"
        FAILED_DIRS+=("${d} (无法解析汇总)")
        continue
    fi

    TOTAL_PASS=$((TOTAL_PASS + pass_n))
    TOTAL_FAIL=$((TOTAL_FAIL + ${fail_n:-0}))

    if [[ "${fail_n:-0}" != "0" ]]; then
        FAILED_DIRS+=("${d} (${fail_n} 个测试失败)")
    fi

    if [[ $QUICK -eq 0 ]]; then
        echo "---- ${d}：sanitizer 模式 ----"
        ( cd "$dir" && make san >/dev/null 2>&1 )
        if [[ $? -ne 0 ]]; then
            echo "[编译失败] ${d}（sanitizer 模式，可能该目录未提供 san 目标）"
        else
            san_bin="$dir/tests_san"
            if [[ -x "$san_bin" ]]; then
                ( cd "$dir" && ./tests_san >/dev/null 2>&1 )
                san_exit=$?
                if [[ $san_exit -ne 0 ]]; then
                    echo "[sanitizer 运行时报错] ${d}，退出码 $san_exit"
                    FAILED_DIRS+=("${d} (sanitizer 运行时报错)")
                else
                    echo "sanitizer 版本运行正常，退出码 0"
                fi
            fi
        fi
    fi
done

echo ""
echo "========================================"
echo " 总计：通过 ${TOTAL_PASS}，失败 ${TOTAL_FAIL}"
if [[ ${#FAILED_DIRS[@]} -eq 0 ]]; then
    echo " 结果：全部通过"
    echo "========================================"
    exit 0
else
    echo " 存在问题的目录："
    for f in "${FAILED_DIRS[@]}"; do
        echo "   - $f"
    done
    echo "========================================"
    exit 1
fi
