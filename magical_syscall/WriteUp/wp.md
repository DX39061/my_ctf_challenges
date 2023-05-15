## 写在前面

- 这篇wp主要从出题人的角度来写，选手角度可以看看[Gift1a](https://github.com/XDSEC/miniLCTF_2023/blob/main/WriteUps/W4ntY0u/W4ntY0u.pdf) 的wp，写得非常详细。

- 题目源码以放在[github仓库](https://github.com/XDSEC/miniLCTF_2023/tree/main/Challenges/magical_syscall/src)，有兴趣可以去看看

## 几个要点

### _init_array

_init_array是程序的初始化列表，其中注册的函数会在main函数开始之前被调用。

### 两个简单的反调试

在初始化列表里注册了两个反调试函数。

一个是通过检查`/proc/self/status`文件中`TracePid`内容，判断是否被调试：

```c
nsigned __int64 sub_128C()
{
  char *i; // rax
  size_t v1; // rax
  FILE *stream; // [rsp+10h] [rbp-120h]
  char v4[264]; // [rsp+20h] [rbp-110h] BYREF
  unsigned __int64 v5; // [rsp+128h] [rbp-8h]

  v5 = __readfsqword(0x28u);
  stream = fopen("/proc/self/status", "r");
  for ( i = fgets(v4, 256, stream); i; i = fgets(v4, 256, stream) )
  {
    if ( strstr(v4, "TracerPid") )
    {
      v1 = strlen(v4);
      if ( atoi(&v4[v1 - 3]) )
      {
        puts("debugger detected, exit...");
        exit(1);
      }
    }
  }
  return v5 - __readfsqword(0x28u);
}
```

另一个是自定义了signal handler，并在程序运行10s后alarm，即当程序运行超过10s后会直接退出。

```c
unsigned int sub_1253()
{
  signal(14, handler);
  signal(5, (__sighandler_t)sub_1236);
  return alarm(0xAu);
}
```

由于这不是这道题的重点，所以没有在这为难大家，只是让大家了解一下。至于绕过只要给exit扬了就行。alarm信号在IDA调试时也可以直接忽略。

### syscall

syscall即系统调用。我们一般的程序都运行在操作系统的用户空间，当需要进行一些更高权限的操作时，就需要通过系统调用进入内核执行代码，从而提高系统安全性和稳定性。

syscall函数是C标准库提供的一个构造syscall的工具函数，第一个参数为系统调用号，后面不定个需要的参数。参数按x64传参顺序依次赋给rdi、rsi、rdx、rcx

### fork

fork是linux的一个系统调用，用来根据当前进程创建子进程。

fork函数是C标准库对fork syscall的封装。值得关注的是函数的返回值。返回值小于0说明创建子进程失败，在子进程中，返回值为0，而在父进程中，返回值为子进程的进程号(pid)。在实际编程中常用if分支通过返回值来区分父子进程，执行不同的代码。

### ptrace

ptrace是linux的一个系统调用，一个进程可以通过ptrace查看甚至控制另一个进程的内部状态。大名鼎鼎的调试器gdb就是基于ptrace实现的，这里推荐一篇[文章](https://xz.aliyun.com/t/6882)

ptrace函数是C标准库对ptrace syscall的封装，函数原型如下：

```c
#include <sys/ptrace.h>       
long ptrace(enum __ptrace_request request, pid_t pid, void *addr, void *data);
```

- request：要进行的ptrace操作
- pid：要操作的进程号
- addr：要监控/修改的内存地址
- data：要读取/写入的数据变量

常见的request操作可以看[这](https://man7.org/linux/man-pages/man2/ptrace.2.html)

### user_regs_struct

这是一个定义了一系列寄存器的结构体，本身是专门为gdb写的，想看详细内容的话可以在C源文件里写一句`#include <sys/user.h>`，然后戳进去看就行了。

这个东西在进行ptrace操作（读写内存、寄存器）时很有用，可以在IDA里导入这个结构体方便分析。

### 通过ptrace自定义syscall

syscall的过程是代码从用户态进入内核态的过程。如果我们把用户态换成子进程，内核态换成父进程，通过ptrace进行父进程对子进程内存空间的读写，模拟内核执行代码时的数据处理，就能实现自定义syscall。这是理解这道题运行逻辑的核心。

## 程序运行逻辑

下面按顺序梳理一下整个程序的运行逻辑

首先fork出子进程，子进程执行tracee函数，父进程执行tracer函数

```c
int sub_1D8B()
{
  signed int v1; // [rsp+Ch] [rbp-4h]

  v1 = fork();
  if ( v1 < 0 )
  {
    puts("failed to creat subprocess");
    exit(1);
  }
  if ( v1 )
    tracer(v1);
  return tracee();
}
```

子进程进入tracee函数，先ptrace TRACEME告诉操作系统自己要被父进程追踪，然后发送SIGCONT信号

```c
int sub_1386()
{
  ptrace(PTRACE_TRACEME, 0LL, 0LL, 0LL);
  return raise(SIGCONT);
}
```

父进程进入tracer函数，先通过waitpid函数等待子进程SIGCONT信号

```c
  waitpid(a1, &stat_loc, 0);
  if ( (unsigned __int8)stat_loc != 127 ) // !WIFSTOPPED(status) 如果子进程不是正常退出的，则进入if分支
  {
    puts("debugger detected, exit...");
    exit(1);
  }
  ptrace(PTRACE_SETOPTIONS, a1, 0LL, PTRACE_O_EXITKILL); // 如果子进程处于退出状态，则kill掉父进程
```

下面就是父进程中一个非常大的while循环，里面定义了一系列syscall，if判断的条件则是上面介绍的user_regs_struct里的`orig_rax`，即自定义syscall的系统调用号。

还有一个要点就是在while循环的开头和结尾都有这一句：

```c
ptrace(PTRACE_SYSCALL, a1, 0LL, 0LL);
```

用处是使子进程在每次syscall开始和结束时停下，把控制权交给父进程进行相应操作。

到这有一点要明确，父进程会一直在while循环里呆着，永远也不会执行到main函数，而只有子进程真正去执行main函数代码。父进程最终会走到以下两个分支之一得以退出：

```c
    if ( arg1 == 8888 )                         // FAIL
      break; 
    if ( arg1 == 9999 )                         // SUCCESS
    {
      puts("congratulations");
      exit(0);
    }
```

即程序最后check成功或失败的判断。

子进程执行的main函数非常简洁：

```c
void __fastcall __noreturn main(int a1, char **a2, char **a3)
{
  puts("input your flag:");
  while ( 1 )
    syscall(
      (unsigned int)dword_40AC[pc + 468],
      (unsigned int)dword_40AC[pc + 1 + 468],
      (unsigned int)dword_40AC[pc + 2 + 468],
      (unsigned int)dword_40AC[pc + 3 + 468]);
}
```

打印提示信息后，只有一个死循环，里面不断去执行syscall，正是这里执行的syscall会被父进程拦截并进行相应操作，syscall的参数即分别为系统调用号和所需参数。有些自定义的syscall并不需要3个参数，但由于这里并不会修改这些值，所以传几个多余的参数不会有任何影响。

## VM逆向

有经验的逆向壬应该一眼vm了，以上内容不关心靠猜也能七七八八。

这道题中vm的突破点应该在变量的识别。首先pc应该很容易看出来，毕竟每个syscall之后都会把它加上2或3或4，即那条指令的长度。其次是导入user_regs_struct之后就可以比较清晰的看懂几个参数了。

然后通过几个syscall对比来看应该也能看出来一些特殊的指令。

比如完全对称的push和pop：

```c
if ( arg1 == 3904 )                         // PUSH
    {
      dword_40B4 = ptrace(PTRACE_PEEKDATA, a1, &dword_40B4, 0LL);
      dword_40AC[++dword_40A4 + 4] = dword_40B4;
      ++pc;
      ptrace(PTRACE_POKEDATA, a1, &pc, (unsigned int)pc);
    }
    if ( arg1 == 3905 )                         // POP
    {
      dword_40B4 = dword_40AC[dword_40A4-- + 4];
      ++pc;
      ptrace(PTRACE_POKEDATA, a1, &pc, (unsigned int)pc);
    }
```

互相有联系的CMP、JE、JNE：

```c
if ( arg1 == 3906 )                         // CMP
    {
      if ( arg2 )
      {
        if ( arg2 == 1 )
          flag_zf = dword_40A8 == dword_40B4;
      }
      else
      {
        flag_zf = dword_40B0 == arg3;
      }
      pc += 3;
      ptrace(PTRACE_POKEDATA, a1, &pc, (unsigned int)pc);
    }
    if ( arg1 == 3907 )                         // JE
    {
      if ( flag_zf )
      {
        pc = arg2;
        ptrace(PTRACE_POKEDATA, a1, &pc, (unsigned int)arg2);
      }
      else
      {
        pc += 2;
        ptrace(PTRACE_POKEDATA, a1, &pc, (unsigned int)pc);
      }
    }
    if ( arg1 == 3908 )                         // JNE
    {
      if ( flag_zf )
      {
        pc += 2;
        ptrace(PTRACE_POKEDATA, a1, &pc, (unsigned int)pc);
      }
      else
      {
        pc = arg2;
        ptrace(PTRACE_POKEDATA, a1, &pc, (unsigned int)arg2);
      }
    }
```

然后基本就能看出来像`dword_40B0`这样的是一些通用寄存器(ax、bx、cx...)

再往下就是一些计算类的指令，比如INC、MOD、XOR、RESET（置零）应该也不难看出

难度较大的应该是ADD、MOV指令，因为这俩实际做的事取决于参数，会对不同的寄存器/内存地址进行ADD、MOV操作，这里就需要认真对参数进行分析，搞清楚具体的指令的含义。

整个VM实际实现了一个RC4加密，然后和密文比较的过程，出题时写的伪汇编看[这里](https://github.com/XDSEC/miniLCTF_2023/blob/main/Challenges/magical_syscall/src/assembly.txt)

另一个点在于XCHG指令，即实现两个值的交换，但这里因为并没有使用临时变量存储其中一个变量原先的值，所以是个假的交换，相当于`a = b; b = a;`这样的操作，这也是RC4的魔改点。

```c
    if ( arg1 == 3912 )                         // XCHG
    {
      *((_DWORD *)&mem + (unsigned int)dword_40B0) = *((_DWORD *)&mem + (unsigned int)dword_40AC[0]);
      *((_DWORD *)&mem + (unsigned int)dword_40AC[0]) = *((_DWORD *)&mem + (unsigned int)dword_40B0);
      ++pc;
      ptrace(PTRACE_POKEDATA, a1, &pc, (unsigned int)pc);
    }
```

## 最终脚本

```python
def rc4_init(s, key, key_len):
    j = 0
    for i in range(256):
        j = (j + s[i] + key[i%key_len])%256
        # tmp = s[i]
        s[i] = s[j]
        s[j] = s[i]

def rc4_generate_keystream(s, length):
    i = 0
    j = 0
    key_stream = []
    while length:
        i = (i + 1) % 256    # 可以保证每256次循环后s盒中的每个元素至少被交换一次
        j = (j + s[i]) % 256
        # tmp = s[i]
        s[i] = s[j]
        s[j] = s[i]
        key_stream.append(s[(s[i] + s[j]) % 256])
        length -= 1
    return key_stream

def main():
    key = [ord(i) for i in "MiniLCTF2023"]        # 准备一些变量
    key_len = len(key)
    # enc = [ord(i) for i in "llac$ys_laci9am_ht1w_en1hc@m_l@utr1v_a"]
    enc = [147, 163, 203, 201, 214, 211, 240, 213, 177, 26, 84, 155, 80, 203, 176, 178, 235, 15, 178, 141, 47, 230, 21, 203, 181, 61, 215, 156, 197, 129, 63, 145, 144, 241, 155, 171, 47, 242]
    enc_len = len(enc)
    cipher = [0] * enc_len

    s = [i for i in range(256)]    # 初始化s盒
    rc4_init(s, key, key_len)      # 使用key打乱s盒
    key_stream = rc4_generate_keystream(s[:], enc_len) # 生成密钥流
    # print(key_stream)
    for i in range(enc_len):     # 逐字节异或加密
        cipher[i] = enc[i] ^ key_stream[i]
    cipher = cipher[::-1]
    print("".join(chr(i) for i in cipher))
    # print(cipher)
    # print(len(cipher))

if __name__ == '__main__':
    main()

```

flag： a_v1rtu@l_m@ch1ne_w1th_ma9ical_sy$call


