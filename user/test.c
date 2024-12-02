#include "kernel/types.h"
#include "kernel/fs.h"
#include "user/user.h"
#include "kernel/fcntl.h"
#include "kernel/stat.h"
#include "user/debug.h"

void print(struct stat st){
    printf("st.dev = %d\n" , st.dev);
    printf("st.ino = %d\n" , st.ino);
    printf("st.type = %d\n" , st.type);
    printf("st.nlink = %d\n" , st.nlink);
    printf("st.size = %lu\n" , st.size);
}

int main(){
    struct stat st;
    fstat(0, &st);
    print(st);
    return 0;
}