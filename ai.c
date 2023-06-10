#include <stdio.h>
#include <stdlib.h>

struct cell;
struct board;
struct int_vec;

typedef enum go_err {
    NO_ERR = 0,
    ERR_PLACED = -1,
    ERR_OOB = -2,
    ERR_SUICIDE = -3,
    ERR_KO = -4
} GO_ERR;

typedef enum cell_color {
    BLACK = 0,
    WHITE = 1,
    EMPTY
} CELL_COLOR;

typedef struct cell {
    CELL_COLOR color;
    int group;
    struct int_vec* liberties;
} CELL;

typedef struct board {
    int size;
    int square;
    int turn;
    int ko;
    CELL *cells;
} BOARD;

typedef struct int_vec {
    int size;
    int capacity;
    int refs;
    int *values;
} INT_VEC;

void *allocate(void *mem, size_t size) {
    if(size == 0) {
        mem = NULL;
    } else {
        mem = realloc(mem, size);
    }
    return mem;
}

void int_vec_init(INT_VEC *vec, int capacity) {
    vec->capacity = capacity;
    vec->size = 0;
    vec->refs = 0;
    vec->values = allocate(NULL, capacity * sizeof(int));
}

void int_vec_add(INT_VEC *vec, int element) {
    if(vec->size >= vec->capacity) {
        vec->capacity = (vec->capacity + 1000) * 2;
        vec->values = allocate(vec->values, vec->capacity * sizeof(int));
    }
    if(vec->values) {
        vec->values[vec->size++] = element;
    }
}

void int_vec_rel(INT_VEC *vec) {
    if(vec->values) {
        allocate(vec->values, 0);
        vec->values = NULL;
    }
}

void int_vec_add_ref(INT_VEC *vec) {
    vec->refs++;
}

void int_vec_dec_ref(INT_VEC *vec) {
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

void board_add_liberty(BOARD *board, INT_VEC *liberties, int x, int y) {
    if(x < 0 || y < 0 || x >= board->size || y >= board->size) return;
    CELL *cell = &board->cells[y * board->size + x];
    if(cell->color == EMPTY) {
        int_vec_add(liberties, y * board->size + x);
    }
}

void board_group_propagate(BOARD* board, INT_VEC *liberties, int x, int y, CELL_COLOR color, int group) {
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

int board_refresh(BOARD *board, int place_x, int place_y, CELL_COLOR color) {
    int x, y, n = 0, group = 1, suicide = 0, maybe_suicide = 0, place = place_y * board->size + place_x;
    int to_remove[board->square], to_remove_n = 0;
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
            if(cell->liberties->size == 0)
                to_remove[to_remove_n++] = n;
            int_vec_dec_ref(cell->liberties);
            if(cell->liberties->refs == 0) {
                allocate(cell->liberties, 0);
                cell->liberties = NULL;
            }
        }
    }

    // check for suicide. ko and other states that can happen
    if(to_remove_n == 1 && to_remove[0] == place) {
        board->cells[to_remove[0]].color = EMPTY;
        return ERR_SUICIDE;
    } else if(to_remove_n == 1) {
        board->ko = to_remove[0];
    } else if(to_remove_n == 2) {
        if(to_remove[0] == place)
            board->ko = to_remove[1];
        else board->ko = to_remove[0];
    } else {
        board->ko = -1;
    }
    for(n = 0; n < to_remove_n; n++) {
        board->cells[to_remove[n]].color = EMPTY;
    }
    // make sure if we removed stones by putting it inside eye, we color the inside back
    board->cells[(place_y * board->size + place_x)].color = color;
    return NO_ERR;
}

int board_place(BOARD* board, int x, int y, CELL_COLOR color) {
    if(x < 0 || y < 0 || x >= board->size || y >= board->size) return ERR_OOB;
    if(y * board->size + x == board->ko) return ERR_KO;
    CELL *cell = &board->cells[y * board->size + x];
    int ok;
    if(cell->color != EMPTY) return ERR_PLACED;
    board->cells[y * board->size + x].color = color;
    return board_refresh(board, x, y, color);
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

int on_load(void *ctx, void *params, int reload) {

}

int on_unload(void *ctx, void *params, int reload) {

}

#ifndef SMOD_SO

// Ko: 2 2 3 2 1 3 4 3 3 3 3 4 2 4 2 3 3 3
// Suicide: 2 2 5 5 1 3 5 6 3 3 5 7 2 4

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
            }
        } while(n < 0);
        board.turn = board.turn == BLACK ? WHITE : BLACK;
    }
}
#endif