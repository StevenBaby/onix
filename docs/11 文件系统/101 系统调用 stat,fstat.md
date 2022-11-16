# 系统调用 stat,fstat

完成以下系统调用：

```c++
// 获取文件状态
int stat(char *filename, stat_t *statbuf);
int fstat(fd_t fd, stat_t *statbuf);
```
