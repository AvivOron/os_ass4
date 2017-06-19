#define FILE 0
#define DIRECTORY 1

char*
strcpy(char *s, char *t)
{
  char *os;

  os = s;
  while((*s++ = *t++) != 0)
    ;
  return os;
}

char* numToCHar(int num, char returnBuff[]){
    char* buff = returnBuff;
    char const numbers[] = "0123456789";
    int numsLeft;
    if(num < 0){
        *buff++ = '-';
        num *= -1;
    }
    numsLeft = num;
    ++buff;
    numsLeft = numsLeft/10;
    while(numsLeft){
        ++buff;
        numsLeft = numsLeft/10;        
    }
    *buff = '\0';
    *--buff = numbers[num%10];
    num = num/10;
    while(num){
        *--buff = numbers[num%10];
        num = num/10;        
    }
    return returnBuff;
}