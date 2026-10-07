# 交叉编译: 使用 buildroot 输出的工具链与 sysroot
# 用法: make        编译
#       make clean  清理

# buildroot 输出目录 (host/ 下是交叉工具链 + pkg-config wrapper)
BR_HOST := $(HOME)/Projects/IMX6ULL_learn/buildroot/output/host

# 交叉编译器与 pkg-config: 都必须用 buildroot 自带的
#   - gcc 自带 sysroot, 自动找 <alsa/asoundlib.h>
#   - pkg-config 是 wrapper, 只查 sysroot 的 .pc, 不会混入主机路径
CC       := $(BR_HOST)/bin/arm-none-linux-gnueabihf-gcc
PKG_CONF := $(BR_HOST)/bin/pkg-config

# 依赖通过 pkg-config 查询, 不硬编码 -I/-L
CFLAGS  += -Wall -O2 $(shell $(PKG_CONF) --cflags alsa)
LDLIBS  += $(shell $(PKG_CONF) --libs alsa)

TARGET := sound_test
SRC    := sound_test.c

$(TARGET): $(SRC)
	$(CC) $(CFLAGS) -o $@ $< $(LDLIBS)

clean:
	rm -f $(TARGET)

.PHONY: clean
