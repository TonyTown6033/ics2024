/***************************************************************************************
 * Copyright (c) 2014-2024 Zihao Yu, Nanjing University
 *
 * NEMU is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan
 *PSL v2. You may obtain a copy of Mulan PSL v2 at:
 *          http://license.coscl.org.cn/MulanPSL2
 *
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY
 *KIND, EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO
 *NON-INFRINGEMENT, MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 *
 * See the Mulan PSL v2 for more details.
 ***************************************************************************************/

#include <isa.h>

/* We use the POSIX regex functions to process regular expressions.
 * Type 'man regex' for more information about POSIX regex functions.
 */
#include <regex.h>
#include <sys/types.h>
#include <memory/vaddr.h>

enum {
  TK_NOTYPE = 256,
  TK_EQ,
  TK_NE,
  TK_AND,
  TK_OR,
  TK_NUM,
  TK_REG,
  TK_DEREF,
  TK_NEG,
};

// 词法规则：用正则把输入串切成 token
static struct rule {
  const char *regex;
  int token_type;
} rules[] = {

    /* TODO: Add more rules.
     * Pay attention to the precedence level of different rules.
     */

    {" +", TK_NOTYPE},           // spaces
    {"0x[0-9a-fA-F]+", TK_NUM},  // hex numbers
    {"[0-9]+", TK_NUM},          // dec numbers
    {"\\$[a-zA-Z0-9]+", TK_REG}, // register
    {"==", TK_EQ},               // equal
    {"!=", TK_NE},               // not equal
    {"&&", TK_AND},              // and
    {"\\|\\|", TK_OR},           // or
    {"\\+", '+'},                // plus
    {"-", '-'},
    {"\\*", '*'},
    {"/", '/'},
    {"\\(", '('},
    {"\\)", ')'}};

#define NR_REGEX ARRLEN(rules)

static regex_t re[NR_REGEX] = {};

/* Rules are used for many times.
 * Therefore we compile them only once before any usage.
 */
void init_regex() {
  int i;
  char error_msg[128];
  int ret;

  for (i = 0; i < NR_REGEX; i++) {
    ret = regcomp(&re[i], rules[i].regex, REG_EXTENDED);
    if (ret != 0) {
      regerror(ret, &re[i], error_msg, 128);
      panic("regex compilation failed: %s\n%s", error_msg, rules[i].regex);
    }
  }
}
#define MAX_TOKEN 1024
typedef struct token {
  int type;
  char str[32];
} Token;

static Token tokens[MAX_TOKEN] __attribute__((used)) = {};
static int nr_token __attribute__((used)) = 0;
static const char *expr_input = NULL;

/* 词法分析主循环：逐位置匹配 rules 并生成 tokens */
static bool make_token(char *e) {
  int position = 0;
  int i;
  regmatch_t pmatch;

  nr_token = 0;

  while (e[position] != '\0') {
    if (nr_token >= MAX_TOKEN) {
      printf("Too many tokens (max %d) in expression: %s\n", MAX_TOKEN, e);
      return false;
    }
    /* Try all rules one by one. */
    for (i = 0; i < NR_REGEX; i++) {
      if (regexec(&re[i], e + position, 1, &pmatch, 0) == 0 &&
          pmatch.rm_so == 0) {
        char *substr_start = e + position;
        int substr_len = pmatch.rm_eo;

        position += substr_len;

        /* TODO: Now a new token is recognized with rules[i]. Add codes
         * to record the token in the array `tokens'. For certain types
         * of tokens, some extra actions should be performed.
         */

        int max = (substr_len >= 32) ? (32 - 1) : substr_len;
        switch (rules[i].token_type) {
        case TK_NOTYPE:
          break;
        case TK_NUM:
        case TK_REG:
          tokens[nr_token].type = rules[i].token_type;
          strncpy(tokens[nr_token].str, substr_start, max);
          tokens[nr_token].str[max] = '\0';
          nr_token++;
          break;
        default:
          tokens[nr_token].type = rules[i].token_type;
          nr_token++;
          break;
        }

        break;
      }
    }
    if (i == NR_REGEX) {
      printf("no match at position %d\n%s\n%*.s^\n", position, e, position, "");
      return false;
    }
  }
  bool prevIsOperand= false;
  for (int i = 0; i < nr_token; i++) {
    if (!prevIsOperand) {
      switch (tokens[i].type) {
      case '-':
        tokens[i].type = TK_NEG;
        break;
      case '*':
        tokens[i].type = TK_DEREF;
        break;
      default:
        break;
      }
    }
    prevIsOperand = tokens[i].type == TK_NUM || tokens[i].type == TK_REG ||
               tokens[i].type == ')';
  }

  return true;
}

/* check_parentheses
 * 判断tokens[l..r] 是否被括号完整包住
 */

static bool check_parentheses(int l, int r) {
  if (!(tokens[l].type == '(' && tokens[r].type == ')')) return false;
  int balance = 0;
  for (int i = l; i <= r; i++) {
    if (tokens[i].type == '(') {
      balance++;
    } else if (tokens[i].type == ')') {
      balance--;
    }
    if (balance == 0 && i < r)  return false; // 括号提前闭合了
  }

  return balance == 0;
}
// priority: smaller value means lower precedence
#define NOT_OP 100
static int priority(int type) {
  switch (type) {
    case TK_OR:
      return 1;
    case TK_AND:
      return 2;
    case TK_EQ:
    case TK_NE:
      return 3;
    case '+':
    case '-':
      return 4;
    case '*':
    case '/':
      return 5;
    case TK_NEG:
    case TK_DEREF:
      return 6;
    default:
      return NOT_OP;
  }
}

// 返回优先级最低的, 最右侧的运算符的位置
// 若返回-1则是错误
static int dominant_op(int l, int r) {
  int balance = 0;
  int min_prior = NOT_OP;
  int op = -1;

  for (int i = l; i <= r; i++) {
    if (tokens[i].type == '(') {
      balance++;
      continue;
    } else if(tokens[i].type == ')') {
      balance--;
      continue;
    }
    // 如果在括号内 跳过
    int p = priority(tokens[i].type);
    if (balance > 0 || p == NOT_OP) continue;
    if (p <= min_prior) {
      min_prior = p;
      op = i; 
    }
  }

  return op;
}

// 计算tokens的值 success 则表示求值成功与否
// 0 只是返回值 要看是否成功运算需要看success的状态
#define ERROR 0
static word_t eval(int l, int r, bool *success) {
  if (!*success) return ERROR;
  if (l > r) {
    *success = false;
    printf("Bad expression: empty range [%d,%d] in expr: %s\n", l, r, expr_input ? expr_input : "<null>");
    return ERROR;
  }
  word_t val = ERROR;
  if (l == r) {
    switch (tokens[l].type) {
      case TK_NUM:
        return (word_t) strtoul(tokens[l].str, NULL, 0);
      case TK_REG:
        val = isa_reg_str2val(tokens[l].str+1, success);
        if (!*success) return ERROR;
        return val;
      default:
        *success = false;
        printf("Unexpected token type %d at position %d in expr: %s\n", tokens[l].type, l, expr_input ? expr_input : "<null>");
        return ERROR;
    }
  }
  if (check_parentheses(l,r)) return eval(l+1, r-1, success);
  int op = dominant_op(l,r);
  if (op == -1) {
    *success = false;
    printf("No dominant operator in range [%d,%d] for expr: %s\n", l, r, expr_input ? expr_input : "<null>");
    return ERROR;
  }
  if (tokens[op].type == TK_NEG) {
    val = eval(op+1 , r, success);
    return -val;
  }else if (tokens[op].type == TK_DEREF) {
    val = eval(op+1, r, success);
    return vaddr_read(val, 4);  //读取4 Bytes 的内容
  }


  word_t val1 = eval(l ,op - 1, success);
  word_t val2 = eval(op + 1, r, success);
      
  switch (tokens[op].type) {
    case '+':
      return val1 + val2;
    case '-':
      return val1 - val2;
    case '*':
      return val1 * val2;
    case '/':
      if (val2 == 0) {
        *success = false;
        printf("Division by zero in expr: %s\n", expr_input ? expr_input : "<null>");
        return ERROR;
      }
      return val1 / val2;
    case TK_EQ:
      return val1 == val2;
    case TK_NE:
      return val1 != val2;
    case TK_AND:
      return val1 && val2;
    case TK_OR:
      return val1 || val2;
    default:
      *success = false;
      printf("Unknown operator type %d in expr: %s\n", tokens[op].type, expr_input ? expr_input : "<null>");
  }
  return ERROR;
}


word_t expr(char *e, bool *success) {
  // 先做分词，失败直接返回
  expr_input = e;
  if (!make_token(e)) {
    *success = false;
    return 0;
  }

  /* TODO: Insert codes to evaluate the expression. */
  *success = true;
  return eval(0, nr_token - 1, success);
}
