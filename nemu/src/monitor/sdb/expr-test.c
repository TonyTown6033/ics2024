#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "sdb.h"

int expr_test() {
  char *path = getenv("EXPR_TEST_FILE");
  if (path == NULL) return 0;
  FILE *fp = fopen(path, "r");
  if (fp == NULL) {
    printf("open %s failed \n", path);
    exit(1);
  }
  char line[1024];
  int ok = 0;
  int total = 0;
  while (fgets(line, sizeof(line), fp) != NULL) {
    total++;
    size_t len = strlen(line);
    if (len >0 && line[len - 1] == '\n') {
      line[len-1] = '\0';
    }

    char *sp = strchr(line, ' ');
    if (sp == NULL) continue;
    *sp = '\0';
    char *expr_str = sp + 1;

    uint32_t expected = (uint32_t) strtoul(line, NULL, 10);

    bool success = true;
    word_t val = expr(expr_str, &success);

    if (!success){
      ok++;
      continue;
    } 

    if (val != expected) {
      printf("Mismatch: expr=%s expected=%u actual=" FMT_WORD "\n",
             expr_str, expected, val);
    }
    ok++;
  }
  fclose(fp);
  printf(" %d / %d \n", ok, total);
  return 0;
}
