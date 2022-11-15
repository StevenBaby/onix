# 系统调用 getcwd,chdir

完成以下系统调用：

```c++
// 获取当前路径
char *getcwd(char *buf, size_t size);
// 切换当前目录
int chdir(char *pathname);
// 切换根目录
int chroot(char *pathname);
```
