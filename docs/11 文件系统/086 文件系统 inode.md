# 文件系统 inode

主要完成以下几个函数：

```c++
inode_t *iget(dev_t dev, idx_t nr); // 获得设备 dev 的 nr inode
void iput(inode_t *inode);          // 释放 inode

// 获取 inode 第 block 块的索引值
// 如果不存在 且 create 为 true，则创建
idx_t bmap(inode_t *inode, idx_t block, bool create);
```
