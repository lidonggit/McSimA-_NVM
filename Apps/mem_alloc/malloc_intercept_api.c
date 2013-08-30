//#include "malloc_intercept_api.h"
#include <stdio.h> 
#include <stdlib.h> 

void beg_malloc_intercept_for_pin()
{
  printf("APP[intercept_api]: Begin heap malloc interception for Pin\n"); 
}

void end_malloc_intercept_for_pin()
{
  printf("APP[intercept_api]: End heap malloc interception for Pin\n"); 
}

/*The followings are Fortran interfaces*/
void beg_malloc_intercept_for_pin_()
{
  beg_malloc_intercept_for_pin();
}

void end_malloc_intercept_for_pin_()
{
  end_malloc_intercept_for_pin();
}

#if 0    //DONG_TRIGGER_SIM
void dong_start_sim()
{
  printf("APP[intercept_api]: Start sim\n");
}

void dong_end_sim()
{
  printf("APP[intercept_api]: End sim\n");
}

/*The followings are Fortran interfaces*/
void dong_start_sim_()
{
  dong_start_sim();
}

void dong_end_sim_()
{
  dong_end_sim();
}
#endif

