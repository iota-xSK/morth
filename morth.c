/*
MIT License

Copyright (c) 2024 Juraj Babić

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/

#include <ctype.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef WORD_N
#define WORD_N 0xffff
#endif

#ifndef DEF_N
#define DEF_N 256
#endif

#ifndef MEMSIZE
#define MEMSIZE 0xffffff
#endif

#ifndef NAMELEN
#define NAMELEN 16
#endif

#ifndef STACHSIZE
#define STACKSIZE 1024
#endif

#ifndef BUFSIZE
#define BUFSIZE 1024
#endif

struct Word;
typedef int cell;
typedef long int dcell;
typedef cell (*func)(struct Word *, struct Word *);
void add_non_primitive(char name[], cell *def, cell def_len);
int ip_d = 0;

typedef struct Word {
  cell def_len;
  char name[16];
  bool immediate;
  func enter;
  cell def[DEF_N]; // indexes
} Word;

typedef struct {
  int data[STACKSIZE];
  int sp;
} Stack;

static Word *dict;
static cell *membank;
static int top_word = -1;
static char *inputbuff;
cell inputidx = 0;
static char next_word[16];
static char lookahead_word[16];
static cell memtop = -1;

int pcount = 0;

char *advance() {
start:
  while (isspace(inputbuff[inputidx]) && inputbuff[inputidx] != '\0')
    inputidx++;

  int i = 0;
  while (!isspace(inputbuff[inputidx + i]) && inputbuff[inputidx + i] != '\0' &&
         i < 15) {
    i++;
  }
  if (i)
    memcpy(next_word, &inputbuff[inputidx], i);
  next_word[i] = '\0';
  inputidx += i;

  if (strcmp(next_word, "(") == 0) {
    pcount++;
    printf("parent: %s, %i\n", next_word, pcount);
    advance();
  } else if (strcmp(next_word, ")") == 0) {
    pcount--;
    printf("parent: %s, %i\n", next_word, pcount);
    if (pcount != 0)
      advance();
    advance();
  }

  return next_word;
}

int lookahead_idx;
char *lookahead() {
  lookahead_idx = inputidx;
  while (isspace(lookahead_word[lookahead_idx]) &&
         lookahead_word[lookahead_idx] != '\0')
    lookahead_idx++;

  int i = 0;
  while (!isspace(lookahead_word[lookahead_idx + i]) &&
         lookahead_word[lookahead_idx + i] != '\0' && i < 15) {
    i++;
  }
  if (i)
    memcpy(next_word, &lookahead_word[lookahead_idx], i);
  next_word[i] = '\0';
  lookahead_idx += i;

  if (strcmp(next_word, "(") == 0) {
    pcount++;
    printf("parent: %s, %i\n", next_word, pcount);
    lookahead();
  } else if (strcmp(next_word, ")") == 0) {
    pcount--;
    printf("parent: %s, %i\n", next_word, pcount);
    if (pcount != 0)
      lookahead();
    lookahead();
  }

  return lookahead_word;
}

static Stack ds = {.data = {0}, .sp = -1};
static Stack rs = {.data = {0}, .sp = -1};

int pop_int(int *err) {
  if (ds.sp < 0) {
    *err = -1;
    return 0;
  }
  *err = 1;
  int data = ds.data[ds.sp--];
  return data;
}

dcell pop_int2(int *err) {
  if (ds.sp < 1) {
    *err = -1;
    return 0;
  }
  *err = 1;
  cell high = ds.data[ds.sp--];
  cell low = ds.data[ds.sp--];
  return ((cell)low | ((dcell)(high) << (sizeof(cell) * 8)));
}

int push_int2(dcell to_push) {
  if (ds.sp >= STACKSIZE) {
    return -2;
  }
  cell high = (cell)(to_push >> 32);       // Extracting the first 32 bits
  cell low = (cell)(to_push & 0xFFFFFFFF); // Extracting the last 32 bits
  ds.data[++ds.sp] = low;
  ds.data[++ds.sp] = high;
  return 1;
}

int push_int(int to_push) {
  if (ds.sp >= STACKSIZE) {
    return -2;
  }
  ds.data[++ds.sp] = to_push;
  return 1;
}

cell popr_int(cell *err) {
  if (rs.sp < 0) {
    *err = -1;
    return 0;
  }
  *err = 1;
  int data = rs.data[rs.sp--];
  return data;
}

cell pushr_int(cell to_push) {
  if (rs.sp >= STACKSIZE) {
    return -2;
  }
  rs.data[++rs.sp] = to_push;
  return 1;
}

cell balloc_int(cell size) {
  if (size <= 0 || size + memtop >= MEMSIZE) {
    return -8;
  }
  cell r = memtop;
  memtop += size;
  return r;
}

cell balloc(Word *self, Word *caller) { // returns index into membank
  int err;
  cell size = pop_int(&err);
  if (err != 1) {
    return err;
  }
  return err = balloc_int(size);
}

cell bfree_int(cell size) {
  if (size <= 0 || memtop - size < 0) {
    return -8;
  }
  memtop -= size;
  return 1;
}

cell bfree(Word *self, Word *caller) { // returns index into membank
  int err;
  cell size = pop_int(&err);
  if (err != 1) {
    return err;
  }
  return err = bfree_int(size);
}

int pop(Word *word, Word *caller) {
  if (ds.sp >= 0) {
    ds.sp--;
  } else {
    return -1;
  }
  return 1;
}

int dot(Word *word, Word *caller) {
  if (ds.sp >= 0) {
    printf("%i ok \n", ds.data[ds.sp--]);
  } else {
    return -1;
  }
  return 1;
}

int add(Word *word, Word *caller) {
  int err = 1;
  int a = pop_int(&err);
  if (err != 1) {
    return err;
  }
  int b = pop_int(&err);
  if (err != 1) {
    return err;
  }
  return push_int(a + b);
}

int add2(Word *word, Word *caller) {
  int err = 1;
  dcell a = pop_int2(&err);
  if (err != 1) {
    return err;
  }
  dcell b = pop_int2(&err);
  if (err != 1) {
    return err;
  }
  return push_int2(a + b);
}

int sub(Word *word, Word *caller) {
  int err = 1;
  int b = pop_int(&err);
  if (err != 1) {
    return err;
  }
  int a = pop_int(&err);
  if (err != 1) {
    return err;
  }
  return push_int(a - b);
}

int sub2(Word *word, Word *caller) {
  int err = 1;
  dcell a = pop_int2(&err);
  if (err != 1) {
    return err;
  }
  dcell b = pop_int2(&err);
  if (err != 1) {
    return err;
  }
  return push_int2(a - b);
}

int mul(Word *word, Word *caller) {
  int err = 1;
  int b = pop_int(&err);
  if (err != 1) {
    return err;
  }
  int a = pop_int(&err);
  if (err != 1) {
    return err;
  }
  return push_int(a * b);
}

int mul2(Word *word, Word *caller) {
  int err = 1;
  int b = pop_int(&err);
  if (err != 1) {
    return err;
  }
  int a = pop_int(&err);
  if (err != 1) {
    return err;
  }
  return push_int(a * b);
}

int divide(Word *word, Word *caller) {
  int err = 1;
  dcell b = pop_int2(&err);
  if (err != 1) {
    return err;
  }
  dcell a = pop_int2(&err);
  if (err != 1) {
    return err;
  }
  return push_int2(a / b);
}

int mod(Word *word, Word *caller) {
  int err = 1;
  int b = pop_int(&err);
  if (err != 1) {
    return err;
  }
  int a = pop_int(&err);
  if (err != 1) {
    return err;
  }
  if (b == 0) {
    return -2;
  }
  return push_int(a % b);
  return 1;
}

int mod2(Word *word, Word *caller) {
  int err = 1;
  dcell b = pop_int2(&err);
  if (err != 1) {
    return err;
  }
  dcell a = pop_int2(&err);
  if (err != 1) {
    return err;
  }
  if (b == 0) {
    return -2;
  }
  return push_int2(a % b);
  return 1;
}

int gth(Word *word, Word *caller) {
  int err = 1;
  int b = pop_int(&err);
  if (err != 1) {
    return err;
  }
  int a = pop_int(&err);
  if (err != 1) {
    return err;
  }
  return push_int(a > b);
}

int gth2(Word *word, Word *caller) {
  int err = 1;
  int b = pop_int2(&err);
  if (err != 1) {
    return err;
  }
  int a = pop_int2(&err);
  if (err != 1) {
    return err;
  }
  return push_int2(a > b);
}

int lth(Word *word, Word *caller) {
  int err = 1;
  int b = pop_int(&err);
  if (err != 1) {
    return err;
  }
  int a = pop_int(&err);
  if (err != 1) {
    return err;
  }
  return push_int(a < b);
}

int lth2(Word *word, Word *caller) {
  int err = 1;
  int b = pop_int2(&err);
  if (err != 1) {
    return err;
  }
  int a = pop_int2(&err);
  if (err != 1) {
    return err;
  }
  return push_int(a < b);
}

int dup(Word *word, Word *caller) {
  int err = 1;
  int a = pop_int(&err);
  if (err != 1) {
    return err;
  }
  err = push_int(a);
  if (err != 1) {
    return err;
  }
  err = push_int(a);
  return err;
}

int swp(Word *word, Word *caller) {
  int err = 1;
  int b = pop_int(&err);
  if (err != 1) {
    return err;
  }
  int a = pop_int(&err);
  if (err != 1) {
    return err;
  }
  err = push_int(b);
  if (err != 1) {
    return err;
  }
  err = push_int(a);
  if (err != 1) {
    return err;
  }
  return 1;
}

int rot(Word *word, Word *caller) {
  int err = 1;
  int x3 = pop_int(&err);
  if (err != 1) {
    return err;
  }
  int x2 = pop_int(&err);
  if (err != 1) {
    return err;
  }
  int x1 = pop_int(&err);
  if (err != 1) {
    return err;
  }
  err = push_int(x2);
  if (err != 1) {
    return err;
  }
  err = push_int(x3);
  if (err != 1) {
    return err;
  }
  err = push_int(x1);
  if (err != 1) {
    return err;
  }
  return 1;
}

int ovr(Word *word, Word *caller) {
  int err = 1;
  int b = pop_int(&err);
  if (err != 1) {
    return err;
  }
  int a = pop_int(&err);
  if (err != 1) {
    return err;
  }
  err = push_int(a);
  if (err != 1) {
    return err;
  }
  err = push_int(b);
  if (err != 1) {
    return err;
  }
  err = push_int(a);
  if (err != 1) {
    return err;
  }
  return 1;
}

int or (Word * word, Word *caller) {
  cell err = 1;
  cell b = pop_int(&err);
  if (err != 1) {
    return err;
  }
  cell a = pop_int(&err);
  if (err != 1) {
    return err;
  }
  err = push_int(a != 0 || b != 0);
  if (err != 1) {
    return err;
  }
  return 1;
}
int and (Word * word, Word *caller) {
  cell err = 1;
  cell b = pop_int(&err);
  if (err != 1) {
    return err;
  }
  cell a = pop_int(&err);
  if (err != 1) {
    return err;
  }
  err = push_int(a != 0 && b != 0);
  if (err != 1) {
    return err;
  }
  return 1;
}

int jmp(Word *word, Word *caller) {
  int err = 1;
  int pos = pop_int(&err);
  if (err != 1) {
    return err;
  }
  if (pos < 0) {
    return -5;
  }
  popr_int(&err);

  if (err != 1) {
    return err;
  }
  printf("jumping to %i\n", pos);
  pushr_int(pos - 1);

  return 1;
}

int jmpz(Word *word, Word *caller) {
  int err = 1;
  int condition = pop_int(&err);
  if (err != 1) {
    return err;
  }
  int pos = pop_int(&err);
  if (err != 1) {
    return err;
  }

  if (pos < 0) {
    return -5;
  }
  popr_int(&err);
  if (condition == 0) {
    if (err != 1) {
      return err;
    }
    printf("jumping to %i\n", pos);
    pushr_int(pos - 1);
  }

  return 1;
}

cell write(Word *self, Word *caller) {
  cell err = 1;
  cell data = pop_int(&err);
  if (err != 1) {
    return err;
  }
  cell addr = pop_int(&err);
  if (err != 1) {
    return err;
  }
  if (addr < MEMSIZE) {
    membank[addr] = data;
  } else {
    return -6;
  }
  return 1;
}

cell read(Word *self, Word *caller) {
  cell err = 1;
  cell addr = pop_int(&err);
  if (err != 1) {
    return err;
  }
  if (addr >= MEMSIZE) {
    return -6;
  }
  err = push_int(membank[addr]);
  if (err != 1) {
    return err;
  };
  return 1;
}

cell negate(Word *self, Word *caller) {
  cell err = 1;
  cell cond = pop_int(&err);
  if (err != 1) {
    return err;
  }
  if (cond == 0) {
    return push_int(1);
  } else {
    return push_int(0);
  }
  return 1;
}
cell bye(Word *self, Word *caller) { exit(0); }

cell search(Word*, Word*);
int allocate_literal(cell value);

int does(Word* self, Word* caller) {

}

int literal(Word *word, Word *caller) {
  cell err = 1;
  cell ip = popr_int(&err);
  err = pushr_int(ip + 1);
  if (err != 1) {
    return err;
  }
  cell n = caller->def[ip + 1];
  return push_int(n);
}

int char_to_int(char c) {
  if (isdigit(c))
    return c - '0';
  else if (isalpha(c)) {
    return toupper(c) - 'A' + 10;
  } else
    return -2; // Invalid character
}

int parse_num(char *str, int base, int *num) {
  int result = 0;
  int index = 0;
  int sign = 1;

  // Check for sign
  if (str[index] == '-') {
    sign = -1;
    index++;
  }

  if (str[index] == '\0') {
    return -7;
  }

  while (isspace(str[index]) == 0 && str[index] != '\0') {
    int digit = char_to_int(str[index]);
    if (digit >= base || digit < 0) {
      return -2;
    }
    result = result * base + digit;
    index++;
  }
  result *= sign;

  *num = result;

  return 1;
}

int search(Word* self, Word* caller) {
  for (int i = top_word; i >= 0; i--) {
    if (strcmp(dict[i].name, next_word) == 0) {
      push_int(i);
      return 1;
    }
  }
  push_int(-1);
  return -1;
}

int find_token_int() {
  char *next = advance();
  if (next[0] != '\0') {
    search(NULL, NULL);
    int err;
    int wordidx = pop_int(&err);
    if (wordidx >= 0) {
      push_int(wordidx);
      return 1;
    } else {
      return -1;
    }
  } else {
    return 2;
  }
  return -1;
}

int tick(Word *word, Word *caller) { return find_token_int(); }

bool state = 0;
cell wordindef = 0;

int allocate_literal(cell value) {
  dict[top_word].def[dict[top_word].def_len++] = 0;
  dict[top_word].def[dict[top_word].def_len++] = value;
  return 1;
}

int semicolon(Word *self, Word *caller) {
  for (int i; i<dict[top_word].def_len; i++) {
    printf("%s ", dict[dict[top_word].def[i]].name);
  }
  putchar('\n');
  state = 0;
  for (int i = 0; i < dict[top_word].def_len; i++) {
    printf("%i ", dict[top_word].def[i]);
  }
  putchar('\n');
  return 1;
}

int fadvance(Word *self, Word *caller) {
  advance();
  return 1;
}

int enter(Word *word, Word *caller) {
  ip_d = 0;
  int err = 1;
  while (ip_d < word->def_len) {
    printf("succesfully entered");
    Word *entering_word = &dict[word->def[ip_d]];
    printf("pushed %i to return\n", ip_d);
    err = pushr_int(ip_d);
    if (err != 1) {
      printf("error pushing onto return stack\n");
      return err;
    }
    entering_word->enter(entering_word, word);
    ip_d = popr_int(&err);
    printf("popped %i from return\n", ip_d);
    if (err != 1) {
      return err;
    }
    ip_d++;
  }
  return 1;
}

int colon(Word *word, Word *caller) {
  wordindef = 0;
  if (*advance() == '\0') {
    printf("error: no definiton name\n");
    return -1;
  }
  printf("defined: %s\n", next_word);
  memcpy(dict[++top_word].name, next_word, sizeof(next_word));
  dict[top_word].enter = enter;
  state = 1;
  return 1;
}

void add_primitive(char *name, func function) {
  top_word++;
  memcpy(dict[top_word].name, name,
         sizeof(char) * strlen(name) + sizeof(char) * 1);
  dict[top_word].enter = function;
  dict[top_word].def_len = 0;
  dict[top_word].immediate = false;
}

void add_primitive_immediate(char *name, func function) {
  top_word++;
  memcpy(dict[top_word].name, name,
         sizeof(char) * strlen(name) + sizeof(char) * 1);
  dict[top_word].enter = function;
  dict[top_word].def_len = 0;
  dict[top_word].immediate = true;
}

void add_non_primitive(char name[], cell *def, cell def_len) {
  top_word++;
  memcpy(dict[top_word].name, name, strlen(name) + 1);
  dict[top_word].enter = enter;
  dict[top_word].def_len = def_len;
  memcpy(&dict[top_word].def, def, def_len * sizeof(cell));
  dict[top_word].immediate = false;
}



int compile(Word* self, Word* caller) {
  int err = 1;
  search(NULL, self);
  int found = pop_int(&err);
  if (err != 1) return err;
  if (found >= 0) {
    if (dict[found].immediate) {
      cell err = dict[found].enter(&dict[found], NULL);
      if (err != 1)
        return err;
    } else {
      dict[top_word].def[dict[top_word].def_len++] = found;
    }
  } else {
    cell to_push = 0;
    if (parse_num(next_word, 10, &to_push) == 1) {
      allocate_literal(to_push);
    }
  }
  return 1;
}

int main() {
  dict = (Word *)malloc(WORD_N * sizeof(Word));
  membank = (cell *)malloc(MEMSIZE * sizeof(cell));
  inputbuff = (char *)malloc(BUFSIZE * sizeof(char));

  int idx = 0;

  add_primitive("literal", literal); // must be first!!!!
  add_primitive("+", add);
  add_primitive("*", mul);
  add_primitive("/", divide);
  add_primitive("-", sub);
  add_primitive("%", sub);
  add_primitive(".", dot);
  add_primitive("dup", dup);
  add_primitive("pop", pop);
  add_primitive("swp", swp);
  add_primitive("ovr", ovr);
  add_primitive("rot", rot);
  add_primitive(":", colon);
  add_primitive("jmp", jmp);
  add_primitive("jmpz", jmpz);
  add_primitive("alloc", balloc);
  add_primitive("free", bfree);
  add_primitive("not", negate);
  add_primitive("or", or);
  add_primitive("and", and);
  add_primitive("bye", bye);
  add_primitive_immediate("compile", compile);
  add_primitive_immediate(";", semicolon);
  add_primitive_immediate("advance", fadvance);

  // cell dupt[2] = {search("dup"), search("+")};
  // add_non_primitive("twice", dupt, 2, 0);
  // cell doubletwice[3] = {search("twice"), search("twice"), search(".")};
  // add_non_primitive("dt", doubletwice, 3, 0);

  // char input[] = " : twice dup + ; 3 2 twice . ";
  // strcpy(inputbuff, input);
  // memcpy(inputbuff, "hello  world", sizeof("hello  world"));
  while (1) {
    inputidx = 0;
    fgets(inputbuff, 1024, stdin);
    while (strcmp(advance(), "") != 0) {
      if (state == 0) { // interpret mode
      int err = search(NULL, NULL);
      int found = pop_int(&err);
        if (found >= 0) {
          cell err = dict[found].enter(&dict[found], NULL);
          if (err != 1)
            printf("ERROR: %i\n", err);
        } else {
          cell to_push = 0;
          if (parse_num(next_word, 10, &to_push) == 1) {
            push_int(to_push);
          }
        }
      } else { // compile mode
        compile(NULL, NULL);
      }
    }
  }
  free(dict);
  free(membank);
  free(inputbuff);
  return 0;
}
