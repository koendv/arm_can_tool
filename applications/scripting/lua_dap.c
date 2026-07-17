#include <rtthread.h>
#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>
#include <stdbool.h>
#include <string.h>

#include "lua_local.h"
#include "free-dap/dap.h"

/* lua cmsis-dap library */

#define DAP_PACKET_SIZE 64

/* lua version of dap init */
static int l_dap_init(lua_State *L)
{
    dap_init();
    return 0;
}

/* lua version of dap process request */
static int l_dap_process_request(lua_State *L)
{
    size_t      len;
    char        app_response_buffer[DAP_PACKET_SIZE] = {0};
    const char *app_request_buffer;

    app_request_buffer = luaL_checklstring(L, 1, &len);
    if (len != DAP_PACKET_SIZE)
    {
        return luaL_error(L, "Expected 64-byte string");
    }

    uint32_t retval = dap_process_request((uint8_t *)app_request_buffer, DAP_PACKET_SIZE, app_response_buffer, DAP_PACKET_SIZE);
    lua_pushlstring(L, app_response_buffer, retval);

    return 1;
}

/* dap library */
static const rotable_Reg dap_lib[] = {
    {           "init",            l_dap_init, 0},
    {"process_request", l_dap_process_request, 0},
    {             NULL,                  NULL, 0}
};

int luaopen_dap_rotable(lua_State *L)
{
    rotable_newlib(L, dap_lib);
    lua_setglobal(L, "dap");
    return 0;
}
