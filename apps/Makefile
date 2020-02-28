#    目标： 预置条件 
#    <TAB> 步骤
#    $@ 当前规则的目标文锦啊
#    $< 第一个预置条件
#    $^ 去重的预置条件
#    gcc -I前缀指定头文件路径

#获取根目录、编译变量、编译规则
ifeq "${ROOTDIR}" ""
export ROOTDIR=$(shell while true; do if [ -f XBEnv.mk ]; then pwd; exit; else cd ..;fi;done;)
endif
include ${ROOTDIR}/XBEnv.mk
include ${ROOTDIR}/XBMakeRules.mk

#定义变量
CC = gcc
XB_CFLAGS = -Wall 

#gcc宏定义
#gcc 中宏定义使用前缀-D,在编译过程中可以把宏定义追加到XB_CFLAGS中，有两种写法
#1. -D DEFINES
#2. -D DEFINES=CONDITION
DEFS = -DTEST_A -DTEST_B=1
XB_CFLAGS += ${DEFS}

#目标文件
XB_TARGET = test

#源文件
SRCS := $(wildcard *.c)      # $(wildcard pattern) 将所有 ".c"后缀的文件存入SRCS变量

#目标文件
OBJS := $(SRCS:%.c=%.o)        # 被称为替代引用 如果 SRCS的值伪 'foo.c bar.c' 那么 BINS 的值伪 'foo bar'

#库文件
#gcc共享库
#增加共享库使用-l前缀，请注意此处必须要小写 共享库查找目录试用 -L
#1. 系统默认的搜索库目录位于 /usr/lib 和 lib
#2. 若共享库不在系统默认库中，可通过-L 参数指定目录，该参数只有在链接过程中有效
#3. 共享库在链接过程中，而不是编译过程中
DLIBS = -ltest 							  # 加载 libtest.so
LDFLAGS = -L${ROOTDIR}/libs/         # 库查找目录
#指定运行时的库文件路径  linux上为 -rpath=<DIR>,在mac上为 -rpath,<DIR>
RPATH = -Wl,-rpath ${ROOTDIR}/libs/   # 上述 DLIBS和 LDFLAGS只有在链接时有效，运行时通过 -Wl, -rpath 指定共享库路径

#头文件查找目录
INC = -I${ROOTDIR}/include/     								
#.PHONY 用来定义这些不是文件的目标。make会默认调用这些伪目标下的步骤，而不去检查#文件存在或最后修改日期
#.PHONY: all clean

#链接为可执行文件
${XB_TARGET}:${OBJS}                # 调用${OBJS}变量中的所有值作为子目标
	#@echo ${ROOTDIR}
	${CC} -o $@ $^ ${LDFLAGS} ${DLIBS} ${RPATH}  

#清理编译文件
clean:
	rm -rvf ${OBJS} ${XB_TARGET} 

#编译规则
%.o:%.c
	${CC} ${XB_CFLAGS} ${INC}  -o $@ -c $<

#.DEFAULT_GOAL 仅能定义一个目标 如果不设置就会将第一个目标为默认目标
#.DEFAULT_GOAL := generate
