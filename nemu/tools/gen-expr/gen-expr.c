/***************************************************************************************
* Copyright (c) 2014-2024 Zihao Yu, Nanjing University
*
* NEMU is licensed under Mulan PSL v2.
* You can use this software according to the terms and conditions of the Mulan PSL v2.
* You may obtain a copy of Mulan PSL v2 at:
*          http://license.coscl.org.cn/MulanPSL2
*
* THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
* EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
* MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
*
* See the Mulan PSL v2 for more details.
***************************************************************************************/

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <assert.h>
#include <string.h>

// this should be enough
static char buf[65536] = {};
#define MAX_LEN 60000
#define MAX_DEPTH 10
static int depth = 0;
static size_t pos = 0;
static char code_buf[65536 + 128] = {}; // a little larger than `buf`
static char *code_format =
"#include <stdio.h>\n"
"int main() { "
"  unsigned result = %s; "
"  printf(\"%%u\", result); "
"  return 0; "
"}";

static uint32_t choose(int i) {
  if (i <= 0) return 0;
  return rand() % i;
}
static void buf_append_number(uint32_t number) {
  if (pos >= sizeof(buf) - 1) return;
  size_t remain = sizeof(buf) - pos;
  int n = snprintf(buf + pos, remain, "%u", number);
  if (n < 0) return;
  if ((size_t)n >= remain) {
    pos = sizeof(buf) - 1;
    buf[pos] = '\0';
    return;
  }
  pos += (size_t)n;
}

static void buf_append_string(const char *str) {
  if (pos >= sizeof(buf) - 1) return;
  size_t remain = sizeof(buf) - pos;
  int n = snprintf(buf + pos, remain, "%s", str);
  if (n < 0) return;
  if ((size_t)n >= remain) {
    pos = sizeof(buf) - 1;
    buf[pos] = '\0';
    return;
  }
  pos += (size_t)n;
}


static void gen(char c) {
  if (pos + 1 >= sizeof(buf)) return;
  buf[pos++] = c;
  buf[pos] = '\0';
}
 

static void gen_num() {
  uint32_t rand_val = choose(3000);

  if (choose(2) == 0) {
    buf_append_string("0x");
  }

  buf_append_number(rand_val);
}
static void gen_rand_op()  {

  switch (choose(6)) {
    case 0:
      gen('+');
      break;
    case 1:
      gen('-');
      break;
    case 2:
      gen('/');
      break;
    case 3:
      gen('*');
      break;
    case 4:
      buf_append_string("||");
      break;
    case 5:
      buf_append_string("&&");
      break;
  }
}

static void gen_rand_expr() {
  if (depth > MAX_DEPTH || pos > MAX_LEN) {
    gen_num();
    return;
  }
  depth++;
  switch (choose(3)) {
    case 0: gen_num(); break;
    case 1: gen('('); gen_rand_expr(); gen(')'); break;
    default: gen_rand_expr(); gen_rand_op(); gen_rand_expr(); break;
  }
  depth--;
}

int main(int argc, char *argv[]) {
 int seed = time(0);
  srand(seed);
  int loop = 1;
  if (argc > 1) {
    sscanf(argv[1], "%d", &loop);
  }
  int i;
  for (i = 0; i < loop; i ++) {
    buf[0] = '\0';
    pos = 0;
    gen_rand_expr();

    sprintf(code_buf, code_format, buf);

    FILE *fp = fopen("/tmp/.code.c", "w");
    assert(fp != NULL);
    fputs(code_buf, fp);
    fclose(fp);

    int ret = system("gcc /tmp/.code.c -o /tmp/.expr");
    if (ret != 0) continue;

    fp = popen("/tmp/.expr", "r");
    assert(fp != NULL);

    int result;
    ret = fscanf(fp, "%d", &result);
    pclose(fp);

    printf("%u %s\n", result, buf);
  }
  return 0;
}
