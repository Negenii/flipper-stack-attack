// Host preview: plays the game with random input and writes frames as PBM files.
// cc -I.. preview.c ../game.c ../render.c -o preview && ./preview outdir
#include "render.h"

#include <stdio.h>
#include <stdlib.h>

static void write_pbm(const char* path, const uint8_t* fb) {
    FILE* f = fopen(path, "wb");
    if(!f) return;
    fprintf(f, "P4\n%d %d\n", FB_W, FB_H);
    for(int y = 0; y < FB_H; y++) {
        for(int bx = 0; bx < FB_STRIDE; bx++) {
            uint8_t b = fb[y * FB_STRIDE + bx], out = 0;
            for(int i = 0; i < 8; i++)
                if(b & (1 << i)) out |= 0x80 >> i;
            fputc(out, f);
        }
    }
    fclose(f);
}

int main(int argc, char** argv) {
    const char* dir = argc > 1 ? argv[1] : ".";
    int ticks = argc > 2 ? atoi(argv[2]) : 3000;
    int every = argc > 3 ? atoi(argv[3]) : 100;
    static uint8_t fb[FB_SIZE];
    char path[512];

    render_title(fb);
    snprintf(path, sizeof(path), "%s/title.pbm", dir);
    write_pbm(path, fb);

    Game g;
    game_init(&g, 12345, 1);
    srand(1);
    uint8_t held = 0;
    for(int t = 0; t < ticks && !g.over; t++) {
        if(t % 12 == 0) {
            int r = rand() % 6;
            held = r < 2 ? G_KEY_LEFT : r < 4 ? G_KEY_RIGHT : 0;
        }
        uint8_t pressed = (rand() % 20 == 0) ? G_KEY_JUMP : 0;
        game_tick(&g, held, pressed);
        if(t % every == 0 || g.over || (g.events & G_EV_CLEAR)) {
            render_game(fb, &g, 684);
            snprintf(path, sizeof(path), "%s/f%05d.pbm", dir, t);
            write_pbm(path, fb);
        }
    }
    printf("ticks=%u score=%lu dropped=%lu rows=%lu over=%d\n", (unsigned)g.tick,
           (unsigned long)g.score, (unsigned long)g.boxes_dropped, (unsigned long)g.rows_cleared,
           g.over);
    return 0;
}
