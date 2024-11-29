#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "kernel/fs.h"
#include "kernel/fcntl.h"
#include "user/debug.h"


char*
fmtname(char *path)
{
  char *p;

  // Find first character after last slash.
  for(p=path+strlen(path); p >= path && *p != '/'; p--)
    ;
  p++;

  // Return blank-padded name.
  return p;
}

void find(char * path , const char * filename){
    char buf[512], *p;
    int fd;
    struct dirent de;
    struct stat st;

    if((fd = open(path , O_RDONLY)) < 0){
        printf("find: cannot open %s\n", path);
        return;
    }

    if(fstat(fd,&st) < 0){
        printf("find: cannot stat %s\n", path);
        close(fd);
        return;
    }

    switch(st.type){
        case T_DEVICE:
        case T_FILE:
            // LOG("T_FILE , path = %s , filename = %s", fmtname(path), filename);
            if(strcmp(fmtname(path) , filename) == 0){
                printf("%s\n", path);
            }
            break;
        case T_DIR:
            // LOG("T_DIR");
            if(strlen(path) + 1 + DIRSIZ + 1 > sizeof buf){
                printf("find: path too long\n");
                break;
            }
            strcpy(buf, path);
            p = buf + strlen(buf);
            *p++ = '/';
            while(read(fd, &de, sizeof(de)) == sizeof(de)){
                if(de.inum == 0){
                    continue;
                }
                if(strcmp(de.name, ".") == 0 || strcmp(de.name, "..") == 0){
                    continue;
                }
                memmove(p, de.name, DIRSIZ);
                p[DIRSIZ] = 0;
                if(stat(buf, &st) < 0){
                    printf("find: cannot stat %s\n", buf);
                    continue;
                }
                find(buf, filename);
            }
            break;

    }
    close(fd);
    return ;
}

int main(int argc , char *argv[]) {
    if(argc != 3){
        printf("Usage: find <path> <filename>\n");
        exit(1);
    }
    find(argv[1], argv[2]);
    return 0;
}