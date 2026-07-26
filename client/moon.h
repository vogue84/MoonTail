#ifndef MOONTAIL_MOON_H
#define MOONTAIL_MOON_H

#include <stdio.h>

#ifdef _WIN32
#  define WIN32_LEAN_AND_MEAN
#  include <windows.h>
#endif

/* Colibrì-style pixel comet: M=magenta spark, W=moon, C=cyan, T/D=tail fade */
static const char * const MOONTAIL_COMET[] = {
    "          MM        ",
    "         WWCC       ",
    "        WWCCCC      ",
    "       WWCCCCCC     ",
    "      WWCCCCCCCC    ",
    "    TTWWCCCCCCWW    ",
    "  TTTTTTTTTTTTTTT   ",
    " TTTTTTTDDDDTTTTTT  ",
    "TTTTTTTTTTTTTTTTTT  ",
    " DDDDDDDDDDDDDDD    ",
};

static inline void moontail_console_utf8(void) {
#ifdef _WIN32
    SetConsoleOutputCP(65001);
    SetConsoleCP(65001);
#endif
}

static inline int moontail_px_color(char ch) {
    switch (ch) {
    case 'M': return 201; /* magenta spark */
    case 'W': return 255; /* moon highlight */
    case 'C': return 51;  /* cyan core */
    case 'T': return 45;  /* teal tail */
    case 'D': return 24;  /* dim tail */
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
    static const char * side[] = {
        "",
        "",
        "  \033[38;5;51mMoonTail\033[0m",
        "  \033[2mtail the moon, split the experts\033[0m",
        "",
        "",
        "",
        "",
        "",
        "",
    };
    int i, n = (int)(sizeof(MOONTAIL_COMET) / sizeof(MOONTAIL_COMET[0]));
    const char * title = use_color ? "\033[38;5;51m" : "";
    const char * reset = use_color ? "\033[0m" : "";

    moontail_console_utf8();
    fputc('\n', stderr);
    for (i = 0; i < n; i++) {
        const char * row = MOONTAIL_COMET[i];
        for (; *row; row++)
            moontail_px(*row, use_color, stderr);
        if (use_color && side[i][0])
            fprintf(stderr, "%s", side[i]);
        else if (!use_color && i == 2)
            fprintf(stderr, "  MoonTail");
        else if (!use_color && i == 3)
            fprintf(stderr, "  tail the moon, split the experts");
        fputc('\n', stderr);
    }
    fprintf(stderr, "%s  reciprocal MoE swarm CLI%s\n\n", title, reset);
}

#endif
