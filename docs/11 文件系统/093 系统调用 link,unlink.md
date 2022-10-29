# 系统调用 link,unlink

完成以下系统调用：

```c++
// 创建文件硬链接
int link(char *oldname, char *newname);

// 删除文件
int unlink(char *filename);
```

`goto` 破坏了程序的结构化，Djikstra，程序里禁止使用 `goto`

C 语言没有异常机制：

try catch finally

我们希望，目录是一种树形结构，没有环，最次，有向无环图。
