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
        i = (i + 1) % 256;   // 可以保证每256次循环后s盒中的每个元素至少被交换一次
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

// 2 4 6 8 10 12 14 16 18 20 22 24 26 28 30 32 34
// 3 9 15 21 27 33
// 5 25
// 7 35
// 11
// 13
// 17
// 19
// 23
// 29
// 31