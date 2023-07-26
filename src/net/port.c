#include <onix/net/port.h>
#include <onix/memory.h>
#include <onix/assert.h>

int port_get(port_map_t *map, u16 port)
{
    if (port == 0)
    {
        port = 4096;
        while (bitmap_test(&map->map, port++))
            ;
        bitmap_set(&map->map, port, true);
        return port;
    }

    if (bitmap_test(&map->map, port))
    {
        return -EOCCUPIED;
    }

    bitmap_set(&map->map, port, true);
    return port;
}

void port_put(port_map_t *map, u16 port)
{
    assert(port > 0);
    assert(bitmap_test(&map->map, port));
    bitmap_set(&map->map, port, false);
}

void port_init(port_map_t *map)
{
    map->buf = alloc_kpage(2);
    bitmap_init(&map->map, (char *)map->buf, PAGE_SIZE * 2, 0);
}