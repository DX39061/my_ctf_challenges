## hacker's gift

### 如何找到主要逻辑

拿gift程序举例：

在函数列表中搜索`main`，发现一个叫`client::main::xxxx`的函数，点开里面就是主要逻辑，而client就是这个rust程序的包名。

再在函数列表中搜索`client`即可看到所有出题人自己写的函数，主要加密逻辑在encrypt函数中。

### 程序运行流程

附件提供流量包目的也是帮选手梳理思路，应该还是挺清晰的。

server程序运行在hacker X的服务器上，client是DX收到的gift。

DX闲着没事干运行这个程序时，client会首先在本地生成一个RSA密钥对，然后把公钥放在http header里，向server发出第一个getkey请求。server收到请求后，会生成一个0-255的随机数，拼接上一个固定的字符串，作为key。用client发来的公钥加密key，放在response body中返回。

client拿到加密的key，用私钥解密拿到明文key。然后遍历同路径下一个叫`dangerous_directory`的文件夹，对于每一个文件，每次读入1024位，与key的第`i%key_len`位异或，直到文件内容读取结束。然后把文件名放在request header，加密数据放到request body里，向server发出upload请求。

### 解题思路 & 脚本

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
