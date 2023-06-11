#ifndef ONIX_SKIP_LIST_H
#define ONIX_SKIP_LIST_H

#include <onix/types.h>

#define element_offset(type, member) (u32)(&((type *)0)->member)
#define element_entry(type, member, ptr) (type *)((u32)ptr - element_offset(type, member))
#define element_node_offset(type, node, key) ((int)(&((type *)0)->key) - (int)(&((type *)0)->node))
#define element_node_key(node, offset) *(int *)((int)node + offset)

#define SKIPLIST_MAX_LEVEL 32 // 限制最大高度

typedef struct skiplist_node_t
{
  struct skiplist_node_t *nexts[SKIPLIST_MAX_LEVEL]; // 指针数组
} skiplist_node_t;

static const double probability = 0.5; // 层数增加的概率

// left > right return 1
// left == right return 0
// left < right return -1
typedef int (*cmp_f)(skiplist_node_t *, skiplist_node_t *);

typedef struct skiplist_t
{
  skiplist_node_t head;
  cmp_f cmp_cb;  // 用于比较的回调函数
  int size;      // 有效节点数
  int max_level; // 有效最高层数
} skiplist_t;

// 初始化跳表
void skip_list_init(skiplist_t *list, cmp_f cb);

// 增加节点
void skip_list_put(skiplist_t *list, skiplist_node_t *node);

// 删除节点
void skip_list_remove(skiplist_t *list, skiplist_node_t *node);

// 节点是否存在
bool skip_list_contains(skiplist_t *list, skiplist_node_t *node);

bool skip_list_empty(skiplist_t *list);

// 第一个节点，可能为 NULL
skiplist_node_t *skip_list_first_node(skiplist_t *list);

// 最后一个节点，可能为 NULL
skiplist_node_t *skip_list_last_node(skiplist_t *list);

// 返回 >= node 最接近 node 的节点，可能为 NULL
skiplist_node_t *skip_list_ceiling_node(skiplist_t *list, skiplist_node_t *node);

// 返回 <= node 最接近 node 的节点，可能为 NULL
skiplist_node_t *skip_list_floor_node(skiplist_t *list, skiplist_node_t *node);

#endif