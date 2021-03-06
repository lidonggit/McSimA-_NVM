/*
 * Copyright (c) 2010 The Hewlett-Packard Development Company
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met: redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer;
 * redistributions in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in the
 * documentation and/or other materials provided with the distribution;
 * neither the name of the copyright holders nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * Authors: Jung Ho Ahn
 */

#ifndef __PTS_H__
#define __PTS_H__

#include "pin.H"
#include <list>
#include <map>
#include <queue>
#include <stack>
#include <vector>
#include <string>
#include <stdlib.h>
#include <stdint.h>
#include <arpa/inet.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/types.h>

const uint32_t instr_batch_size = 32;

enum pts_msg_type
{
  pts_constructor,
  pts_destructor,
  pts_resume_simulation,
  pts_add_instruction,
  pts_set_stack_n_size,
  pts_set_active,
  pts_get_num_hthreads,
  pts_get_param_uint64,
  pts_get_param_bool,
  pts_get_curr_time,
  pts_invalid,
  #ifdef MALLOC_INTERCEPT
  pts_add_heap_mem,
  //pts_search_heap_mem,
  pts_free_heap_mem,
  pts_update_heap_mem_for_hpl,
  #endif
};


struct PTSInstr
{
  uint32_t hthreadid_;
  uint64_t curr_time_;
  uint64_t waddr;
  UINT32   wlen;
  uint64_t raddr;
  uint64_t raddr2;
  UINT32   rlen;
  uint64_t ip;
  uint32_t category;
  bool     isbranch;
  bool     isbranchtaken;
  bool     islock;
  bool     isunlock;
  bool     isbarrier;
  uint32_t rr0;
  uint32_t rr1;
  uint32_t rr2;
  uint32_t rr3;
  uint32_t rw0;
  uint32_t rw1;
  uint32_t rw2;
  uint32_t rw3;
};


typedef union
{
  PTSInstr   instr[instr_batch_size];
  char       str[2048];
} instr_n_str;


struct PTSMessage
{
  pts_msg_type type;
  bool         bool_val;
  uint32_t     uint32_t_val;
  uint64_t     uint64_t_val;
  ADDRINT      stack_val;
  //ADDRINT      stacksize_val;
  ADDRINT      memsize_val;
  instr_n_str  val;
};


const uint32_t instr_group_size = 100000;

struct PTSInstrTrace
{
  uint64_t waddr;
  uint32_t wlen;
  uint64_t raddr;
  uint64_t raddr2;
  uint32_t rlen;
  uint64_t ip;
  uint32_t category;
  bool     isbranch;
  bool     isbranchtaken;
  uint32_t rr0;
  uint32_t rr1;
  uint32_t rr2;
  uint32_t rr3;
  uint32_t rw0;
  uint32_t rw1;
  uint32_t rw2;
  uint32_t rw3;
};


using namespace std;

namespace PinPthread 
{
  class McSim;

#define STRING_NAME_LENGTH 1024
  struct HeapMemRec
  {
    uint64_t start_address;
    uint64_t end_address;
    bool avail_flag;           //whether the heap mem is freed.
    char rtn_name[STRING_NAME_LENGTH];  //where the heap mem is allocated 
    char lib_name[STRING_NAME_LENGTH];  //where the heap mem is allocated
  };

  class PthreadTimingSimulator
  {
    public:
      PthreadTimingSimulator(const string & mdfile);
      PthreadTimingSimulator(int port_num);
      ~PthreadTimingSimulator();

      pair<uint32_t, uint64_t> resume_simulation(bool must_switch);
      bool add_instruction(
	  uint32_t hthreadid_,
	  uint64_t curr_time_,
	  uint64_t waddr,
	  UINT32   wlen,
	  uint64_t raddr,
	  uint64_t raddr2,
	  UINT32   rlen,
	  uint64_t ip,
	  uint32_t category,
	  bool     isbranch,
	  bool     isbranchtaken,
	  bool     islock,
	  bool     isunlock,
	  bool     isbarrier,
	  uint32_t rr0, uint32_t rr1, uint32_t rr2, uint32_t rr3,
	  uint32_t rw0, uint32_t rw1, uint32_t rw2, uint32_t rw3,
	  bool     can_be_piled = false
	  );  // whether we have to resume simulation
      void set_stack_n_size(int32_t pth_id, ADDRINT stack, ADDRINT stacksize);
      void set_active(int32_t pth_id, bool is_active);

      uint32_t get_num_hthreads();
      uint64_t get_param_uint64(const string & idx_, uint64_t def_value);
      bool     get_param_bool(const string & idx_, bool def_value);
      string   get_param_str(const string & idx_);
      uint64_t get_curr_time();
     
      #ifdef MALLOC_INTERCEPT
      //void add_heap_mem(HeapMemRec * hmr);
      //HeapMemRec * search_heap_mem(uint64_t addr);
      void add_heap_mem(uint64_t addr, int heap_size);
      void free_heap_mem(uint64_t addr);
      //void search_heap_mem(uint64_t addr);
      #ifdef APP_HPL
       void update_heap_mem_for_hpl(uint64_t addr, int heap_size);
      #endif
      #endif  //MALLOC_INTERCEPT

      struct sockaddr_in their_addr;
      struct sockaddr_in my_addr;
      socklen_t          addr_len;
      int                sockfd;
      char             * buffer;
      uint32_t           num_piled_instr;      // in this object
      uint32_t           num_hthreads;
      uint32_t        *  num_available_slot;   // in the timing simulator

      /*#ifdef MALLOC_INTERCEPT	
      McSim * mcsim;   
      #endif  */ 
    private:
      void send_instr_batch();
  };
}

#endif  //__PTS_H__
