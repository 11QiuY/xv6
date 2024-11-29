#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

int main(int argc , char *argv[]) {
    int fd1[2];
    int fd2[2];
    pipe(fd1);
    pipe(fd2);

    char buf[1];
    int pid = fork();
    if(pid == 0){// child
        int pid = getpid();
        close(fd1[1]);
        read(fd1[0], buf, 1);
        printf("%d: received ping\n",pid);
        close(fd1[0]);
        close(fd2[0]);
        write(fd2[1], buf, 1);
        close(fd2[1]);
    } else {// parent
        int xpid = getpid();
        close(fd1[0]);
        write(fd1[1], "a", 1);
        close(fd1[1]);
        close(fd2[1]);
        read(fd2[0], buf, 1);
        printf("%d: received pong\n", xpid);
        close(fd2[0]);
    }
    exit(0);
}