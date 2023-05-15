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
