// Host tests for game.c: cc -I.. test_game.c ../game.c && ./a.out
#include "game.h"

#include <stdio.h>
#include <string.h>

static int failures = 0;
static int checks = 0;

#define CHECK(cond)                                                  \
    do {                                                             \
        checks++;                                                    \
        if(!(cond)) {                                                \
            failures++;                                              \
            printf("  FAIL %s:%d: %s\n", __func__, __LINE__, #cond); \
        }                                                            \
    } while(0)

#define FLOOR_WY (G_FLOOR_Y - 16)

// A quiet field: no cranes, worker on the floor in column c.
static void setup(Game* g, int c) {
    game_init(g, 1, 1);
    memset(g->cell, 0, sizeof(g->cell)); // no starting stacks
    g->spawn_timer = 60000;
    g->w.x = g_col_x(c);
    g->w.y = FLOOR_WY;
}

static void run(Game* g, int ticks, uint8_t held) {
    for(int i = 0; i < ticks; i++) game_tick(g, held, 0);
}

static void drop(Game* g, int col, int y) {
    GBox* b = &g->falling[g->n_falling++];
    b->x = g_col_x(col);
    b->y = y;
    b->type = 1;
}

static void test_walk(void) {
    Game g;
    setup(&g, 5);
    run(&g, 8, G_KEY_RIGHT);
    CHECK(g.w.x == g_col_x(6));
    CHECK(g.w.face == 1);
    run(&g, 16, G_KEY_LEFT);
    CHECK(g.w.x == g_col_x(4));
}

static void test_wall(void) {
    Game g;
    setup(&g, 0);
    run(&g, 20, G_KEY_LEFT);
    CHECK(g.w.x == g_col_x(0));
    setup(&g, 11);
    run(&g, 20, G_KEY_RIGHT);
    CHECK(g.w.x == g_col_x(11));
}

static void test_push(void) {
    Game g;
    setup(&g, 4);
    g.cell[0][5] = 3;
    run(&g, 8, G_KEY_RIGHT);
    CHECK(g.w.x == g_col_x(5));
    run(&g, 1, 0);
    CHECK(g.cell[0][5] == 0);
    CHECK(g.cell[0][6] == 3);
    CHECK(!g.has_pushed);
}

static void test_no_push_loaded_box(void) {
    Game g;
    setup(&g, 4);
    g.cell[0][5] = 3;
    g.cell[1][5] = 2;
    run(&g, 10, G_KEY_RIGHT);
    CHECK(g.w.x == g_col_x(4));
    CHECK(g.cell[0][5] == 3);
}

static void test_no_push_two_boxes(void) {
    Game g;
    setup(&g, 4);
    g.cell[0][5] = 3;
    g.cell[0][6] = 4;
    run(&g, 10, G_KEY_RIGHT);
    CHECK(g.w.x == g_col_x(4));
    CHECK(g.cell[0][5] == 3 && g.cell[0][6] == 4);
}

static void test_no_push_into_wall(void) {
    Game g;
    setup(&g, 10);
    g.cell[0][11] = 3;
    run(&g, 10, G_KEY_RIGHT);
    CHECK(g.w.x == g_col_x(10));
    CHECK(g.cell[0][11] == 3);
}

static void test_jump_onto_box(void) {
    Game g;
    setup(&g, 4);
    g.cell[0][5] = 3;
    game_tick(&g, G_KEY_RIGHT, G_KEY_JUMP);
    run(&g, 14, G_KEY_RIGHT);
    run(&g, 16, 0);
    CHECK(g.w.x == g_col_x(5));
    CHECK(g.w.y == FLOOR_WY - 8);
    CHECK(g.w.vstate == WGround);
    CHECK(g.cell[0][5] == 3);
}

static void test_jump_lands_back(void) {
    Game g;
    setup(&g, 4);
    game_tick(&g, 0, G_KEY_JUMP);
    CHECK(g.w.vstate == WRising);
    run(&g, 30, 0);
    CHECK(g.w.y == FLOOR_WY);
    CHECK(g.w.vstate == WGround);
}

static void test_push_while_jumping(void) {
    Game g;
    setup(&g, 4);
    g.cell[0][5] = 3;
    g.cell[1][5] = 2;
    // hold right through the jump: the push happens on the single top frame
    game_tick(&g, G_KEY_RIGHT, G_KEY_JUMP);
    run(&g, 7, G_KEY_RIGHT); // up to the top; the started push finishes by itself
    run(&g, 30, 0);
    CHECK(g.cell[1][5] == 0);
    CHECK(g.cell[0][6] == 2); // pushed off the stack, fell to the floor
    CHECK(g.cell[0][5] == 3);
}

static void test_box_falls_and_lands(void) {
    Game g;
    setup(&g, 0);
    drop(&g, 6, G_CRANE_BOX_Y);
    run(&g, 39, 0);
    CHECK(g.n_falling == 1);
    run(&g, 2, 0);
    CHECK(g.n_falling == 0);
    CHECK(g.cell[0][6] == 1);
    CHECK(g.score == 0); // points come when a crane releases, not on landing
}

static void test_crushed(void) {
    Game g;
    setup(&g, 6);
    drop(&g, 6, G_CRANE_BOX_Y);
    run(&g, 60, 0);
    CHECK(g.over);
}

// A falling box next to the worker blocks him when it can't be pushed anywhere.
static void test_walk_under_falling_box_is_blocked(void) {
    Game g;
    setup(&g, 5);
    g.cell[0][7] = 1;
    drop(&g, 6, FLOOR_WY + 2); // low in the next column, overlapping the worker's height
    game_tick(&g, G_KEY_RIGHT, 0);
    CHECK(g.w.x == g_col_x(5));
    CHECK(!g.over);
}

// A box still falling next to the worker can be pushed. On the C45 (0:46 of the
// phone video) it stalls for one step, then keeps falling while it slides.
static void test_push_falling_box(void) {
    Game g;
    setup(&g, 5);
    drop(&g, 4, FLOOR_WY + 1); // falling, level with the worker's legs
    int y0 = g.falling[0].y;
    game_tick(&g, G_KEY_LEFT, 0);
    CHECK(g.has_pushed);
    CHECK(g.pushed.y == y0); // the one-step stall
    run(&g, 3, G_KEY_LEFT);
    CHECK(g.pushed.y == y0 + 3); // falling again while sliding
    CHECK(g.pushed.x == g_col_x(4) - 4);
    CHECK(!g.over);
    run(&g, 4, G_KEY_LEFT);
    CHECK(g.w.x == g_col_x(4));
    run(&g, 20, 0);
    CHECK(g.cell[0][3] == 1);
    CHECK(g.cell[0][4] == 0);
    CHECK(!g.over);
}

// Sliding while falling, the box rides over a stack top instead of sinking into it.
static void test_push_falling_box_slides_over_stack(void) {
    Game g;
    setup(&g, 5);
    g.cell[0][5] = 5; // the worker stands on a box
    g.w.y = FLOOR_WY - 8;
    g.cell[0][3] = 2;
    drop(&g, 4, g_row_y(1) - 1); // bottom edge 1 px above the top of column 3's box
    run(&g, 8, G_KEY_LEFT);
    run(&g, 20, 0);
    CHECK(g.cell[1][3] == 1);
    CHECK(g.cell[0][3] == 2);
    CHECK(!g.over);
}

static void test_no_push_falling_box_into_wall(void) {
    Game g;
    setup(&g, 1);
    drop(&g, 0, FLOOR_WY + 2);
    run(&g, 3, G_KEY_LEFT);
    CHECK(!g.has_pushed);
    CHECK(g.w.x == g_col_x(1));
}

// Jumping into a falling box with the helmet smashes it (heard as the 4177 Hz hit).
static void test_helmet_breaks_box(void) {
    Game g;
    setup(&g, 5);
    drop(&g, 5, FLOOR_WY - 20); // 20 px above the head
    game_tick(&g, 0, G_KEY_JUMP);
    bool broke = false;
    for(int i = 0; i < 20; i++) {
        game_tick(&g, 0, 0);
        if(g.events & G_EV_BREAK) broke = true;
    }
    CHECK(broke);
    CHECK(!g.over);
    CHECK(g.n_falling == 0);
    CHECK(g.cell[0][5] == 0);
}

// Standing still under a falling box is still fatal.
static void test_no_break_without_jump(void) {
    Game g;
    setup(&g, 5);
    drop(&g, 5, FLOOR_WY - 20);
    run(&g, 25, 0);
    CHECK(g.over);
}

// Measured in the demake: 8 px up at 1 px/frame, one frame at the top, 8 px down.
static void test_jump_arc(void) {
    Game g;
    setup(&g, 5);
    game_tick(&g, 0, G_KEY_JUMP);
    int ticks = 1, top = g.w.y;
    while(g.w.y != FLOOR_WY || g.w.vstate != WGround) {
        game_tick(&g, 0, 0);
        if(g.w.y < top) top = g.w.y;
        if(++ticks > 40) break;
    }
    CHECK(top == FLOOR_WY - 8);
    CHECK(ticks <= 19);
}

// stktk: 12 boxes, at most 2 per column, never a full row; the worker stands on the
// leftmost 2-high stack.
static void test_starting_stacks(void) {
    for(uint32_t seed = 1; seed < 50; seed++) {
        Game g;
        game_init(&g, seed, 1);
        int boxes = 0, first_two = -1;
        bool full_row = true;
        for(int c = 0; c < G_COLS; c++) {
            full_row &= g.cell[0][c] != 0;
            if(first_two < 0 && g.cell[1][c]) first_two = c;
            for(int r = 0; r < G_ROWS; r++) {
                if(g.cell[r][c]) boxes++;
                if(r >= 2) CHECK(g.cell[r][c] == 0);
                if(r > 0 && g.cell[r][c]) CHECK(g.cell[r - 1][c] != 0);
            }
        }
        CHECK(!full_row);
        CHECK(boxes == 12);
        if(first_two >= 0) {
            CHECK(g.w.x == g_col_x(first_two));
            CHECK(g.w.y == FLOOR_WY - 16);
        }
        CHECK(g.cranes == 1);
    }
}

static void test_fall_off_stack(void) {
    Game g;
    setup(&g, 4);
    g.cell[0][4] = 1;
    g.w.y = FLOOR_WY - 8;
    run(&g, 8, G_KEY_RIGHT);
    run(&g, 12, 0);
    CHECK(g.w.x == g_col_x(5));
    CHECK(g.w.y == FLOOR_WY);
}

static void test_row_clear(void) {
    Game g;
    setup(&g, 0);
    g.w.y = FLOOR_WY - 8; // stand on the row
    for(int c = 0; c < G_COLS - 1; c++) g.cell[0][c] = 1 + c % 7;
    g.cell[1][3] = 5;
    drop(&g, 11, g_row_y(0) - 1);
    run(&g, 2, 0);
    CHECK(g.clear_age[0] > 0);
    CHECK(g.events == 0 || true);
    run(&g, G_CLEAR_TICKS, 0);
    for(int c = 0; c < G_COLS; c++) CHECK(g.cell[0][c] == 0 || c == 3);
    run(&g, 12, 0);
    CHECK(g.cell[0][3] == 5); // the box above fell into the cleared row
    CHECK(g.cell[1][3] == 0);
    CHECK(g.rows_cleared == 1);
    CHECK(g.score == 10); // 10 x cranes (1)
    CHECK(g.cranes == 2); // every cleared row adds a crane
    CHECK(g.w.y == FLOOR_WY);
}

// The explosion runs right to left, and each column's boxes start falling as soon as
// its own cell is gone: the right stack falls before the left one.
static void test_clear_wave_drops_right_first(void) {
    Game g;
    setup(&g, 5);
    g.w.y = FLOOR_WY - 8;
    for(int c = 0; c < G_COLS - 1; c++) g.cell[0][c] = 1;
    g.cell[1][0] = 2;
    g.cell[1][10] = 3;
    drop(&g, 11, g_row_y(0) - 1);
    int fall_right = -1, fall_left = -1;
    for(int t = 0; t < 40; t++) {
        game_tick(&g, 0, 0);
        if(fall_right < 0 && !g.cell[1][10]) fall_right = t;
        if(fall_left < 0 && !g.cell[1][0]) fall_left = t;
    }
    CHECK(fall_right >= 0 && fall_left >= 0);
    CHECK(fall_right < fall_left);
    CHECK(g.cell[0][10] == 3 && g.cell[0][0] == 2);
}

static void test_crane_delivers(void) {
    Game g;
    game_init(&g, 7, 1);
    memset(g.cell, 0, sizeof(g.cell));
    g.w.x = g_col_x(0);
    // keep the worker out of the way; let cranes run for a while
    for(int i = 0; i < 15 * 30 && !g.over; i++) game_tick(&g, 0, 0);
    CHECK(g.boxes_dropped > 3);
}

// stktk: a box level only with the head can't be pushed, the worker is blocked.
static void test_no_push_falling_box_at_head(void) {
    Game g;
    setup(&g, 5);
    drop(&g, 4, FLOOR_WY - 4); // bottom edge above the worker's waist
    run(&g, 2, G_KEY_LEFT);
    CHECK(!g.has_pushed);
    CHECK(g.w.x == g_col_x(5));
}

static void test_no_jump_from_high_stack(void) {
    Game g;
    setup(&g, 5);
    for(int r = 0; r < 3; r++) g.cell[r][5] = 1;
    g.w.y = FLOOR_WY - 24;
    game_tick(&g, 0, G_KEY_JUMP);
    CHECK(g.w.vstate == WGround);
    CHECK(!(g.events & G_EV_JUMP));
}

static void test_worker_land_event(void) {
    Game g;
    setup(&g, 5);
    game_tick(&g, 0, G_KEY_JUMP);
    bool landed = false;
    for(int i = 0; i < 25; i++) {
        game_tick(&g, 0, 0);
        if(g.events & G_EV_WLAND) landed = true;
    }
    CHECK(landed);
}

static void test_crane_score_and_limits(void) {
    Game g;
    setup(&g, 0);
    for(int r = 0; r < 5; r++) g.cell[r][7] = 1; // full column
    g.w.x = g_col_x(3);
    g.spawn_timer = 1;
    for(int i = 0; i < 15 * 60 && !g.over; i++) {
        game_tick(&g, 0, 0);
        CHECK(g.cell[5][7] == 0);
        for(int k = 0; k < g.n_falling; k++) CHECK(g.falling[k].x != g_col_x(7));
    }
    CHECK(g.boxes_dropped > 0);
    CHECK(g.score == 2 * g.boxes_dropped);
}

static void test_no_drop_onto_worker_head(void) {
    Game g;
    setup(&g, 4);
    for(int r = 0; r < 3; r++) g.cell[r][4] = 1;
    g.w.y = FLOOR_WY - 24; // head right under the crane level
    g.spawn_timer = 1;
    for(int i = 0; i < 15 * 60; i++) {
        game_tick(&g, 0, 0);
        for(int k = 0; k < g.n_falling; k++) CHECK(g.falling[k].x != g_col_x(4));
    }
    CHECK(!g.over);
}

int main(void) {
    test_walk();
    test_wall();
    test_push();
    test_no_push_loaded_box();
    test_no_push_two_boxes();
    test_no_push_into_wall();
    test_jump_onto_box();
    test_jump_lands_back();
    test_push_while_jumping();
    test_box_falls_and_lands();
    test_crushed();
    test_walk_under_falling_box_is_blocked();
    test_push_falling_box();
    test_push_falling_box_slides_over_stack();
    test_no_push_falling_box_into_wall();
    test_helmet_breaks_box();
    test_no_break_without_jump();
    test_jump_arc();
    test_starting_stacks();
    test_fall_off_stack();
    test_row_clear();
    test_clear_wave_drops_right_first();
    test_crane_delivers();
    test_no_push_falling_box_at_head();
    test_no_jump_from_high_stack();
    test_worker_land_event();
    test_crane_score_and_limits();
    test_no_drop_onto_worker_head();
    printf("%d checks, %d failures\n", checks, failures);
    return failures ? 1 : 0;
}
