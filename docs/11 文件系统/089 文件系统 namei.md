# 文件系统 namei

主要完成以下函数：

```c++
// 获取 pathname 对应的父目录 inode
static inode_t *named(char *pathname, char **next);

// 获取 pathname 对应的 inode
static inode_t *namei(char *pathname);
```
