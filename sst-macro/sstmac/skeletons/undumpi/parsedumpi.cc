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

#include <sstmac/software/process/operating_system.h>
#include <sstmac/common/event_manager.h>
#include <sstmac/common/runtime.h>
#include <sstmac/skeletons/undumpi/parsedumpi.h>
#include <sstmac/skeletons/undumpi/parsedumpi_callbacks.h>
#include <sstmac/dumpi_util/dumpi_meta.h>
#include <sstmac/dumpi_util/dumpi_util.h>
#include <sumi-mpi/mpi_api.h>
#include <sprockit/errors.h>
#include <sprockit/sim_parameters.h>
#include <sprockit/keyword_registration.h>
#include <stdio.h>
#include <errno.h>
#include <cstring>
#include <fstream>
#include <algorithm>

RegisterKeywords(
{ "parsedumpi_timescale", "the scale factor for time between MPI calls, < 1 means speedup" },
{ "parsedumpi_print_progress", "whether to print the progress of the trace" },
{ "parsedumpi_terminate_count", "the number of global collectives to run, then terminate" },
{ "launch_dumpi_metaname", "DEPRECATED: the meta file for the DUMPI trace" },
{ "dumpi_metaname", "the meta file for the DUMPI trace" },
);

namespace sumi{

using namespace sstmac::hw;


ParseDumpi::ParseDumpi(SST::Params& params, SoftwareId sid,
                       sstmac::sw::OperatingSystem* os) :
  App(params, sid, os),
  mpi_(nullptr)
{
  fileroot_ = params.find<std::string>("dumpi_metaname");

  timescaling_ = params.find<double>("parsedumpi_timescale", 1);

  print_progress_ = params.find<bool>("parsedumpi_print_progress", true);

  early_terminate_count_ = params.find<int>("parsedumpi_terminate_count", -1);
}

ParseDumpi::~ParseDumpi() throw()
{
}

int ParseDumpi::skeletonMain()
{
  int rank = this->tid();
  mpi_ = getApi<MpiApi>("mpi");

  sstmac::sw::DumpiMeta* meta = new   sstmac::sw::DumpiMeta(fileroot_);
  ParsedumpiCallbacks cbacks(this);
  std::string fname = sstmac::sw::dumpiFileName(rank, meta->dirplusfileprefix_);
  // Ready to go.
  //only rank 0 should print progress
  bool print_my_progress = rank == 0 && print_progress_;

  try {
    cbacks.parseStream(fname.c_str(), print_my_progress);
  } catch (ParseDumpi::early_termination& e) {
    //do nothing - happily move on and finalize
    mpi_->finalize();
  }

  if (rank == 0) {
    std::cout << "Parsedumpi finalized on rank 0 - trace "
      << fileroot_ << " successful!" << std::endl;
  }

  return 0;

}


}
