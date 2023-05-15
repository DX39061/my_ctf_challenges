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
    # cipher = cipher[::-1]b
    print("".join(chr(i) for i in cipher))
    print(cipher)
    # print(len(cipher))

if __name__ == '__main__':
    main()
