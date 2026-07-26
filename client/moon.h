#ifndef MOONTAIL_MOON_H
#define MOONTAIL_MOON_H

#include <stdio.h>

/* 8-bit pixel moon — shown on MoonTail CLI startup (Claude Code crab energy) */
static inline void moontail_print_moon(int use_color) {
    const char * y = use_color ? "\033[38;5;229m" : "";
    const char * d = use_color ? "\033[38;5;245m" : "";
    const char * r = use_color ? "\033[0m" : "";
    fputc('\n', stderr);
    fprintf(stderr, "%s", y);
    fprintf(stderr, "        ████╗\n");
    fprintf(stderr, "     ██████████╗\n");
    fprintf(stderr, "   ██████████████╗\n");
    fprintf(stderr, "  █████%s░░░%s███████╗\n", d, y);
    fprintf(stderr, " █████%s░░░░░░░%s██████╗\n", d, y);
    fprintf(stderr, " █████%s░░██░░░░%s█████╗\n", d, y);
    fprintf(stderr, " █████%s░████░░░%s█████╗\n", d, y);
    fprintf(stderr, " ██████%s░░░░░░░%s██████╗\n", d, y);
    fprintf(stderr, "  ███████%s░░░%s███████╗\n", d, y);
    fprintf(stderr, "   ██████████████╝\n");
    fprintf(stderr, "     ██████████╝\n");
    fprintf(stderr, "        ████╝%s\n", r);
    fprintf(stderr, "%s  MoonTail%s  \033[2mreciprocal MoE swarm CLI\033[0m\n\n", y, r);
}

#endif
