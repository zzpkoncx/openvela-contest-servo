# Makefile for desktop_pet
# 路径：apps/desktop_pet/Makefile
# openvela/NuttX 应用 Makefile 标准格式

CONFIG_APP_DESKTOP_PET ?= n

ifeq ($(CONFIG_APP_DESKTOP_PET),y)

# 主程序入口
PROGNAME = desktop_pet
PRIORITY = 100
STACKSIZE = 8192

# C 源文件
CSRCS = desktop_pet_main.c
ifeq ($(CONFIG_APP_DESKTOP_PET_FACE),y)
CSRCS += face_lcd.c
endif
ifeq ($(CONFIG_APP_DESKTOP_PET_SERVO),y)
CSRCS += servo_drv.c
endif
ifeq ($(CONFIG_APP_DESKTOP_PET_VOICE_BRIDGE),y)
CSRCS += voice_bridge.c
endif

# 头文件路径
CFLAGS += -I$(APPDIR)/external/desktop_pet
CFLAGS += -Wno-unused-function
CFLAGS += -Wno-unused-variable

# 依赖
ifeq ($(CONFIG_APP_DESKTOP_PET_FACE),y)
CFLAGS += -I$(TOPDIR)/drivers/lcd
endif

ifeq ($(CONFIG_APP_DESKTOP_PET_SERVO),y)
CFLAGS += -I$(TOPDIR)/drivers/ledc
endif

# 链接选项
LDADD += -lm

endif
