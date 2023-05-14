
CFLAGS:= -m32 # 32 位的程序
CFLAGS+= -Qn # 去掉 gcc 版本信息
# CFLAGS+= -fno-builtin	# 不需要 gcc 内置函数
# CFLAGS+= -nostdinc		# 不需要标准头文件
CFLAGS+= -fno-pic		# 不需要位置无关的代码  position independent code
# CFLAGS+= -fno-pie		# 不需要位置无关的可执行程序 position independent executable
# CFLAGS+= -nostdlib		# 不需要标准库
CFLAGS+= -fno-stack-protector	# 不需要栈保护
CFLAGS+= -fomit-frame-pointer	# 不需要栈帧
CFLAGS+= -mpreferred-stack-boundary=2	# 不需要栈对齐
CFLAGS+= -fno-asynchronous-unwind-tables	# 不需要 CFI 信息
CFLAGS:=$(strip ${CFLAGS})

.PHONY: hello.s
hello.s: hello.c
	gcc $(CFLAGS) -S $< -o $@

.PHONY: params.s
params.s: params.c
	gcc $(CFLAGS) -S $< -o $@


.PHONY: clean
clean:
	rm -rf *.o
	rm -rf *.out
