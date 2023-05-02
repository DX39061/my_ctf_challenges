## keypoints

### _init_array & _fini_array

functions registered in `_init_array` and `_fini_array` will be executed before and after main function.

sub_1A20(in _init_array)does some variable initialization and string decoding work.

sub_16C5(in _fini_array)is the key function.

### pipe IPC

pipe is one of the IPC ways in linux，which can pass data from one process to another in one direction.

In this problem, pipe is used to established a communication channel between parent and child processes. `read`and `write` function are used to read and write data with the channel.

### Multiprocess prime sieve

idea comes from：https://swtch.com/~rsc/thread/ , which is the begining of concurrent programming. Those who are interested can have a look。

For exanple：initially, the main process get numbers 2-35. It will pick the fist number 2 as `base`, then for each of the rest of numbers, if n mod 2 == 0，it must not be a prime number and will be discarded, otherwise n will be pass to the child process. The child process will also pick the fist number received as `base`(here is 3), then for each of number received after that, if n mod 3 == 0，it must not be a prime number and will be discarded, otherwise n will be pass to the child's child process. It will not stop until no number to pass to child process, and you've got all the primes.

### Some confusing items

Since the memory space between the parent process and the child process is completely isolated, the modification of global variables in the child process will not affect the parent process. And the final check is completed in the parent process, so some operations in the child process are interference and will not affect the result.

## Expected solution

The expected solution is to restore the encryption logic under the condition of understanding the algorithm, leaving out the prime sieve part of the code，finally bruting force to get the flag (also you can try constraint solving tools like z3).

script：

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
