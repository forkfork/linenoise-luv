#include <lua.h>
#include <lauxlib.h>
#include <stdlib.h>
#include <string.h>
#include "linenoise.h"

#if LUA_VERSION_NUM < 502
#define LUA_OK 0
#define luaL_newlib(L, l) (lua_newtable(L), luaL_register(L, NULL, l))

static void luaL_setfuncs(lua_State *L, const luaL_Reg *l, int nup)
{
    luaL_checkstack(L, nup + 1, "too many upvalues");
    for (; l->name != NULL; l++) {
        int i;
        for (i = 0; i < nup; i++) {
            lua_pushvalue(L, -nup);
        }
        lua_pushcclosure(L, l->func, nup);
        lua_setfield(L, -(nup + 2), l->name);
    }
    lua_pop(L, nup);
}
#endif

#define LN_COMPLETION_TYPE "linenoiseCompletions*"

#ifdef _WIN32
#define LN_EXPORT __declspec(dllexport)
#else
#define LN_EXPORT extern
#endif

static int completion_func_ref = LUA_NOREF;
static int hints_func_ref = LUA_NOREF;
static lua_State *cb_state;

static void completion_wrapper(const char *line, linenoiseCompletions *completions)
{
    lua_State *L = cb_state;
    if (!L || completion_func_ref == LUA_NOREF) return;

    lua_rawgeti(L, LUA_REGISTRYINDEX, completion_func_ref);
    *((linenoiseCompletions **) lua_newuserdata(L, sizeof(linenoiseCompletions *))) = completions;
    luaL_getmetatable(L, LN_COMPLETION_TYPE);
    lua_setmetatable(L, -2);
    lua_pushstring(L, line);

    if (lua_pcall(L, 2, 0, 0) != LUA_OK) {
        fprintf(stderr, "linenoise completion error: %s\n", lua_tostring(L, -1));
        lua_pop(L, 1);
    }
}

static char *hints_wrapper(const char *line, int *color, int *bold)
{
    lua_State *L = cb_state;
    char *result = NULL;

    if (!L || hints_func_ref == LUA_NOREF) return NULL;

    lua_rawgeti(L, LUA_REGISTRYINDEX, hints_func_ref);
    lua_pushstring(L, line);

    if (lua_pcall(L, 1, 2, 0) != LUA_OK) {
        fprintf(stderr, "linenoise hints error: %s\n", lua_tostring(L, -1));
        lua_pop(L, 1);
        return NULL;
    }

    if (!lua_isnoneornil(L, -2) && lua_isstring(L, -2)) {
        const char *hint = lua_tostring(L, -2);
        result = strdup(hint);
    }

    if (!lua_isnoneornil(L, -1) && lua_istable(L, -1)) {
        lua_getfield(L, -1, "color");
        if (lua_isnumber(L, -1)) *color = lua_tointeger(L, -1);
        lua_pop(L, 1);

        lua_getfield(L, -1, "bold");
        if (lua_toboolean(L, -1)) *bold = 1;
        lua_pop(L, 1);
    }

    lua_pop(L, 2);
    return result;
}

static void free_hints_wrapper(void *hint)
{
    free(hint);
}

/* --- Blocking API --- */

static int l_linenoise(lua_State *L)
{
    const char *prompt = luaL_checkstring(L, 1);
    char *line;

    cb_state = L;
    line = linenoise(prompt);
    cb_state = NULL;

    if (!line) {
        lua_pushnil(L);
        return 1;
    }
    lua_pushstring(L, line);
    linenoiseFree(line);
    return 1;
}

/* --- Non-blocking API --- */

static struct linenoiseState edit_state;
static char edit_buf[4096];
static int edit_active = 0;

static int l_editstart(lua_State *L)
{
    const char *prompt = luaL_checkstring(L, 1);

    if (edit_active) {
        return luaL_error(L, "edit session already active");
    }

    cb_state = L;
    int rc = linenoiseEditStart(&edit_state, -1, -1, edit_buf, sizeof(edit_buf), prompt);
    if (rc == -1) {
        cb_state = NULL;
        lua_pushnil(L);
        lua_pushstring(L, "failed to start edit");
        return 2;
    }
    edit_active = 1;
    lua_pushboolean(L, 1);
    return 1;
}

static int l_editfeed(lua_State *L)
{
    if (!edit_active) {
        return luaL_error(L, "no edit session active");
    }

    char *line = linenoiseEditFeed(&edit_state);

    if (line == linenoiseEditMore) {
        lua_pushnil(L);
        lua_pushboolean(L, 1);
        return 2;
    }

    edit_active = 0;
    cb_state = NULL;

    if (!line) {
        lua_pushnil(L);
        lua_pushnil(L);
        return 2;
    }

    lua_pushstring(L, line);
    linenoiseFree(line);
    return 1;
}

static int l_editstop(lua_State *L)
{
    (void)L;
    linenoiseEditStop(&edit_state);
    edit_active = 0;
    cb_state = NULL;
    return 0;
}

/* --- Completion API --- */

static int l_setcompletion(lua_State *L)
{
    if (lua_isnoneornil(L, 1)) {
        luaL_unref(L, LUA_REGISTRYINDEX, completion_func_ref);
        completion_func_ref = LUA_NOREF;
        linenoiseSetCompletionCallback(NULL);
    } else {
        luaL_checktype(L, 1, LUA_TFUNCTION);
        lua_pushvalue(L, 1);
        if (completion_func_ref == LUA_NOREF) {
            completion_func_ref = luaL_ref(L, LUA_REGISTRYINDEX);
        } else {
            lua_rawseti(L, LUA_REGISTRYINDEX, completion_func_ref);
        }
        linenoiseSetCompletionCallback(completion_wrapper);
    }
    return 0;
}

static int l_addcompletion(lua_State *L)
{
    linenoiseCompletions *completions = *((linenoiseCompletions **) luaL_checkudata(L, 1, LN_COMPLETION_TYPE));
    const char *entry = luaL_checkstring(L, 2);
    linenoiseAddCompletion(completions, entry);
    return 0;
}

/* --- Hints API --- */

static int l_sethints(lua_State *L)
{
    if (lua_isnoneornil(L, 1)) {
        luaL_unref(L, LUA_REGISTRYINDEX, hints_func_ref);
        hints_func_ref = LUA_NOREF;
        linenoiseSetHintsCallback(NULL);
        linenoiseSetFreeHintsCallback(NULL);
    } else {
        luaL_checktype(L, 1, LUA_TFUNCTION);
        lua_pushvalue(L, 1);
        if (hints_func_ref == LUA_NOREF) {
            hints_func_ref = luaL_ref(L, LUA_REGISTRYINDEX);
        } else {
            lua_rawseti(L, LUA_REGISTRYINDEX, hints_func_ref);
        }
        linenoiseSetHintsCallback(hints_wrapper);
        linenoiseSetFreeHintsCallback(free_hints_wrapper);
    }
    return 0;
}

/* --- History API --- */

static int l_historyadd(lua_State *L)
{
    const char *line = luaL_checkstring(L, 1);
    lua_pushboolean(L, linenoiseHistoryAdd(line));
    return 1;
}

static int l_historysetmaxlen(lua_State *L)
{
    int len = luaL_checkinteger(L, 1);
    lua_pushboolean(L, linenoiseHistorySetMaxLen(len));
    return 1;
}

static int l_historysave(lua_State *L)
{
    const char *filename = luaL_checkstring(L, 1);
    if (linenoiseHistorySave(filename) == -1) {
        lua_pushnil(L);
        lua_pushstring(L, "failed to save history");
        return 2;
    }
    lua_pushboolean(L, 1);
    return 1;
}

static int l_historyload(lua_State *L)
{
    const char *filename = luaL_checkstring(L, 1);
    if (linenoiseHistoryLoad(filename) == -1) {
        lua_pushnil(L);
        lua_pushstring(L, "failed to load history");
        return 2;
    }
    lua_pushboolean(L, 1);
    return 1;
}

/* --- Terminal utilities --- */

static int l_clearscreen(lua_State *L)
{
    (void)L;
    linenoiseClearScreen();
    return 0;
}

static int l_setmultiline(lua_State *L)
{
    linenoiseSetMultiLine(lua_toboolean(L, 1));
    return 0;
}

static int l_setmaskmode(lua_State *L)
{
    if (lua_toboolean(L, 1)) {
        linenoiseMaskModeEnable();
    } else {
        linenoiseMaskModeDisable();
    }
    return 0;
}

static int l_printkeycodes(lua_State *L)
{
    (void)L;
    linenoisePrintKeyCodes();
    return 0;
}

/* --- Module registration --- */

static luaL_Reg module_funcs[] = {
    { "linenoise", l_linenoise },
    { "editstart", l_editstart },
    { "editfeed", l_editfeed },
    { "editstop", l_editstop },
    { "setcompletion", l_setcompletion },
    { "addcompletion", l_addcompletion },
    { "sethints", l_sethints },
    { "historyadd", l_historyadd },
    { "historysetmaxlen", l_historysetmaxlen },
    { "historysave", l_historysave },
    { "historyload", l_historyload },
    { "clearscreen", l_clearscreen },
    { "setmultiline", l_setmultiline },
    { "setmaskmode", l_setmaskmode },
    { "printkeycodes", l_printkeycodes },
    { NULL, NULL }
};

static luaL_Reg completion_methods[] = {
    { "add", l_addcompletion },
    { NULL, NULL }
};

LN_EXPORT int luaopen_linenoise_luv(lua_State *L)
{
    luaL_newmetatable(L, LN_COMPLETION_TYPE);
    lua_pushboolean(L, 0);
    lua_setfield(L, -2, "__metatable");
    lua_pushvalue(L, -1);
    lua_setfield(L, -2, "__index");
    luaL_setfuncs(L, completion_methods, 0);
    lua_pop(L, 1);

    luaL_newlib(L, module_funcs);
    return 1;
}
