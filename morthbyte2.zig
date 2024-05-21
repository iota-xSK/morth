const std = @import("std");

const StackErr = error{
    StackUnderflow,
    StackOverflow,
};

const Data = union {
    op: u32,
    data: i32,
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

const FCtx = struct { ip: u32, rs: Stack, ds: Stack, compile: bool, mem: [1024]i32};

fn lit(data: Data, ctx: *FCtx) StackErr!void {
    try ctx.ds.push(data);
    ctx.ip += 2;
}

fn dot(_: Data, ctx: *FCtx) StackErr!void {
    const popped = try ctx.ds.pop();
    std.debug.print("{d}\n", .{popped.data});
    ctx.ip += 2;
}


fn add(_: Data, ctx: *FCtx) StackErr!void {
    const b = try ctx.ds.pop();
    const a = try ctx.ds.pop();
    try ctx.ds.push(.{.data= a.data + b.data});
    ctx.ip += 2;
}


fn sub(_: Data, ctx: *FCtx) StackErr!void {
    const b = try ctx.ds.pop();
    const a = try ctx.ds.pop();
    try ctx.ds.push(.{.data= a.data - b.data});
    ctx.ip += 2;
}

fn mul(_: Data, ctx: *FCtx) StackErr!void {
    const b = try ctx.ds.pop();
    const a = try ctx.ds.pop();
    try ctx.ds.push(.{.data= a.data - b.data});
    ctx.ip += 2;
}

fn div(_: Data, ctx: *FCtx) StackErr!void {
    const b = try ctx.ds.pop();
    const a = try ctx.ds.pop();
    try ctx.ds.push(.{.data= a.data / b.data});
    ctx.ip += 2;
}

fn mod(_: Data, ctx: *FCtx) StackErr!void {
    const b = try ctx.ds.pop();
    const a = try ctx.ds.pop();
    try ctx.ds.push(.{.data= a.data % b.data});
    ctx.ip += 2;
}

fn eq(_: Data, ctx: *FCtx) StackErr!void {
    const b = try ctx.ds.pop();
    const a = try ctx.ds.pop();
    if (a.data == b.data) {
        try ctx.ds.push(.{.data= -1});
    } else {
        try ctx.ds.push(.{.data= 0});
    }
    ctx.ip += 2;
}

fn gth(_: Data, ctx: *FCtx) StackErr!void {
    const b = try ctx.ds.pop();
    const a = try ctx.ds.pop();
    if (a.data > b.data) {
        try ctx.ds.push(.{.data= -1});
    } else {
        try ctx.ds.push(.{.data= 0});
    }
    ctx.ip += 2;
}


fn lth(_: Data, ctx: *FCtx) StackErr!void {
    const b = try ctx.ds.pop();
    const a = try ctx.ds.pop();
    if (a.data < b.data) {
        try ctx.ds.push(.{.data= -1});
    } else {
        try ctx.ds.push(.{.data= 0});
    }
    ctx.ip += 2;
}

fn @"and"(_: Data, ctx: *FCtx) StackErr!void {
    const b = try ctx.ds.pop();
    const a = try ctx.ds.pop();
    if (a.data != 0 and b.data != 0) {
        try ctx.ds.push(.{.data= -1});
    } else {
        try ctx.ds.push(.{.data= 0});
    }
    ctx.ip += 2;
}


fn @"or"(_: Data, ctx: *FCtx) StackErr!void {
    const b = try ctx.ds.pop();
    const a = try ctx.ds.pop();
    if (a.data != 0 or b.data != 0) {
        try ctx.ds.push(.{.data= -1});
    } else {
        try ctx.ds.push(.{.data= 0});
    }
    ctx.ip += 2;
}


fn @"not"(_: Data, ctx: *FCtx) StackErr!void {
    const a = try ctx.ds.pop();
    if (a.data != 0) {
        try ctx.ds.push(.{.data= -1});
    } else {
        try ctx.ds.push(.{.data= 0});
    }
    ctx.ip += 2;
}

fn write(_: Data, ctx: *FCtx) StackErr!void {
    const data = try ctx.ds.pop();
    const addr = try ctx.ds.pop();

    ctx.mem[@intCast(addr.data)] = data.data;
    ctx.ip+=2;
}

fn read(_: Data, ctx: *FCtx) StackErr!void {
    const addr = try ctx.ds.pop();

    ctx.ds.push(ctx.mem[@intCast(addr.data)]);
    ctx.ip+=2;
}

fn quit(_: Data, _: *FCtx) StackErr!void {
    std.process.exit(0);
}

fn subroutine(data: Data, ctx: *FCtx) StackErr!void {
    ctx.ip+=2;
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
        ctx.ip+=2;
    }
}

fn ovr(_: Data, ctx: *FCtx) StackErr!void {
    const b = try ctx.ds.pop();
    const a = try ctx.ds.pop();
    try ctx.ds.push(a);
    try ctx.ds.push(b);
    try ctx.ds.push(a);
    ctx.ip+=2;
}

fn dup(_: Data, ctx: *FCtx) StackErr!void {
    const a = try ctx.ds.pop();
    try ctx.ds.push(a);
    try ctx.ds.push(a);
    ctx.ip+=2;
}

fn swp(_: Data, ctx: *FCtx) StackErr!void {
    const a = try ctx.ds.pop();
    const b = try ctx.ds.pop();
    try ctx.ds.push(a);
    try ctx.ds.push(b);
    ctx.ip+=2;
}

pub fn main() !void {
    const program: [36]u32 = .{
        0x00000003,0x00000022, // lit
        0x00000001,0x00000008, // subroutine 4
        0x0000000A,0x00000000, // dot
        0x0, 0x0,

        0x00000002,0x00000000, // dup
        0x00000003,0x00000002, // lit 2
        0x00000005,0x00000000, // lth
        0x00000006,0x00000022, // jnz 11

        0x00000002,0x00000000, // dup
        0x00000003,0x00000001, // lit 1
        0x00000009,0x00000000, // sub
        0x00000001,0x00000008, // subroutine 4

        0x00000007,0x00000000, // swp
        0x00000003,0x00000002, // lit 2
        0x00000009,0x00000000, // sub
        0x00000001,0x00000008, // subroutine 4

        0x00000008,0x00000000, // add
        0x0000000B,0x00000004, // ret
    };
    var ctx: FCtx = .{
        .compile = false,
        .mem = undefined,
        .ds = Stack.init(),
        .rs = Stack.init(),
        .ip = 0 };
    while (true) {
       try switch (program[@intCast(ctx.ip)]) {
            0x0 => quit(undefined, &ctx),
            0x1 => subroutine(Data{.op = @intCast(program[@intCast(ctx.ip + 1)])}, &ctx),
            0x2 => dup(undefined, &ctx),
            0x3 => lit(Data{.data = @intCast(program[@intCast(ctx.ip + 1)])}, &ctx),
            0x5 => lth(undefined, &ctx),
            0x6 => jnz(Data{.op = @intCast(program[@intCast(ctx.ip + 1)])}, &ctx),
            0x7 => swp(undefined, &ctx),
            0x8 => add(undefined, &ctx),
            0x9 => sub(undefined, &ctx),
            0xA => dot(undefined, &ctx),
            0xB => ret(undefined, &ctx),
           else => break
        };
    }
}
