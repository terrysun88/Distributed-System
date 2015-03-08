#include <iostream>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
using namespace std;

void encode(char *dest, char *srs) {
   size_t len = strlen(srs)+1;
   memcpy(dest,&len,sizeof(int));
   memcpy(&dest[4],srs, len-1);
   //strcpy(&dest[4],srs);
   dest[len+3]='\0';
}

int decode(char *dest, char *srs) {
   size_t len = 0;
   memcpy(&len,srs,4);
   memcpy(dest,&srs[4],len);
   return len;
}

