"""
core_0   core_1   core_2  ...  core_N
 L1_0     L1_1     L1_2   ...   L1_N
 L2_0     L2_1     L2_2   ...   L2_N
               BUS
            Shared L3
         MemoryController
      Main Memory (TimingDRAM)
"""

import sst
import os
import sys

app = sys.argv[1]
out_path = sys.argv[2]
app_path = os.environ['APP_HOME']

## Core Flags
corecount = 1
llc_per_core = 2 # in MB
instr_count = 10000000
CPUFrequency = "3.4 Ghz"
issue_width = 4

## Memory Flags
MemoryFrequency = "2800 Mhz" # 5600 MT/s
local_memory_capacity = 1024 # in MB
cxl_memory_capacity = 32*1024  # in MB
coherenceProtocol = "MESI"
rplPolicy = "lru"
busLat = "50 ps"
cacheLineSize = 64
page_size = 4096


## CXL Flags
CXL_offset = local_memory_capacity*1024*1024
CXL_BW = "32 GiB/s" # PCIe 5.0 x8
CXL_lat = "35 ns" # One-way latency

# Debug flags
memDebug = 1
memDebugLevel = 20

app_arguments = []
os.environ['OMP_NUM_THREADS'] = str(corecount)

# GAP BS
if app == "bfs":
   exe = app_path + "/gapbs/bfs"
   app_arguments.append("-f")
   app_arguments.append(app_path + "/gapbs/benchmark/graphs/twitter.sg")
   app_arguments.append("-n")
   app_arguments.append("50")
elif app == "cc":
   exe = app_path + "/gapbs/cc"
   app_arguments.append("-f")
   app_arguments.append(app_path + "/gapbs/benchmark/graphs/twitter.sg")
   app_arguments.append("-n")
   app_arguments.append("50")
elif app == "cc_sv":
   exe = app_path + "/gapbs/cc_sv"
   app_arguments.append("-f")
   app_arguments.append(app_path + "/gapbs/benchmark/graphs/twitter.sg")
   app_arguments.append("-n")
   app_arguments.append("50")
elif app == "bc":
   exe = app_path + "/gapbs/bc"
   app_arguments.append("-f")
   app_arguments.append(app_path + "/gapbs/benchmark/graphs/twitter.sg")
   app_arguments.append("-n")
   app_arguments.append("50")
elif app == "pr":
   exe = app_path + "/gapbs/pr"
   app_arguments.append("-f")
   app_arguments.append(app_path + "/gapbs/benchmark/graphs/twitter.sg")
   app_arguments.append("-n")
   app_arguments.append("50")
elif app == "sssp":
   exe = app_path + "/gapbs/sssp"
   app_arguments.append("-f")
   app_arguments.append(app_path + "/gapbs/benchmark/graphs/twitter.wsg")
   app_arguments.append("-n")
   app_arguments.append("50")
elif app == "tc":
   exe = app_path + "/gapbs/tc"
   app_arguments.append("-f")
   app_arguments.append(app_path + "/gapbs/benchmark/graphs/twitterU.sg")
   app_arguments.append("-n")
   app_arguments.append("50")

## Processor Model
ariel = sst.Component("A0", "ariel.ariel")
ariel.addParams({
   "verbose"             : "0",
   "maxcorequeue"        : 1024,
   "maxissuepercycle"    : issue_width,
   "pipetimeout"         : "0",
   "maxtranscore"        : 16,
   "arielinterceptcalls" : "1",
   "launchparamcount"    : 1,
   "launchparam0"        : "-ifeellucky",
   "arielmode"           : "2",
#   "skipcount"           : 10000000000,
   "corecount"           : corecount,
   "writepayloadtrace"   : "1",
   "max_insts"           : instr_count,
   "executable"          : exe,
   "outdir"          : out_path,
})
ariel.addParam("appargcount",len(app_arguments))
for i in range(len(app_arguments)):
   ariel.addParam("apparg"+str(i),app_arguments[i])

mgr = ariel.setSubComponent("memmgr", "ariel.MemoryManagerSimple")
mgr.addParams({
   "pagecount0" : int(local_memory_capacity/page_size*1024*1024),
    "pagecountcxl" : int(cxl_memory_capacity/page_size*1024*1024),
   "pagesize0"  : page_size,
})


## MemHierarchy

def genMemHierarchy(cores):

   membus = sst.Component("membus", "memHierarchy.Bus")
   membus.addParams({
       "bus_frequency" : CPUFrequency,
   })

   local_memctrl = sst.Component("local_memory", "memHierarchy.MemController")
   local_memctrl.addParams({
        "debug"                 : memDebug,
        "clock"                 : MemoryFrequency,
        "verbose"               : 2,
        "request_width"         : cacheLineSize,
        "addr_range_start"      : 0,
        "addr_range_end"        : CXL_offset-1,
   })

   local_memory = local_memctrl.setSubComponent("backend", "memHierarchy.timingDRAM")
   local_memory.addParams({
        "mem_size"      : str(local_memory_capacity) + "MiB",
        "id" : 0,
        "addrMapper" : "memHierarchy.simpleAddrMapper",
        "addrMapper.interleave_size" : "64B",
        "addrMapper.row_size" : "1KiB",
        "clock" : MemoryFrequency,
        "channels" : 2,
        "channel.numRanks" : 2,
        "channel.rank.numBanks" : 8,
        "channel.transaction_Q_size" : 32,
        "channel.rank.bank.CL" : 40,
        "channel.rank.bank.CL_WR" : 40,
        "channel.rank.bank.RCD" : 40,
        "channel.rank.bank.TRP" : 40,
        "channel.rank.bank.dataCycles" : 2,
        "channel.rank.bank.pagePolicy" : "memHierarchy.simplePagePolicy",
        "channel.rank.bank.transactionQ" : "memHierarchy.reorderTransactionQ",
        "channel.rank.bank.pagePolicy.close" : 1,
   })
   
   cxl_memctrl = sst.Component("cxl_memory", "memHierarchy.MemController")
   cxl_memctrl.addParams({
        "debug"                 : memDebug,
        "clock"                 : MemoryFrequency,
        "verbose"               : 2,
        "request_width"         : cacheLineSize,
        "addr_range_start"      : CXL_offset,
   })

   cxl_memory = cxl_memctrl.setSubComponent("backend", "memHierarchy.timingDRAM")
   cxl_memory.addParams({
        "mem_size"      : str(cxl_memory_capacity) + "MiB",
        "id" : 0,
        "addrMapper" : "memHierarchy.simpleAddrMapper",
        "addrMapper.interleave_size" : "64B",
        "addrMapper.row_size" : "1KiB",
        "clock" : MemoryFrequency,
        "channels" : 2,
        "channel.numRanks" : 2,
        "channel.rank.numBanks" : 8,
        "channel.transaction_Q_size" : 32,
        "channel.rank.bank.CL" : 40,
        "channel.rank.bank.CL_WR" : 40,
        "channel.rank.bank.RCD" : 40,
        "channel.rank.bank.TRP" : 40,
        "channel.rank.bank.dataCycles" : 2,
        "channel.rank.bank.pagePolicy" : "memHierarchy.simplePagePolicy",
        "channel.rank.bank.transactionQ" : "memHierarchy.reorderTransactionQ",
        "channel.rank.bank.pagePolicy.close" : 1,
   })


   for core in range (cores):
       l1 = sst.Component("l1cache_%d"%core, "memHierarchy.Cache")
       l1.addParams({
            "cache_frequency"       : CPUFrequency,
            "cache_size"            : "64 KiB",
            "cache_line_size"       : cacheLineSize,
            "associativity"         : "8",
            "access_latency_cycles" : "4",
            "coherence_protocol"    : coherenceProtocol,
            "replacement_policy"    : rplPolicy,
            "L1"                    : "1",
            "debug"                 : memDebug,
            "debug_level"           : memDebugLevel,
            "verbose"               : 2,
       })

       l2 = sst.Component("l2cache_%d"%core, "memHierarchy.Cache")
       l2.addParams({
            "cache_frequency"       : CPUFrequency,
            "cache_size"            : "512 KiB",
            "cache_line_size"       : cacheLineSize,
            "associativity"         : "8",
            "access_latency_cycles" : "10",
            "coherence_protocol"    : coherenceProtocol,
            "replacement_policy"    : rplPolicy,
            "L1"                    : "0",
            "debug"                 : memDebug,
            "debug_level"           : memDebugLevel,
            "verbose"               : 2,
            "mshr_num_entries"      : 32,
            "mshr_latency_cycles"   : 2,
       })

       ## SST Links
       # Ariel -> L1(PRIVATE) -> L2(PRIVATE)  -> L3 (SHARED) -> RC -> CXL Memory
       ArielL1Link = sst.Link("cpu_cache_%d"%core)
       ArielL1Link.connect((ariel, "cache_link_%d"%core, busLat), (l1, "high_network_0", busLat))
       L1L2Link = sst.Link("l1_l2_%d"%core)
       L1L2Link.connect((l1, "low_network_0", busLat), (l2, "high_network_0", busLat))
       L2MembusLink = sst.Link("l2_membus_%d"%core)
       L2MembusLink.connect((l2, "low_network_0", busLat), (membus, "high_network_%d"%core, busLat))


   l3 = sst.Component("L3cache", "memHierarchy.Cache")
   l3.addParams({
        "cache_frequency"       : CPUFrequency,
        "cache_size"            : str(llc_per_core*corecount)+"MiB",
        "cache_line_size"       : cacheLineSize,
        "associativity"         : "16",
        "access_latency_cycles" : "20",
        "coherence_protocol"    : coherenceProtocol,
        "replacement_policy"    : rplPolicy,
        "L1"                    : "0",
        "debug"                 : memDebug,
        "debug_level"           : memDebugLevel,
        "mshr_num_entries"      : 64*corecount,
        "mshr_latency_cycles"   : 2,
        "verbose"               : 2,
   })

   cxl_dummy = sst.Component("CXLdummy", "memHierarchy.Cache")
   cxl_dummy.addParams({
        "cache_frequency"       : CPUFrequency,
        "cache_size"            : str(llc_per_core*corecount)+"MiB",
        "cache_line_size"       : cacheLineSize,
        "associativity"         : "16",
        "access_latency_cycles" : "1",
        "coherence_protocol"    : coherenceProtocol,
        "replacement_policy"    : rplPolicy,
        "L1"                    : "0",
        "debug"                 : memDebug,
        "debug_level"           : memDebugLevel,
        "mshr_num_entries"      : 128*corecount,
        "mshr_latency_cycles"   : 0,
        "memNIC.network_bw"     : CXL_BW,
        "memNIC.min_packet_size": "16B",
        "verbose"               : 2,
   })

   root_complex = sst.Component("root_complex", "memHierarchy.Bus")
   root_complex.addParams({
       "bus_frequency" : CPUFrequency,
   })

   PCIe = sst.Component("PCIe", "merlin.hr_router")
   PCIe.addParams({
            "id": 0,
            "topology": "merlin.singlerouter",
            "link_bw" : CXL_BW,     
            "xbar_bw" : CXL_BW,
            "flit_size" : "16B",
            "input_latency" : CXL_lat,
            "output_latency" : "0 ns",
            "input_buf_size" : "1KiB",
            "output_buf_size" : "1KiB",
            "num_ports"       : 2,
            "debug"                 : memDebug,
            "debug_level"           : memDebugLevel,
   })
   PCIe.setSubComponent("topology", "merlin.singlerouter")

   dc_params = {
      "coherence_protocol"    : coherenceProtocol,
      "memNIC.network_bw"     : CXL_BW,
      "memNIC.min_packet_size": "16B",
      "clock"                 : CPUFrequency,
      "debug"                 : memDebug,
      "debug_level"           : memDebugLevel,
      "addr_range_start"      : CXL_offset,
   }
   dc = sst.Component("dc", "memHierarchy.DirectoryController")
   dc.addParams(dc_params)

   # Bus <-> L3
   BusL3Link = sst.Link("bus_L3")
   BusL3Link.connect((membus, "low_network_0", busLat), (l3, "high_network_0", busLat))

   # L3 <-> Root complex
   L3RC = sst.Link("L3RC")
   L3RC.connect((l3, "low_network_0", busLat), (root_complex, "high_network_0", busLat))

   # Root complex <-> Local memory
   RCLocal = sst.Link("RCLocal")
   RCLocal.connect((root_complex, "low_network_0", busLat), (local_memctrl, "direct_link", busLat))

   # Root complex <-> Dummy node (Required for packet type conversion)
   RCdum = sst.Link("RCdum")
   RCdum.connect((root_complex, "low_network_1", busLat), (cxl_dummy, "high_network_0", busLat))
   
   # Dummy node <-> PCIe
   dumPCIe = sst.Link("dumPCIe")
   dumPCIe.connect((cxl_dummy, "directory", busLat), (PCIe, "port0", busLat))

   # PCIe <-> CXL memory
   PCIeCXL = sst.Link("PCIeCXL")
   PCIeCXL.connect((PCIe, "port1", busLat), (dc, "network", busLat))
   PCIeCXL_ = sst.Link("PCIeCXL_")
   PCIeCXL_.connect((dc, "memory", busLat), (cxl_memctrl, "direct_link", busLat))

genMemHierarchy(corecount)

# sst global option
sst.setStatisticLoadLevel(16)
sst.enableAllStatisticsForAllComponents({"type":"sst.AccumulatorStatistic"})
sst.setStatisticOutput("sst.statOutputCSV")
sst.setStatisticOutputOptions( {
	"filepath"  : out_path + '/stats.csv',
	"separator" : ", "
} )
#sst.setProgramOption("heartbeat-period", "500ms")
sst.setProgramOption("print-timing-info", "1")
