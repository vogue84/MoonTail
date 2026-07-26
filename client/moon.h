#ifndef MOONTAIL_MOON_H
#define MOONTAIL_MOON_H

#include <stdio.h>

#ifdef _WIN32
#  define WIN32_LEAN_AND_MEAN
#  include <windows.h>
#endif

/* Pixel comet sprite: W/Y/O core, C/T/D/B tail (assets/moontail-logo.png) */
static const char * const MOONTAIL_LOGO[] = {
    "                                ",
    "                  BBBBBBBB      ",
    "              BBBBBBBBBBBBBB    ",
    "          BBBBBBBBBBBBBBBBBBBB  ",
    "      DDDDDDDDDDDDDDDDDDDDDDDD  ",
    "  TTTTTTTTTTTTTTTTTTTTTTTTTTTTT ",
    " TTCCCCCCCCCCCCCCCCCCCCCCCCTTT  ",
    "CCOOYYYYWWWWYYYYOOCCCCCCCCCCTT  ",
    "CCOOYYYOOWWOOYYYYOOCCCCCCCCCT   ",
    " TTCCCCCCCCCCCCCCCCCCCCCCCCT    ",
    "    TTTTTTTTTTTTTTTTTTTTT       ",
    "        DDDDDDDDDDDD            ",
    "             BB                 ",
};

static inline void moontail_console_utf8(void) {
#ifdef _WIN32
    SetConsoleOutputCP(65001);
    SetConsoleCP(65001);
#endif
}

static inline int moontail_px_color(char ch) {
    switch (ch) {
    case 'W': return 255; /* white core */
    case 'Y': return 220; /* yellow glow */
    case 'O': return 208; /* orange rim */
    case 'C': return 51;  /* cyan coma */
    case 'T': return 45;  /* teal tail */
    case 'D': return 37;  /* dark teal */
    case 'B': return 24;  /* tail fade */
    default: return -1;
    }
}

static inline void moontail_px(char ch, int use_color, FILE * out) {
    int c = moontail_px_color(ch);
    if (c < 0) {
        fputc(' ', out);
        fputc(' ', out);
        return;
    }
    if (use_color)
        fprintf(out, "\033[38;5;%dm██\033[0m", c);
    else
        fputs("██", out);
}

static inline void moontail_print_moon(int use_color) {
    int i, n = (int)(sizeof(MOONTAIL_LOGO) / sizeof(MOONTAIL_LOGO[0]));
    const char * hi = use_color ? "\033[38;5;51m" : "";
    const char * reset = use_color ? "\033[0m" : "";

    moontail_console_utf8();
    fputc('\n', stderr);
    for (i = 0; i < n; i++) {
        const char * row = MOONTAIL_LOGO[i];
        for (; *row; row++)
            moontail_px(*row, use_color, stderr);
        fputc('\n', stderr);
    }
    fprintf(stderr, "%s  MoonTail%s  \033[2mtail the moon, split the experts\033[0m\n", hi, reset);
    fprintf(stderr, "%s  reciprocal MoE swarm CLI%s\n\n", hi, reset);
}

#endif
