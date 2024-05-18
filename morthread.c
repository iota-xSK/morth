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

#ifndef STACHSIZE
#define STACKSIZE 1024
#endif

#ifndef BUFSIZE
#define BUFSIZE 1024
#endif

#include <stdbool.h>
#include <stdint.h>

#define CHECK(_err)                                                            \
  if (_err != 1) {                                                             \
    return _err;                                                               \
  }

struct Op;
typedef int32_t cell_t;
typedef int64_t dcell_t;
char *inputbuff;
char next_word[32] = "";
cell_t inputidx;

char *advance() {
  static int pcount = 0;
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

  // FIXME: comments don't comment!
  if (strcmp(next_word, "(") == 0) {
    pcount++;
    advance();
  } else if (strcmp(next_word, ")") == 0) {
    pcount--;
    if (pcount != 0)
      advance();
  }

  return next_word;
}

struct Op;
typedef union Data {
  struct Op *op;
  cell_t data;
} Data;

typedef struct {
  Data data[STACKSIZE];
  cell_t sp;
} Stack;

typedef struct Word {
  struct Op *Def;
  char *name;
  bool immediate;
} Word;

typedef struct Dict {
  Word word;
  struct Dict *next;
} Dict;

typedef struct Ctx {
  struct Op *ip;
  Stack rs;
  Stack ds;
  bool compile;
  Dict dict;
} Ctx;

typedef cell_t (*func)(Data data, Ctx *context);

typedef struct Op {
  func head;
  Data body;
} Op;


cell_t new_word(Dict** dict, char* name, Op ops[], cell_t length) {
  Op* new_ops = (Op*)malloc(length*sizeof(Op));
  if (new_ops == NULL) {
    return -2;
  }
  memcpy(new_ops, ops, sizeof(Op)*length);
  char* new_name = (char*)malloc(strlen(name)+1);
  if (new_name== NULL) {
    return -2;
  }
  memcpy(new_name, name, strlen(name) + 1);
  Dict* new_dict = (Dict*)malloc(sizeof(Dict));
  if (new_dict== NULL) {
    return -2;
  }
  if (new_name== NULL) {
    return -2;
  }
  new_dict->next = *dict;
  *dict = new_dict;
  return 1;
}


Data pop(cell_t *err, Stack *ds) {
  if (ds->sp < 0) {
    *err = -1;
    Data d = {.data = 0};
    return d;
  }
  *err = 1;
  Data data = ds->data[ds->sp--];
  return data;
}

dcell_t pop2(cell_t *err, Stack *ds) {
  if (ds->sp < 1) {
    *err = -1;
    return 0;
  }
  *err = 1;
  cell_t high = ds->data[ds->sp--].data;
  cell_t low = ds->data[ds->sp--].data;
  return ((cell_t)low | ((dcell_t)(high) << (sizeof(cell_t) * 8)));
}

cell_t push(Data to_push, Stack *ds) {
  if (ds->sp >= STACKSIZE) {
    return -2;
  }
  ds->data[++ds->sp] = to_push;
  return 1;
}

cell_t push2(dcell_t to_push, Stack *ds) {
  if (ds->sp >= STACKSIZE) {
    return -2;
  }
  cell_t high = (cell_t)(to_push >> 32);       // Extracting the first 32 bits
  cell_t low = (cell_t)(to_push & 0xFFFFFFFF); // Extracting the last 32 bits
  ds->data[++ds->sp].data = low;
  ds->data[++ds->sp].data = high;
  return 1;
}

cell_t add(Data data, Ctx *ctx) {
  cell_t err = 1;
  cell_t b = pop(&err, &ctx->ds).data;
  CHECK(err);
  cell_t a = pop(&err, &ctx->ds).data;
  CHECK(err);
  Data d = {.data = a + b};
  err = push(d, &ctx->ds);
  CHECK(err);
  ctx->ip++;
  return 1;
}

cell_t sub(Data data, Ctx *ctx) {
  cell_t err = 1;
  cell_t b = pop(&err, &ctx->ds).data;
  CHECK(err);
  cell_t a = pop(&err, &ctx->ds).data;
  CHECK(err);
  Data d = {.data = a * b};
  err = push(d, &ctx->ds);
  CHECK(err);
  ctx->ip++;
  return 1;
}

cell_t divide(Data data, Ctx *ctx) {
  cell_t err = 1;
  cell_t b = pop(&err, &ctx->ds).data;
  CHECK(err);
  cell_t a = pop(&err, &ctx->ds).data;
  CHECK(err);
  Data d = {.data = a / b};
  err = push(d, &ctx->ds);
  CHECK(err);
  ctx->ip++;
  return 1;
}

cell_t mul(Data data, Ctx *ctx) {
  cell_t err = 1;
  cell_t b = pop(&err, &ctx->ds).data;
  CHECK(err);
  cell_t a = pop(&err, &ctx->ds).data;
  CHECK(err);
  Data d = {.data = a * b};
  err = push(d, &ctx->ds);
  CHECK(err);
  ctx->ip++;
  return 1;
}

cell_t mod(Data data, Ctx *ctx) {
  cell_t err = 1;
  cell_t b = pop(&err, &ctx->ds).data;
  CHECK(err);
  cell_t a = pop(&err, &ctx->ds).data;
  CHECK(err);
  Data d = {.data = a % b};
  err = push(d, &ctx->ds);
  CHECK(err);
  ctx->ip++;
  return 1;
}

cell_t gth(Data data, Ctx *ctx) {
  cell_t err = 1;
  cell_t b = pop(&err, &ctx->ds).data;
  CHECK(err);
  cell_t a = pop(&err, &ctx->ds).data;
  CHECK(err);
  if (a > b) {
    Data d = {.data = -1};
    err = push(d, &ctx->ds);
  } else {
    Data d = {.data = 0};
    err = push(d, &ctx->ds);
  }
  CHECK(err);
  ctx->ip++;
  return 1;
}

cell_t lth(Data data, Ctx *ctx) {
  cell_t err = 1;
  cell_t b = pop(&err, &ctx->ds).data;
  CHECK(err);
  cell_t a = pop(&err, &ctx->ds).data;
  CHECK(err);
  if (a < b) {
    Data d = {.data = -1};
    err = push(d, &ctx->ds);
  } else {
    Data d = {.data = 0};
    err = push(d, &ctx->ds);
  }
  CHECK(err);
  ctx->ip++;
  return 1;
}

cell_t dup(Data data, Ctx *ctx) {
  cell_t err = 1;
  Data a = pop(&err, &ctx->ds);
  CHECK(err);
  err = push(a, &ctx->ds);
  CHECK(err);
  err = push(a, &ctx->ds);
  CHECK(err);
  ctx->ip++;
  return 1;
}

cell_t dot(Data data, Ctx *ctx) {
  cell_t err = 1;
  cell_t a = pop(&err, &ctx->ds).data;
  CHECK(err);
  printf("%i\n", a);
  ctx->ip++;
  return 1;
}

cell_t ret(Data data, Ctx *ctx) {
  cell_t err = 1;
  Op *a = pop(&err, &ctx->rs).op;
  CHECK(err);
  ctx->ip = a;
  return 1;
}

cell_t lit(Data data, Ctx *ctx) {
  int err = push(data, &ctx->ds);
  CHECK(err);
  ctx->ip++;
  return 1;
}

cell_t subroutine(Data data, Ctx *ctx) {
  ctx->ip++;
  Data d = {.op = ctx->ip};
  cell_t err = push(d, &ctx->rs);
  CHECK(err);
  ctx->ip = data.op;
  return 1;
}

cell_t halt(Data data, Ctx *ctx) { exit(0); }

cell_t search(Data data, Ctx *ctx) {
  Dict *acc = &ctx->dict;
  advance();
  while (strcmp(acc->word.name, next_word) != 0) {
    if (acc == NULL) {
      Data d = {.data = 0};
      push(d, &ctx->ds);
      return -2;
    }
    acc = acc->next;
  }

  Data d = {.op = acc->word.Def};
  push(d, &ctx->ds);
  return 1;
}

cell_t colon(Data data, Ctx* ctx) {
  ctx->compile = true;
  advance();
  return 1;
}

cell_t jmp(Data data, Ctx *ctx) {
  int err = 1;
  Data d = pop(&err, &ctx->ds);
  CHECK(err);
  ctx->ip = d.op;
  return 1;
}

cell_t jmpif(Data data, Ctx *ctx) {
  int err = 1;
  Data condition = pop(&err, &ctx->ds);
  CHECK(err);
  Data address = pop(&err, &ctx->ds);
  CHECK(err);
  if (condition.data != 0) {
    ctx->ip = address.op;
  } else {
    ctx->ip++;
  }
  return 1;
}

int main() {
  Op three[] = {
      {.head = lit, .body = {.data = 1}},
      {.head = lit, .body = {.data = 2}},
      {.head = add},
      {.head = ret},
  };
  Op two[] = {
      {.head = lit, .body = {.data = 1}},
      {.head = dup},
      {.head = add},
      {.head = dot},
      {.head = ret},
  };

  Ctx ctx = {.ds = {.sp = -1},
             .rs = {.sp = -1},
             .ip = &two[0],
             .compile = false};

  for (;;) {
    if (ctx.ip->head(ctx.ip->body, &ctx) != 1) {
      printf("error!\n");
      break;
    }
  }
  return 1;
}
