#include <rtthread.h>
#define DBG_TAG "LUA"
#define DBG_LVL DBG_INFO
#include <rtdbg.h>

#include "lua.h"
#include "lauxlib.h"
#include "lualib.h"
#include "lua_local.h"

static struct rt_memheap lua_memheap;
static uint8_t          *lua_heap_memory = RT_NULL;

/* use memheap */

void *lua_alloc(void *ud, void *ptr, size_t osize, size_t nsize)
{
    if (nsize == 0)
    {
        if (ptr)
            rt_memheap_free(ptr);
        return RT_NULL;
    }

    if (ptr == RT_NULL)
        return rt_memheap_alloc(&lua_memheap, nsize);

    return rt_memheap_realloc(&lua_memheap, ptr, nsize);
}


int lua_heap_init()
{
    lua_heap_memory = rt_malloc(LUA_HEAP_SIZE);
    if (!lua_heap_memory)
    {
        LOG_E("heap malloc(%d) fail", LUA_HEAP_SIZE);
        return 1;
    }

    if (rt_memheap_init(&lua_memheap, "lua", lua_heap_memory, LUA_HEAP_SIZE) != RT_EOK)
    {
        LOG_E("rt_memheap_init fail");
        return 1;
    }

    return 0;
}

void lua_memheap_info(rt_size_t *total, rt_size_t *used, rt_size_t *max_used)
{
    rt_memheap_info(&lua_memheap, total, used, max_used);
}
