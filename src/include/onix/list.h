#ifndef ONIX_LIST_H
#define ONIX_LIST_H

#include <onix/types.h>

#define element_offset(type, member) (u32)(&((type *)0)->member)
#define element_entry(type, member, ptr) (type *)((u32)ptr - element_offset(type, member))
#define element_node_offset(type, node, key) ((int)(&((type *)0)->key) - (int)(&((type *)0)->node))
#define element_node_key(node, offset) *(int *)((int)node + offset)

// 链表结点
typedef struct list_node_t
{
    struct list_node_t *prev; // 下一个结点
    struct list_node_t *next; // 前一个结点
} list_node_t;

// 链表
typedef struct list_t
{
    list_node_t head; // 头结点
    list_node_t tail; // 尾结点
} list_t;

// 初始化链表
void list_init(list_t *list);

// 在 anchor 结点前插入结点 node
void list_insert_before(list_node_t *anchor, list_node_t *node);

// 在 anchor 结点后插入结点 node
void list_insert_after(list_node_t *anchor, list_node_t *node);

// 插入到头结点后
void list_push(list_t *list, list_node_t *node);

// 移除头结点后的结点
list_node_t *list_pop(list_t *list);

// 插入到尾结点前
void list_pushback(list_t *list, list_node_t *node);

// 移除尾结点前的结点
list_node_t *list_popback(list_t *list);

// 查找链表中结点是否存在
bool list_search(list_t *list, list_node_t *node);

// 从链表中删除结点
void list_remove(list_node_t *node);

// 判断链表是否为空
bool list_empty(list_t *list);

// 获得链表长度
u32 list_size(list_t *list);

// 链表插入排序
void list_insert_sort(list_t *list, list_node_t *node, int offset);

#endif