# 操作系统内核实现

学习《操作系统真象还原》并实现每章的代码。

## 操作系统

Ubuntu14.04

## 环境配置

- 安装 bochs
    `sudo apt install bochs`
- 安装 nasm
    `sudo apt install nasm`

## 代码使用

- 克隆仓库
    `git clone git@github.com:zhangwenxiao/os-core.git` 
- 新建虚拟硬盘
    `bximage -hd -mode="flat" -size=60 -q hd60M.img`
- 编译 mbr.S
    ```shell
    cd boot
    nasm -o mbr.bin mbr.S
    cd ..
    ```
- 将 mbr.bin 写入虚拟硬盘
    `dd if=boot/mbr.bin of=hd60M.img bs=512 count=1 conv=notrunc`
- 运行 bochs
    `bochs -f bochsrc.disk`

## 开发进度
- 2020.09.20 完成第二章源码
