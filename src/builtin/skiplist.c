#include <onix/skiplist.h>
#include <onix/stdlib.h>
#include <onix/stdio.h>
#include <onix/syscall.h>

typedef struct data_t
{
  int key;
  skiplist_node_t node;
} data_t;

static int compare_key(skiplist_node_t *lhs, skiplist_node_t *rhs)
{
  int first = element_node_key(lhs, element_node_offset(data_t, node, key));
  int second = element_node_key(rhs, element_node_offset(data_t, node, key));
  if (first > second)
  {
    return 1;
  }
  else if (first < second)
  {
    return -1;
  }
  else
  {
    return 0;
  }
}

void skip_list_dump(skiplist_t *list)
{
  for (int i = list->max_level; i >= 0; i--)
  {
    printf("Level %d:", i);
    skiplist_node_t *cur = &list->head;
    while (cur->nexts[i] != NULL)
    {
      skiplist_node_t *next = cur->nexts[i];
      int offset = element_node_offset(data_t, node, key);
      printf("(%d)", element_node_key(next, offset));
      cur = next;
    }
    printf("\n");
  }
}

void test_skiplist()
{
  int count = 20;
  data_t d[count];

  skiplist_t list;

  skip_list_init(&list, compare_key);

  for (size_t i = 0; i < count / 2; i++)
  {
    d[i].key = rand() % count;
    skip_list_put(&list, &d[i].node);
  }

  for (size_t i = count / 2; i < count; i++)
  {
    d[i].key = rand() % count;
    skip_list_put(&list, &d[i].node);
  }

  skip_list_dump(&list);

  data_t x;
  x.key = 9;

  skiplist_node_t *ceil = skip_list_ceiling_node(&list, &x.node);
  printf("\nCur:%d, Ceil:%d, Address:%p\n", x.key, element_node_key(ceil, element_node_offset(data_t, node, key)), ceil);

  skiplist_node_t *floor = skip_list_floor_node(&list, &x.node);
  printf("\nCur:%d, Floor:%d, Address:%p\n", x.key, element_node_key(floor, element_node_offset(data_t, node, key)), floor);

  sleep(5000);

  for (int i = count; i >= 0; i--)
  {
    printf("\x1b[2J\x1b[0;0H"); // clear
    skip_list_dump(&list);
    printf("\nNext time remove key (%d)\n", d[i].key);
    skip_list_remove(&list, &d[i].node);
    sleep(1000);
  }
}

int main()
{
  test_skiplist();
  return 0;
}