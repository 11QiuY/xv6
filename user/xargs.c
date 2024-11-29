#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

int phares_line(char * buf,char * to){
    int i = 0;
    int num = 0;
    while(buf[i] != '\0'){
        if(buf[i] == '\n'){
            to[i] = '\0';
            num++;
            i++;
        }else{
        to[i] = buf[i];
        i++;
        }
    }
    to[i] = '\0';
    return num;
}

int main(int argc,char *argv[]){
    // read args from stdin
    char * buf = malloc(512);
    int i = 0;
    int n = 0;
    while(( n = read(0,buf+i,sizeof(buf))) > 0){
        i += n;
    }
    int num = phares_line(buf, buf);
    char * args[32];
     for(int i = 1; i < argc; i++){
        args[i-1] = argv[i];
    }
    for(int i = 0 ; i < num ; i ++){
        args[argc - 1] = buf;
        // printf("num = %d , buf = %s\n",i,buf);
        args[argc] = 0;
        buf += strlen(buf) + 1; // move to next line
        int pid = fork();
        if(pid == 0){
            exec(args[0],args);
        }else{
            wait(0);
        }
    }
    exit(0);
}
