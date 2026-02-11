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

#include <isa.h>
#include <cpu/cpu.h>
#include <readline/readline.h>
#include <readline/history.h>
#include <stdio.h>
#include <string.h>
#include "sdb.h"
#include <memory/vaddr.h>

static int is_batch_mode = false;

void init_regex();
void init_wp_pool();

/* We use the `readline' library to provide more flexibility to read from stdin. */
static char* rl_gets() {
  static char *line_read = NULL;

  if (line_read) {
    free(line_read);
    line_read = NULL;
  }

  line_read = readline("(nemu) ");

  if (line_read && *line_read) {
    add_history(line_read);
  }

  return line_read;
}

static int cmd_c(char *args) {
  cpu_exec(-1);
  return 0;
}


static int cmd_q(char *args) {
  return -1;
}

static int cmd_help(char *args);

static int cmd_info(char *args);

static int cmd_p(char *args); 

static int cmd_si(char *args); 

static int cmd_x(char *args); 

static int cmd_w(char *args); 

static int cmd_d(char *args); 

// 命令表：name/description/handler
static struct {
  const char *name;
  const char *description;
  int (*handler) (char *);
} cmd_table [] = {
  { "help", "Display information about all supported commands", cmd_help },
  { "c", "Continue the execution of the program", cmd_c },
  { "q", "Exit NEMU", cmd_q },
  { "info", "Get the value of registers(include PC)", cmd_info},
  { "p", "Print the value of the expression", cmd_p},
  { "si", "Let program run x step then stop execution, when x is not given x = 1", cmd_si},
  { "x", "Calculate the value of the EXPR, set the result as the memory address,  \
          print the continus 4 Bytes in this memory address", cmd_x},
  { "w", "Stop the program when EXPR changes", cmd_w},
  { "d", "Delete the watch point which order is n", cmd_d},

  /* TODO: Add more commands */

};

#define NR_CMD ARRLEN(cmd_table)

static int cmd_help(char *args) {
  /* extract the first argument */
  char *arg = strtok(NULL, " ");
  int i;

  if (arg == NULL) {
    /* no argument given */
    for (i = 0; i < NR_CMD; i ++) {
      printf("%s - %s\n", cmd_table[i].name, cmd_table[i].description);
    }
  }
  else {
    for (i = 0; i < NR_CMD; i ++) {
      if (strcmp(arg, cmd_table[i].name) == 0) {
        printf("%s - %s\n", cmd_table[i].name, cmd_table[i].description);
        return 0;
      }
    }
    printf("Unknown command '%s'\n", arg);
  }
  return 0;
}

static int cmd_info(char *args) {
   if (args == NULL) {
    printf(" info [r][w] used to show values of [r]gister or [w]atchpoint \n");
    return 0;
  }  
  char *arg = strtok(args, " ");
  if (strcmp(arg, "r") == 0) {
    isa_reg_display();
  } else if (strcmp(arg, "w") == 0) {
    wp_list();
  }
  return 0;
}

static int cmd_p(char *args) {
  if (args == NULL) {
    printf("p expr to calculate this expr into values \n");
    return 0;
  }
  bool success = true;
  word_t val = expr(args, &success);

  if (success) {
    printf(FMT_WORD "\n", val);
  } else {
    printf("Here is error happened when calculate \n");
  }
  return 0;
}

static int cmd_x(char *args) {
  if (args == NULL) {
    printf("usage: x N EXPR \n");
    return 0;
  }
  while(isspace((unsigned char) *args)) args++;
  if (*args == '\0') {
    printf("usage: x N EXPR \n");
    return 0;
  }
  char *end = NULL;
  unsigned long n = strtoul(args, &end, 10);
  if (end == args || n == 0 || *args == '-') {
    printf("usage: x N EXPR \n");
    return 0;
  }
  while(isspace((unsigned char) *end)) end++;
  if (*end == '\0') {
    printf("usage: x N EXPR \n");
    return 0;
  }
  bool success = true;
  word_t addr = expr(end, &success);
  if (!success) {
    printf("bad EXPR: %s \n", end);
    return 0;
  }

  for(size_t i = 0; i < n; i++) {
    vaddr_t cur = addr + i * 4;
    word_t data = vaddr_read(cur, 4);
    printf(FMT_WORD ": " FMT_WORD "\n", cur,data);
  }

  return 0;
}

static int cmd_d(char *args) {
  if (args == NULL) {
    printf("usage: d No \n");
    return 0;
  }
  while (isspace((unsigned char) *args)) args++;
  if (*args == '\0') {
    printf("usage: d No \n");
    return 0;
  }
  char *end = NULL;
  unsigned long n = strtoul(args, &end, 10);
  if (end == args || n == 0 || *args == '-') {
    printf("useage: x N EXPR \n");
    return 0;
  }
  bool success = wp_remove((int) n);
  if (!success) {
    printf("remove watch point falied, no %lu \n", n);
  }

  return 0;
}

static int cmd_w(char *args) {
  if (args == NULL) {
    printf("usage: w EXPR \n");
    return 0;
  }
  if (!wp_add((const char *) args)) {
    printf("add watch point failed, args is %s \n", args);
  }
  return 0;
}

static int cmd_si(char *args) {
  uint64_t n = 1;
  if (args != NULL) {
    char *arg = strtok(args, " ");
    if (arg != NULL) {
      char *end = NULL;
      unsigned long v = (uint64_t) strtoul(arg, &end, 10);
      if (end == arg || *end != '\0' || arg[0] == '-') {
        printf("usage: si [N] (N is positive integer) \n");
        return 0;
      }
      n = (uint64_t) v;
    }
  }
  // cpu_exec 对n进行了校验
  cpu_exec(n);
  return 0;
}

void sdb_set_batch_mode() {
  // 批处理模式：直接执行 c 命令，不进入交互
  is_batch_mode = true;
}

void sdb_mainloop() {
  // 主循环：读命令、解析参数、分发到 handler
  if (is_batch_mode) {
    cmd_c(NULL);
    return;
  }

  for (char *str; (str = rl_gets()) != NULL; ) {
    char *str_end = str + strlen(str);

    /* extract the first token as the command */
    char *cmd = strtok(str, " ");
    if (cmd == NULL) { continue; }

    /* treat the remaining string as the arguments,
     * which may need further parsing
     */
    char *args = cmd + strlen(cmd) + 1;
    if (args >= str_end) {
      args = NULL;
    }

#ifdef CONFIG_DEVICE
    extern void sdl_clear_event_queue();
    sdl_clear_event_queue();
#endif

    int i;
    for (i = 0; i < NR_CMD; i ++) {
      if (strcmp(cmd, cmd_table[i].name) == 0) {
        if (cmd_table[i].handler(args) < 0) { return; }
        break;
      }
    }

    if (i == NR_CMD) { printf("Unknown command '%s'\n", cmd); }
  }
}

void init_sdb() {
  /* Compile the regular expressions. */
  init_regex();

  /* Initialize the watchpoint pool. */
  init_wp_pool();
}
