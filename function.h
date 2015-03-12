#ifndef __PRNG_H__
#define __PRNG_H__

//Length Assumption
#define TYPE_LEN 18
#define ADDRESS_LEN 64
#define PORT_LEN 4
#define NAME_LEN 20

struct Function {
   char *name;
   int typesize;
   int *types;
   skeleton f;
};

#endif
