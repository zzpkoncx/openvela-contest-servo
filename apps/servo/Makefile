# Makefile for servo (独立 NSH 命令，驱动 270° 舵机)
# 路径：apps/servo/Makefile
# openvela/NuttX 应用 Makefile 标准格式
#
# 为什么单独成一个 app：
#   voice_bridge / ai_agent 经 run_shell 调用 `servo set 90` 这类命令，
#   `servo` 必须是标准 NSH 内建命令（独立 PROGNAME）。舵机驱动逻辑与
#   desktop_pet 守护进程共享同一份 servo_drv.c（两份拷贝，构建各自链接）。

CONFIG_APP_SERVO ?= n

ifeq ($(CONFIG_APP_DESKTOP_PET_SERVO),y)

# 主程序入口（注册为 NSH 命令 `servo`）
PROGNAME = servo
PRIORITY = 100
STACKSIZE = 4096

# C 源文件：命令入口 + 舵机驱动
CSRCS = servo_cmd_main.c servo_drv.c

# 头文件路径（servo_drv.c / servo_cmd_main.c 与 desktop_pet.h 同目录）
CFLAGS += -I$(APPDIR)/external/servo
CFLAGS += -Wno-unused-function
CFLAGS += -Wno-unused-variable

# 依赖
ifeq ($(CONFIG_APP_DESKTOP_PET_SERVO),y)
CFLAGS += -I$(TOPDIR)/drivers/ledc
endif

# 链接选项
LDADD += -lm

endif
