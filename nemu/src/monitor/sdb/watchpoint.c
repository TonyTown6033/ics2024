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

#include <monitor/sdb.h>

#define NR_WP 32
#define EXPR_MAX 65536
typedef struct watchpoint {
  int NO;
  char expr[EXPR_MAX];
  struct watchpoint *next;
  word_t value;

} WP;

// 监视点池：数组 + 空闲链表
static WP wp_pool[NR_WP] = {};
// head: 已用链表；free_: 空闲链表
static WP *head = NULL, *free_ = NULL;

void init_wp_pool() {
  // 初始化空闲链表
  int i;
  for (i = 0; i < NR_WP; i ++) {
    wp_pool[i].NO = i;
    wp_pool[i].next = (i == NR_WP - 1 ? NULL : &wp_pool[i + 1]);
  }

  head = NULL;
  free_ = wp_pool;
}

/* TODO: Implement the functionality of watchpoint */

bool wp_add(const char *expr_str) {
  if(free_ ==  NULL) {
    printf("watch ponint add failed , no free node \n");
    return false;
  } 
  // 计算expr的值到 wp->value
  bool success = true;
  word_t expr_value = expr((char *)expr_str, &success);
  if (!success) {
    printf("Calculate the value of expr falied , expr is %s \n", expr_str);
    return false;
  } 

  // 添加expr_str到 wp->expr
  if (strlen(expr_str) >= EXPR_MAX) {
    printf("expr is too long for watch point , size of expr_str is %zu \n", strlen(expr_str));
    return false;
  }

  WP *front = free_;
  free_ = free_->next;
  front->next = head;
  head = front;

  snprintf(front->expr, EXPR_MAX, "%s", expr_str);
  front->expr[strlen(expr_str)] = '\0';

  front->value = expr_value;
  printf("Watch Point added NO:%d EXPR:%s Value: " FMT_WORD "\n", front->NO, front->expr, front->value);

  return true; }

bool wp_remove(int no) {
  bool status = false;
  WP *cur = head;
  WP *prev = NULL;
  while(cur != NULL) {
    if (cur->NO == no) {
      status = true;
      break;
    }
    prev = cur;
    cur = cur->next;
  }
  if (!status) {
    printf("no found NO: %d watch point \n", no);
    return false;
  }

  if (prev != NULL) {
    prev->next = cur->next;
  } else {
    head = head -> next;
  }

  cur->next = free_;
  free_ = cur;

  return true;
}
void wp_list() {
  WP *cur = head;
  while(cur != NULL) {
    printf("NO:%d  %s " FMT_WORD"\n", 
        cur->NO, cur->expr, cur->value);
    cur = cur->next;
  }
}
bool check_wp() {
  if (head == NULL) return false;
  bool changed = false;
  WP *cur = head;
  while(cur != NULL) {
    bool success = true;
    word_t newValue = expr(cur->expr, &success);
    if (!success) {
      printf("Calculate value failed , expr is %s\n",cur->expr);
      return false;
    }
    if (newValue != cur->value) {
      changed = true;
      printf("NO:%d  EXPR: %s changed " FMT_WORD "->" FMT_WORD "\n",cur->NO, cur->expr, cur->value, newValue);
      cur->value = newValue;
    }
    cur = cur->next;
  }
  return changed;
}

