# Copyright 2009-2022 NTESS. Under the terms
# of Contract DE-NA0003525 with NTESS, the U.S.
# Government retains certain rights in this software.
#
# Copyright (c) 2009-2022, NTESS
# All rights reserved.
#
# This file is part of the SST software package. For license
# information, see the LICENSE file in the top level directory of the
# distribution.
import sst

# Define SST core options
sst.setProgramOption("stopAtCycle", "10000s")

# Define the simulation components
comp_clocker0 = sst.Component("clocker0", "coreTestElement.coreTestRNGComponent")
comp_clocker0.addParams({
      "count" : "100000",
      "seed_z" : "1053",
      "verbose" : "1",
      "rng" : "marsaglia",
      "seed_w" : "1447"
})


# Define the simulation links