#include <stdio.h>
#include <stdlib.h>
//#include "malloc_intercept_api.h"

//#define BLOCK_SIZE 4000000
#define BLOCK_SIZE 1000

extern "C" {
extern void mcsim_skip_instrs_begin();
extern void mcsim_skip_instrs_end();
extern void mcsim_spinning_begin();
extern void mcsim_spinning_end();
extern void beg_malloc_intercept_for_pin();
extern void end_malloc_intercept_for_pin();
}

long int global_sum=0;
float dummy[45];
long int func1()
{
   int *p = NULL;
   long int func1_sum=0;

   printf("heap size=%d\n", sizeof(int)*BLOCK_SIZE);
   beg_malloc_intercept_for_pin();
   p = (int *)malloc(sizeof(int)*BLOCK_SIZE);
   end_malloc_intercept_for_pin();

   //dong_start_sim();
   int i;
   for(i=0; i<BLOCK_SIZE; i++)
   {
     p[i] = i;
   }
   for(i=2; i<BLOCK_SIZE; i++)
   {
     p[i] = p[i]*10;
     func1_sum = p[i] + func1_sum;
   }
   free(p);
   //dong_end_sim();
   printf("func1_sum=%ld\n", func1_sum); 
   return func1_sum;
}

int func2()
{
   int *p = NULL;
   int func2_sum=0;
   p = (int *)malloc(sizeof(int)*BLOCK_SIZE);
   int i;
   for(i=BLOCK_SIZE; i<2*BLOCK_SIZE; i++)
   {
     p[i-BLOCK_SIZE] = i;
   }
   for(i=1; i<BLOCK_SIZE; i++)
   {
     p[i] = p[i-1]*p[i+1];
     func2_sum = p[i] + func2_sum;
   }
   free(p);
   return func2_sum;
}


main(){
  int i;
  printf("sizeof(int)=%d\n", sizeof(int));
  static int test_static=1;
  printf("test_static=%d\n", test_static);

  //for(i=0; i<10; i++)
  for(i=0; i<1; i++)
  {
    global_sum = func1() + global_sum;
    //global_sum = func2() + global_sum;
  }

  printf("global_sum=%ld\n", global_sum);
}
