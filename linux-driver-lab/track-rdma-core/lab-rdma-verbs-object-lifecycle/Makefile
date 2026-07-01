APP := build/rdma-lifecycle
SOURCES := src/main.c src/rdma_device.c src/rdma_memory.c src/rdma_queue.c
OBJECTS := $(patsubst src/%.c,build/%.o,$(SOURCES))

# 优先从 pkg-config 获取发行版相关头文件和链接参数。
PKG_CFLAGS := $(shell pkg-config --cflags libibverbs 2>/dev/null)
PKG_LIBS := $(shell pkg-config --libs libibverbs 2>/dev/null)

CPPFLAGS += -Iinclude $(PKG_CFLAGS)
CFLAGS ?= -O2 -g
CFLAGS += -std=c11 -Wall -Wextra -Wpedantic
LDLIBS += $(if $(strip $(PKG_LIBS)),$(PKG_LIBS),-libverbs)

.PHONY: all test clean

all: $(APP)

# 链接阶段只组合对象文件和 libibverbs，不引入额外运行时框架。
$(APP): $(OBJECTS)
	$(CC) $(LDFLAGS) -o $@ $^ $(LDLIBS)

# 头文件变化会触发所有模块重新编译，避免结构定义与对象文件不一致。
build/%.o: src/%.c include/rdma_resources.h | build
	$(CC) $(CPPFLAGS) $(CFLAGS) -c -o $@ $<

build:
	mkdir -p $@

test: all
	bash tests/lifecycle_test.sh

clean:
	rm -rf build
