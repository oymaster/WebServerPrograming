#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <fcntl.h>

bool daemonize(){
    pid_t pid = fork();
    if (pid < 0)
        return false;
    else if (pid > 0) {
        printf("child pid is :%d\n", pid);
        exit(0);
    }
    umask(0);
    pid_t sid = setsid();
    if (sid < 0)
        return false;
    if (chdir("/") < 0)
        return false;
    close(STDIN_FILENO);
    close(STDOUT_FILENO);
    close(STDERR_FILENO);
    open("/dev/null", O_RDONLY);
    open("/dev/null", O_RDWR);
    open("/dev/null", O_RDWR);
}

int main(){
    if (!daemonize()) {
        perror("Daemonization failed");
        return 1;
    }
    while (1) {
        sleep(10);
    }
    return 0;
}