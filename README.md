# 操作系统内核实现

学习《操作系统真象还原》并实现每章的代码。

## 操作系统

Ubuntu14.04

## 环境配置

- 安装 bochs
    ```shell
    sudo apt install bochs
    ```
- 安装 nasm
    ```shell
    sudo apt install nasm
    ```

## 代码使用

- 克隆仓库
    ```shell
    git clone git@github.com:zhangwenxiao/os-core.git
    cd os-core
    ``` 
- 新建虚拟硬盘
    ```shell
    bximage -hd -mode="flat" -size=60 -q hd60M.img
    ```
- 编译 mbr.S 和 loader.S
    ```shell
    cd boot
    nasm -I include -o mbr.bin mbr.S
    nasm -I include -o loader.bin loader.S
    cd ..
    ```
- 将 mbr.bin 和 loader.bin 写入虚拟硬盘
    ```shell
    dd if=boot/mbr.bin of=hd60M.img bs=512 count=1 conv=notrunc
    dd if=boot/loader.bin of=hd60M.img bs=512 count=2 seek=2 conv=notrunc
    ```
- 运行 bochs
    ```shell
    bochs -f bochsrc.disk
    ```

## 开发进度
- 2020.09.20 完成第二章源码
- 2020.09.27 完成第三章源码
- 2020.10.08 完成第四章源码
