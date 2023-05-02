#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int key1;
int key2;
unsigned long long f = 1;
unsigned char fail[] = {103, 123, 102, 102, 109, 56, 52, 100, 120, 110, 52, 96, 102, 109, 52, 117, 115, 117, 125, 122, 58};
unsigned char success[] = {103, 113, 107, 57, 108, 123, 62, 108, 119, 121, 118, 106, 63};
unsigned char fake_enc[] = {219, 182, 42, 4, 199, 185, 104, 224, 189, 62, 4, 111, 211, 56, 16, 107, 74, 229, 97, 167, 36, 13, 252, 115, 170, 113, 248, 16, 13, 125, 85, 110, 67};

unsigned char rc4_s[256];
unsigned char rc4_key[100] = {124, 78, 26, 72, 27, 70, 24, 116, 95, 27, 116, 79, 117, 24, 72, 95, 77};
unsigned char rc4_keystream[100];
int rc4_key_len = 17;

int pipe9[2];
unsigned char input[40];
int input_len;
int throw_cnt = 1;
unsigned char enc[100] = {247, 95, 231, 176, 154, 180, 224, 231, 158, 5, 254, 216, 53, 92, 114, 224, 134, 222, 115, 159, 154, 246, 13, 220, 200, 79, 194, 164, 122, 181, 227, 205, 96, 157, 4, 31};

void before_main() __attribute__((constructor));
void after_main() __attribute__((destructor));

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

void child_work(int *pipe_pre) {
    int base;
    int status;
    int child_pid;
    int pipe_last[2];
    close(pipe_pre[1]);
    if (read(pipe_pre[0], &base, sizeof(int)) == 0) {
        exit(1);
    }
    
    if (pipe(pipe_last) == -1) {
        // fprintf(2, "create pipe error");
        exit(1);
    }
    child_pid = fork();
    if (child_pid < 0) {
        // fprintf(2, "create child process error");
        exit(1);
    }
    if (child_pid == 0) {
        child_work(pipe_last);
    } else {
        int num;
        close(pipe_last[0]);
        close(pipe9[0]);
        write(pipe9[1], &base, sizeof(int));
        close(pipe9[1]);
        while(read(pipe_pre[0], &num, sizeof(int)) != 0) {
            if (num % base != 0) {
                write(pipe_last[1], &num, sizeof(int));
            } else {
                throw_cnt++;
            }
        }
        close(pipe_last[1]);
        wait(&status);
        rc4_key[rc4_key_len++] = rc4_s[base];
        rc4_shuffle(rc4_s, rc4_key, rc4_key_len);
        for (int i = 0; i < 256-throw_cnt; i++) {
            rc4_gen_keystream(rc4_s, input_len, rc4_keystream);
            for (int i = 0; i < input_len; i += 2) {
                // printf("%d, %d  ", input[i], input[i+1]);
                input[i] = (input[i] + input[i+1]) ^ rc4_keystream[i];
                input[i+1] = (input[i] - input[i+1]) ^ rc4_keystream[i+1];
                // printf("%d, %d, ", input[i], input[i+1]);
            }
        }
        exit(0);
    }
}

void after_main() {
    int pipe0[2];
    int child_pid;
    int status;

    if (pipe(pipe0) == -1) {
        // fprintf(2, "create pipe error");
        exit(1);
    }
    child_pid = fork();
    if (child_pid < 0) {
        // fprintf(2, "create child process error");
        exit(1);
    }
    if (child_pid == 0) {
        child_work(pipe0);
    } else {
        close(pipe0[0]);
        int base = 2;
        for (int i = 3; i < input_len; i++) {
            if (i % base != 0) {
                write(pipe0[1], &i, sizeof(int));
            } else {
                throw_cnt++;
            }
        }
        close(pipe0[1]);
        close(pipe9[1]);
        int num;
        while(read(pipe9[0], &num, sizeof(int))) {
            rc4_key[rc4_key_len++] = num;
        }
        close(pipe9[0]);
        wait(&status);
        if (status != 0) {
            puts(fail);
            exit(1);
        } 
        child_pid = fork();
        if (child_pid < 0) {
            exit(1);
        }
        
        if (!child_pid) {
            rc4_shuffle(rc4_s, rc4_key, rc4_key_len);
            for (int i = 0; i < throw_cnt; i++) {
                rc4_gen_keystream(rc4_s, input_len, rc4_keystream);
            }
            // printf("\nthrow_cnt: %d\n", throw_cnt);
            for (int i = 0; i < input_len; i += 2) {
                // printf("%d, %d   ", input[i], input[i+1]);
                input[i] = (input[i] + input[i+1]) ^ rc4_keystream[i];
                input[i+1] = (input[i] - input[i+1]) ^ rc4_keystream[i+1];
                // printf("%d, %d\n", input[i], input[i+1]);
            }
            
            for (int i = 0; i < input_len; i++) {
                // printf("%d, ", input[i]);
                if (input[i] != enc[i]) {
                    exit(1);
                }
            }
            exit(0);
        } else {
            wait(&status);
            if (!status) {
                puts(success);
            } else {
                puts(fail);
            }
            exit(0);
        }
    }
}


void before_main() {
    if (f) {
        key1 = 20;
        key2 = 23;
        
    } else {
        key1 = 23;
        key2 = 20;
    }
    for (int i = 0; i < 17; i++) {
        rc4_key[i] ^= (key1 + key2);
    }
    for (int i = 0; i < 21; i++) {
        fail[i] ^= 20;
    }
    for (int i = 0; i < 13; i++) {
        success[i] ^= 30;
    }
}

int main() {
    pipe(pipe9);
    scanf("%s", input);
    input_len = strlen(input);
    rc4_init_s(rc4_s);
    rc4_shuffle(rc4_s, rc4_key, rc4_key_len);
    rc4_gen_keystream(rc4_s, input_len, rc4_keystream);
    for (int i = 0; i < input_len; i++) {
        input[i] ^= rc4_keystream[i];
    }
    // rc4_keystream: 184, 134, 68, 99, 181, 216, 28, 149, 209, 126, 112, 94, 188, 86, 99, 52, 40, 144, 21, 248, 77, 82, 157, 30, 245, 31, 200, 100, 82, 27, 100, 15, 36, 147, 64, 93
    for (int i = 0; i < input_len; i++) {
        if (input[i] != fake_enc[i]) {
            exit(1);
        }
    }
}