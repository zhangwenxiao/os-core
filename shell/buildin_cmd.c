#include "buildin_cmd.h"
#include "syscall.h"
#include "stdio.h"
#include "string.h"
#include "fs.h"
#include "global.h"
#include "dir.h"
#include "shell.h"
#include "assert.h"

// 将路径 old_abs_path 中的 .. 和 . 转换为实际路径后存入 new_abs_path
static void wash_path(char* old_abs_path, char* new_abs_path) {
    assert(old_abs_path[0] == '/');
    char name[MAX_FILE_NAME_LEN] = {0};
    char* sub_path = old_abs_path;
    sub_path = path_parse(sub_path, name);
    if (name[0] == 0) {
        new_abs_path[0] = '/';
        new_abs_path[1] = 0;
        return;
    }
    new_abs_path[0] = 0; // 避免传给 new_abs_path 的缓冲区不干净
    strcat(new_abs_path, "/");
    while (name[0]) {
        // 如果是上一级目录 ..
        if (!strcmp("..", name)) {
            char* slash_ptr = strrchr(new_abs_path, '/');
            // 如果未到 new_abs_path 中的顶层目录, 就将最右边的 '/' 替换为 0
            // 这样便去除了 new_abs_path 中最后一层路径, 相当于到了上一级目录
            if (slash_ptr != new_abs_path) { // 如 new_abs_path 为 "/a/b", ".." 之后变成 "/a"
                *slash_ptr = 0;
            } else { // 如 new_abs_path 为 "/a", ".." 之后则变为 "/"
                *(slash_ptr + 1) = 0;
            }
        } else if (strcmp(".", name)) { // 如果路径不是 ".", 就将 name 拼接到 new_abs_path
            if (strcmp(new_abs_path, "/")) {
                strcat(new_abs_path, "/");
            }
            strcat(new_abs_path, name);
        } // 若 name 为当前目录 ".", 无须处理 new_abs_path

        // 继续遍历下一层路径
        memset(name, 0, MAX_FILE_NAME_LEN);
        if (sub_path) {
            sub_path = path_parse(sub_path, name);
        }
    }
}

// 将 path 处理成不含 .. 和 . 的绝对路径, 存储在 final_path
void make_clear_abs_path(char* path, char* final_path) {
    char abs_path[MAX_PATH_LEN] = {0};
    // 先判断是否输入的是绝对路径
    if (path[0] != '/') { // 若输入的不是绝对路径, 就拼接成绝对路径
        memset(abs_path, 0, MAX_PATH_LEN);
        if (getcwd(abs_path, MAX_PATH_LEN) != NULL) {
            // 若 abs_path 表示的当前目录不是根目录
            if (!((abs_path[0] == '/') && (abs_path[1] == 0))) {
                strcat(abs_path, "/");
            }
        }
    }
    strcat(abs_path, path);
    wash_path(abs_path, final_path);
}
