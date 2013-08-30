/***Intercept malloc ops***/
#include "intercept_malloc.h" 
#include "mypthreadtool.h"

using namespace PinPthread;

BOOL heap_flag1 = false;  //flag for the interception API
BOOL heap_flag2 = false;  //flag for the appropriate heap size
int heap_size = 0;

VOID InterceptMalloc_beg()
{
  cout << "[Pthread]: InterceptMalloc_beg: set heap_flag to true" << endl;
  heap_flag1 = true; heap_flag2 = false; heap_size = 0;
}

VOID InterceptMalloc_end()
{
  cout << "[Pthread]: InterceptMalloc_end: set heap_flag to true" << endl;
  heap_flag1 = false; heap_flag2 = false; heap_size = 0;
}

VOID MallocBefore(CHAR *name, int size, ADDRINT returnip)
{

  #ifdef DONG_DEBUG
  INT32 line;
  string file;
  PIN_LockClient();  //must use lock, otherwise the compiler will complain
  PIN_GetSourceLocation(returnip, NULL, &line, &file);
  PIN_UnlockClient();
  cout << "[Pthread]: " << name << "_bef  " << dec << "file=" << file <<",   "
    << "line=" << line << ",    "
    << "size=" << size << ",   returnip="
    << hex << returnip << dec << endl;
  #endif

  if(size != exp_heap_size)
    heap_flag2 = false;
  else{
    heap_flag2 = true;
    heap_size = size;
  }
}

VOID MallocAfter(CHAR *name, ADDRINT ret_addr, ADDRINT returnip)
{
  #ifdef DONG_DEBUG
  cout << "[Pthread]: "<< name << "_aft  returns " << hex
    << ret_addr << ",  returnip=" << returnip << dec 
    << "  heap_flag1=" << heap_flag1 << "  heap_flag2=" << heap_flag2 
    << "  size=" << heap_size << endl;
  #endif

  if(heap_flag1 && heap_flag2)
  {
    #if 0
    //add to the list
    HeapMemRec * hmr = new HeapMemRec();
    hmr->start_address = ret_addr; hmr->end_address = ret_addr + heap_size -1; 
    hmr->avail_flag = true;
    //hmr->rtn_name and hmr->lib_name are ignored for now, but can be considered later
    //pthreadsim->scheduler->pts->mcsim->ckpm->ckpm_l.push_front(hmr);
    pthreadsim->scheduler->pts->add_heap_mem(hmr);
    #endif

    pthreadsim->scheduler->pts->add_heap_mem(ret_addr, heap_size);
    cout << "[Pthread]: " << name << "_aft  returns " << hex
      << ret_addr << ",  returnip=" << returnip << dec
      << "  size=" << heap_size << "  added into the McSim" << endl;
  }
}

/*This routine is specific for HPL */
#ifdef APP_HPL
//VOID HPL_malloc_assist(ADDRINT addr, long size)
VOID HPL_malloc_assist(ADDRINT addr, int size)
{
  cout << "[Pthread HPL_malloc_assist]: addr=" << hex << addr <<  dec 
    << "  size=" << size << endl;
  pthreadsim->scheduler->pts->update_heap_mem_for_hpl(addr, size);      
}
#endif

VOID FreeBefore(CHAR *name, ADDRINT addr)
{
  /*HeapMemRec *hmr = pthreadsim->scheduler->pts->search_heap_mem(addr);
  if(hmr)
  {
    hmr->avail_flag = false;
    #ifdef DONG_DEBUG
    cout << name <<  "free_bef: " << hex << addr << dec << endl;
    #endif
  } */

  /*pthreadsim->scheduler->pts->search_heap_mem(addr);*/
  #ifdef DONG_DEBUG
  cout << "[Pthread]: "<< name << "_bef: " << hex << addr << dec << endl;
  #endif
  pthreadsim->scheduler->pts->free_heap_mem(addr);
}

VOID ReallocBefore(CHAR *name, ADDRINT ptr, int size, ADDRINT returnip)
{
  FreeBefore(name, ptr);
  MallocBefore(name, size, returnip);
}

VOID CallocBefore(CHAR *name, int nmemb, int size, ADDRINT returnip)
{
  MallocBefore(name, nmemb*size, returnip);
}
