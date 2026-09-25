#ifndef MOONTAIL_BANNER_H
#define MOONTAIL_BANNER_H
#include <stdio.h>
#ifdef _WIN32
#  define WIN32_LEAN_AND_MEAN
#  include <windows.h>
static inline void moontail_console_utf8(void) {
    SetConsoleOutputCP(65001);
    SetConsoleCP(65001);
}
#else
static inline void moontail_console_utf8(void) { (void)0; }
#endif
static inline void moontail_print_moon(int use_color) {
    const char * hi = use_color ? "\033[38;5;51m" : "";
    const char * reset = use_color ? "\033[0m" : "";
    moontail_console_utf8();
    fprintf(stderr, "\n%s     .%s\n  %s_/\\_%s   MoonTail\n %s(====)%s  tail the moon, split the experts\n\n",
            hi, reset, hi, reset, hi, reset);
}
#endif
