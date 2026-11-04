#ifndef LGC5_PYROWAVE_DIAG_H
#define LGC5_PYROWAVE_DIAG_H
#include <stdio.h>
#include <stdarg.h>
#include <time.h>
static inline void lgc5_pw_diag(const char *fmt, ...) {
 FILE *f=fopen("/tmp/lgc5-pyrowave-diag.log","a");
 if (!f) return;
 fprintf(f,"%ld ",(long)time(NULL));
 va_list args; va_start(args,fmt); vfprintf(f,fmt,args); va_end(args);
 fputc('\n',f); fclose(f);
}
#endif
