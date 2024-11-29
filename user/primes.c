#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "user/debug.h"


void prime(int left){
    // LOG("left = %d", left);
    int fd[2];
    pipe(fd);
    int primex;
    int n;
    if(read(left,& primex,sizeof(primex)) == 0){
        close(fd[0]);
        close(fd[1]);
        return;
    }else{
        printf("prime %d\n", primex);
        while(read(left,&n,sizeof(n)) != 0){
            if(n %  primex!= 0){ write(fd[1],&n,sizeof(n)); }
        }
        close(fd[1]);
        close(left);
        prime(fd[0]);
    }
}

int main(){
    // LOG("main");
    int fd[2];
    pipe(fd);
    int primex = 3;
    printf("prime 2\n");
    printf("prime 3\n");
    for(int i = 3 ; i < 280;i+=2){
        if(i % primex != 0){
        //    LOG("write %d", i);
           write(fd[1],&i,sizeof(i)); 
        }
    }
    close(fd[1]);
    prime(fd[0]);
    exit(0);
}