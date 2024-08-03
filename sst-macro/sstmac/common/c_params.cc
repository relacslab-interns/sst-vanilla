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

#include <sprockit/sim_parameters.h>
#include <sstmac/common/c_params.h>
#include <sstmac/common/sstmac_env.h>
#include <sstmac/software/process/thread.h>
#include <sstmac/software/process/app.h>
#include <sstmac/software/process/operating_system.h>

static inline SST::Params& get_local_params() {
  return sstmac::sw::OperatingSystem::currentThread()->parentApp()->params();
}

union param_val {
  double d;
  int i;
  long l;
};

using LookupMap = std::unordered_map<void*,param_val>;
static std::map<int,LookupMap> cache;

extern "C" int
sstmac_get_int_param(const char* str)
{
  return get_local_params().find<int>(str);
}

extern "C" int
sstmac_get_optional_int_param(const char* str, int val)
{
  return get_local_params().find<int>(str, val);
}

extern "C" long
sstmac_get_long_param(const char* str)
{
  return get_local_params().find<long>(str);
}

extern "C" long
sstmac_get_optional_long_param(char* str, long val)
{
  return get_local_params().find<long>(str, val);
}

extern "C" double
sstmac_get_double_param(const char* str)
{
  return get_local_params().find<double>(str);
}

extern "C" double
sstmac_get_optional_double_param(const char* str, double val)
{
  return get_local_params().find<double>(str, val);
}

extern "C" const char*
sstmac_get_param(const char* str)
{
  std::string param = get_local_params().find<std::string>(str);
  char* param_cstr = new char[param.size() + 1];
  ::strcpy(param_cstr, param.c_str());
  return param_cstr;
}

extern "C" const char*
sstmac_get_optional_param(const char* str, const char* val)
{
  std::string param = get_local_params().find<std::string>(str, val);
  char* param_cstr = new char[param.size() + 1];
  ::strcpy(param_cstr, param.c_str());
  return param_cstr;
}

extern "C" double
sstmac_get_time_param(const char *str)
{
  return get_local_params().find<SST::UnitAlgebra>(str).getValue().toDouble();
}
