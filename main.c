#include "ai.h"

#ifndef GUS_EXE
#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>
#include "../src/80s.h"
#endif

void *allocate(void *mem, size_t size) {
    if(size == 0) {
        mem = NULL;
    } else {
        mem = realloc(mem, size);
    }
    return mem;
}

#ifdef GUS_EXE
// Ko: 2 2 3 2 1 3 4 3 3 3 3 4 2 4 2 3 3 3
// Suicide 1: 2 2 5 5 1 3 5 6 3 3 5 7 2 4
// Suicide 2: 1 3 1 9 2 2 2 9 3 2 3 9 2 4 4 9 3 4 3 3 4 3 3 3

int main() {
    int x, y, n = 0;
    BOARD board;
    board_init(&board, 9);
    for(;;) {
        printf("It's %s's turn!\n\n", board.turn == BLACK ? "black" : "white");
        board_print(&board);
        do {
            // loop until correct input is submitted
            do {
                printf("\nEnter your move (X Y): ");
                n = scanf("%d %d", &x, &y);
            } while(n != 2);

            // loop until valid move is entered
            x--, y--;
            n = board_place(&board, x, y, board.turn);
            if(n < 0) {
                switch(n) {
                    case ERR_OOB:
                        printf("Entered position is outside of bounds!\n");
                        break;
                    case ERR_SUICIDE:
                        printf("Entered position is suicidal!\n");
                        break;
                    case ERR_KO:
                        printf("Entered position ends up in Ko!\n");
                        break;
                    case ERR_PLACED:
                        printf("There is already a stone placed on entered position!\n");
                        break;
                }
            } else if(n > 0) {
                printf("Removed %d stones!\n", n);
            }
        } while(n < 0);
        board.turn = board.turn == BLACK ? WHITE : BLACK;
    }
}
#else

static int l_gus_place(lua_State *L) {
    if(lua_gettop(L) != 3 || lua_type(L, 1) != LUA_TLIGHTUSERDATA || lua_type(L, 2) != LUA_TNUMBER || lua_type(L, 3) != LUA_TNUMBER) {
        return luaL_error(L, "expecting 3 arguments: board (lightuserdata), x (int), y (int)");
    }
    BOARD *board = (BOARD*)lua_touserdata(L, 1);
    int x = lua_tointeger(L, 2);
    int y = lua_tointeger(L, 3);
    int ok = board_place(board, x, y, board->turn);
    if(ok >= 0) {
        board->turn = board->turn == BLACK ? WHITE : BLACK;
    }
    lua_pushinteger(L, ok);
    return 1;
}

static int l_gus_state(lua_State *L) {
    if(lua_gettop(L) != 1 || lua_type(L, 1) != LUA_TLIGHTUSERDATA) {
        return luaL_error(L, "expecting 1 argument: board (lightuserdata)");
    }
    int n;
    BOARD *board = (BOARD*)lua_touserdata(L, 1);
    lua_newtable(L);
    for(n=0; n < board->square; n++) {
        lua_pushinteger(L, board->cells[n].color);
        lua_rawseti(L, -2, n + 1);
    }
    return 1;
}

static int l_gus_new(lua_State *L) {
    if(lua_gettop(L) != 1 || lua_type(L, 1) != LUA_TNUMBER) {
        return luaL_error(L, "expecting 1 argument: size (int)");
    }
    int size = lua_tointeger(L, 1);
    BOARD* board = allocate(NULL, sizeof(BOARD));
    if(!board) return 0;
    board_init(board, size);
    lua_pushlightuserdata(L, board);
    return 1;
}

static int l_gus_free(lua_State *L) {
    if(lua_gettop(L) != 1 || lua_type(L, 1) != LUA_TLIGHTUSERDATA) {
        return luaL_error(L, "expecting 1 argument: board (lightuserdata)");
    }
    BOARD *board = (BOARD*)lua_touserdata(L, 1);
    free(board);
    return 0;
}

static int l_gus_encode(lua_State *L) {
    if(lua_gettop(L) != 1 || lua_type(L, 1) != LUA_TLIGHTUSERDATA) {
        return luaL_error(L, "expecting 1 argument: board (lightuserdata)");
    }
    BOARD *board = (BOARD*)lua_touserdata(L, 1);
    char data[board->square + 50];
    board_encode(board, data, sizeof(data));
    lua_pushstring(L, data);
    return 1;
}

static int l_gus_decode(lua_State *L) {
    if(lua_gettop(L) != 1 || lua_type(L, 1) != LUA_TSTRING) {
        return luaL_error(L, "expecting 2 arguments: text (string)");
    }
    const char *encoded = lua_tostring(L, 1);
    BOARD *board = board_decode(encoded);
    if(!board) return 0;
    lua_pushlightuserdata(L, board);
    return 1;
}

static int luaopen_gus(lua_State *L) {
    int i;
    const luaL_Reg guslib[] = {
        {"place", l_gus_place},
        {"new", l_gus_new},
        {"free", l_gus_free},
        {"state", l_gus_state},
        {"encode", l_gus_encode},
        {"decode", l_gus_decode},
        {NULL, NULL}};
#if LUA_VERSION_NUM > 501
    luaL_newlib(L, guslib);
#else
    luaL_openlib(L, "gus", guslib, 0);
#endif
    return 1;
}

int on_load(lua_State *L, struct serve_params *params, int reload) {
#if LUA_VERSION_NUM > 501
    luaL_requiref(L, "gus", luaopen_gus, 1);
    lua_pop(L, 1);
#else
    luaopen_gus(L);
#endif
}

int on_unload(lua_State *L, struct serve_params *params, int reload) {
    // ...
}
#endif