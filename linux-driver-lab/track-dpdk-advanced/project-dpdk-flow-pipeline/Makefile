# 根 Makefile 汇总 smoke、调优矩阵和错误边界三个验证入口。
.PHONY: all clean test test-tuning test-boundary test-all

all:
	$(MAKE) -C app

clean:
	$(MAKE) -C app clean
	rm -rf tests/runtime

test:
	bash tests/flow_pipeline_smoke.sh

test-tuning:
	bash tests/tuning_matrix_test.sh

test-boundary:
	bash tests/error_boundary_test.sh

test-all: test test-tuning test-boundary
	@echo DPDK_FLOW_PIPELINE_CURRENT_ENV_COMPLETE
