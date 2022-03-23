# 问题及答案

## gcc -m32 链接错误

添加 `-m32` 指定 32 位模式时，可能会发生 `/usr/bin/ld: skipping incompatible ... *.so` 的错误，这时可能需要安装 32 位动态链接库：

    sudo pacman -S lib32-gcc-libs

-----
