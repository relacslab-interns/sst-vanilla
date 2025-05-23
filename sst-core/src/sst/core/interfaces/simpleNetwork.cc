// Copyright 2009-2022 NTESS. Under the terms
// of Contract DE-NA0003525 with NTESS, the U.S.
// Government retains certain rights in this software.
//
// Copyright (c) 2009-2022, NTESS
// All rights reserved.
//
// This file is part of the SST software package. For license
// information, see the LICENSE file in the top level directory of the
// distribution.
//
#include "sst_config.h"

#include "sst/core/interfaces/simpleNetwork.h"

#include "sst/core/objectComms.h"

using namespace std;

namespace SST {
namespace Interfaces {

const SimpleNetwork::nid_t SimpleNetwork::INIT_BROADCAST_ADDR = 0xffffffffffffffffl;

} // namespace Interfaces
} // namespace SST
