# 打开 inode

主要完成以下函数：

```c++
// 打开文件，返回 inode，用于系统调用 open
inode_t *inode_open(char *pathname, int flag, int mode);
```