#include <rtthread.h>
#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>
#include <stdbool.h>
#include <string.h>

#include "free-dap/dap.h"

/* lua cmsis-dap library

Example:
msh />lua
> dap.init()
> request=string.char(0) .. string.char(2) .. string.rep("\0", 62)
> dap.process_request(request)
Generic CMSIS-DAP Adapter

*/

#define DAP_PACKET_SIZE 64

static const char l_dap_help_str[] = "\n\
dap.help()\n\
dap.init()\n\
dap.process_request(str)\
";

static int l_dap_help(lua_State *L)
{
    lua_pushboolean(L, 1);
    lua_pushstring(L, l_dap_help_str);
    return 2;
}

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
static const struct luaL_Reg dap_lib[] = {
    {           "help",            l_dap_help},
    {           "init",            l_dap_init},
    {"process_request", l_dap_process_request},
    {             NULL,                  NULL}
};

/* called from lua init, registers dap library */
int luaopen_dap(lua_State *L)
{
    luaL_newlib(L, dap_lib);
    return 1;
}

