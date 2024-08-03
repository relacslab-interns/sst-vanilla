/**
Copyright 2009-2022 National Technology and Engineering Solutions of Sandia,
LLC (NTESS).  Under the terms of Contract DE-NA-0003525, the U.S. Government
retains certain rights in this software.

Sandia National Laboratories is a multimission laboratory managed and operated
by National Technology and Engineering Solutions of Sandia, LLC., a wholly
owned subsidiary of Honeywell International, Inc., for the U.S. Department of
Energy's National Nuclear Security Administration under contract DE-NA0003525.

Copyright (c) 2009-2022, NTESS

All rights reserved.

Redistribution and use in source and binary forms, with or without modification, 
are permitted provided that the following conditions are met:

    * Redistributions of source code must retain the above copyright
      notice, this list of conditions and the following disclaimer.

    * Redistributions in binary form must reproduce the above
      copyright notice, this list of conditions and the following
      disclaimer in the documentation and/or other materials provided
      with the distribution.

    * Neither the name of the copyright holder nor the names of its
      contributors may be used to endorse or promote products derived
      from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
"AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

Questions? Contact sst-macro-help@sandia.gov
*/

#ifndef SSTMAC_HARDWARE_PROCESSOR_COMPUTEscheduleR_H_INCLUDED
#define SSTMAC_HARDWARE_PROCESSOR_COMPUTEscheduleR_H_INCLUDED

#include <sstmac/hardware/common/flow.h>
#include <sstmac/common/event_scheduler.h>
#include <sstmac/software/process/thread_fwd.h>
#include <sstmac/software/process/operating_system_fwd.h>
#include <sprockit/sim_parameters_fwd.h>
#include <sprockit/factory.h>
#include <sprockit/debug.h>

DeclareDebugSlot(compute_scheduler)

namespace sstmac {
namespace sw {

class ComputeScheduler
{
 public:
  SST_ELI_DECLARE_BASE(ComputeScheduler)
  SST_ELI_DECLARE_DEFAULT_INFO()
  SST_ELI_DECLARE_CTOR(SST::Params&,sw::OperatingSystem*, int/*ncores*/, int/*nsockets*/)

  ComputeScheduler(SST::Params&  /*params*/, sw::OperatingSystem* os,
                    int ncores, int nsockets) :
    ncores_(ncores), 
    nsocket_(nsockets),
    os_(os)
  {
  }

  virtual ~ComputeScheduler() {}


  int ncores() const {
    return ncores_;
  }

  int nsocket() const {
    return nsocket_;
  }

  /**
   * @brief reserve_core
   * @param thr   The physical thread requesting to compute
   */
  virtual void reserveCores(int ncore, Thread* thr) = 0;
  
  virtual void releaseCores(int ncore, Thread* thr) = 0;


 protected:
  int ncores_;
  int nsocket_;
  int cores_per_socket_;
  sw::OperatingSystem* os_;

};

}
} //end of namespace sstmac
#endif
