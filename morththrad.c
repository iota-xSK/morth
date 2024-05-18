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

#include <stdio.h>
#include <stdlib.h>
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

#include <stdint.h>
#define CHECK \
    if (err != 1) {\
        return err;\
    }

struct Word;
typedef int32_t cell;
typedef int64_t dcell;

struct Word;
union WordBody;
typedef struct {
  struct Word* data[STACKSIZE];
  int sp;
} RStack;

typedef struct {
  cell data[STACKSIZE];
  cell sp;
} Stack;

typedef struct Ctx {
    struct Word* ip;
    RStack rs;
    Stack ds;
} Ctx;

typedef union WordBody {
    struct Word *word;
    cell data;
} WordBody;

typedef cell (*func)(WordBody data, Ctx *context);

typedef struct Word {
    func head;
    WordBody body;
} Word;


cell pop(cell *err, Stack* ds) {
  if (ds->sp < 0) {
    *err = -1;
    return 0;
  }
  *err = 1;
  int data = ds->data[ds->sp--];
  return data;
}

dcell pop2(cell *err, Stack* ds) {
  if (ds->sp < 1) {
    *err = -1;
    return 0;
  }
  *err = 1;
  cell high = ds->data[ds->sp--];
  cell low = ds->data[ds->sp--];
  return ((cell)low | ((dcell)(high) << (sizeof(cell) * 8)));
}

cell push(int to_push, Stack *ds) {
  if (ds->sp >= STACKSIZE) {
    return -2;
  }
  ds->data[++ds->sp] = to_push;
  return 1;
}

cell push2(dcell to_push, Stack *ds) {
  if (ds->sp >= STACKSIZE) {
    return -2;
  }
  cell high = (cell)(to_push >> 32);       // Extracting the first 32 bits
  cell low = (cell)(to_push & 0xFFFFFFFF); // Extracting the last 32 bits
  ds->data[++ds->sp] = low;
  ds->data[++ds->sp] = high;
  return 1;
}


Word* popr(int *err, RStack* rs) {
  if (rs->sp < 0) {
    *err = -1;
    return 0;
  }
  *err = 1;
  Word * data = rs->data[rs->sp--];
  return data;
}

cell pushr(Word* to_push, RStack* rs) {
  if (rs->sp >= STACKSIZE) {
    return -2;
  }
  rs->data[++rs->sp] = to_push;
  return 1;
}

cell add(WordBody data, Ctx* ctx) {
    cell err = 1;
    cell b = pop(&err, &ctx->ds);
    CHECK;
    cell a = pop(&err, &ctx->ds);
    CHECK;
    err = push(a + b, &ctx->ds);
    CHECK;
    ctx->ip++;
    return 1;
}

cell dup(WordBody data, Ctx* ctx) {
    cell err = 1;
    cell a = pop(&err, &ctx->ds);
    CHECK;
    err = push(a, &ctx->ds);
    CHECK;
    err = push(a, &ctx->ds);
    CHECK;
    ctx->ip++;
    return 1;
}


cell dot(WordBody data, Ctx* ctx) {
    cell err = 1;
    cell a = pop(&err, &ctx->ds);
    CHECK;
    printf("%i\n", a);
    ctx->ip++;
    return 1;
}

cell ret(WordBody data, Ctx* ctx) {
    cell err = 1;
    Word* a = popr(&err, &ctx->rs);
    printf("popr err: %i", err);
    CHECK;
    ctx->ip = a;
    return 1;
}

cell lit(WordBody data, Ctx* ctx) {
    int err = push(data.data, &ctx->ds);
    CHECK;
    ctx->ip++;
    return 1;
}

cell subroutine(WordBody data, Ctx* ctx) {
    ctx->ip++;
    cell err = pushr(ctx->ip, &ctx->rs);
    CHECK;
    ctx->ip = data.word;
    return 1;
}


cell halt(WordBody data, Ctx* ctx) {
    exit(0);
}

int main() {
    Word three[] = {
    {.head=lit, .body = {.data = 1}},
    {.head=lit, .body = {.data = 2}},
    {.head=add},
    {.head=ret},
    };
    Word two[] = {
    {.head=lit, .body = {.data = 1}},
    {.head=dup},
    {.head=add},
    {.head=ret},
    };
    Word program[] = {
    {.head=subroutine, .body={.word=&three[0]}},
    {.head=subroutine, .body={.word=&two[0]}},
    {.head=add},
    {.head=dot},
    {.head=halt}
    };


    Ctx ctx = {.ds = {.sp = -1}, .rs = {.sp = -1}, .ip = &program[0]};

    for(;;) {
        if (ctx.ip->head(ctx.ip->body, &ctx)!=1) {
            printf("error!\n");
            break;
        }
    }
    return 1;
}
