//MIT License
//
//Copyright (c) 2024 Juraj Babić
//
//Permission is hereby granted, free of charge, to any person obtaining a copy
//of this software and associated documentation files (the "Software"), to deal
//in the Software without restriction, including without limitation the rights
//to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
//copies of the Software, and to permit persons to whom the Software is
//furnished to do so, subject to the following conditions:
//
//The above copyright notice and this permission notice shall be included in all
//copies or substantial portions of the Software.
//
//THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
//IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
//FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
//AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
//LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
//OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
//SOFTWARE.
const std = @import("std");

const Data = union {
    op: [*]Op,
    data: i32,
};

const Op = struct {
    head: *const fn (data: Data, ctx: *FCtx) StackErr!void,
    body: Data,
};

const StackErr = error{
    StackUnderflow,
    StackOverflow,
};

const Stack = struct {
    data: [1024]Data,
    sp: i32,
    const Self = @This();
    pub fn init() Self {
        return .{ .sp = -1, .data = undefined };
    }
    pub fn push(self: *Self, data: Data) StackErr!void {
        if (self.sp >= 1024) {
            return StackErr.StackOverflow;
        }
        self.sp += 1;
        self.data[@intCast(self.sp)] = data;
    }
    pub fn pop(self: *Self) StackErr!Data {
        if (self.sp < 0) {
            return StackErr.StackUnderflow;
        }
        self.sp -= 1;
        return self.data[@intCast(self.sp + 1)];
    }
};

const FCtx = struct { ip: [*]Op, rs: Stack, ds: Stack, compile: bool, mem: [1024]i32};

const Word = struct {
    def: std.ArrayList(*Op),
    immediate: bool,
    name: *u8,
};

fn lit(data: Data, ctx: *FCtx) StackErr!void {
    try ctx.ds.push(data);
    ctx.ip += 1;
}

fn dot(_: Data, ctx: *FCtx) StackErr!void {
    const popped = try ctx.ds.pop();
    std.debug.print("{d}\n", .{popped.data});
    ctx.ip += 1;
}


fn add(_: Data, ctx: *FCtx) StackErr!void {
    const b = try ctx.ds.pop();
    const a = try ctx.ds.pop();
    try ctx.ds.push(.{.data= a.data + b.data});
    ctx.ip += 1;
}


fn sub(_: Data, ctx: *FCtx) StackErr!void {
    const b = try ctx.ds.pop();
    const a = try ctx.ds.pop();
    try ctx.ds.push(.{.data= a.data - b.data});
    ctx.ip += 1;
}

fn mul(_: Data, ctx: *FCtx) StackErr!void {
    const b = try ctx.ds.pop();
    const a = try ctx.ds.pop();
    try ctx.ds.push(.{.data= a.data - b.data});
    ctx.ip += 1;
}

fn div(_: Data, ctx: *FCtx) StackErr!void {
    const b = try ctx.ds.pop();
    const a = try ctx.ds.pop();
    try ctx.ds.push(.{.data= a.data / b.data});
    ctx.ip += 1;
}

fn mod(_: Data, ctx: *FCtx) StackErr!void {
    const b = try ctx.ds.pop();
    const a = try ctx.ds.pop();
    try ctx.ds.push(.{.data= a.data % b.data});
    ctx.ip += 1;
}

fn eq(_: Data, ctx: *FCtx) StackErr!void {
    const b = try ctx.ds.pop();
    const a = try ctx.ds.pop();
    if (a.data == b.data) {
        try ctx.ds.push(.{.data= -1});
    } else {
        try ctx.ds.push(.{.data= 0});
    }
    ctx.ip += 1;
}

fn gth(_: Data, ctx: *FCtx) StackErr!void {
    const b = try ctx.ds.pop();
    const a = try ctx.ds.pop();
    if (a.data > b.data) {
        try ctx.ds.push(.{.data= -1});
    } else {
        try ctx.ds.push(.{.data= 0});
    }
    ctx.ip += 1;
}


fn lth(_: Data, ctx: *FCtx) StackErr!void {
    const b = try ctx.ds.pop();
    const a = try ctx.ds.pop();
    if (a.data < b.data) {
        try ctx.ds.push(.{.data= -1});
    } else {
        try ctx.ds.push(.{.data= 0});
    }
    ctx.ip += 1;
}

fn @"and"(_: Data, ctx: *FCtx) StackErr!void {
    const b = try ctx.ds.pop();
    const a = try ctx.ds.pop();
    if (a.data != 0 and b.data != 0) {
        try ctx.ds.push(.{.data= -1});
    } else {
        try ctx.ds.push(.{.data= 0});
    }
    ctx.ip += 1;
}


fn @"or"(_: Data, ctx: *FCtx) StackErr!void {
    const b = try ctx.ds.pop();
    const a = try ctx.ds.pop();
    if (a.data != 0 or b.data != 0) {
        try ctx.ds.push(.{.data= -1});
    } else {
        try ctx.ds.push(.{.data= 0});
    }
    ctx.ip += 1;
}


fn @"not"(_: Data, ctx: *FCtx) StackErr!void {
    const a = try ctx.ds.pop();
    if (a.data != 0) {
        try ctx.ds.push(.{.data= -1});
    } else {
        try ctx.ds.push(.{.data= 0});
    }
    ctx.ip += 1;
}

fn write(_: Data, ctx: *FCtx) StackErr!void {
    const data = try ctx.ds.pop();
    const addr = try ctx.ds.pop();

    ctx.mem[@intCast(addr.data)] = data.data;
    ctx.ip+=1;
}

fn read(_: Data, ctx: *FCtx) StackErr!void {
    const addr = try ctx.ds.pop();

    ctx.ds.push(ctx.mem[@intCast(addr.data)]);
    ctx.ip+=1;
}

fn quit(_: Data, _: *FCtx) StackErr!void {
    std.process.exit(0);
}

fn subroutine(data: Data, ctx: *FCtx) StackErr!void {
    ctx.ip+=1;
    try ctx.rs.push(.{.op = ctx.ip});
    ctx.ip = data.op;
}

fn ret(_: Data, ctx: *FCtx) StackErr!void {
    ctx.ip = (try ctx.rs.pop()).op;
}

fn jmp(data: Data, ctx: *FCtx) StackErr!void {
    ctx.ip=data.op;
}

fn jnz(data: Data, ctx: *FCtx) StackErr!void {
    const a = try ctx.ds.pop();
    if (a.data != 0) {
        ctx.ip=data.op;
    } else {
        ctx.ip+=1;
    }
}

fn ovr(_: Data, ctx: *FCtx) StackErr!void {
    const b = try ctx.ds.pop();
    const a = try ctx.ds.pop();
    try ctx.ds.push(a);
    try ctx.ds.push(b);
    try ctx.ds.push(a);
    ctx.ip+=1;
}


fn dup(_: Data, ctx: *FCtx) StackErr!void {
    const a = try ctx.ds.pop();
    try ctx.ds.push(a);
    try ctx.ds.push(a);
    ctx.ip+=1;
}

fn swp(_: Data, ctx: *FCtx) StackErr!void {
    const a = try ctx.ds.pop();
    const b = try ctx.ds.pop();
    try ctx.ds.push(a);
    try ctx.ds.push(b);
    ctx.ip+=1;
}


fn drop(_: Data, ctx: *FCtx) StackErr!void {
    _ = try ctx.ds.pop();
}

pub fn main() !void {
    var gpa = std.heap.GeneralPurposeAllocator(.{}){};
    defer _ = gpa.deinit();
    var fib: [14]Op = .{
        Op{.head = dup, .body = undefined}, // 3 3
        Op{.head = lit, .body = Data {.data = 2}}, // 3 3 2
        Op{.head = lth, .body = undefined}, // 3 0
        Op{.head= jnz, .body = undefined}, //  3

        Op{.head = dup, .body = undefined}, // 3 3
        Op{.head = lit, .body = Data {.data = 1}}, // 3 3 1
        Op{.head = sub, .body = undefined}, // 3 2
        Op{.head = subroutine, .body = undefined}, // 3 x

        Op{.head = swp, .body = undefined}, // x 3
        Op{.head = lit, .body = Data {.data = 2}}, // x 3 2
        Op{.head = sub, .body = undefined}, // x 1
        Op{.head = subroutine, .body = undefined}, // x y

        Op{.head = add, .body = undefined},

        Op{.head = ret, .body = undefined},
    };
    fib[3].body = Data {.op = @as([*]Op, &fib) + 13};
    fib[7].body = Data {.op = @as([*]Op, &fib)};
    fib[11].body = Data {.op = @as([*]Op, &fib)};
    var fibmain: [4]Op = .{
        Op{.head = lit, .body = Data {.data = 0x22}},
        Op{.head = subroutine, .body = Data {.op = @as([*]Op, &fib) }},
        Op{.head = dot, .body = undefined},
        Op{ .head = quit, .body = Data{ .data = 0 } } };
    var ctx: FCtx = .{
        .compile = false,
        .mem = undefined,
        .ds = Stack.init(),
        .rs = Stack.init(),
        .ip = @as([*]Op, &fibmain) };
    while (true) {
        try ctx.ip[0].head(ctx.ip[0].body, &ctx);
    }
}
