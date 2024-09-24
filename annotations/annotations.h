#ifndef _ANNOTATIONS_H
#define _ANNOTATIONS_H
#include <stdint.h>
#include <stdio.h> 

unsigned __attribute__ ((noinline)) ariel_enable()
{
          printf("ariel enable\n");
          return 1;
}

#endif
