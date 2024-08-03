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

#include <sstmac/replacements/mutex>
#include <sstmac/skeleton.h>
#include <sstmac/compute.h>
#include <sstmac/software/process/cppglobal.h>
#include <iostream>
#include <pthread.h>

extern "C" int ubuntu_cant_name_mangle() { return 0; }

struct tag1{}; struct tag2{};
sstmac::CppVarTemplate<tag1,int,true> count(0);
sstmac::CppVarTemplate<tag2,int,true> id(0);

void * thrash(void * myId)
{
  id() = (long)myId;
  for (int i=0; i < 3; ++i){
    std::cout << "Thread " << id()
        << " has count " << count() << std::endl;
    sstmac_sleep(1);
    count() += 1;
  }

  return nullptr;
}


#define sstmac_app_name test_tls

int USER_MAIN(int  /*argc*/, char**  /*argv*/)
{
  //now test some mutexes
  pthread_t t0, t1, t2;

  pthread_create(&t0, nullptr, thrash, (void*)0);
  pthread_create(&t1, nullptr, thrash, (void*)1);
  pthread_create(&t2, nullptr, thrash, (void*)2);

  pthread_join(t0, nullptr);
  pthread_join(t1, nullptr);
  pthread_join(t2, nullptr);

  return 0;
}

