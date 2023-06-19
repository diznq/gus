#ifndef __S80_GUS_AI__
#define __S80_GUS_AI__
#include <stdlib.h>
#include <stddef.h>
#include <stdint.h>

struct cell;
struct board;
struct int_vec;

#define MAX_BOARD 19

typedef enum go_err {
    NO_ERR = 0,
    ERR_PLACED = -1,
    ERR_OOB = -2,
    ERR_SUICIDE = -3,
    ERR_KO = -4,
    ERR_PASS = -5
} GO_ERR;

typedef enum cell_color {
    BLACK = 0,
    WHITE = 1,
    EMPTY = 2
} CELL_COLOR;

typedef struct cell {
    CELL_COLOR color;
    int group;
    int n_liberties;
    struct int_vec* liberties;
} CELL;

typedef struct board {
    int size;
    int square;
    int turn;
    int ko;
    CELL cells[MAX_BOARD * MAX_BOARD];
    // scoring params
    int white_score;
    int black_score;
    int white_liberties;
    int black_liberties;
    int white;
    int black;
    int id;
    double score;
    double max;
    struct board *parent;
    struct board *best_child;
} BOARD;

typedef struct int_vec {
    int size;
    int capacity;
    int refs;
    int on_stack;
    int *values;
} INT_VEC;

void *allocate(void *mem, size_t size);
void board_init(BOARD *board, int size, int komi);
int  board_refresh(BOARD *board, int place_x, int place_y, CELL_COLOR color, int update);
int  board_place(BOARD *board, int x, int y, CELL_COLOR color);
int  board_predict(BOARD* board, CELL_COLOR color, int *best_x, int *best_y);
void board_print(BOARD *board);
void board_encode(BOARD *board, char *out, size_t buffer);
BOARD *board_decode(const char *text);
#endif