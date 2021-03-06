#include "mypthreadtool.h"
#include "PTS.h"

#ifdef DONG_TRIGGER_SIM
#include "triggersim_api.h"
#endif

#ifdef APP_HPL
#include <sys/file.h>
#include <fcntl.h>
#include <unistd.h>
#endif

#ifdef MALLOC_INTERCEPT
#include "intercept_malloc.h"
#endif

using namespace PinPthread;

/* ================================================================== */

int main(int argc, char** argv) 
{
    Init(argc, argv);
    PIN_InitSymbols();
    PIN_Init(argc, argv);
    IMG_AddInstrumentFunction(FlagImg, 0);
    RTN_AddInstrumentFunction(FlagRtn, 0);
    TRACE_AddInstrumentFunction(FlagTrace, 0);
    #ifdef MALLOC_INTERCEPT 
    IMG_AddInstrumentFunction(FlagHeap, 0);
    #endif

    #ifdef DONG_TRIGGER_SIM
    IMG_AddInstrumentFunction(TriggerSim, 0);
    #endif

    PIN_AddFiniFunction(Fini, 0);
    PIN_StartProgram();
    return 0;
}

/* ================================================================== */

namespace PinPthread
{

/* ------------------------------------------------------------------ */
/* Initalization & Clean Up                                           */
/* ------------------------------------------------------------------ */
PthreadSim* pthreadsim;
#ifdef MALLOC_INTERCEPT
int exp_heap_size;   //expected heap size
#endif
#ifdef DONG_TRIGGER_SIM 
bool start_sim_flag = false;
#endif

VOID Init(uint32_t argc, char** argv) 
{
    pthreadsim = new PthreadSim(argc, argv);

    #ifdef MALLOC_INTERCEPT
    char *char_heap_mem_size=NULL;

    char_heap_mem_size = getenv("HEAP_MEM_SIZE");
    if(char_heap_mem_size == NULL)
    {
      cout << "[Pthread]: HEAP_MEM_SIZE is not set. exp_heap_size is set as 1MByte by default" << endl;
      exp_heap_size = 1024*1024;  //1MByte by default
    }
    else
    {
      exp_heap_size = atol(char_heap_mem_size);
      cout << "[Pthread]: HEAP_MEM_SIZE is set as " << exp_heap_size << endl;
    } 
    #endif   //MALLOC_INTERCEPT
}

VOID Fini(INT32 code, VOID* v) 
{
    delete pthreadsim;
}

VOID ProcessMemIns(
    CONTEXT * context,
    ADDRINT ip,
    ADDRINT raddr, ADDRINT raddr2, UINT32 rlen,
    ADDRINT waddr, UINT32  wlen,
    BOOL    isbranch,
    BOOL    isbranchtaken,
    UINT32  category,
    UINT32  rr0,
    UINT32  rr1,
    UINT32  rr2,
    UINT32  rr3,
    UINT32  rw0,
    UINT32  rw1,
    UINT32  rw2,
    UINT32  rw3)
{ // for memory address and register index, '0' means invalid
  if (pthreadsim->first_instrs < pthreadsim->skip_first)
  {
    pthreadsim->first_instrs++;
    return;
  }
  else if (pthreadsim->first_instrs == pthreadsim->skip_first)
  {
    pthreadsim->first_instrs++;
    pthreadsim->initiate(context);
  }

  #ifdef DONG_TRIGGER_SIM
  if(start_sim_flag)
  #endif
  {
    pthreadsim->process_ins(
     context,
     ip,
     raddr, raddr2, rlen,
     waddr,         wlen,
     isbranch,
     isbranchtaken,
     category,
     rr0, rr1, rr2, rr3,
     rw0, rw1, rw2, rw3);
  }
}


VOID NewPthreadSim(CONTEXT* ctxt)
{
  pthreadsim->set_stack(ctxt);
}


/* ------------------------------------------------------------------ */
/* Instrumentation Routines                                           */
/* ------------------------------------------------------------------ */

VOID  DummyFunc(void*) {}  //dong moved this from the header file to here

VOID FlagImg(IMG img, VOID* v) 
{
  RTN rtn;
  rtn = RTN_FindByName(img, "__kmp_get_global_thread_id");
  if (rtn != RTN_Invalid()) 
  {
    RTN_Replace(rtn, (AFUNPTR)CallPthreadSelf);
    //RTN_ReplaceWithUninstrumentedRoutine(rtn, (AFUNPTR)CallPthreadSelf);
  }
  rtn = RTN_FindByName(img, "__kmp_check_stack_overlap");
  if (rtn != RTN_Invalid()) 
  {
    RTN_Replace(rtn, (AFUNPTR)DummyFunc);
    //RTN_ReplaceWithUninstrumentedRoutine(rtn, (AFUNPTR)DummyFunc);
  }
  rtn = RTN_FindByName(img, "mcsim_skip_instrs_begin");
  if (rtn != RTN_Invalid())
  {
    RTN_Replace(rtn, (AFUNPTR)CallMcSimSkipInstrsBegin);
  }
  rtn = RTN_FindByName(img, "mcsim_skip_instrs_end");
  if (rtn != RTN_Invalid())
  {
    RTN_Replace(rtn, (AFUNPTR)CallMcSimSkipInstrsEnd);
  }
  rtn = RTN_FindByName(img, "mcsim_spinning_begin");
  if (rtn != RTN_Invalid())
  {
    RTN_Replace(rtn, (AFUNPTR)CallMcSimSpinningBegin);
  }
  rtn = RTN_FindByName(img, "mcsim_spinning_end");
  if (rtn != RTN_Invalid())
  {
    RTN_Replace(rtn, (AFUNPTR)CallMcSimSpinningEnd);
  }
  rtn = RTN_FindByName(img, "__parsec_bench_begin");
  if (rtn != RTN_Invalid())
  {
    RTN_Replace(rtn, (AFUNPTR)CallMcSimSkipInstrsBegin);
  }
  rtn = RTN_FindByName(img, "__parsec_roi_begin");
  if (rtn != RTN_Invalid())
  {
    RTN_Replace(rtn, (AFUNPTR)CallMcSimSkipInstrsEnd);
  }
  rtn = RTN_FindByName(img, "__parsec_roi_end");
  if (rtn != RTN_Invalid())
  {
    RTN_Replace(rtn, (AFUNPTR)CallMcSimSkipInstrsBegin);
  }
  rtn = RTN_FindByName(img, "__parsec_bench_end");
  if (rtn != RTN_Invalid())
  {
    RTN_Replace(rtn, (AFUNPTR)CallMcSimSkipInstrsBegin);
  }

  rtn = RTN_FindByName(img, "pthread_attr_destroy");
  if (rtn != RTN_Invalid())
  {
    RTN_ReplaceSignature(rtn, (AFUNPTR)CallPthreadAttrDestroy,
        IARG_G_ARG0_CALLEE,
        IARG_RETURN_REGS, REG_GAX,
        IARG_END);
  }
  rtn = RTN_FindByName(img, "pthread_attr_getdetachstate");
  if (rtn != RTN_Invalid())
  {
    RTN_ReplaceSignature(rtn, (AFUNPTR)CallPthreadAttrGetdetachstate,
        IARG_G_ARG0_CALLEE,
        IARG_G_ARG1_CALLEE,
        IARG_RETURN_REGS, REG_GAX,
        IARG_END);
  }
  rtn = RTN_FindByName(img, "pthread_attr_getstackaddr");
  if (rtn != RTN_Invalid())
  {
    RTN_ReplaceSignature(rtn, (AFUNPTR)CallPthreadAttrGetstackaddr,
        IARG_G_ARG0_CALLEE,
        IARG_G_ARG1_CALLEE,
        IARG_RETURN_REGS, REG_GAX,
        IARG_END);
  }
  rtn = RTN_FindByName(img, "pthread_attr_getstacksize");
  if (rtn != RTN_Invalid())
  {
    RTN_ReplaceSignature(rtn, (AFUNPTR)CallPthreadAttrGetstacksize,
        IARG_G_ARG0_CALLEE,
        IARG_G_ARG1_CALLEE,
        IARG_RETURN_REGS, REG_GAX,
        IARG_END);
  }
  rtn = RTN_FindByName(img, "pthread_attr_getstack");
  if (rtn != RTN_Invalid())
  {
    RTN_ReplaceSignature(rtn, (AFUNPTR)CallPthreadAttrGetstack,
        IARG_G_ARG0_CALLEE,
        IARG_G_ARG1_CALLEE,
        IARG_G_ARG2_CALLEE,
        IARG_RETURN_REGS, REG_GAX,
        IARG_END);
  }
  rtn = RTN_FindByName(img, "pthread_attr_init");
  if (rtn != RTN_Invalid())
  {
    RTN_ReplaceSignature(rtn, (AFUNPTR)CallPthreadAttrInit,
        IARG_G_ARG0_CALLEE,
        IARG_RETURN_REGS, REG_GAX,
        IARG_END);
  }
  rtn = RTN_FindByName(img, "pthread_attr_setdetachstate");
  if (rtn != RTN_Invalid())
  {
    RTN_ReplaceSignature(rtn, (AFUNPTR)CallPthreadAttrSetdetachstate,
        IARG_G_ARG0_CALLEE,
        IARG_G_ARG1_CALLEE,
        IARG_RETURN_REGS, REG_GAX,
        IARG_END);
  }
  rtn = RTN_FindByName(img, "pthread_attr_setstackaddr");
  if (rtn != RTN_Invalid())
  {
    RTN_ReplaceSignature(rtn, (AFUNPTR)CallPthreadAttrSetstackaddr,
        IARG_G_ARG0_CALLEE,
        IARG_G_ARG1_CALLEE,
        IARG_G_ARG2_CALLEE,
        IARG_RETURN_REGS, REG_GAX,
        IARG_END);
  }
  rtn = RTN_FindByName(img, "pthread_attr_setstacksize");
  if (rtn != RTN_Invalid())
  {
    RTN_ReplaceSignature(rtn, (AFUNPTR)CallPthreadAttrSetstacksize,
        IARG_G_ARG0_CALLEE,
        IARG_G_ARG1_CALLEE,
        IARG_RETURN_REGS, REG_GAX,
        IARG_END);
  }
  rtn = RTN_FindByName(img, "pthread_attr_setstack");
  if (rtn != RTN_Invalid())
  {
    RTN_ReplaceSignature(rtn, (AFUNPTR)CallPthreadAttrSetstack,
        IARG_G_ARG0_CALLEE,
        IARG_G_ARG1_CALLEE,
        IARG_G_ARG2_CALLEE,
        IARG_RETURN_REGS, REG_GAX,
        IARG_END);
  }
  rtn = RTN_FindByName(img, "pthread_cancel");
  if (rtn != RTN_Invalid())
  {
    RTN_ReplaceSignature(rtn, (AFUNPTR)CallPthreadCancel,
        IARG_G_ARG0_CALLEE,
        IARG_RETURN_REGS, REG_GAX,
        IARG_END);
  }
  rtn = RTN_FindByName(img, "pthread_cleanup_pop");
  if (rtn != RTN_Invalid())
  {
    RTN_ReplaceSignature(rtn, (AFUNPTR)CallPthreadCleanupPop,
        IARG_CONTEXT,
        IARG_G_ARG0_CALLEE,
        IARG_END);
  }
  rtn = RTN_FindByName(img, "pthread_cleanup_push");
  if (rtn != RTN_Invalid())
  {
    RTN_ReplaceSignature(rtn, (AFUNPTR)CallPthreadCleanupPush,
        IARG_G_ARG0_CALLEE,
        IARG_G_ARG1_CALLEE,
        IARG_END);
  }
  rtn = RTN_FindByName(img, "pthread_condattr_destroy");
  if (rtn != RTN_Invalid())
  {
    RTN_ReplaceSignature(rtn, (AFUNPTR)CallPthreadCondattrDestroy,
        IARG_G_ARG0_CALLEE,
        IARG_RETURN_REGS, REG_GAX,
        IARG_END);
  }
  rtn = RTN_FindByName(img, "pthread_condattr_init");
  if (rtn != RTN_Invalid())
  {
    RTN_ReplaceSignature(rtn, (AFUNPTR)CallPthreadCondattrInit,
        IARG_G_ARG0_CALLEE,
        IARG_G_ARG1_CALLEE,
        IARG_RETURN_REGS, REG_GAX,
        IARG_END);
  }
  rtn = RTN_FindByName(img, "pthread_cond_broadcast");
  if (rtn != RTN_Invalid())
  {
    RTN_ReplaceSignature(rtn, (AFUNPTR)CallPthreadCondBroadcast,
        IARG_G_ARG0_CALLEE,
        IARG_RETURN_REGS, REG_GAX,
        IARG_END);
  }
  rtn = RTN_FindByName(img, "pthread_cond_destroy");
  if (rtn != RTN_Invalid())
  {
    RTN_ReplaceSignature(rtn, (AFUNPTR)CallPthreadCondDestroy,
        IARG_G_ARG0_CALLEE,
        IARG_RETURN_REGS, REG_GAX,
        IARG_END);
  }
  rtn = RTN_FindByName(img, "pthread_cond_init");
  if (rtn != RTN_Invalid())
  {
    RTN_ReplaceSignature(rtn, (AFUNPTR)CallPthreadCondInit,
        IARG_G_ARG0_CALLEE,
        IARG_G_ARG1_CALLEE,
        IARG_RETURN_REGS, REG_GAX,
        IARG_END);
  }
  rtn = RTN_FindByName(img, "pthread_cond_signal");
  if (rtn != RTN_Invalid())
  {
    RTN_ReplaceSignature(rtn, (AFUNPTR)CallPthreadCondSignal,
        IARG_G_ARG0_CALLEE,
        IARG_RETURN_REGS, REG_GAX,
        IARG_END);
  }
  rtn = RTN_FindByName(img, "pthread_cond_timedwait");
  if (rtn != RTN_Invalid())
  {
    RTN_ReplaceSignature(rtn, (AFUNPTR)CallPthreadCondTimedwait,
        IARG_CONTEXT,
        IARG_G_ARG0_CALLEE,
        IARG_G_ARG1_CALLEE,
        IARG_G_ARG2_CALLEE,
        IARG_RETURN_REGS, REG_GAX,
        IARG_END);
  }
  rtn = RTN_FindByName(img, "pthread_cond_wait");
  if (rtn != RTN_Invalid())
  {
    RTN_ReplaceSignature(rtn, (AFUNPTR)CallPthreadCondWait,
        IARG_CONTEXT,
        IARG_G_ARG0_CALLEE,
        IARG_G_ARG1_CALLEE,
        IARG_RETURN_REGS, REG_GAX,
        IARG_END);
  }
  rtn = RTN_FindByName(img, "pthread_create");
  if (rtn != RTN_Invalid())
  {
    RTN_ReplaceSignature(rtn, (AFUNPTR)CallPthreadCreate,
        IARG_CONTEXT,
        IARG_G_ARG0_CALLEE,
        IARG_G_ARG1_CALLEE,
        IARG_G_ARG2_CALLEE,
        IARG_G_ARG3_CALLEE,
        IARG_END);
  }
  rtn = RTN_FindByName(img, "pthread_detach");
  if (rtn != RTN_Invalid())
  {
    RTN_ReplaceSignature(rtn, (AFUNPTR)CallPthreadDetach,
        IARG_G_ARG0_CALLEE,
        IARG_RETURN_REGS, REG_GAX,
        IARG_END);
  }
  rtn = RTN_FindByName(img, "pthread_equal");
  if (rtn != RTN_Invalid())
  {
    RTN_ReplaceSignature(rtn, (AFUNPTR)CallPthreadEqual,
        IARG_G_ARG0_CALLEE,
        IARG_G_ARG1_CALLEE,
        IARG_RETURN_REGS, REG_GAX,
        IARG_END);
  }
  rtn = RTN_FindByName(img, "pthread_exit");
  if (rtn != RTN_Invalid())
  {
    RTN_ReplaceSignature(rtn, (AFUNPTR)CallPthreadExit,
        IARG_CONTEXT,
        IARG_G_ARG0_CALLEE,
        IARG_END);
  }
  rtn = RTN_FindByName(img, "pthread_getattr");
  if (rtn != RTN_Invalid())
  {
    RTN_ReplaceSignature(rtn, (AFUNPTR)CallPthreadGetattr,
        IARG_G_ARG0_CALLEE,
        IARG_G_ARG1_CALLEE,
        IARG_RETURN_REGS, REG_GAX,
        IARG_END);
  }
  rtn = RTN_FindByName(img, "pthread_getspecific");
  if (rtn != RTN_Invalid())
  {
    RTN_ReplaceSignature(rtn, (AFUNPTR)CallPthreadGetspecific,
        IARG_G_ARG0_CALLEE,
        IARG_RETURN_REGS, REG_GAX,
        IARG_END);
  }
  rtn = RTN_FindByName(img, "pthread_join");
  if (rtn != RTN_Invalid())
  {
    RTN_ReplaceSignature(rtn, (AFUNPTR)CallPthreadJoin,
        IARG_CONTEXT,
        IARG_G_ARG0_CALLEE,    
        IARG_G_ARG1_CALLEE,                       
        IARG_END);
  }
  rtn = RTN_FindByName(img, "pthread_key_create");
  if (rtn != RTN_Invalid())
  {
    RTN_ReplaceSignature(rtn, (AFUNPTR)CallPthreadKeyCreate,
        IARG_G_ARG0_CALLEE,
        IARG_G_ARG1_CALLEE, 
        IARG_RETURN_REGS, REG_GAX,
        IARG_END);
  }
  rtn = RTN_FindByName(img, "pthread_key_delete");
  if (rtn != RTN_Invalid())
  {
    RTN_ReplaceSignature(rtn, (AFUNPTR)CallPthreadKeyDelete,
        IARG_G_ARG0_CALLEE,
        IARG_RETURN_REGS, REG_GAX,
        IARG_END);
  }
  rtn = RTN_FindByName(img, "pthread_kill");
  if (rtn != RTN_Invalid())
  {
    RTN_ReplaceSignature(rtn, (AFUNPTR)CallPthreadKill,
        IARG_G_ARG0_CALLEE,
        IARG_G_ARG1_CALLEE,
        IARG_RETURN_REGS, REG_GAX,
        IARG_END);
  }
  rtn = RTN_FindByName(img, "pthread_mutexattr_destroy");
  if (rtn != RTN_Invalid())
  {
    RTN_ReplaceSignature(rtn, (AFUNPTR)CallPthreadMutexattrDestroy,
        IARG_G_ARG0_CALLEE,
        IARG_RETURN_REGS, REG_GAX,
        IARG_END);
  }
  rtn = RTN_FindByName(img, "pthread_mutexattr_gettype");
  if (rtn != RTN_Invalid())
  {
    RTN_ReplaceSignature(rtn, (AFUNPTR)CallPthreadMutexattrGetkind,
        IARG_G_ARG0_CALLEE,
        IARG_G_ARG1_CALLEE,
        IARG_RETURN_REGS, REG_GAX,
        IARG_END);
  }
  rtn = RTN_FindByName(img, "pthread_mutexattr_getkind");
  if (rtn != RTN_Invalid())
  {
    RTN_ReplaceSignature(rtn, (AFUNPTR)CallPthreadMutexattrGetkind,
        IARG_G_ARG0_CALLEE,
        IARG_G_ARG1_CALLEE,
        IARG_RETURN_REGS, REG_GAX,
        IARG_END);
  }
  rtn = RTN_FindByName(img, "pthread_mutexattr_init");
  if (rtn != RTN_Invalid())
  {
    RTN_ReplaceSignature(rtn, (AFUNPTR)CallPthreadMutexattrInit,
        IARG_G_ARG0_CALLEE,
        IARG_RETURN_REGS, REG_GAX,
        IARG_END);
  }
  rtn = RTN_FindByName(img, "pthread_mutexattr_settype");
  if (rtn != RTN_Invalid())
  {
    RTN_ReplaceSignature(rtn, (AFUNPTR)CallPthreadMutexattrSetkind,
        IARG_G_ARG0_CALLEE,
        IARG_G_ARG1_CALLEE,
        IARG_RETURN_REGS, REG_GAX,
        IARG_END);
  }
  rtn = RTN_FindByName(img, "pthread_mutexattr_setkind");
  if (rtn != RTN_Invalid())
  {
    RTN_ReplaceSignature(rtn, (AFUNPTR)CallPthreadMutexattrSetkind,
        IARG_G_ARG0_CALLEE,
        IARG_G_ARG1_CALLEE,
        IARG_RETURN_REGS, REG_GAX,
        IARG_END);
  }
  rtn = RTN_FindByName(img, "pthread_mutex_destroy");
  if (rtn != RTN_Invalid())
  {
    RTN_ReplaceSignature(rtn, (AFUNPTR)CallPthreadMutexDestroy,
        IARG_G_ARG0_CALLEE,
        IARG_RETURN_REGS, REG_GAX,
        IARG_END);
  }
  rtn = RTN_FindByName(img, "pthread_mutex_init");
  if (rtn != RTN_Invalid())
  {
    RTN_ReplaceSignature(rtn, (AFUNPTR)CallPthreadMutexInit,
        IARG_G_ARG0_CALLEE,
        IARG_G_ARG1_CALLEE,
        IARG_RETURN_REGS, REG_GAX,
        IARG_END);
  }
  rtn = RTN_FindByName(img, "pthread_mutex_lock");
  if (rtn != RTN_Invalid())
  {
    RTN_ReplaceSignature(rtn, (AFUNPTR)CallPthreadMutexLock,
        IARG_CONTEXT,
        IARG_G_ARG0_CALLEE,
        IARG_RETURN_REGS, REG_GAX,
        IARG_END);
  }
  rtn = RTN_FindByName(img, "pthread_mutex_trylock");
  if (rtn != RTN_Invalid())
  {
    RTN_ReplaceSignature(rtn, (AFUNPTR)CallPthreadMutexTrylock,
        IARG_G_ARG0_CALLEE,
        IARG_RETURN_REGS, REG_GAX,
        IARG_END);
  }
  rtn = RTN_FindByName(img, "pthread_mutex_unlock");
  if (rtn != RTN_Invalid())
  {
    RTN_ReplaceSignature(rtn, (AFUNPTR)CallPthreadMutexUnlock,
        IARG_G_ARG0_CALLEE,
        IARG_RETURN_REGS, REG_GAX,
        IARG_END);
  }
  rtn = RTN_FindByName(img, "pthread_once");
  if (rtn != RTN_Invalid())
  {
    RTN_ReplaceSignature(rtn, (AFUNPTR)CallPthreadOnce,
        IARG_CONTEXT,
        IARG_G_ARG0_CALLEE,
        IARG_G_ARG1_CALLEE,
        IARG_END);
  }
  rtn = RTN_FindByName(img, "pthread_self");
  if (rtn != RTN_Invalid())
  {
    RTN_ReplaceSignature(rtn, (AFUNPTR)CallPthreadSelf,
        IARG_RETURN_REGS, REG_GAX,
        IARG_END);
  }
  rtn = RTN_FindByName(img, "pthread_setcancelstate");
  if (rtn != RTN_Invalid())
  {
    RTN_ReplaceSignature(rtn, (AFUNPTR)CallPthreadSetcancelstate,
        IARG_G_ARG0_CALLEE,
        IARG_G_ARG1_CALLEE,
        IARG_RETURN_REGS, REG_GAX,
        IARG_END);
  }
  rtn = RTN_FindByName(img, "pthread_setcanceltype");
  if (rtn != RTN_Invalid())
  {
    RTN_ReplaceSignature(rtn, (AFUNPTR)CallPthreadSetcanceltype,
        IARG_G_ARG0_CALLEE,
        IARG_G_ARG1_CALLEE,
        IARG_RETURN_REGS, REG_GAX,
        IARG_END);
  }
  rtn = RTN_FindByName(img, "pthread_setspecific");
  if (rtn != RTN_Invalid())
  {
    RTN_ReplaceSignature(rtn, (AFUNPTR)CallPthreadSetspecific,
        IARG_G_ARG0_CALLEE,
        IARG_G_ARG1_CALLEE,
        IARG_RETURN_REGS, REG_GAX,
        IARG_END);
  }
  rtn = RTN_FindByName(img, "libc_tsd_set");
  if (rtn != RTN_Invalid())
  {
    RTN_ReplaceSignature(rtn, (AFUNPTR)CallPthreadSetspecific,
        IARG_G_ARG0_CALLEE,
        IARG_G_ARG1_CALLEE,
        IARG_RETURN_REGS, REG_GAX,
        IARG_END);
  }
  rtn = RTN_FindByName(img, "pthread_testcancel");
  if (rtn != RTN_Invalid())
  {
    RTN_ReplaceSignature(rtn, (AFUNPTR)CallPthreadTestcancel,
        IARG_CONTEXT,
        IARG_END);
  }
  rtn = RTN_FindByName(img, "pthread_barrier_init");
  if (rtn != RTN_Invalid())
  {
    RTN_ReplaceSignature(rtn, (AFUNPTR)CallPthreadBarrierInit,
        IARG_G_ARG0_CALLEE,
        IARG_G_ARG1_CALLEE,
        IARG_G_ARG2_CALLEE,
        IARG_RETURN_REGS, REG_GAX,
        IARG_END);
  }
  rtn = RTN_FindByName(img, "pthread_barrier_destroy");
  if (rtn != RTN_Invalid())
  {
    RTN_ReplaceSignature(rtn, (AFUNPTR)CallPthreadBarrierDestroy,
        IARG_G_ARG0_CALLEE,
        IARG_RETURN_REGS, REG_GAX,
        IARG_END);
  }
  rtn = RTN_FindByName(img, "pthread_barrier_wait");
  if (rtn != RTN_Invalid())
  {
    RTN_ReplaceSignature(rtn, (AFUNPTR)CallPthreadBarrierWait,
        IARG_CONTEXT,
        IARG_G_ARG0_CALLEE,
        IARG_RETURN_REGS, REG_GAX,
        IARG_END);
  }
  rtn = RTN_FindByName(img, "pthread_barrierattr_init");
  if (rtn != RTN_Invalid())
  {
    RTN_ReplaceSignature(rtn, (AFUNPTR)CallPthreadBarrierattrInit,
        IARG_G_ARG0_CALLEE,
        IARG_RETURN_REGS, REG_GAX,
        IARG_END);
  }
  rtn = RTN_FindByName(img, "pthread_barrierattr_destroy");
  if (rtn != RTN_Invalid())
  {
    RTN_ReplaceSignature(rtn, (AFUNPTR)CallPthreadBarrierattrDestory,
        IARG_G_ARG0_CALLEE,
        IARG_RETURN_REGS, REG_GAX,
        IARG_END);
  }
  rtn = RTN_FindByName(img, "pthread_barrierattr_getpshared");
  if (rtn != RTN_Invalid())
  {
    RTN_ReplaceSignature(rtn, (AFUNPTR)CallPthreadBarrierattrGetpshared,
        IARG_G_ARG0_CALLEE,
        IARG_G_ARG1_CALLEE,
        IARG_RETURN_REGS, REG_GAX,
        IARG_END);
  }
  rtn = RTN_FindByName(img, "pthread_barrierattr_setpshared");
  if (rtn != RTN_Invalid())
  {
    RTN_ReplaceSignature(rtn, (AFUNPTR)CallPthreadBarrierattrSetpshared,
        IARG_G_ARG0_CALLEE,
        IARG_G_ARG1_CALLEE,
        IARG_RETURN_REGS, REG_GAX,
        IARG_END);
  }
}


VOID FlagRtn(RTN rtn, VOID* v) 
{
  RTN_Open(rtn);
  string * rtn_name = new string(RTN_Name(rtn));
#if VERYVERBOSE
  RTN_InsertCall(rtn, IPOINT_BEFORE, (AFUNPTR)PrintRtnName,
      IARG_PTR, rtn_name,
      IARG_END);
#endif

  if (rtn_name->find("main") != string::npos)
  {
    RTN_InsertCall(rtn, IPOINT_BEFORE, (AFUNPTR)NewPthreadSim,
        IARG_CONTEXT,
        IARG_END);
  }
  else if (((rtn_name->find("__kmp") != string::npos) &&
        (rtn_name->find("yield") != string::npos)) ||
      (rtn_name->find("__sleep") != string::npos) ||
      (rtn_name->find("__kmp_wait_sleep") != string::npos))
  {
    RTN_InsertCall(rtn, IPOINT_BEFORE, (AFUNPTR)DoContextSwitch,
        IARG_CONTEXT,
        IARG_END);
  }
  /*else if ((rtn_name->find("__pthread_return_void") != string::npos) ||
    (rtn_name->find("pthread_mutex_t") != string::npos) ||
    (rtn_name->find("pthread_atfork") != string::npos))
  {
  }*/
  else if ((rtn_name->find("pthread") != string::npos) ||
      (rtn_name->find("sigwait") != string::npos) ||
      (rtn_name->find("tsd") != string::npos) ||
      ((rtn_name->find("fork") != string::npos) &&
       (rtn_name->find("__kmp") == string::npos)))
  {
    RTN_InsertCall(rtn, IPOINT_BEFORE, (AFUNPTR)WarnNYI,
        IARG_PTR, rtn_name,
        IARG_INST_PTR,
        IARG_END);
  }
  RTN_Close(rtn);
}


VOID FlagTrace(TRACE trace, VOID* v) 
{
  if (TRACE_Address(trace) == (ADDRINT)pthread_exit) 
  {
    TRACE_InsertCall(trace, IPOINT_BEFORE, (AFUNPTR)CallPthreadExit,
        IARG_CONTEXT,
        IARG_G_ARG0_CALLEE,
        IARG_END);
  }
  else if (!INS_IsAddedForFunctionReplacement(BBL_InsHead(TRACE_BblHead(trace)))) 
  {
    for (BBL bbl = TRACE_BblHead(trace); BBL_Valid(bbl); bbl = BBL_Next(bbl)) 
    {
      for (INS ins = BBL_InsHead(bbl); INS_Valid(ins); ins = INS_Next(ins)) 
      {
        if (INS_IsCall(ins) && !INS_IsDirectBranchOrCall(ins))            // indirect call
        {
          INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)ProcessCall,
              IARG_BRANCH_TARGET_ADDR,
              IARG_G_ARG0_CALLER,
              IARG_G_ARG1_CALLER,
              IARG_BOOL, false,
              IARG_END);
        }
        else if (INS_IsDirectBranchOrCall(ins))                           // tail call or conventional call
        {
          ADDRINT target = INS_DirectBranchOrCallTargetAddress(ins);
          RTN src_rtn = INS_Rtn(ins);
          RTN dest_rtn = RTN_FindByAddress(target);
          if (INS_IsCall(ins) || (src_rtn != dest_rtn)) 
          {
            BOOL tailcall = !INS_IsCall(ins);
            INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)ProcessCall,
                IARG_ADDRINT, target,
                IARG_G_ARG0_CALLER,
                IARG_G_ARG1_CALLER,
                IARG_BOOL, tailcall,
                IARG_END);
          }
        }
        else if (INS_IsRet(ins))                                          // return
        {
          RTN rtn = INS_Rtn(ins);
          if (RTN_Valid(rtn) && (RTN_Name(rtn) != "_dl_runtime_resolve")) 
          {
            INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)ProcessReturn,
                IARG_PTR, new string(RTN_Name(rtn)),
                IARG_END);
          }
        }

        if (((INS_Address(ins) - (ADDRINT)StartThreadFunc) < 0) ||
            ((INS_Address(ins) - (ADDRINT)StartThreadFunc) > 8 * sizeof(ADDRINT)))
        {
          bool is_mem_wr   = INS_IsMemoryWrite(ins);
          bool is_mem_rd   = INS_IsMemoryRead(ins);
          bool has_mem_rd2 = INS_HasMemoryRead2(ins);

          if (is_mem_wr && is_mem_rd && has_mem_rd2) 
          {
            INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)ProcessMemIns,
                IARG_CONTEXT,
                IARG_INST_PTR,
                IARG_MEMORYREAD_EA,
                IARG_MEMORYREAD2_EA,
                IARG_MEMORYREAD_SIZE,
                IARG_MEMORYWRITE_EA,
                IARG_MEMORYWRITE_SIZE,
                IARG_BOOL, INS_IsBranchOrCall(ins),
                IARG_BRANCH_TAKEN,
                IARG_UINT32,  INS_Category(ins),
                IARG_UINT32, INS_RegR(ins, 0),
                IARG_UINT32, INS_RegR(ins, 1),
                IARG_UINT32, INS_RegR(ins, 2),
                IARG_UINT32, INS_RegR(ins, 3),
                IARG_UINT32, INS_RegW(ins, 0),
                IARG_UINT32, INS_RegW(ins, 1),
                IARG_UINT32, INS_RegW(ins, 2),
                IARG_UINT32, INS_RegW(ins, 3),
                IARG_END);
          }
          else if (is_mem_wr && is_mem_rd && !has_mem_rd2) 
          {
            INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)ProcessMemIns,
                IARG_CONTEXT,
                IARG_INST_PTR,
                IARG_MEMORYREAD_EA,
                IARG_ADDRINT, (ADDRINT)0,
                IARG_MEMORYREAD_SIZE,
                IARG_MEMORYWRITE_EA,
                IARG_MEMORYWRITE_SIZE,
                IARG_BOOL, INS_IsBranchOrCall(ins),
                IARG_BRANCH_TAKEN,
                IARG_UINT32,  INS_Category(ins),
                IARG_UINT32, INS_RegR(ins, 0),
                IARG_UINT32, INS_RegR(ins, 1),
                IARG_UINT32, INS_RegR(ins, 2),
                IARG_UINT32, INS_RegR(ins, 3),
                IARG_UINT32, INS_RegW(ins, 0),
                IARG_UINT32, INS_RegW(ins, 1),
                IARG_UINT32, INS_RegW(ins, 2),
                IARG_UINT32, INS_RegW(ins, 3),
                IARG_END);
          }
          else if (is_mem_wr && !is_mem_rd) 
          {
            INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)ProcessMemIns,
                IARG_CONTEXT,
                IARG_INST_PTR,
                IARG_ADDRINT, (ADDRINT)0,
                IARG_ADDRINT, (ADDRINT)0,
                IARG_UINT32, 0,
                IARG_MEMORYWRITE_EA,
                IARG_MEMORYWRITE_SIZE,
                IARG_BOOL, INS_IsBranchOrCall(ins),
                IARG_BRANCH_TAKEN,
                IARG_UINT32, INS_Category(ins),
                IARG_UINT32, INS_RegR(ins, 0),
                IARG_UINT32, INS_RegR(ins, 1),
                IARG_UINT32, INS_RegR(ins, 2),
                IARG_UINT32, INS_RegR(ins, 3),
                IARG_UINT32, INS_RegW(ins, 0),
                IARG_UINT32, INS_RegW(ins, 1),
                IARG_UINT32, INS_RegW(ins, 2),
                IARG_UINT32, INS_RegW(ins, 3),
                IARG_END);
          }
          else if (!is_mem_wr && is_mem_rd && has_mem_rd2)
          {
            INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)ProcessMemIns,
                IARG_CONTEXT,
                IARG_INST_PTR,
                IARG_MEMORYREAD_EA,
                IARG_MEMORYREAD2_EA,
                IARG_MEMORYREAD_SIZE,
                IARG_ADDRINT, (ADDRINT)0,
                IARG_UINT32, 0,
                IARG_BOOL, INS_IsBranchOrCall(ins),
                IARG_BRANCH_TAKEN,
                IARG_UINT32, INS_Category(ins),
                IARG_UINT32, INS_RegR(ins, 0),
                IARG_UINT32, INS_RegR(ins, 1),
                IARG_UINT32, INS_RegR(ins, 2),
                IARG_UINT32, INS_RegR(ins, 3),
                IARG_UINT32, INS_RegW(ins, 0),
                IARG_UINT32, INS_RegW(ins, 1),
                IARG_UINT32, INS_RegW(ins, 2),
                IARG_UINT32, INS_RegW(ins, 3),
                IARG_END);
          }
          else if (!is_mem_wr && is_mem_rd && !has_mem_rd2) 
          {
            INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)ProcessMemIns,
                IARG_CONTEXT,
                IARG_INST_PTR,
                IARG_MEMORYREAD_EA,
                IARG_ADDRINT, (ADDRINT)0,
                IARG_MEMORYREAD_SIZE,
                IARG_ADDRINT, (ADDRINT)0,
                IARG_UINT32, 0,
                IARG_BOOL, INS_IsBranchOrCall(ins),
                IARG_BRANCH_TAKEN,
                IARG_UINT32, INS_Category(ins),
                IARG_UINT32, INS_RegR(ins, 0),
                IARG_UINT32, INS_RegR(ins, 1),
                IARG_UINT32, INS_RegR(ins, 2),
                IARG_UINT32, INS_RegR(ins, 3),
                IARG_UINT32, INS_RegW(ins, 0),
                IARG_UINT32, INS_RegW(ins, 1),
                IARG_UINT32, INS_RegW(ins, 2),
                IARG_UINT32, INS_RegW(ins, 3),
                IARG_END);
          }
          else
          {
            INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)ProcessMemIns,
                IARG_CONTEXT,
                IARG_INST_PTR,
                IARG_ADDRINT, (ADDRINT)0,
                IARG_ADDRINT, (ADDRINT)0,
                IARG_UINT32,  0,
                IARG_ADDRINT, (ADDRINT)0,
                IARG_UINT32,  0,
                IARG_BOOL, INS_IsBranchOrCall(ins),
                IARG_BRANCH_TAKEN,
                IARG_UINT32, INS_Category(ins),
                IARG_UINT32, INS_RegR(ins, 0),
                IARG_UINT32, INS_RegR(ins, 1),
                IARG_UINT32, INS_RegR(ins, 2),
                IARG_UINT32, INS_RegR(ins, 3),
                IARG_UINT32, INS_RegW(ins, 0),
                IARG_UINT32, INS_RegW(ins, 1),
                IARG_UINT32, INS_RegW(ins, 2),
                IARG_UINT32, INS_RegW(ins, 3),
                IARG_END);
          }
        }
      }
    }
  }
}

#ifdef DONG_TRIGGER_SIM
VOID TriggerSim(IMG img, void *v)
{
        RTN startsimRtn = RTN_FindByName(img, "dong_start_sim");
        if (RTN_Valid(startsimRtn))
        {
                RTN_Open(startsimRtn);

                // Instrument malloc() to print the input argument value and the return value.
                RTN_InsertCall(startsimRtn, IPOINT_BEFORE, (AFUNPTR)startsim,
                                IARG_END);
                RTN_Close(startsimRtn);
        }

        RTN endsimRtn = RTN_FindByName(img, "dong_end_sim");
        if (RTN_Valid(endsimRtn))
        {
                RTN_Open(endsimRtn);

                // Instrument malloc() to print the input argument value and the return value.
                RTN_InsertCall(endsimRtn, IPOINT_BEFORE, (AFUNPTR)endsim,
                                IARG_END);
                RTN_Close(endsimRtn);
        }
}
#endif

#ifdef MALLOC_INTERCEPT

#define MALLOC "malloc"
#define MALLOC_AFTER "malloc"
#define REALLOC "realloc"
#define REALLOC_AFTER "realloc"
#define CALLOC "calloc"
#define CALLOC_AFTER "calloc"
#define FREE "free"
#define HPLMALLOC "dong_malloc_intercept_assist_for_HPL" //specific assist for HPL

VOID FlagHeap(IMG img, void *v)
{
        RTN begmallocinterRtn = RTN_FindByName(img, "beg_malloc_intercept_for_pin");
        if (RTN_Valid(begmallocinterRtn))
        {
                RTN_Open(begmallocinterRtn);

                // Instrument malloc() to print the input argument value and the return value.
                RTN_InsertCall(begmallocinterRtn, IPOINT_BEFORE, (AFUNPTR)InterceptMalloc_beg,
                                IARG_END);
                RTN_Close(begmallocinterRtn);
        }

        RTN endmallocinterRtn = RTN_FindByName(img, "end_malloc_intercept_for_pin");
        if (RTN_Valid(endmallocinterRtn))
        {
                RTN_Open(endmallocinterRtn);

                // Instrument malloc() to print the input argument value and the return value.
                RTN_InsertCall(endmallocinterRtn, IPOINT_BEFORE, (AFUNPTR)InterceptMalloc_end,
                                IARG_END);
                RTN_Close(endmallocinterRtn);
        }

        RTN mallocRtn = RTN_FindByName(img, MALLOC);
        if (RTN_Valid(mallocRtn))
        {
                RTN_Open(mallocRtn);

                // Instrument malloc() to print the input argument value and the return value.
                RTN_InsertCall(mallocRtn, IPOINT_BEFORE, (AFUNPTR)MallocBefore,
                                IARG_ADDRINT, MALLOC,
                                IARG_FUNCARG_ENTRYPOINT_VALUE, 0,
                                IARG_RETURN_IP,
                                IARG_END);
                RTN_Close(mallocRtn);
        }
        RTN mallocaftRtn = RTN_FindByName(img, MALLOC_AFTER);
        if (RTN_Valid(mallocaftRtn))
        {
                RTN_Open(mallocaftRtn);
                RTN_InsertCall(mallocaftRtn, IPOINT_AFTER, (AFUNPTR)MallocAfter,
                                IARG_ADDRINT, MALLOC_AFTER,
                                IARG_FUNCRET_EXITPOINT_VALUE,
                                IARG_RETURN_IP,
                                IARG_END);
                RTN_Close(mallocaftRtn);
        }


        RTN reallocRtn = RTN_FindByName(img, REALLOC);
        if (RTN_Valid(reallocRtn))
        {
                RTN_Open(reallocRtn);

                // Instrument malloc() to print the input argument value and the return value.
                RTN_InsertCall(reallocRtn, IPOINT_BEFORE, (AFUNPTR)ReallocBefore,
                                IARG_ADDRINT, REALLOC,
                                IARG_FUNCARG_ENTRYPOINT_VALUE, 0,
                                IARG_FUNCARG_ENTRYPOINT_VALUE, 1,
                                IARG_RETURN_IP,
                                IARG_END);
                RTN_Close(reallocRtn);
        }

        RTN reallocaftRtn = RTN_FindByName(img, REALLOC_AFTER);
        if (RTN_Valid(reallocaftRtn))
        {
                RTN_Open(reallocaftRtn);
                RTN_InsertCall(reallocaftRtn, IPOINT_AFTER, (AFUNPTR)MallocAfter,
                                IARG_ADDRINT, REALLOC_AFTER,
                                IARG_FUNCRET_EXITPOINT_VALUE,
                                IARG_RETURN_IP,
                                IARG_END);
                RTN_Close(reallocaftRtn);
        }

        RTN callocRtn = RTN_FindByName(img, CALLOC);
        if (RTN_Valid(callocRtn))
        {
                RTN_Open(callocRtn);
                RTN_InsertCall(callocRtn, IPOINT_BEFORE, (AFUNPTR)CallocBefore,
                                IARG_ADDRINT, CALLOC,
                                IARG_FUNCARG_ENTRYPOINT_VALUE, 0,
                                IARG_FUNCARG_ENTRYPOINT_VALUE, 1,
                                IARG_RETURN_IP,
                                IARG_END);
                RTN_Close(callocRtn);
        }

        RTN callocaftRtn = RTN_FindByName(img, CALLOC_AFTER);
        if (RTN_Valid(callocaftRtn))
        {
                RTN_Open(callocaftRtn);
                RTN_InsertCall(callocaftRtn, IPOINT_AFTER, (AFUNPTR)MallocAfter,
                                IARG_ADDRINT, CALLOC_AFTER,
                                IARG_FUNCRET_EXITPOINT_VALUE,
                                IARG_RETURN_IP,
                                IARG_END);
                RTN_Close(callocaftRtn);
        }

        #ifdef APP_HPL
        RTN HPL_malloc_assistRtn = RTN_FindByName(img, HPLMALLOC);
        if (RTN_Valid(HPL_malloc_assistRtn))
        {
                RTN_Open(HPL_malloc_assistRtn);
                RTN_InsertCall(HPL_malloc_assistRtn, IPOINT_BEFORE, (AFUNPTR)HPL_malloc_assist,
                                //IARG_FUNCARG_ENTRYPOINT_VALUE, 0,
                                IARG_FUNCARG_ENTRYPOINT_VALUE, 0,
                                IARG_FUNCARG_ENTRYPOINT_VALUE, 1,
                                IARG_END);
                RTN_Close(HPL_malloc_assistRtn);
        }
        #endif

        RTN freeRtn = RTN_FindByName(img, FREE);
        if (RTN_Valid(freeRtn))
        {
                RTN_Open(freeRtn);
                // Instrument free() to print the input argument value.
                RTN_InsertCall(freeRtn, IPOINT_BEFORE, (AFUNPTR)FreeBefore,
                                IARG_ADDRINT, FREE,
                                IARG_FUNCARG_ENTRYPOINT_VALUE, 0,
                                IARG_END);
                RTN_Close(freeRtn);
        }
}
#endif    //MALLOC_INTERCEPT
 

/* ------------------------------------------------------------------ */
/* Pthread Hooks                                                      */
/* ------------------------------------------------------------------ */

int CallPthreadAttrDestroy(ADDRINT _attr) 
{
  return PthreadAttr::_pthread_attr_destroy((pthread_attr_t*)_attr);
}

int CallPthreadAttrGetdetachstate(ADDRINT _attr, ADDRINT _detachstate) 
{
  return PthreadAttr::_pthread_attr_getdetachstate((pthread_attr_t*)_attr, (int*)_detachstate);
}

int CallPthreadAttrGetstack(ADDRINT _attr, ADDRINT _stackaddr, ADDRINT _stacksize) 
{
  return PthreadAttr::_pthread_attr_getstack((pthread_attr_t*)_attr,
      (void**)_stackaddr, (size_t*)_stacksize);
}

int CallPthreadAttrGetstackaddr(ADDRINT _attr, ADDRINT _stackaddr) 
{
  return PthreadAttr::_pthread_attr_getstackaddr((pthread_attr_t*)_attr, (void**)_stackaddr);
}

int CallPthreadAttrGetstacksize(ADDRINT _attr, ADDRINT _stacksize) 
{
  return PthreadAttr::_pthread_attr_getstacksize((pthread_attr_t*)_attr, (size_t*)_stacksize);
}

int CallPthreadAttrInit(ADDRINT _attr) 
{
  return PthreadAttr::_pthread_attr_init((pthread_attr_t*)_attr);
}

int CallPthreadAttrSetdetachstate(ADDRINT _attr, ADDRINT _detachstate) 
{
  return PthreadAttr::_pthread_attr_setdetachstate((pthread_attr_t*)_attr, (int)_detachstate);
}

int CallPthreadAttrSetstack(ADDRINT _attr, ADDRINT _stackaddr, ADDRINT _stacksize) 
{
  return PthreadAttr::_pthread_attr_setstack((pthread_attr_t*)_attr,
      (void*)_stackaddr, (size_t)_stacksize);
}

int CallPthreadAttrSetstackaddr(ADDRINT _attr, ADDRINT _stackaddr) 
{
  return PthreadAttr::_pthread_attr_setstackaddr((pthread_attr_t*)_attr, (void*)_stackaddr);
}

int CallPthreadAttrSetstacksize(ADDRINT _attr, ADDRINT _stacksize) 
{
  return PthreadAttr::_pthread_attr_setstacksize((pthread_attr_t*)_attr, (size_t)_stacksize);
}

int CallPthreadCancel(ADDRINT _thread) 
{
  return pthreadsim->pthread_cancel((pthread_t)_thread);
}

VOID CallPthreadCleanupPop(CONTEXT* ctxt, ADDRINT _execute) 
{
  pthreadsim->pthread_cleanup_pop_((int)_execute, ctxt);
}

VOID CallPthreadCleanupPush(ADDRINT _routine, ADDRINT _arg) 
{
  pthreadsim->pthread_cleanup_push_(_routine, _arg);
}

int CallPthreadCondattrDestroy(ADDRINT _attr) 
{
  return 0;
}

int CallPthreadCondattrInit(ADDRINT _attr) 
{
  return 0;
}

int CallPthreadCondBroadcast(ADDRINT _cond) 
{
  return pthreadsim->pthread_cond_broadcast((pthread_cond_t*)_cond);
}

int CallPthreadCondDestroy(ADDRINT _cond) 
{
  return pthreadsim->pthread_cond_destroy((pthread_cond_t*)_cond);
}

int CallPthreadCondInit(ADDRINT _cond, ADDRINT _condattr) 
{
  return PthreadCond::pthread_cond_init((pthread_cond_t*)_cond, (pthread_condattr_t*)_condattr);
}

int CallPthreadCondSignal(ADDRINT _cond) 
{
  return pthreadsim->pthread_cond_signal((pthread_cond_t*)_cond);
}

VOID CallPthreadCondTimedwait(CONTEXT* context, ADDRINT _cond, ADDRINT _mutex, ADDRINT _abstime) 
{
  pthreadsim->pthread_cond_timedwait((pthread_cond_t*)_cond, (pthread_mutex_t*)_mutex,
      (const struct timespec*)_abstime, context);
}

VOID CallPthreadCondWait(CONTEXT* context, ADDRINT _cond, ADDRINT _mutex) 
{
  pthreadsim->pthread_cond_wait((pthread_cond_t*)_cond, (pthread_mutex_t*)_mutex, context);
}

VOID CallPthreadCreate(CONTEXT* ctxt,
    ADDRINT _thread, ADDRINT _attr, ADDRINT _func, ADDRINT _arg) 
{
  pthreadsim->pthread_create((pthread_t*)_thread, (pthread_attr_t*)_attr,
      ctxt, _func, _arg);
}

int CallPthreadDetach(ADDRINT _th) 
{
  return pthreadsim->pthread_detach((pthread_t)_th);
}

int CallPthreadEqual(ADDRINT _thread1, ADDRINT _thread2) 
{
  return pthreadsim->pthread_equal((pthread_t)_thread1, (pthread_t)_thread2);
}

VOID CallPthreadExit(CONTEXT* ctxt, ADDRINT _retval)  
{
  pthreadsim->pthread_exit((void*)_retval, ctxt);
}

int CallPthreadGetattr(ADDRINT _th, ADDRINT _attr) 
{
  return pthreadsim->pthread_getattr((pthread_t)_th, (pthread_attr_t*)_attr);
}

VOID* CallPthreadGetspecific(ADDRINT _key) 
{
  return pthreadsim->pthread_getspecific((pthread_key_t)_key);
}

VOID CallPthreadJoin(CONTEXT* ctxt,
    ADDRINT _th, ADDRINT _thread_return)
{
  pthreadsim->pthread_join((pthread_t)_th, (void**)_thread_return, ctxt);
}

int CallPthreadKeyCreate(ADDRINT _key, ADDRINT _func) 
{
  return pthreadsim->pthread_key_create((pthread_key_t*)_key, (void(*)(void*))_func);
}

int CallPthreadKeyDelete(ADDRINT _key) 
{
  return pthreadsim->pthread_key_delete((pthread_key_t)_key);
}

int CallPthreadKill(ADDRINT _thread, ADDRINT _signo) 
{
  return pthreadsim->pthread_kill((pthread_t)_thread, (int)_signo);
}

int CallPthreadMutexattrDestroy(ADDRINT _attr) 
{
  return PthreadMutexAttr::_pthread_mutexattr_destroy((pthread_mutexattr_t*) _attr);
}

int CallPthreadMutexattrGetkind(ADDRINT _attr, ADDRINT _kind) 
{
  return PthreadMutexAttr::_pthread_mutexattr_getkind((pthread_mutexattr_t*)_attr, (int*)_kind);
}

int CallPthreadMutexattrInit(ADDRINT _attr) 
{
  return PthreadMutexAttr::_pthread_mutexattr_init((pthread_mutexattr_t*)_attr);
}

int CallPthreadMutexattrSetkind(ADDRINT _attr, ADDRINT _kind) 
{
  return PthreadMutexAttr::_pthread_mutexattr_setkind((pthread_mutexattr_t*)_attr, (int)_kind);
}

int CallPthreadMutexDestroy(ADDRINT _mutex) 
{
  return PthreadMutex::_pthread_mutex_destroy((pthread_mutex_t*)_mutex);
}

int CallPthreadMutexInit(ADDRINT _mutex, ADDRINT _mutexattr) 
{
  return PthreadMutex::_pthread_mutex_init((pthread_mutex_t*)_mutex, (pthread_mutexattr_t*)_mutexattr);
}

VOID CallPthreadMutexLock(CONTEXT* context, ADDRINT _mutex) 
{
  pthreadsim->pthread_mutex_lock((pthread_mutex_t*)_mutex, context);
}

int CallPthreadMutexTrylock(ADDRINT _mutex) 
{
  return pthreadsim->pthread_mutex_trylock((pthread_mutex_t*)_mutex);
}

int CallPthreadMutexUnlock(ADDRINT _mutex) 
{
  return pthreadsim->pthread_mutex_unlock((pthread_mutex_t*)_mutex);
}

VOID CallPthreadOnce(CONTEXT* ctxt, ADDRINT _oncecontrol, ADDRINT _initroutine) 
{
  PthreadOnce::pthread_once((pthread_once_t*)_oncecontrol, _initroutine, ctxt);
}

pthread_t CallPthreadSelf() 
{
  return pthreadsim->pthread_self();
}

int CallPthreadSetcancelstate(ADDRINT _state, ADDRINT _oldstate) 
{
  return pthreadsim->pthread_setcancelstate((int)_state, (int*)_oldstate);
}

int CallPthreadSetcanceltype(ADDRINT _type, ADDRINT _oldtype) 
{
  return pthreadsim->pthread_setcanceltype((int)_type, (int*)_oldtype);
}

int CallPthreadSetspecific(ADDRINT _key, ADDRINT _pointer) 
{
  return pthreadsim->pthread_setspecific((pthread_key_t)_key, (VOID*)_pointer);
}

int CallPthreadBarrierInit(ADDRINT _barrier, ADDRINT _barrierattr, ADDRINT num) 
{
  return pthreadsim->pthread_barrier_init((pthread_barrier_t *)_barrier, (pthread_barrierattr_t *)_barrierattr, (unsigned int) num);
}

int CallPthreadBarrierDestroy(ADDRINT _barrier) 
{
  return pthreadsim->pthread_barrier_destroy((pthread_barrier_t *)_barrier);
}

int CallPthreadBarrierWait(CONTEXT* context, ADDRINT _barrier) 
{
  return pthreadsim->pthread_barrier_wait((pthread_barrier_t *)_barrier, context);
}

int CallPthreadBarrierattrInit(ADDRINT _barrierattr)
{
  return 0;  // not implemented yet
}

int CallPthreadBarrierattrDestory(ADDRINT _barrierattr)
{
  return 0;  // not implemented yet
}

int CallPthreadBarrierattrGetpshared(ADDRINT _barrierattr, ADDRINT value)
{
  return 0;  // not implemented yet
}

int CallPthreadBarrierattrSetpshared(ADDRINT _barrierattr, ADDRINT value)
{
  return 0;  // not implemented yet
}

VOID CallPthreadTestcancel(CONTEXT* ctxt) 
{
  pthreadsim->pthread_testcancel(ctxt);
}

VOID CallMcSimSkipInstrsBegin()
{
  pthreadsim->mcsim_skip_instrs_begin();
}

VOID CallMcSimSkipInstrsEnd()
{
  pthreadsim->mcsim_skip_instrs_end();
}

VOID CallMcSimSpinningBegin()
{
  pthreadsim->mcsim_spinning_begin();
}

VOID CallMcSimSpinningEnd()
{
  pthreadsim->mcsim_spinning_end();
}


/* ------------------------------------------------------------------ */
/* Thread-Safe Memory Allocation Support                              */
/* ------------------------------------------------------------------ */

VOID ProcessCall(ADDRINT target, ADDRINT arg0, ADDRINT arg1, BOOL tailcall) 
{
  PIN_LockClient();
  RTN rtn = RTN_FindByAddress(target);
  PIN_UnlockClient();
  if (RTN_Valid(rtn)) 
  {
    string temp_string(RTN_Name(rtn));
    pthreadsim->threadsafemalloc(true, tailcall, &temp_string);
  }
}

VOID ProcessReturn(const string* rtn_name) 
{
  ASSERTX(rtn_name != NULL);
  pthreadsim->threadsafemalloc(false, false, rtn_name);
}

/* ------------------------------------------------------------------ */
/* Thread Scheduler                                                   */
/* ------------------------------------------------------------------ */

VOID DoContextSwitch(CONTEXT* context) 
{
  pthreadsim->docontextswitch(context);
}

/* ------------------------------------------------------------------ */
/* Debugging Support                                                  */
/* ------------------------------------------------------------------ */

VOID WarnNYI(const string* rtn_name,
    ADDRINT ip) 
{
  std::cout << "NYI: " << *rtn_name << " at: 0x" << hex << ip << dec <<  "\n" << flush;
  //ASSERTX(0);
}

VOID PrintRtnName(const string* rtn_name) 
{
  std::cout << "RTN " << *rtn_name << "\n" << flush;
}

} // namespace PinPthread

