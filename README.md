# tsn-demo
demo of tsn

## 编译
### x86-64
```shell
# mkdir build; cd build;
# cmake ..
# make
```

### aarch64
* 编译Debug程序
```shell
# mkdir build-aarch64; cd build-aarch64;
# cmake -DCMAKE_TOOLCHAIN_FILE=../toolchain.cmake -DCMAKE_INSTALL_PREFIX=/home/opt/gcc-arm-10.2-2020.11-x86_64-aarch64-none-linux-gnu/aarch64-none-linux-gnu/libc/usr -DCMAKE_BUILD_TYPE=Debug   ..
# make
```

* 编译Release程序
```shell
# mkdir build-aarch64; cd build-aarch64;
# cmake -DCMAKE_TOOLCHAIN_FILE=../toolchain.cmake -DCMAKE_INSTALL_PREFIX=/home/opt/gcc-arm-10.2-2020.11-x86_64-aarch64-none-linux-gnu/aarch64-none-linux-gnu/libc/usr -DCMAKE_BUILD_TYPE=Release   ..
# make
```
