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

#include <sstmac/software/libraries/compute/compute_event.h>
#include <sstmac/hardware/processor/processor.h>
#include <sstmac/hardware/memory/memory_model.h>
#include <sstmac/hardware/node/node.h>
#include <sstmac/common/event_callback.h>
#include <sprockit/statics.h>
#include <sprockit/util.h>
#include <sprockit/sim_parameters.h>
#include <sprockit/keyword_registration.h>

RegisterDebugSlot(processor);

RegisterKeywords(
{ "frequency", "the frequency of the CPU" },
{ "ncores", "the number of core in a processor" },
{ "node_cores", "DEPRECATED: the number of cores per processor" },
{ "node_frequency", "DEPRECATED: the frequency of the CPU" },
);

namespace sstmac {
namespace hw {

static sprockit::NeedDeletestatics<Processor> del_statics;


Processor::~Processor()
{
}

Processor::Processor(SST::Params& params, MemoryModel* mem, Node* nd) :
  mem_(mem), node_(nd)
{
  freq_ = params.find<SST::UnitAlgebra>("frequency").getValue().toDouble();
  if (freq_ == 0){
    spkt_abort_printf("Zero or missing proc.frequency parameter");
  }
  mem_freq_ = freq_;
  ncores_ = params.find<int>("ncores", 1);
}

void
Processor::deleteStatics()
{
}

}
} //end of namespace sstmacs
