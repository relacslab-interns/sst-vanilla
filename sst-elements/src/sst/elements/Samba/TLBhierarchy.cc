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


#include <sst_config.h>

#include "TLBhierarchy.h"

#include <sst/core/event.h>
#include <sst/core/sst_types.h>
#include <sst/core/component.h>
#include <sst/core/link.h>
#include <sst/core/timeConverter.h>
#include <sst/core/output.h>
#include <sst/core/interfaces/stringEvent.h>
#include <sst/elements/memHierarchy/memEvent.h>

using namespace SST;
using namespace SST::Interfaces;
using namespace SST::MemHierarchy;
using namespace SST::SambaComponent;

TLBhierarchy::TLBhierarchy(ComponentId_t id, int tlb_id) : ComponentExtension(id)
{
    coreID = tlb_id;
    char* subID = (char*) malloc(sizeof(char) * 32);
    sprintf(subID, "%" PRIu32, coreID);
}


TLBhierarchy::TLBhierarchy(ComponentId_t id, int tlb_id, int _levels, Params& params) : ComponentExtension(id)
{
    /* Set up state varaialbles */
    timeStamp = 0;
    hold = 0; // Hold is set to 1 by the page table walker due to fault or shootdown, 
              // note that since we don't execute page fault handler or TLB shootdown routine on the core, 
              // we just stall TLB hierarchy to emulate the performance effect

    shootdown = 0;  // is set to 1 by the page table walker due to shootdown
    output = new SST::Output("SambaComponent[@f:@l:@p] ", 1, 0, SST::Output::STDOUT);

    /* Get parameters */
    levels = _levels;
    coreID = tlb_id;
    emulate_faults  = ((uint32_t) params.find<uint32_t>("emulate_faults", 0));
    
    if (emulate_faults)
    {
        /* Page fault emulation parameters */
        page_placement  = ((uint32_t) params.find<uint32_t>("page_placement", 0));
        shootdown_delay = ((uint32_t) params.find<uint32_t>("shootdown_delay", 8000));

        if (0 == coreID)
            shootdown_delay = (shootdown_delay*2);

        page_swapping_delay = ((uint32_t) params.find<uint32_t>("page_swapping_delay", 1000));
        
        /* Get memory size and parse */
        std::string mem_size = ((std::string) params.find<std::string>("memory", "0"));

        trim(mem_size);
        size_t pos;
        std::string checkstring = "kKmMgGtTpP";
        if ((pos = mem_size.find_first_of(checkstring)) != std::string::npos) {
            pos++;
            if (pos < mem_size.length() && mem_size[pos] == 'B') {
                mem_size.insert(pos, "i");
            }
        }
        UnitAlgebra ua(mem_size);
        memory_size = ua.getRoundedValue();
        //std::cout<< getName().c_str() << " Core: " << coreID << std::hex << " Memory size: " << memory_size << std::endl;
    }

    PTW = loadComponentExtension<PageTableWalker>(coreID, nullptr, 0, params);
    max_shootdown_width = ((uint32_t) params.find<uint32_t>("max_shootdown_width", 4));
    ptw_confined  = ((uint32_t) params.find<uint32_t>("ptw_confined", 0));

    if (levels)
    {
        /* Construct TLB hierarchy (level=1 is defined as L1 TLB) */
        TLB_CACHE[levels] = loadComponentExtension<TLB>(coreID, nullptr, levels, params);
        TLB_CACHE[levels]->setPTW(PTW);
        TLB * prev = TLB_CACHE[levels];
        
        for (int level=levels-1; level>=1; level--)
        {
            TLB_CACHE[level] = loadComponentExtension<TLB>(coreID, (TLB *) prev, level, params);
            prev = TLB_CACHE[level];
        }

        /* Link translation response paths on each level to mem_req */
        for (int level=2; level<=levels; level++)
        {
            TLB_CACHE[level]->setServiceBack(TLB_CACHE[level-1]->getPushedBack());
            TLB_CACHE[level]->setServiceBackSize(TLB_CACHE[level-1]->getPushedBackSize());
        }

        PTW->setServiceBack(TLB_CACHE[levels]->getPushedBack());
        PTW->setServiceBackSize(TLB_CACHE[levels]->getPushedBackSize());

        TLB_CACHE[1]->setServiceBack(&mem_reqs);
        TLB_CACHE[1]->setServiceBackSize(&mem_reqs_sizes);
    }
    else
    {
        /* 
         * Let PTW directly service memory hierarhcy 
         * TODO: SST 12 does not consider non-TLB mode for now (see handleEvent_CPU)
         */
        assert(0);
        PTW->setServiceBack(&mem_reqs);
        PTW->setServiceBackSize(&mem_reqs_sizes);
    }

    /* Setup PTW-related parameters */
    PTW->setHold(&hold);
    PTW->setShootDownEvents(&shootdown,&hasInvalidAddrs,&invalid_addrs);
    PTW->setPagePlacement(&page_placement);
    PTW->setTimeStamp(&timeStamp);
    PTW->setLocalMemorySize(&memory_size);

    curr_time = 0;

    /* Stats-related */
    char* subID = (char*) malloc(sizeof(char) * 32);
    sprintf(subID, "%" PRIu32, coreID);
    total_waiting = registerStatistic<uint64_t>( "total_waiting", subID );
}


TLBhierarchy::TLBhierarchy(ComponentId_t id, Params& params, int tlb_id) : ComponentExtension(id)
{
    coreID = tlb_id;
}

/* For now, implement to return a dummy address */
Address_t TLBhierarchy::translate(Address_t VA) { return 0;}

void TLBhierarchy::handleEvent_CACHE( SST::Event * ev )
{
    /* Bypass data response from dcache hierarchy to CPU cores */
    to_cpu->send(ev);
}

void TLBhierarchy::handleEvent_CPU(SST::Event* event)
{
    /* Push event to L1 TLB and record timestamp */
    time_tracker[event]= curr_time;
    MemEventBase* mEvent = static_cast<MemEventBase*>(event);
    TLB_CACHE[1]->push_request(mEvent);
}

bool TLBhierarchy::tick(SST::Cycle_t x)
{
    /* Increment timestamp */
    timeStamp++;

    /* Check PTW is under fault handling or shootdown */
    if (hold==1) 
    {
        /* TLBhierarchy stops processing any events */
        PTW->tick(x);
        return false;
    }

    /* Process PTW & TLB hierarchy */
    for (int level = levels; level >= 1; level--)
        TLB_CACHE[level]->tick(x);

    PTW->tick(x);

    /* Check responded packets from lower levels */
    curr_time = x;
    while(!mem_reqs.empty() && !shootdown && !hold)
    {
        MemHierarchy::MemEventBase * event= mem_reqs.back();
        if (time_tracker.find(event) == time_tracker.end())
        {
            std::cout << "Danger! Something is terribly wrong..." << std::endl;
            mem_reqs_sizes.erase(event);
            mem_reqs.pop_back();
            continue;
        }

        /* 
         * Override physical address provided by ariel memory 
         * ariel memory is managed by the one provided by page fault handler
         */
        if (emulate_faults)
        {
            Address_t vaddr = ((MemEvent*) event)->getVirtualAddress();
            if (!ptw_confined)
            {
                if ((*PTE).find(vaddr/4096)==(*PTE).end())
                    std::cout<<"Error: That page has never been mapped:  " << vaddr / 4096 << std::endl;

                ((MemEvent*) event)->setAddr((((*PTE)[vaddr / 4096] + vaddr % 4096) / 64) * 64);
                ((MemEvent*) event)->setBaseAddr((((*PTE)[vaddr / 4096] + vaddr % 4096) / 64) * 64);
            }
            else
            {
                uint64_t offset = (uint64_t)512*512*512*512;
                if ((*PTE).find((vaddr/4096)%offset)==(*PTE).end())
                    std::cout<<"Error: That page has never been mapped:  " << vaddr / 4096 << std::endl;

                ((MemEvent*) event)->setAddr((((*PTE)[(vaddr / 4096)%offset] + vaddr % 4096)));
                ((MemEvent*) event)->setBaseAddr((((*PTE)[(vaddr / 4096)%offset] + vaddr % 4096) / 64) * 64);
            }

//            if (page_placement) {
//                if ((*PTE)[vaddr / 4096] < memory_size ) {
//                    if ((*PTR_map).find((*PTE)[vaddr / 4096]) == (*PTR_map).end())
//                      std::cout << "Error: page reference is not mapped, vaddress:" 
//                          << vaddr / 4096 << " physical page: " << (*PTE)[vaddr / 4096] << " PTE " << std::endl;
//
//                    std::cerr << Owner->getName().c_str() << " Page table reference update address: " 
//                        << (*PTR)[(*PTR_map)[(*PTE)[vaddr / 4096]]].first << " index: " 
//                        << (*PTR_map)[(*PTE)[vaddr / 4096]] << " PTE " << std::endl;
//                    W->updatePTR((*PTE)[vaddr / 4096]);
//                    (*PTR)[(*PTE)[vaddr / 4096]] = 1;
//                }
//            }
        }

        /* Accumulate time lapse */
        uint64_t time_diff = (uint64_t ) x - time_tracker[event];
        time_tracker.erase(event);
        total_waiting->addData(time_diff);

        /* Send translated event to dcache hierarhcy */
        to_cache->send(event); //TODO-hklee: need to another direct path to STU connection

        /* 
         * We remove the size of that translation, 
         * TODO: we might for future versions use the translation size to obtain statistics
         */
        mem_reqs_sizes.erase(event);
        mem_reqs.pop_back();
    }

    return false;
}


void TLBhierarchy::finish()
{
    for (int level=1; level<=levels;level++)
    {
        TLB_CACHE[level]->finish();
    }
}

