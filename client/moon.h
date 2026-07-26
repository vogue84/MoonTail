#ifndef MOONTAIL_MOON_H
#define MOONTAIL_MOON_H

#include <stdio.h>

#ifdef _WIN32
#  define WIN32_LEAN_AND_MEAN
#  include <windows.h>
#endif

static inline void moontail_console_utf8(void) {
#ifdef _WIN32
    SetConsoleOutputCP(65001);
    SetConsoleCP(65001);
#endif
}

static inline void moontail_print_moon(int use_color) {
    static const char * lines[] = {
        "       ██       ",
        "     ██████     ",
        "   ██████████   ",
        "  ████████████  ",
        " ████··██··████ ",
        " ████·····████  ",
        "  ████████████  ",
        "   ██████████   ",
        "     ██████     ",
        "       ██       ",
    };
    const char * y = use_color ? "\033[38;5;229m" : "";
    const char * r = use_color ? "\033[0m" : "";
    int i;

    moontail_console_utf8();
    fputc('\n', stderr);
    for (i = 0; i < 10; i++)
        fprintf(stderr, "%s%s%s\n", y, lines[i], r);
    fprintf(stderr, "%s  MoonTail%s  \033[2mreciprocal MoE swarm CLI\033[0m\n\n", y, r);
}

#endif
