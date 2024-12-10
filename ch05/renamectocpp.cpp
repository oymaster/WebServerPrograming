#include <iostream>
#include <dirent.h>
#include <sys/stat.h>
#include <cstring>
#include <cstdio>
#include <string>

// 函数：检查文件是否是 .c 后缀的文件
bool hasCExtension(const std::string& filename) {
    return filename.size() >= 2 && filename.substr(filename.size() - 2) == ".c";
}

// 函数：修改文件的后缀名 .c -> .cpp
std::string renameToCpp(const std::string& filename) {
    return filename.substr(0, filename.size() - 2) + ".cpp";
}

int main() {
    const char* current_dir = ".";  // 当前目录
    DIR* dir;
    struct dirent* entry;
    struct stat file_stat;

    // 打开当前目录
    dir = opendir(current_dir);
    if (dir == nullptr) {
        std::cerr << "Failed to open directory!" << std::endl;
        return 1;
    }

    // 遍历当前目录下的文件
    while ((entry = readdir(dir)) != nullptr) {
        std::string filename = entry->d_name;

        // 跳过 "." 和 ".." 目录
        if (filename == "." || filename == "..") {
            continue;
        }

        // 获取文件状态
        if (stat(filename.c_str(), &file_stat) == -1) {
            std::cerr << "Error getting file status for: " << filename << std::endl;
            continue;
        }

        // 如果是普通文件并且是 .c 后缀文件
        if (S_ISREG(file_stat.st_mode) && hasCExtension(filename)) {
            std::string new_filename = renameToCpp(filename);
            
            // 将文件重命名为 .cpp 后缀
            if (rename(filename.c_str(), new_filename.c_str()) == 0) {
                std::cout << "Renamed: " << filename << " -> " << new_filename << std::endl;
            } else {
                std::cerr << "Failed to rename: " << filename << std::endl;
            }
        }
    }

    // 关闭目录
    closedir(dir);
    return 0;
}
