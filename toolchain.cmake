SET(CMAKE_SYSTEM_NAME Linux)
SET(CMAKE_SYSTEM_PROCESSOR aarch64)


# 代表一系列相关文件夹路径的根目录的变更，编译器到指定根目录下寻找相应的系统库
set(CMAKE_SYSROOT /opt/aarch64-none-linux-gnu/aarch64-none-linux-gnu/libc)
set(CMAKE_FIND_ROOT_PATH 
    /opt/aarch64-none-linux-gnu
    /opt/aarch64-none-linux-gnu/aarch64-none-linux-gnu
)

set(CMAKE_C_COMPILER /opt/aarch64-none-linux-gnu/bin/aarch64-none-linux-gnu-gcc)
set(CMAKE_CXX_COMPILER /opt/aarch64-none-linux-gnu/bin/aarch64-none-linux-gnu-g++)
#set(CMAKE_LD /usr/aarch64-linux-gnu/bin/ld)
#set(CMAKE_AR /usr/aarch64-linux-gnu/bin/ar)

#对FIND_PROGRAM()起作用，有三种取值，NEVER,ONLY,BOTH,第一个表示不在你CMAKE_FIND_ROOT_PATH下进行查找，第二个表示只在这个路径下查找，第三个表示先查找这个路径，再查找全局路径，对于这个变量来说，一般都是调用宿主机的程序，所以一般都设置成NEVER
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)

#下面的三个选项表示只在交叉环境中查找库和头文件
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)

set(BUILD_SHARED_LIBS ON)
