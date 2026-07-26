#ifndef MOONTAIL_MOON_H
#define MOONTAIL_MOON_H

#include <stdio.h>

#ifdef _WIN32
#  define WIN32_LEAN_AND_MEAN
#  include <windows.h>
#endif

/* Pixel UFO: W=dome, C=saucer, M=portholes, T=rim glow, D=beam tail */
static const char * const MOONTAIL_LOGO[] = {
    "         WW         ",
    "        WCCC        ",
    "      CCCCCCCC      ",
    "     CCCCCCCCCC     ",
    "    CCMMCCCCMMCC    ",
    "   CCCCCCCCCCCCCC   ",
    "    TTTTTTTTTTTT    ",
    "     DDDDDDDDDD     ",
    "      DDDDDD        ",
    "       DDDD         ",
};

static inline void moontail_console_utf8(void) {
#ifdef _WIN32
    SetConsoleOutputCP(65001);
    SetConsoleCP(65001);
#endif
}

static inline int moontail_px_color(char ch) {
    switch (ch) {
    case 'M': return 201; /* porthole lights */
    case 'W': return 255; /* dome highlight */
    case 'C': return 51;  /* saucer hull */
    case 'T': return 45;  /* rim glow */
    case 'D': return 24;  /* tractor beam tail */
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
        "",
        "",
        "  \033[38;5;51mMoonTail\033[0m",
        "  \033[2mtail the moon, split the experts\033[0m",
        "",
        "",
        "",
        "",
    };
    int i, n = (int)(sizeof(MOONTAIL_LOGO) / sizeof(MOONTAIL_LOGO[0]));
    const char * title = use_color ? "\033[38;5;51m" : "";
    const char * reset = use_color ? "\033[0m" : "";

    moontail_console_utf8();
    fputc('\n', stderr);
    for (i = 0; i < n; i++) {
        const char * row = MOONTAIL_LOGO[i];
        for (; *row; row++)
            moontail_px(*row, use_color, stderr);
        if (use_color && side[i][0])
            fprintf(stderr, "%s", side[i]);
        else if (!use_color && i == 4)
            fprintf(stderr, "  MoonTail");
        else if (!use_color && i == 5)
            fprintf(stderr, "  tail the moon, split the experts");
        fputc('\n', stderr);
    }
    fprintf(stderr, "%s  reciprocal MoE swarm CLI%s\n\n", title, reset);
}

#endif
