#include <stdio.h>
#include <stdlib.h>
#include "ai.h"

static void int_vec_init(INT_VEC *vec, int capacity) {
    vec->capacity = capacity;
    vec->size = 0;
    vec->refs = 0;
    vec->values = allocate(NULL, capacity * sizeof(int));
}

static void int_vec_add(INT_VEC *vec, int element) {
    if(vec->size >= vec->capacity) {
        vec->capacity = (vec->capacity + 1000) * 2;
        vec->values = allocate(vec->values, vec->capacity * sizeof(int));
    }
    if(vec->values) {
        vec->values[vec->size++] = element;
    }
}

static void int_vec_rel(INT_VEC *vec) {
    if(vec->values) {
        allocate(vec->values, 0);
        vec->values = NULL;
    }
}

static void int_vec_add_ref(INT_VEC *vec) {
    vec->refs++;
}

static void int_vec_dec_ref(INT_VEC *vec) {
    vec->refs--;
    if(vec->refs == 0) {
        int_vec_rel(vec);
    }
}

void board_init(BOARD* board, int size) {
    int n, sq;
    board->size = size;
    board->square = size * size;
    board->ko = -1;
    board->turn = BLACK;
    board->cells = (CELL*)allocate(NULL, board->square * sizeof(CELL));
    for(n = 0; n < board->square; n++) {
        board->cells[n].color = EMPTY;
        board->cells[n].group = 0;
        board->cells[n].liberties = NULL;
    }
}

static void board_add_liberty(BOARD *board, INT_VEC *liberties, int x, int y) {
    if(x < 0 || y < 0 || x >= board->size || y >= board->size) return;
    CELL *cell = &board->cells[y * board->size + x];
    if(cell->color == EMPTY) {
        int_vec_add(liberties, y * board->size + x);
    }
}

static void board_group_propagate(BOARD* board, INT_VEC *liberties, int x, int y, CELL_COLOR color, int group) {
    if(x < 0 || y < 0 || x >= board->size || y >= board->size) return;
    CELL *cell = &board->cells[y * board->size + x];
    if(cell->color == color && cell->group == 0) {
        cell->group = group;
        cell->liberties = liberties;
        int_vec_add_ref(liberties);
        board_add_liberty(board, liberties, x + 1, y);
        board_add_liberty(board, liberties, x - 1, y);
        board_add_liberty(board, liberties, x, y - 1);
        board_add_liberty(board, liberties, x, y + 1);
        board_group_propagate(board, liberties, x + 1, y, color, group);
        board_group_propagate(board, liberties, x - 1, y, color, group);
        board_group_propagate(board, liberties, x, y - 1, color, group);
        board_group_propagate(board, liberties, x, y + 1, color, group);
        //printf("pos %d, %d belongs to group %d with %d liberties\n", x + 1, y + 1, group, liberties->size);
    }
}

static int board_refresh(BOARD *board, int place_x, int place_y, CELL_COLOR color) {
    int x, y, n = 0, group = 1, suicide = 0, maybe_suicide = 0, place = place_y * board->size + place_x;
    int to_remove[board->square], to_remove_n = 0, self_remove = 0;
    CELL *cell;
    INT_VEC *liberties;
    // clean-up old state if there was any
    for(n = 0; n < board->square; n++) {
        cell = &board->cells[n];
        cell->group = 0;
        cell->liberties = NULL;
    }

    n = 0;

    // propagate state
    for(y = 0; y < board->size; y++) {
        for(x = 0; x < board->size; x++, n++) {
            cell = &board->cells[n];
            if(cell->group) {
                continue;
            } else if(cell->color != EMPTY) {
                liberties = (INT_VEC*)allocate(NULL, sizeof(INT_VEC));
                int_vec_init(liberties, board->square >> 1);
                board_group_propagate(board, liberties, x, y, cell->color, group++);
            } else {
                cell->group = 0;
                cell->liberties = NULL;
            }
        }
    }

    // check our liberties
    for(n = 0; n < board->square; n++) {
        cell = &board->cells[n];
        if(cell->liberties) {
            if(cell->liberties->size == 0) {
                if(cell->color == color) self_remove++;
                else to_remove[to_remove_n++] = n;
            }
            int_vec_dec_ref(cell->liberties);
            if(cell->liberties->refs == 0) {
                allocate(cell->liberties, 0);
                cell->liberties = NULL;
            }
        }
    }

    // check for suicide. ko and other states that can happen
    if(to_remove_n == 0 && self_remove > 0) {
        board->cells[place].color = EMPTY;
        return ERR_SUICIDE;
    } else if(to_remove_n == 1) {
        board->ko = to_remove[0];
    } else {
        board->ko = -1;
    }
    for(n = 0; n < to_remove_n; n++) {
        board->cells[to_remove[n]].color = EMPTY;
    }

    return to_remove_n;
}

int board_place(BOARD* board, int x, int y, CELL_COLOR color) {
    if(x < 0 || y < 0 || x >= board->size || y >= board->size) return ERR_OOB;
    if(y * board->size + x == board->ko) return ERR_KO;
    CELL *cell = &board->cells[y * board->size + x];
    if(cell->color != EMPTY) return ERR_PLACED;
    board->cells[y * board->size + x].color = color;
    return board_refresh(board, x, y, color);
}

void board_encode(BOARD *board, char *out, size_t size) {
    if(size < (board->square + 40)) {
        *out = 0;
        return;
    }
    int off = sprintf(out, "%04d %01d %04d ",  board->size, board->turn, board->ko), n = 0;
    char *p = out + off;
    for(n = 0; n < board->square; n++) {
        switch(board->cells[n].color) {
            case EMPTY:
                *p = '+';
                break;
            case BLACK:
                *p = 'X';
                break;
            case WHITE:
                *p = 'O';
                break;
        }
        p++;
    }
    *p = 0;
}

BOARD *board_decode(const char *text) {
    int size, turn, ko;
                        //0000 0 0000
    int n = sscanf(text, "%04d %01d %04d", &size, &turn, &ko);
    if(n != 3) return NULL;
    BOARD *board = (BOARD*)allocate(NULL, sizeof(BOARD));
    if(!board) return NULL;
    board_init(board, size);
    board->turn = turn;
    board->ko = ko;
    n = 0;
    text += 12;
    while(*text && n < board->square) {
        board->cells[n].color = *text == '+' ? EMPTY : (*text == 'X' ? BLACK : WHITE);
        text++; n++;
    }
    return board;
}

void board_print(BOARD *board) {
    int n;
    CELL *cell;
    char picture[(board->size + 1) * (board->size + 1) + 5], *p;
    p = picture;
    for(n = 0; n < board->square; n++) {
        cell = &board->cells[n];
        switch(cell->color) {
            case EMPTY:
                *p = '+';
                break;
            case BLACK:
                *p = 'X';
                break;
            case WHITE:
                *p = 'O';
                break;
        }
        p++;
        if(n % board->size == board->size - 1) {
            *p = '\n'; p++;
        }
    }
    *p = 0;
    printf("%s", picture);
}