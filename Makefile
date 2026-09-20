# Makefile —— 顶层入口
#
#   make            构建并运行所有实验（等价于 make all）
#   make quiet      同上，只打印汇总表
#   make project    只构建并运行 10_project
#   make asan       10_project 的 sanitizer 版本
#   make leaks      10_project 的泄漏检测（macOS）
#   make env        打印本机编译环境
#   make clean      清理所有生成物

EXP := c-experiments

.PHONY: all quiet project asan leaks env clean

all:
	@bash $(EXP)/run_all.sh

quiet:
	@bash $(EXP)/run_all.sh --quiet

project:
	@$(MAKE) -C $(EXP)/10_project run

asan:
	@$(MAKE) -C $(EXP)/10_project asan

leaks:
	@$(MAKE) -C $(EXP)/10_project >/dev/null
	@MallocStackLogging=1 leaks --atExit -- $(EXP)/10_project/build/demo 2>&1 \
		| grep -E 'leaks for|nodes malloced' || true

env:
	@echo "== uname =="      && uname -a
	@echo "== cc =="         && cc --version | head -3
	@echo "== C17 支持 ==" && printf '#include <stdio.h>\nint main(void){printf("%%ldL\\n",__STDC_VERSION__);return 0;}\n' \
		> /tmp/_c17probe.c && cc -std=c17 /tmp/_c17probe.c -o /tmp/_c17probe && /tmp/_c17probe
	@echo "== sanitizer ==" && (cc -std=c17 -fsanitize=address,undefined /tmp/_c17probe.c -o /tmp/_c17san 2>/dev/null \
		&& echo "ASan + UBSan: 可用" || echo "ASan + UBSan: 不可用")
	@echo "== leaks ==" && (command -v leaks >/dev/null && echo "leaks: 可用" || echo "leaks: 不可用")
	@echo "== valgrind ==" && (command -v valgrind >/dev/null && echo "valgrind: 可用" || echo "valgrind: 不可用（Apple Silicon 不支持）")
	@echo "== 栈大小 ==" && echo "soft: $$(ulimit -Ss) KB   hard: $$(ulimit -Hs) KB"

clean:
	@bash $(EXP)/run_all.sh --clean
