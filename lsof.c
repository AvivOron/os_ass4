#include "types.h"
#include "stat.h"
#include "user.h"
#include "fs.h"

int
main(int argc, char *argv[])
{
  struct dirent de;
  struct dirent deP;
  char path[100];
  char pathF[1000];
  char buff[100]; 
  int fd = open("/proc",0);

  while(read(fd, &de, sizeof(de)) == sizeof(de)){
    if(strcmp(de.name, ".") != 0 && strcmp(de.name, "..") != 0
      && strcmp(de.name, "blockstat") != 0 && strcmp(de.name, "inodestat") != 0) {
        strcpy(path, "");
        strcpy(path,"/proc/");
        strcpy(path+strlen(path),de.name);
        strcpy(path+strlen(path),"/fdinfo");
        int fdP = open(path,0);

        while(read(fdP, &deP, sizeof(deP)) == sizeof(deP)){
          if(strcmp(deP.name, ".") != 0 && strcmp(deP.name, "..") != 0){
              strcpy(pathF, "");
              strcpy(pathF,"/proc/");
              strcpy(pathF+strlen(pathF),de.name);
              strcpy(pathF+strlen(pathF),"/fdinfo/");
              strcpy(pathF+strlen(pathF), deP.name);
              int fdF = open(pathF,0);
              read(fdF, &buff, 100);
              printf(2,"PID: %s %s\n", de.name,buff);
              strcpy(buff,"");
              close(fdF);
              strcpy(pathF, "");
          }
        }
        strcpy(path,"");
        close(fdP);
    }
  }

  close(fd);
  exit();
}
