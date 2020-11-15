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
- 编译、运行
    ```shell
    make all
    ```    

## 开发进度
- 2020.09.20 完成第二章源码
- 2020.09.27 完成第三章源码
- 2020.10.08 完成第四章源码
- 2020.10.18 完成第五章源码
- 2020.10.27 完成第六章源码
- 2020.11.10 完成第七章源码
- 2020.11.15 完成第八章源码
