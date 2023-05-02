## 题目考点

### _init_array _fini_array

_init_array和_fini_array中的函数分别会在main函数之前和之后被调用执行。

_init_array里sub_1A20函数做了一些变量的初始化和字符串的解密。

_fini_array里sub_16C5函数是题目主要逻辑，具体算法见下文。

### pipe进程间通信

pipe是一种linux的进程间通信方式，可以单向的从一个进程向另一个进程传递数据。

本题中使用pipe建立了父进程与子进程的通信管道，使用read和write读写消息。

### 多进程素数筛

想法来自：https://swtch.com/~rsc/thread/ 这是并发编程的最初尝试，感兴趣的师傅可以看看。

举个例子：主进程拿到一列数2-35，会先把第一个数2拿出来作为base，然后遍历这列数，如果某个数mod 2 == 0，则一定不是素数，丢弃，否则将这个数向子进程传递。子进程也会选择接收到的第一个数3作为base，然后遍历接收到的所有数，如果某个数mod 3 == 0，则一定不是素数，丢弃，否则将这个数向它的子进程继续传递。依次类推，直到整列数消耗完毕。

### 一些干扰

由于父进程和子进程之间的内存空间完全隔离，子进程中对全局变量的修改也不会影响到父进程，而最后对密文的检验是在父进程中完成的，所以子进程中的一些操作为干扰，对结果不会产生影响。

## 预期解

预期解是在看懂算法的情况下还原加密逻辑，省去素数筛部分代码，爆破可见字符，脚本如下：

```c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

unsigned char enc[] = {247, 95, 231, 176, 154, 180, 224, 231, 158, 5, 254, 216, 53, 92, 114, 224, 134, 222, 115, 159, 154, 246, 13, 220, 200, 79, 194, 164, 122, 181, 227, 205, 96, 157, 4, 31};
unsigned char rc4_s[256];
unsigned char rc4_key[] = "We1c0m3_t0_d^3ctf";

void rc4_init_s(unsigned char *s) {
    for (int i = 0; i < 256; i++) {
        s[i] = i;
    }
}

void rc4_shuffle(unsigned char *s, unsigned char *key, int key_len) {
    int j = 0;
    for (int i = 0; i < 256; i++) {
        j = (j + s[i] + key[i%key_len])%256;
        int tmp = s[i];
        s[i] = s[j];
        s[j] = tmp;
    }
}

void rc4_gen_keystream(unsigned char *s, int len, unsigned char *key_stream) {
    int i = 0, j = 0, cnt = -1;
    while (len) {
        i = (i + 1) % 256;   
        j = (j + s[i]) % 256;
        int tmp = s[i];
        s[i] = s[j];
        s[j] = tmp;
        key_stream[++cnt] = s[(s[i] + s[j]) % 256];
        len -= 1;
    }
}

int main() {
    unsigned char keystream1[50];
    unsigned char keystream[50];
    rc4_init_s(rc4_s);
    rc4_shuffle(rc4_s, rc4_key, 17);
    rc4_gen_keystream(rc4_s, 36, keystream1);
    // keystream1: 184, 134, 68, 99, 181, 216, 28, 149, 209, 126, 112, 94, 188, 86, 99, 52, 40, 144, 21, 248, 77, 82, 157, 30, 245, 31, 200, 100, 82, 27, 100, 15, 36, 147, 64, 93
    unsigned char primes[] = {3, 5, 7, 11, 13, 17, 19, 23, 29, 31};
    memcpy(rc4_key+17, primes, 10);
    rc4_shuffle(rc4_s, rc4_key, 27);
    for (int i = 0; i < 17; i++) {
        rc4_gen_keystream(rc4_s, 36, keystream);
    }
    // keystream: 53, 75, 160, 96, 8, 80, 165, 241, 51, 151, 178, 19, 203, 76, 13, 207, 163, 124, 87, 83, 226, 169, 101, 78, 14, 199, 122, 15, 253, 181, 158, 180, 51, 249, 97, 211
    for (int i = 0; i < 36; i += 2) {
        for (unsigned char a1 = 0; a1 < 0xff; a1++) {
            for (unsigned char a2 = 0; a2 < 0xff; a2++) {
                unsigned char a10 = a1;
                unsigned char a20 = a2;
                a1 = a1 ^ keystream1[i];
                a2 = a2 ^ keystream1[i+1];
                a1 = (a1 + a2) ^ keystream[i];
                a2 = (a1 - a2) ^ keystream[i+1];
                if ( a1 == enc[i] && a2 == enc[i+1]) {
                    printf("%c%c", a10, a20);
                }
                a1 = a10;
                a2 = a20;
            }
        }
    }

}
```
