#include <stdio.h>
#include <arpa/inet.h>  // 包含 inet_ntoa 和 in_addr 相关函数

int main() {
    //测试inet_ntoa
    struct in_addr addr1, addr2;

    // 使用 inet_aton 将字符串形式的 IP 地址转换为二进制格式
    if (inet_aton("1.2.3.4", &addr1) == 0) {
        printf("Invalid IP address 1.2.3.4\n");
        return 1;
    }

    if (inet_aton("112.2.3.4", &addr2) == 0) {
        printf("Invalid IP address 112.2.3.4\n");
        return 1;
    }

    // 使用 inet_ntoa 将二进制格式的 IP 地址转换为字符串格式
    char *szvalue1 = inet_ntoa(addr1);
    char *szvalue2 = inet_ntoa(addr2);
    
    // 打印转换后的 IP 地址
    printf("IP address 1: %s\n", szvalue1);
    printf("IP address 2: %s\n", szvalue2);

    return 0;
}
