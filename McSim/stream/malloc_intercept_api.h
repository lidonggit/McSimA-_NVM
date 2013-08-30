#include "stdio.h"

void dong_start_sim();
void dong_end_sim();
void beg_malloc_intercept_for_pin();
void end_malloc_intercept_for_pin();


//Note: REG_DGEMM and FT_DGEMM cannot be defined at the same time
//#define REG_DGEMM   
#define FT_DGEMM   

#define SAMPLING_FT_COMP  //only compute a few iterations to save simulation time 
			  //for the ABFT algorithm
