/*
** lua_completion.c — tab completion for Lua with microrl.
**
** Completes globals and one level of table/rotable members.
** Candidates are buffered and printed in one write after the loop,
** only when there is more than one match.
**
** For globals, _G is iterated first (lua_next), then the _BASE rotable
** is iterated to cover base library functions registered via
** luaopen_base_ro(). This means overridden globals (e.g. print) may
** appear twice in the candidate list; the common-prefix logic is
** unaffected.
**
** Memory usage: ~250 bytes stack (obuf 160 + completion 80 + misc).
*/

#include <string.h>
#include <ctype.h>
#include "lua_completion.h"
#include "rotable.h"

#define OBUF_SIZE 160

/* -----------------------------------------------------------------------
** process_candidate — update common prefix; buffer the candidate.
**
** 'completion' holds the bare common prefix — no suffix hint.
** If the candidate does not fit in obuf, sets *overflow; the common
** prefix is still updated so the suffix injection remains correct.
** --------------------------------------------------------------------- */
static void process_candidate(const char *key,
                              char        completion[MAX_COMPLETION_LEN],
                              char *obuf, size_t *opos,
                              int *overflow, int *count)
{
    if (*count == 0)
    {
        strncpy(completion, key, MAX_COMPLETION_LEN - 1);
        completion[MAX_COMPLETION_LEN - 1] = '\0';
    }
    else
    {
        /* shrink to longest common prefix */
        size_t i = 0;
        while (i < MAX_COMPLETION_LEN - 1 && completion[i] && key[i] && completion[i] == key[i])
            i++;
        completion[i] = '\0';
    }

    /* append "key\r\n" to obuf — no stack buffer; flag overflow if it does not fit */
    if (!*overflow)
    {
        size_t keylen = strlen(key);
        if (keylen + 2 > OBUF_SIZE - 1 - *opos)
        {
            *overflow = 1;
        }
        else
        {
            memcpy(obuf + *opos, key, keylen);
            *opos           += keylen;
            obuf[(*opos)++]  = '\r';
            obuf[(*opos)++]  = '\n';
            obuf[*opos]      = '\0';
        }
    }

    (*count)++;
}


/* -----------------------------------------------------------------------
** iter_rotable — iterate a rotable on top of the stack and collect
** matches against prefix[0..prefix_len-1].
** Does not push or pop anything; the rotable must already be on the stack
** at index 'ridx'.
** --------------------------------------------------------------------- */
static void iter_rotable(lua_State *L, int ridx,
                         const char *prefix, size_t prefix_len,
                         char  completion[MAX_COMPLETION_LEN],
                         char *obuf, size_t *opos,
                         int *overflow, int *count, char *hint)
{
    const rotable_Reg *e;
    for (e = rotable_entries(L, ridx); e && e->name; e++)
    {
        if (strncmp(e->name, prefix, prefix_len) == 0)
        {
            if (*count == 0)
                *hint = e->func ? '(' : '\0';
            process_candidate(e->name, completion, obuf, opos,
                              overflow, count);
        }
    }
}


/* -----------------------------------------------------------------------
** microrl_get_completion — main entry point called by microrl on TAB.
** --------------------------------------------------------------------- */
int microrl_get_completion(struct microrl *rl, char *buf, uint32_t buflen)
{
    char   completion[MAX_COMPLETION_LEN] = "";
    char   obuf[OBUF_SIZE]                = "\r\n"; /* list starts on fresh line */
    size_t opos                           = 2;      /* length of "\r\n" */
    int    overflow                       = 0;
    int    count                          = 0;

    size_t      prefix_len;
    const char *prefix;
    int         table_index;

    /* locate start of the token under the cursor */
    char *start = buf + buflen;
    char *token = start;
    while (token > buf)
    {
        char c = *(token - 1);
        if (!(isalnum((unsigned char)c) || c == '_' || c == '.' || c == ':'))
            break;
        token--;
    }
    buflen = start - token;

    /* locate possible table separator ('.' or ':') */
    char *dot   = strrchr(token, '.');
    char *colon = strrchr(token, ':');
    char *sep   = dot;
    if (colon && (!dot || colon > dot))
        sep = colon;

    if (sep)
    {
        char saved = *sep;
        *sep       = '\0'; /* temporarily terminate table name */
        prefix     = sep + 1;

        lua_getglobal(L, token);

        if (!lua_istable(L, -1) && !rotable_isrotable(L, -1))
        {
            lua_pop(L, 1);
            *sep = saved;
            return 0;
        }

        table_index = lua_gettop(L);
        *sep        = saved;
    }
    else
    {
        prefix = token;
        lua_pushglobaltable(L);
        table_index = lua_gettop(L);
    }

    prefix_len = strlen(prefix);

    /* ----------------------------------------------------------------
    ** Iterate the target table/rotable and collect matches.
    ** hint: '(' for function, '.' for table/rotable, '\0' for other.
    ** Only recorded on the first match; only injected when count == 1.
    ** ------------------------------------------------------------ */
    char hint = '\0';

    if (rotable_isrotable(L, table_index))
    {
        /*
        ** Named table member completion (e.g. "math.s<TAB>"):
        ** target is already a rotable — iterate it directly.
        */
        iter_rotable(L, table_index, prefix, prefix_len,
                     completion, obuf, &opos, &overflow, &count, &hint);
    }
    else
    {
        /*
        ** Global completion or normal table member completion.
        ** For globals: iterate _G via lua_next, then also iterate the
        ** _BASE rotable so base library functions are included.
        */
        lua_pushnil(L);
        while (lua_next(L, table_index) != 0)
        {
            const char *key = lua_tostring(L, -2);
            if (key && strncmp(key, prefix, prefix_len) == 0)
            {
                if (count == 0)
                {
                    if (lua_isfunction(L, -1))
                        hint = '(';
                    else if (lua_istable(L, -1) || rotable_isrotable(L, -1))
                        hint = '.';
                }
                process_candidate(key, completion, obuf, &opos,
                                  &overflow, &count);
            }
            lua_pop(L, 1); /* remove value, keep key for lua_next */
        }

        /*
        ** If we are completing globals (sep == NULL), also search _BASE
        ** for base library functions registered by luaopen_base_ro().
        ** This second pass is skipped for normal table member completion
        ** (sep != NULL) since _BASE is only a global fallback, not a
        ** member of arbitrary tables.
        */
        if (!sep)
        {
            lua_getglobal(L, "_BASE");
            if (rotable_isrotable(L, -1))
            {
                iter_rotable(L, lua_gettop(L), prefix, prefix_len,
                             completion, obuf, &opos, &overflow, &count, &hint);
            }
            lua_pop(L, 1); /* pop _BASE */
        }
    }

    lua_pop(L, 1); /* pop table/rotable (table_index) */

    /* print candidate list and redraw prompt if more than one match */
    if (count > 1)
    {
        microrl_print(rl, obuf);
        if (overflow)
            microrl_print(rl, "...\r\n");
        microrl_redraw_terminal(rl);
    }

    /* feed the common suffix (and hint if unique match) into the input line */
    if (strlen(completion) > prefix_len)
    {
        char   suffix[MAX_COMPLETION_LEN + 1];
        size_t slen = strlen(completion) - prefix_len;
        memcpy(suffix, completion + prefix_len, slen);
        suffix[slen] = '\0';
        if (count == 1 && hint != '\0' && slen < sizeof(suffix) - 1)
        {
            suffix[slen]     = hint;
            suffix[slen + 1] = '\0';
            slen++;
        }
        microrl_processing_input(rl, suffix, slen);
    }

    return 0;
}
