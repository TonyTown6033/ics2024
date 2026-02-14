#include <am.h>
#include <klib.h>
#include <klib-macros.h>
#include <stdarg.h>

#if !defined(__ISA_NATIVE__) || defined(__NATIVE_USE_KLIB__)

int printf(const char *fmt, ...) {
  panic("Not implemented");
}

int vsprintf(char *out, const char *fmt, va_list ap) {
  char *p = out;
  int n = 0;
  
  for (const char *f = fmt; *f; f++) {
    if (*f != '%') {
      *p++ = *f;
      n++;
      continue;
    }

    f++;
    switch (*f) {
      case 's':
        {
          const char *s = va_arg(ap, const char *);
          if (!s) s = "(null)";
          while (*s) {
            *p++ = *s++;
            n++;
          }
          break;
        }
      case 'd':
        {
          int v = va_arg(ap, int);
          unsigned int u;
          if (v < 0) {
            *p++ = '-';
            n++;
            u = (unsigned int)(- (unsigned int)v);
          } else {
            u = (unsigned int)v;
          }
          
          char tmp[16];
          int k = 0;
          do {
            tmp[k++] = '0' + (u % 10);
            u /= 10;
          } while (u);
          while(k--) {
            *p++ = tmp[k];
            n++;
          }
          break;
        }
      default:
        if (*f) {
          *p++ = *f;
          n++;
        }
    }
  }
  *p = '\0';
  return n;
}

int sprintf(char *out, const char *fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  int ret = vsprintf(out, fmt, ap);
  va_end(ap);
  return ret;
}

int snprintf(char *out, size_t n, const char *fmt, ...) {
  panic("Not implemented");
}

int vsnprintf(char *out, size_t n, const char *fmt, va_list ap) {
  panic("Not implemented");
}

#endif
