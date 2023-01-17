# 问题及答案

## gcc -m32 链接错误

添加 `-m32` 指定 32 位模式时，可能会发生 `/usr/bin/ld: skipping incompatible ... *.so` 的错误，这时可能需要安装 32 位动态链接库：

    sudo pacman -S lib32-gcc-libs

-----

## 无法调试汇编代码

根据大家反馈的描述，可能存在的问题：

1. vscode 调试配置 `launch.json` 中调试目录 `cwd` 配置的不对，请尝试配置到 `${workspaceFolder}/src`，或者与 `makefile` 位置相同；

2. `nasm` 版本不对，参考相关版本 `nasm >= 2.15.05`，主要问题是 `-g` 生成的 DWARF 信息版本不对；

3. `vscode` 设置需要勾选 **Debug:Allow Breakpoints Everywhere**；

