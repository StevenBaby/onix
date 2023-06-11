#include <onix/skiplist.h>
#include <onix/types.h>
#include <onix/stdlib.h>
#include <onix/stdio.h>

void skip_list_init(skiplist_t *list, cmp_f cb)
{
  for (int i = 0; i < SKIPLIST_MAX_LEVEL; i++)
  {
    list->head.nexts[i] = NULL;
  }
  list->cmp_cb = cb;
  list->size = 0;
  list->max_level = 0;
}

// 在第level层里，往右移动
// 现在来到的节点是cur，来到了cur的level层，在level层上，找到 < key 最后一个节点并返回
static skiplist_node_t *pre_node_in_level(skiplist_t *list, skiplist_node_t *node, skiplist_node_t *cur, int level)
{
  skiplist_node_t *next = cur->nexts[level];
  while (next != NULL && list->cmp_cb(next, node) < 0)
  {
    cur = next;
    next = cur->nexts[level];
  }
  return cur;
}

// 从最高层开始，一路找下去，
// 最终，找到第 0 层 < key 的最右的节点
static skiplist_node_t *pre_node_in_tree(skiplist_t *list, skiplist_node_t *node)
{
  if (node == NULL)
  {
    return NULL;
  }

  int level = list->max_level;
  skiplist_node_t *cur = &list->head;
  while (level >= 0)
  {
    // 从上层往下层找
    cur = pre_node_in_level(list, node, cur, level--);
  }
  return cur;
}

bool skip_list_empty(skiplist_t *list)
{
  return list->size == 0;
}

// 是否包含这个节点
bool skip_list_contains(skiplist_t *list, skiplist_node_t *node)
{
  if (node == NULL)
  {
    return true;
  }
  skiplist_node_t *pre = pre_node_in_tree(list, node);
  skiplist_node_t *next = pre->nexts[0];
  while (next != NULL && list->cmp_cb(next, node) == 0)
  {
    if (next == node)
    {
      // key 相同，找到地址相同的节点
      return true;
    }
    next = next->nexts[0];
  }
  return false;
}

// 新增节点
void skip_list_put(skiplist_t *list, skiplist_node_t *node)
{
  if (node == NULL)
  {
    return;
  }

  list->size++;
  int newNodeLevel = 0;

  while ((rand() & 0xffff) < (probability * 0xffff) && newNodeLevel < SKIPLIST_MAX_LEVEL - 1)
  {
    // 随机生成层数，每次增加的概率为 probability
    newNodeLevel++;
  }

  while (newNodeLevel > list->max_level)
  {
    list->max_level++;
  }
  int level = list->max_level;
  skiplist_node_t *pre = &list->head;
  while (level >= 0)
  {
    // 在 level 层中，找到最右的 < key 的节点
    pre = pre_node_in_level(list, node, pre, level);
    if (level <= newNodeLevel)
    {
      // 将两个相邻的节点连接
      node->nexts[level] = pre->nexts[level];
      pre->nexts[level] = node;
    }
    level--;
  }
}

// 删除节点
void skip_list_remove(skiplist_t *list, skiplist_node_t *node)
{
  if (skip_list_contains(list, node))
  {
    list->size--;
    int level = list->max_level;
    skiplist_node_t *pre = &list->head;
    while (level >= 0)
    {
      pre = pre_node_in_level(list, node, pre, level);
      skiplist_node_t *next = pre->nexts[level];
      while (next != NULL && list->cmp_cb(next, node) == 0)
      {
        if (next == node)
        {
          // 从 key 值相同的节点中，找到地址相同的
          break;
        }
        next = next->nexts[0];
      }
      if (next != NULL && list->cmp_cb(next, node) == 0)
      {
        pre->nexts[level] = next->nexts[level];
      }
      // 在level层只有一个节点了，就是默认节点head
      if (level != 0 && pre == &list->head && pre->nexts[level] == NULL)
      {
        // 删除 level
        list->head.nexts[level] = NULL;
        list->max_level--;
      }
      level--;
    }
  }
}

// 返回第一个节点
skiplist_node_t *skip_list_first_node(skiplist_t *list)
{
  return list->head.nexts[0];
}

// 返回最后一个节点
skiplist_node_t *skip_list_last_node(skiplist_t *list)
{
  int level = list->max_level;
  skiplist_node_t *cur = &list->head;
  while (level >= 0)
  {
    skiplist_node_t *next = cur->nexts[level];
    while (next != NULL)
    {
      cur = next;
      next = cur->nexts[level];
    }
    level--;
  }
  return cur;
}

// 返回 >= node 最接近 node 的节点
skiplist_node_t *skip_list_ceiling_node(skiplist_t *list, skiplist_node_t *node)
{
  skiplist_node_t *less = pre_node_in_tree(list, node);
  skiplist_node_t *next = less->nexts[0];
  return next;
}

// 返回 <= node 最接近 node 的节点
skiplist_node_t *skip_list_floor_node(skiplist_t *list, skiplist_node_t *node)
{
  skiplist_node_t *less = pre_node_in_tree(list, node);
  skiplist_node_t *next = less->nexts[0];
  while (next != NULL && list->cmp_cb(next, node) == 0)
  {
    less = next;
    next = next->nexts[0];
  }
  return list->cmp_cb(next, node) == 0 ? next : less;
}