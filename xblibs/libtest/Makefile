#说明
#1. 目标未libtest.so,请务必以lib开头.so结尾，在使用该共享库时系统会自动的去除lib和.so，使用时写成-ltest
#2. 在链接过程中加入-shared参数，意为共享形式的目标文件  链接过程加入 -shared
#3. 在编译过程中加入-fPIC参数，意为生成和地址无关的目标文件 编译过程加入 -fPIC
#4. 在链接完成之后，通过mv指令把libtest.so移动到同级的lib目录中

#获取根目录、编译变量、编译规则
ifeq "${ROOTDIR}" ""
export ROOTDIR=$(shell while true; do if [ -f XBEnv.mk ]; then pwd; exit; else cd ..;fi;done;)
endif
include ${ROOTDIR}/XBEnv.mk
include ${ROOTDIR}/XBMakeRules.mk

# 指令编译器和选项
CC = gcc
CFLAGS = -Wall

# 目标文件
TARGET = libtest.so

#源文件
SRCS := $(wildcard *.c)

#目标文件
OBJS := $(SRCS:%.c=%.o) 

#链接 这里需要优化
$(TARGET):$(OBJS)
	$(CC) -shared -o $@ $^
	cp -f $(TARGET) ${ROOTDIR}/libs/
	cp -f libtest.h ${ROOTDIR}/include/   

clean:
	rm -rf $(TARGET) $(OBJS)

#编译规则
%.o:%.c
	$(CC) $(CFLAGS) -o $@ -fPIC -c $<
