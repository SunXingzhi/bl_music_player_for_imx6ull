# 交叉编译: 使用 buildroot 输出的工具链与 pkg-config
# 用法:  make              编译 SRC_DIR 下所有 .c -> OUT_DIR 下同名可执行
#        make OUT_DIR=bin  改输出目录
#        make -j8          并行编译(各目标独立, 自动并行)
#        make clean        清理
#
# 可配置:
#   SRC_DIR  源文件目录     
#   OUT_DIR  可执行输出目录 
#   链接库   改 LDLIBS 所需的 pkg-config 模块名即可(如 dbus-1)

SRC_DIR ?= test
OUT_DIR ?= test

# buildroot 输出目录 (host/ 下是交叉工具链 + pkg-config wrapper)
BR_HOST := $(HOME)/Projects/IMX6ULL_learn/buildroot/output/host

# 交叉编译器与 pkg-config: 都必须用 buildroot 自带的
#   - gcc 自带 sysroot, 自动找 <alsa/asoundlib.h>
#   - pkg-config 是 wrapper, 只查 sysroot 的 .pc, 不会混入主机路径
CC       := $(BR_HOST)/bin/arm-none-linux-gnueabihf-gcc
PKG_CONF := $(BR_HOST)/bin/pkg-config

# 依赖通过 pkg-config 查询, 不硬编码 -I/-L
CFLAGS += -Wall -O2 $(shell $(PKG_CONF) --cflags alsa)
LDLIBS += $(shell $(PKG_CONF) --libs alsa)
LDLIBS += -lm

# 推导出同名目标列表
SRCS    := $(wildcard $(SRC_DIR)/*.c)
TARGETS := $(patsubst $(SRC_DIR)/%.c, $(OUT_DIR)/%, $(SRCS))

# 默认目标: 编译全部
all: $(TARGETS)

$(OUT_DIR)/%: $(SRC_DIR)/%.c | $(OUT_DIR)
	$(CC) $(CFLAGS) -o $@ $< $(LDLIBS)

# 创建输出目录
$(OUT_DIR):
	mkdir -p $@

clean:
	# 只删生成的可执行文件, 不删源文件 (SRC_DIR 与 OUT_DIR 可能是同一目录)
	rm -f $(TARGETS)

.PHONY: all clean
