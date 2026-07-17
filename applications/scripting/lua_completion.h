/* tab completion for lua with microrl. memory usage: 200 bytes stack. */

#ifndef LUA_COMPLETION_H
#define LUA_COMPLETION_H

#include <lua.h>
#include <microrl.h>

#define MAX_COMPLETIONS    8  /* max number of completions to display */
#define MAX_COMPLETION_LEN 80 /* max length of a completion */

extern lua_State *L;

extern int microrl_print(microrl_t *mrl, const char *str);
extern int microrl_get_completion(struct microrl *rl, char *buf, uint32_t buflen);

#endif
