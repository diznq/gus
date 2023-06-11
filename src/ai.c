#include <stdio.h>
#include <stdlib.h>
#include <math.h>
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

void board_init(BOARD* board, int size, int komi) {
    int n, sq;
    if(size > 19) size = 19;
    if(size < 0) size = 0;
    board->parent = NULL;
    board->size = size;
    board->square = size * size;
    board->ko = -1;
    board->white_score = komi;
    board->black_score = 0;
    board->turn = BLACK;
    for(n = 0; n < board->square; n++) {
        board->cells[n].color = EMPTY;
        board->cells[n].group = 0;
        board->cells[n].liberties = NULL;
    }
}

void board_release(BOARD *board) {

}

static void board_copy(BOARD* out, BOARD* board) {
    int n;
    out->parent = board->parent;
    out->best_child = board->best_child;
    out->size = board->size;
    out->square = board->square;
    out->id = board->id;
    out->score = board->score;
    out->white_score = board->white_score;
    out->black_score = board->black_score;
    out->black_liberties = board->black_liberties;
    out->white_liberties = board->white_liberties;
    out->ko = board->ko;
    out->turn = board->turn;
    for(n = 0; n < board->square; n++) {
        out->cells[n].color = board->cells[n].color;
        out->cells[n].n_liberties = board->cells[n].n_liberties;
        out->cells[n].group = board->cells[n].group;
        out->cells[n].liberties = board->cells[n].liberties;
    }
}

static void board_add_liberty(BOARD *board, INT_VEC *liberties, int x, int y) {
    if(x < 0 || y < 0 || x >= board->size || y >= board->size) return;
    int place = y * board->size + x, n;
    CELL *cell = &board->cells[place];
    for(n = 0; n < liberties->size; n++) {
        if(liberties->values[n] == place) return;
    }
    if(cell->color == EMPTY) {
        int_vec_add(liberties, place);
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

static int board_refresh(BOARD *board, int place_x, int place_y, CELL_COLOR color, int update) {
    int x, y, n = 0, group = 1, suicide = 0, maybe_suicide = 0, place = place_y * board->size + place_x;
    int to_remove[board->square], to_remove_n = 0, self_remove = 0;
    CELL *cell;
    INT_VEC *liberties;
    // clean-up old state if there was any
    board->white_liberties = 0;
    board->black_liberties = 0;
    board->white = 0;
    board->black = 0;
    for(n = 0; n < board->square; n++) {
        cell = &board->cells[n];
        cell->group = 0;
        cell->liberties = NULL;
        cell->n_liberties = 0;
        board->white += cell->color == WHITE;
        board->black += cell->color == BLACK;
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
            cell->n_liberties = cell->liberties->size;
            if(cell->liberties->size == 0) {
                if(cell->color == color) self_remove++;
                else to_remove[to_remove_n++] = n;
            }
            int_vec_dec_ref(cell->liberties);
            if(cell->liberties->refs == 0) {
                if(cell->color == BLACK) board->black_liberties += cell->liberties->size;
                else if(cell->color == WHITE) board->white_liberties += cell->liberties->size;
                allocate(cell->liberties, 0);
                cell->liberties = NULL;
            }
        }
    }

    if(update) {
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
            cell = &board->cells[to_remove[n]];
            if(cell->color == WHITE) {
                board->white--;
                board->black_score += 10;
            } else if(cell->color == BLACK) {
                board->black--;
                board->white_score += 10;
            }
            cell->color = EMPTY;
        }

        return to_remove_n;
    } else {
        return to_remove_n;
    }
}

int board_place(BOARD* board, int x, int y, CELL_COLOR color) {
    if(x < 0 || y < 0 || x >= board->size || y >= board->size) return ERR_OOB;
    if(y * board->size + x == board->ko) return ERR_KO;
    CELL *cell = &board->cells[y * board->size + x];
    if(cell->color != EMPTY) return ERR_PLACED;
    board->cells[y * board->size + x].color = color;
    return board_refresh(board, x, y, color, 1);
}

static double uniform(double w) {
    return w * (double)rand() / RAND_MAX;
}

static double make_rating(BOARD *clone, CELL_COLOR color) {
    double my_lib, my_area, my_score, op_lib, op_area, op_score, rating;
    my_lib = color == BLACK ? clone->black_liberties : clone->white_liberties;
    my_area = color == BLACK ? clone->black : clone->white;
    my_score = color == BLACK ? clone->black_score : clone->white_score;

    op_lib = color != BLACK ? clone->black_liberties : clone->white_liberties;
    op_area = color != BLACK ? clone->black : clone->white;
    op_score = color != BLACK ? clone->black_score : clone->white_score;
    
    rating = (my_score - op_score) * 100 - op_lib * (1.2 + uniform(0.4)) + my_lib * uniform(0.1) + (op_area / (1.0 + my_area)) * 0.4;
    return rating;
}

static int rating_sort(const void *a, const void *b) {
    const BOARD *A = (const BOARD*)a;
    const BOARD *B = (const BOARD*)b;
    return B->score - A->score;
}

int board_predict(BOARD* board, CELL_COLOR color, int *best_x, int *best_y) {
    CELL_COLOR o_color = color;
    int pick_rate = 2,
        pass = 0,
        y = 0,
        x = 0,
        n = 0,
        m = 1,
        d = 0,
        ok = 0,
        pivot = 0,
        sector_start = 0,
        sector_end = 1,
        depth = 5;

    double best_move = make_rating(board, color) * 0.3, rating;
    int to_alloc = 1, last_pow = pick_rate;
    for(d = 0; d < depth; d++) {
        to_alloc += last_pow;
        last_pow = last_pow * last_pow;
    }
    printf("to alloc: %d\n", to_alloc);


    BOARD *boards = calloc(to_alloc, sizeof(BOARD));
    BOARD helper[board->square];
    BOARD *sel;
    int stops[10][2];
    board_copy(boards, board);
    boards->id = 0;
    boards->parent = NULL;
    boards->best_child = NULL;
    boards->id = ERR_PASS;
    boards->score = make_rating(boards, color);

    stops[0][0] = 0; stops[0][1] = 1;
    for(d=0; d < depth; d++) {
        for(pivot = sector_start; pivot < sector_end; pivot++) {
            n = 0;
            for(y = 0; y < board->size; y++) {
                for(x = 0; x < board->size; x++, n++) {
                    board_copy(helper + n, boards + pivot);
                    ok = board_place(helper + n, x, y, color);
                    helper[n].parent = boards + pivot;
                    helper[n].id = n;
                    helper[n].best_child = NULL;
                    if(ok >= 0) {
                        helper[n].score = make_rating(helper + n, color);
                    } else {
                        helper[n].id = ERR_PASS;
                        helper[n].score = -1000000.0;
                    }
                }
            }
            qsort(helper, n, sizeof(BOARD), rating_sort);
            for(n = 0; n < pick_rate; n++, m++) {
                board_copy(boards + m, helper + n);
            }
        }
        sector_start = sector_end;
        sector_end = m;
        stops[d + 1][0] = sector_start;
        stops[d + 1][1] = sector_end;
        stops[d + 2][0] = -1;
        color = color == BLACK ? WHITE : BLACK;
    }

    color = o_color;

    for(d = depth; d > 0; d--) {
        for(pivot = stops[d][0]; pivot < stops[d][1]; pivot++) {
            sel = boards + pivot;
            if(sel->parent->best_child == NULL || sel->score > sel->parent->best_child->score) {
                sel->parent->best_child = sel;
            }
        }
    }

    sel = boards[0].best_child;
    
    if(sel->id < 0) {
        *best_x = -1;
        *best_y = -1;
        free(boards);
        return ERR_PASS;
    }
    *best_x = sel->id % board->size;
    *best_y = sel->id / board->size;
    free(boards);
    return board_place(board, *best_x, *best_y, color);
}

void board_encode(BOARD *board, char *out, size_t size) {
    if(size < (board->square + 30)) {
        *out = 0;
        return;
    }
    int off = sprintf(out, "%01d %04d %04d %04d %04d ", board->turn, board->size, board->ko, board->black_score, board->white_score), n = 0;
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
    int size, turn, ko, white, black;
                        //0 0000 0000 0000 0000
    int n = sscanf(text, "%01d %04d %04d %04d %04d", &turn, &size, &ko, &black, &white);
    if(n != 5) return NULL;
    BOARD *board = (BOARD*)allocate(NULL, sizeof(BOARD));
    if(!board) return NULL;
    board_init(board, size, 65);
    board->turn = turn;
    board->ko = ko;
    board->white_score = white;
    board->black_score = black;
    n = 0;
    text += 22;
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