
#include <onix/list.h>
#include <onix/assert.h>

// 初始化链表
void list_init(list_t *list)
{
    list->head.prev = NULL;
    list->tail.next = NULL;
    list->head.next = &list->tail;
    list->tail.prev = &list->head;
}

// 在 anchor 结点前插入结点 node
void list_insert_before(list_node_t *anchor, list_node_t *node)
{
    node->prev = anchor->prev;
    node->next = anchor;

    anchor->prev->next = node;
    anchor->prev = node;
}

// 在 anchor 结点后插入结点 node
void list_insert_after(list_node_t *anchor, list_node_t *node)
{
    node->prev = anchor;
    node->next = anchor->next;

    anchor->next->prev = node;
    anchor->next = node;
}

// 插入到头结点后
void list_push(list_t *list, list_node_t *node)
{
    assert(!list_search(list, node));
    list_insert_after(&list->head, node);
}

// 移除头结点后的结点
list_node_t *list_pop(list_t *list)
{
    assert(!list_empty(list));

    list_node_t *node = list->head.next;
    list_remove(node);

    return node;
}

// 插入到尾结点前
void list_pushback(list_t *list, list_node_t *node)
{
    assert(!list_search(list, node));
    list_insert_before(&list->tail, node);
}

// 移除尾结点前的结点
list_node_t *list_popback(list_t *list)
{
    assert(!list_empty(list));

    list_node_t *node = list->tail.prev;
    list_remove(node);

    return node;
}

// 查找链表中结点是否存在
bool list_search(list_t *list, list_node_t *node)
{
    list_node_t *next = list->head.next;
    while (next != &list->tail)
    {
        if (next == node)
            return true;
        next = next->next;
    }
    return false;
}

// 从链表中删除结点
void list_remove(list_node_t *node)
{
    assert(node->prev != NULL);
    assert(node->next != NULL);

    node->prev->next = node->next;
    node->next->prev = node->prev;
    node->next = NULL;
    node->prev = NULL;
}

// 判断链表是否为空
bool list_empty(list_t *list)
{
    return (list->head.next == &list->tail);
}

// 获得链表长度
u32 list_size(list_t *list)
{
    list_node_t *next = list->head.next;
    u32 size = 0;
    while (next != &list->tail)
    {
        size++;
        next = next->next;
    }
    return size;
}
