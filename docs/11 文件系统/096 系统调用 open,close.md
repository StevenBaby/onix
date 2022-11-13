# 系统调用 open,close

完成以下系统调用：

```c++
// 打开文件
fd_t open(char *filename, int flags, int mode);
// 创建普通文件
fd_t creat(char *filename, int mode);
// 关闭文件
void close(fd_t fd);
```
