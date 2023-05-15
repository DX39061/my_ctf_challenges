## 普通Rust程序逆向

这里的普通指的是程序重点在于实现了某种加密算法，这个算法拿其他语言也能写，但是用rust写更难看。

这种找到主函数一般问题不大，之后就主打硬看，经验上可以注意以下几点：

- rust程序真正的main函数并不是IDA识别出来的main函数，而是`包名::main::xxxxx`这样一个函数，一般直接函数列表搜main就能看到。

- 函数列表东西很多，但大多数都是库函数，如`std::xxx::yyy`、`core::xxx::yyy`等等，不知道的时候百度一下名字多半就有

- 函数中、函数名可能有大量的`drop_in_place`，这是rust编译器生成的，在变量离开作用域时销毁变量的操作，不要理会

- 当你感觉某个函数少了什么参数的时候，看看汇编，很有可能IDA反编译没出来（同样适用go逆向）

- rust并不会像C一样给每个字符串后面添加`\0`，经常某个参数牵扯一大串，手动截断以下看着更舒服（同样适用go逆向）

- 对于整个程序的大部分内容，你只需要大概理解干了什么事，不要去硬抠每一个变量。对于关键函数再去细看

- rust程序一般不会设置反调，当硬看很难看的时候试试调试

## 奇怪的Rust阅读题

说的就是N1CTF2021的babyrust，可惜找了半天找不到题目了，百度能搜到一堆wp可以瞅瞅。

源码逆向，考点就是rust的`macro_rules`，即宏定义，类似于C的define，但却比define强大很多。这种就没什么办法，只能是现学，这也不是这篇文章的重点，写在这就是提一句。

## Rust多线程程序逆向

### Rust多线程综述

rust作为一个现代编程语言，必然要支持并发编程，这部分想详细了解可以看[这篇文章](https://course.rs/advance/concurrency-with-threads/concurrency-parallelism.html)。并发编程简单说就是多线程编程。rust开发常用的有两种实现多线程的方法。

一种是标准库支持的，直接调用操作系统API创建线程，程序内的线程数和该程序在操作系统中占用的线程数相同。另一种是第三方库支持的，由第三方库在操作系统之前管理调度线程，程序内部的M个线程会以某种方式映射到操作系统中的N个线程。

作为一个逆向壬，知道这些已经足够了，下面结合一些代码来具体说说。

### 标准库实现的多线程

直接上代码：

```rust
use std::thread;    // 导入标准库中的thread包

fn run_in_new_thread() {    // 新线程执行函数
    println!("Running in a new thread!"); 
}

fn main() {
    let handle = thread::spawn(run_in_new_thread);  // 创建新线程

    handle.join().unwrap();    // 阻塞等待线程退出
    println!("Thread completed!");
}
```

但这么简单的东西在IDA里看就会很抽象：

```rust
__int64 __fastcall test::main::h23ffb018909e13c3(__int64 a1, int a2, int a3, int a4, int a5, int a6)
{
  __int64 v6; // rax
  __int64 v7; // rdx
  int v9; // [rsp+0h] [rbp-68h]
  int v10[2]; // [rsp+8h] [rbp-60h] BYREF
  __int64 v11; // [rsp+10h] [rbp-58h]
  __int64 v12; // [rsp+18h] [rbp-50h]
  __int64 v13; // [rsp+20h] [rbp-48h] BYREF
  __int64 v14; // [rsp+28h] [rbp-40h]
  __int64 v15; // [rsp+30h] [rbp-38h]
  int v16[6]; // [rsp+38h] [rbp-30h] BYREF
  struct _Unwind_Exception *v17; // [rsp+50h] [rbp-18h]
  int v18; // [rsp+58h] [rbp-10h]

  std::thread::spawn::h98ea0099a2b0719e(
    (int)v10,
    a2,
    a3,
    a4,
    a5,
    a6,
    v9,
    v10[0],
    v11,
    v12,
    v13,
    v14,
    v15,
    v16[0],
    v16[2],
    v16[4],
    v17,
    v18);
  v13 = *(_QWORD *)v10;
  v14 = v11;
  v15 = v12;
  v6 = std::thread::JoinHandle$LT$T$GT$::join::h1ddc62add6cab2d7(&v13);
  core::result::Result$LT$T$C$E$GT$::unwrap::h1f8f2b208edb20af(v6, v7, &off_56D08);
  core::fmt::Arguments::new_v1::hcd11d69245dac270(
    v16,
    &off_56D20,
    1LL,
    "./test.rsThread completed!\n"
    "called `Option::unwrap()` on a `None` value/rustc/e972bc8083d5228536dfd42913c8778b6bb04c8e/library/std/src/thread/mo"
    "d.rsfailed to spawn threadthread name may not contain interior null bytesfatal runtime error: \n"
    "thread result panicked on drop",
    0LL);
  return std::io::stdio::_print::h98d08d18ce2c4627(v16);
}
```

一般来说，重点要看的是新线程内部的内容，但这里你会发现完全找不到我们写的`run_in_new_thread`

怎么办呢？先说原理，看不懂也没关系

`thread::spawn`函数定义长这样：

```rust
pub fn spawn<F, T>(f: F) -> JoinHandle<T> // 参数为f，类型为F（这里是泛型），返回值类型是JoinHandle<T>
where    // F和T必须满足的约束
    F: FnOnce() -> T + Send + 'static,
    T: Send + 'static,
```

我们上面的例子中向spawn函数中传递的参数是一个函数，还有一种用法是传递一个闭包，像[官方文档](https://doc.rust-lang.org/std/thread/fn.spawn.html)里写的这样：

```rust
use std::thread;

let handler = thread::spawn(|| {
    // thread code
});

handler.join().unwrap();
```

但无论是函数还是闭包，都必须满足实现FnOnce这个trait(rust术语，类似于interface)。而IDA识别函数时，会给实现了FnOnce的函数/闭包外层套一个函数名中带有FnOnce的函数。

**所以我们只需要在函数列表搜索`FnOnce`即可**

例如这个例子中搜索结果长这样：

```
Function name    Segment    Start    Length    Locals    Arguments    R    F    L    M    S    B    T    =
_$LT$core..panic..unwind_safe..AssertUnwindSafe$LT$F$GT$$u20$as$u20$core..ops..function..FnOnce$LT$$LP$$RP$$GT$$GT$::call_once::h86f62913b3a01feb    .text    000000000000B300    00000008    00000000        R    .    .    .    .    .    T    .
_$LT$core..panic..unwind_safe..AssertUnwindSafe$LT$F$GT$$u20$as$u20$core..ops..function..FnOnce$LT$$LP$$RP$$GT$$GT$::call_once::hc892338ce1be86ae    .text    000000000000B310    00000008    00000000        R    .    .    .    .    .    .    .
core::ops::function::FnOnce::call_once$u7b$$u7b$vtable.shim$u7d$$u7d$::h16c01ee6da75490f    .text    000000000000E490    00000008    00000000        R    .    .    .    .    .    T    .
core::ops::function::FnOnce::call_once$u7b$$u7b$vtable.shim$u7d$$u7d$::hea400b215dae484e    .text    000000000000E4A0    0000000B    00000000        R    .    .    .    .    .    T    .
core::ops::function::FnOnce::call_once::h0ae5d1532e17cb93    .text    000000000000E4B0    00000009    00000000        R    .    .    .    .    .    T    .
core::ops::function::FnOnce::call_once::h3d2fbb6252806bee    .text    000000000000E4C0    00000008    00000000        R    .    .    .    .    .    T    .
core::ops::function::FnOnce::call_once::h457f1b37350864df    .text    000000000000E4D0    0000003E    00000028    0000001C    R    .    .    .    .    .    T    .
core::ops::function::FnOnce::call_once::heb6523a76773118b    .text    000000000000E510    00000036    00000028    0000001C    R    .    .    .    .    .    T    .
core::ops::function::FnOnce::call_once::hef6ad134fc9103ae    .text    000000000000E550    00000005    00000000        R    .    .    .    .    .    .    .
core::ops::function::FnOnce::call_once$u7b$$u7b$vtable.shim$u7d$$u7d$::h1875b7d2a48541fb    .text    000000000000F230    00000005    00000000        R    .    .    .    .    .    .    .
core::ops::function::FnOnce::call_once$u7b$$u7b$vtable.shim$u7d$$u7d$::h277e46b18f00224c    .text    000000000000F240    0000000C            R    .    .    .    .    .    T    .
core::ops::function::FnOnce::call_once$u7b$$u7b$vtable.shim$u7d$$u7d$::h4f660ca895af9563    .text    000000000000F250    00000005    00000000        R    .    .    .    .    .    .    .
core::ops::function::FnOnce::call_once$u7b$$u7b$vtable.shim$u7d$$u7d$::h50909e6076d28381    .text    000000000000F260    0000000C            R    .    .    .    .    .    T    .
core::ops::function::FnOnce::call_once$u7b$$u7b$vtable.shim$u7d$$u7d$::he0372060e69105d1    .text    000000000000F270    00000061    00000028        R    .    .    .    .    .    T    .
core::ops::function::FnOnce::call_once::hafb5080c7603e593    .text    000000000000F2F0    00000006            .    .    .    .    .    .    T    .
core::ptr::drop_in_place$LT$alloc..boxed..Box$LT$dyn$u20$core..ops..function..FnOnce$LT$$LP$$RP$$GT$$u2b$Output$u20$$u3d$$u20$$LP$$RP$$GT$$GT$::h22561082141dbf37    .text    000000000000F620    00000039    00000010        R    .    .    .    .    .    .    .
core::ops::function::FnOnce::call_once::h448eabacb91f4aa5    .text    0000000000040290    00000012            .    .    .    .    .    .    .    .
```

东西并不多，稍微翻翻就能找到我们写的`run_in_new_thread`函数。

另一个例子是今年NU1L-Junior的`checkin-rs`，附件放这了，需要自取

大家都能找到的应该是`re_checkin_rs::main`这个函数：

```c
volatile signed __int64 *re_checkin_rs::main::h7930adfed10cc1f8()
{
  _OWORD *v0; // rax
  _OWORD *v1; // r15
  char *input_1; // r12
  __int64 input_len; // rbx
  __int64 input_2; // r14
  __int64 input_len_1; // r13
  __int64 v6; // rax
  __int128 v7; // xmm0
  __int128 v8; // xmm1
  __int128 v9; // rdi
  __int128 v10; // rax
  char v11; // al
  volatile signed __int64 *result; // rax
  char v13; // [rsp+Ch] [rbp-BCh]
  __int128 input_stream; // [rsp+10h] [rbp-B8h] BYREF
  __int128 v15; // [rsp+20h] [rbp-A8h]
  __int128 v16; // [rsp+30h] [rbp-98h]
  __int64 v17; // [rsp+40h] [rbp-88h]
  __int64 v18; // [rsp+48h] [rbp-80h]
  volatile signed __int32 *v19[2]; // [rsp+58h] [rbp-70h] BYREF
  char *input; // [rsp+68h] [rbp-60h] BYREF
  __int128 v21; // [rsp+70h] [rbp-58h]
  __int64 v22[9]; // [rsp+80h] [rbp-48h] BYREF

  v0 = (_OWORD *)_rust_alloc(46LL, 1LL);
  if ( !v0 )
    alloc::alloc::handle_alloc_error::h87e3407648c6f1ae(46LL);
  v1 = v0;
  *v0 = xmmword_55CFF2D43AB0;
  v0[1] = xmmword_55CFF2D43AC0;
  qmemcpy(v0 + 2, "R~[PE@]LF\\ZHI^", 14);
  *(_QWORD *)&input_stream = &off_55CFF2D55D90; // input your flag\n
  *((_QWORD *)&input_stream + 1) = 1LL;
  *(_QWORD *)&v15 = 0LL;
  *(_QWORD *)&v16 = "called `Result::unwrap()` on an `Err` valuesrc/main.rsWrong!\n";
  *((_QWORD *)&v16 + 1) = 0LL;
  std::io::stdio::_print::h5cbb3fae9e78870c(&input_stream);
  input = (char *)1;
  v21 = 0LL;
  v19[0] = (volatile signed __int32 *)std::io::stdio::stdin::h7ddc52b357ca7934();
  std::io::stdio::Stdin::read_line::hc035b1783e397e56((__int64)&input_stream, v19, (__int64)&input);
  if ( (_QWORD)input_stream )
  {
    v22[0] = *((_QWORD *)&input_stream + 1);
    core::result::unwrap_failed::hb53671404b9e33c2(
      "failed to input.Congratulation!\ncalled `Result::unwrap()` on an `Err` valuesrc/main.rsWrong!\n",
      16LL,
      v22,
      &off_55CFF2D55D30,
      &off_55CFF2D55DA0);
  }
  input_1 = input;
  input_len = *((_QWORD *)&v21 + 1);
  if ( *((_QWORD *)&v21 + 1) && input[*((_QWORD *)&v21 + 1) - 1] == '\n' )
  {
    input_len = *((_QWORD *)&v21 + 1) - 1LL;
    if ( *((_QWORD *)&v21 + 1) == 1LL )
    {
      input_2 = 1LL;
    }
    else
    {
      if ( input_len < 0 )
        alloc::raw_vec::capacity_overflow::h52630126fb18cfa2();
      input_2 = _rust_alloc(input_len, input_len >= 0);
      if ( !input_2 )
        alloc::alloc::handle_alloc_error::h87e3407648c6f1ae(input_len);
    }
    memcpy((void *)input_2, input_1, input_len);
    v13 = 1;
    input_1 = (char *)input_2;
    input_len_1 = input_len;
  }
  else
  {
    input_len_1 = v21;
    v13 = 0;
  }
  v15 = 0LL;
  LOBYTE(v17) = 2;
  input_stream = xmmword_55CFF2D436E0;
  v6 = _rust_alloc(56LL, 8LL);
  if ( !v6 )
    alloc::alloc::handle_alloc_error::h87e3407648c6f1ae(56LL);
  *(_QWORD *)(v6 + 48) = v17;
  v7 = input_stream;
  v8 = v15;
  *(_OWORD *)(v6 + 32) = v16;
  *(_OWORD *)(v6 + 16) = v8;
  *(_OWORD *)v6 = v7;
  if ( _InterlockedIncrement64((volatile signed __int64 *)v6) <= 0 )
    BUG();
  v19[0] = 0LL;
  v19[1] = (volatile signed __int32 *)v6;
  *(_QWORD *)&input_stream = input_1;
  *((_QWORD *)&input_stream + 1) = input_len_1;
  *(_QWORD *)&v15 = input_len;
  *((_QWORD *)&v15 + 1) = v1;
  v16 = xmmword_55CFF2D43AD0;
  v17 = 0LL;
  v18 = v6;
  *(_QWORD *)&v9 = v22;
  *((_QWORD *)&v9 + 1) = &input_stream;
  std::thread::spawn::h07216e9b542ea8b6(v9);
  *(_QWORD *)&v10 = std::thread::JoinHandle$LT$T$GT$::join::h18c7fa589a3e95f8((__int64)v22);
  if ( (_QWORD)v10 )
  {
    input_stream = v10;
    core::result::unwrap_failed::hb53671404b9e33c2(
      "called `Result::unwrap()` on an `Err` valuesrc/main.rsWrong!\n",
      43LL,
      &input_stream,
      &off_55CFF2D55D50,
      &off_55CFF2D55DB8);
  }
  v11 = std::sync::mpsc::Receiver$LT$T$GT$::recv::h45da6b3200285188((__int64)v19);
  if ( v11 == 2 )
    core::result::unwrap_failed::hb53671404b9e33c2(
      "called `Result::unwrap()` on an `Err` valuesrc/main.rsWrong!\n",
      43LL,
      &input_stream,
      &off_55CFF2D55D70,
      &off_55CFF2D55DD0);
  if ( v11 )
    *(_QWORD *)&input_stream = &off_55CFF2D55DE8;// Congratulations
  else
    *(_QWORD *)&input_stream = &off_55CFF2D55DF8;// wrong
  *((_QWORD *)&input_stream + 1) = 1LL;
  *(_QWORD *)&v15 = 0LL;
  v16 = (unsigned __int64)"called `Result::unwrap()` on an `Err` valuesrc/main.rsWrong!\n";
  std::io::stdio::_print::h5cbb3fae9e78870c(&input_stream);
  result = core::ptr::drop_in_place$LT$std..sync..mpsc..Receiver$LT$bool$GT$$GT$::h6962096ca1e9139b((__int64 *)v19);
  if ( v13 )
  {
    if ( (_QWORD)v21 )
      return (volatile signed __int64 *)_rust_dealloc(input, v21, 1LL);
  }
  return result;
}
```

难点就在于出题人把核心逻辑藏在了`thread::spawn`开辟的新线程中，并在后面使用mpsc的send、recv进行线程间通信

我们继续使用前面介绍的技巧，在函数列表搜索`FnOnce`，我们能找到这样一个函数：

```c
volatile signed __int64 *__fastcall core::ops::function::FnOnce::call_once$u7b$$u7b$vtable.shim$u7d$$u7d$::h9cde18e06e45a304(
        __m128i *a1)
{
  __int64 v1; // r12
  __int64 v3; // rax
  __int64 v4; // rdx
  volatile signed __int64 *v5; // rax
  __int64 v6; // rsi
  __m128i v7; // xmm1
  __m128i v8; // xmm2
  __int64 v9; // rbp
  __int64 v10; // rdi
  __int64 v11; // rax
  __int64 v12; // rsi
  volatile signed __int64 **v13; // rbx
  volatile signed __int64 *result; // rax
  __m128i v15[6]; // [rsp+0h] [rbp-68h] BYREF

  v3 = std::thread::Thread::cname::h97872c30cf024672();
  if ( v3 )
    std::sys::unix::thread::Thread::set_name::h0b8bb0703e84d731(v3, v4);
  v5 = (volatile signed __int64 *)std::io::stdio::set_output_capture::h7b64176f573d3f01(a1->m128i_i64[1]);
  v15[0].m128i_i64[0] = (__int64)v5;
  if ( v5 && !_InterlockedDecrement64(v5) )
    alloc::sync::Arc$LT$T$GT$::drop_slow::h09c9a74f4be846f1(v15);
  std::sys::unix::thread::guard::current::h0e11811e8b98b5c7(v15);
  v6 = a1->m128i_i64[0];
  std::sys_common::thread_info::set::hd2260a9241afaa5b(v15);
  v15[0] = a1[1];
  v7 = a1[3];
  v8 = a1[4];
  v15[1] = a1[2];
  v15[2] = v7;
  v15[3] = v8;
  std::sys_common::backtrace::__rust_begin_short_backtrace::hff8c1f96768f50f6(v15);
  v9 = a1[5].m128i_i64[0];
  if ( *(_QWORD *)(v9 + 24) )
  {
    v10 = *(_QWORD *)(v9 + 32);
    if ( v10 )
    {
      (**(void (__fastcall ***)(__int64, __int64))(v9 + 40))(v10, v6);
      v11 = *(_QWORD *)(v9 + 40);
      v12 = *(_QWORD *)(v11 + 8);
      if ( v12 )
        _rust_dealloc(*(_QWORD *)(v9 + 32), v12, *(_QWORD *)(v11 + 16));
    }
  }
  v13 = (volatile signed __int64 **)&a1[5];
  *(_QWORD *)(v9 + 24) = 1LL;
  *(_QWORD *)(v9 + 32) = 0LL;
  *(_QWORD *)(v9 + 40) = v1;
  result = *v13;
  if ( !_InterlockedDecrement64(*v13) )
    return (volatile signed __int64 *)alloc::sync::Arc$LT$T$GT$::drop_slow::h91d3fcb60e526133(v13);
  return result;
}
```

显然是跟线程操作有关的内容，再随便点点，会发现真正的逻辑就在`std::sys_common::backtrace::__rust_begin_short_backtrace::hff8c1f96768f50f6`这个函数里

剩下就是硬看的部分了，出题人用最抽象的写法实现了一个最简单的异或加密，就不多说了。

### 第三方库实现的多线程

在实际的开发中，仅仅使用标准库提供的多线程实现往往是不够的，一般会选择使用第三方库提供的`M:N线程模型`，比如大名鼎鼎的tokio库。`hacker‘s gift`这道题就使用了tokio库。

打开`gift`这个附件，直接在函数列表搜索main，会看到一个叫`client::main`的函数，client就是这个程序的包名。再搜索client其实就能看到所有我自己写的函数了。让人困惑的可能是下面这两类长得差不多的函数：

```
client::encrypt::_$u7b$$u7b$closure$u7d$$u7d$::h629702448e6675af    .text    00000000000977E0    00000CD3    00000838        R    .    .    .    .    .    T    .
client::get_key::_$u7b$$u7b$closure$u7d$$u7d$::h75aae23b3028ae16    .text    0000000000098710    000010A3    00000000    00002DD0    R    .    .    .    .    .    .    .
client::get_key::_$u7b$$u7b$closure$u7d$$u7d$::_$u7b$$u7b$closure$u7d$$u7d$::h490f5fa52e13c750    .text    0000000000099B10    000002D1    00000538        R    .    .    .    .    .    T    .
client::upload::_$u7b$$u7b$closure$u7d$$u7d$::h64b8e010a7519210    .text    0000000000099E80    00000EFC    00000000    00002770    R    .    .    .    .    .    T    .
client::upload::_$u7b$$u7b$closure$u7d$$u7d$::_$u7b$$u7b$closure$u7d$$u7d$::ha62b3bacec3fcb81    .text    000000000009AFD0    000002D1    00000538        R    .    .    .    .    .    T    .
client::main::_$u7b$$u7b$closure$u7d$$u7d$::h219227d57fb13740    .text    000000000009B340    00000C2D    00000000    00001D40    R    .    .    .    .    .    T    .
client::util::http::empty::_$u7b$$u7b$closure$u7d$$u7d$::h6f4121e9527678a9    .text    00000000000A6C50    0000000A    00000010        .    .    .    .    .    .    .    .
client::util::http::full::h93fe388ae8991df7    .text    00000000000A6C60    0000004F    00000078        R    .    .    .    .    .    T    .
client::util::http::full::_$u7b$$u7b$closure$u7d$$u7d$::h61a4b155093e9be5    .text    00000000000A6CB0    0000000A    00000010        .    .    .    .    .    .    .    .
client::encrypt::h935c399f56e46c49    .text    00000000000A7CB0    0000005E    000006E8        R    .    .    .    .    .    T    .
client::get_key::h8072fa4860ae680a    .text    00000000000A7D10    0000002B    00000388        R    .    .    .    .    .    T    .
client::upload::h5bc15dfc365716e5    .text    00000000000A7D40    00000062    000001F8        R    .    .    .    .    .    T    .
client::main::h5561e6fa511fe72d    .text    00000000000A7DB0    000001E7    00000000    0000221C    R    .    .    .    .    .    T    .
```

下面这几个函数名比较短的函数点开你会发现完全看不懂在干什么，这其实是跟rust的异步编程有关，它们实际并不执行函数内容，而只是构造了一个`feature`（一种类似于闭包的东西）返回，这里不展开说了，详细看[这](https://huangjj27.github.io/async-book/index.html)

真正的函数内容在上面名字里带closure的函数里。server程序同理。

## hacker's gift运行流程

附件提供流量包目的也是帮选手梳理思路，应该还是挺清晰的。

server程序运行在hacker X的服务器上，client是DX收到的gift。

DX闲着没事干运行这个程序时，client会首先在本地生成一个RSA密钥对，然后把公钥放在http header里，向server发出第一个getkey请求。server收到请求后，会生成一个0-255的随机数，拼接上一个固定的字符串，作为key。用client发来的公钥加密key，放在response body中返回。

client拿到加密的key，用私钥解密拿到明文key。然后遍历同路径下一个叫`dangerous_directory`的文件夹，对于每一个文件，每次读入1024位，与key的第`i%key_len`位异或，直到文件内容读取结束。然后把文件名放在request header，加密数据放到request body里，向server发出upload请求。

## 解题思路 & 脚本

搞清楚流程之后解题就很简单了。只需从流量包里把flag文件数据搞出来，再拿到密钥进行异或解密即可。

至于密钥前几位的随机数，可以看到流量包里上传的还有一个jpg文件，根据jpg文件头恢复key前几位即可。

根据选手的反馈，每次读入1024并以1024位为单位进行加密这个点不容易看出来，这块就很需要耐心。

```c
$LT$std..process..ChildStdout$u20$as$u20$std..io..Read$GT$::read::h253e3cddca3ae7b5(
          &v81,
          a2 + 1720,
          a2,
          1024LL);
```

read函数最后一个参数指明了每次读入长度为1024

最终脚本：

```rust
use std::fs;
use std::io::Read;
use std::io::Write;
fn main() {
    let key = Vec::from("221e07f177-e9e8-494d-bd46-ab791cb4c694");
    let key_len = key.len();
    let mut file_read = fs::OpenOptions::new().read(true).open("flag.enc").unwrap();
    let mut file_write = fs::OpenOptions::new().write(true).append(true).open("flag").unwrap();
    let mut buf = [0; 1024];
    let mut buf_write = Vec::new();
    while let Ok(bytes_read) = file_read.read(&mut buf) {
        if bytes_read == 0 {
            break;
        }
        for i in 0..bytes_read {
            buf[i] ^= key[i % key_len];
            buf_write.push(buf[i]);
        }
        file_write.write(&buf_write).unwrap();
        buf_write.clear();
    }
}
```

解密出来的flag是个二维码，扫码即可得flag
