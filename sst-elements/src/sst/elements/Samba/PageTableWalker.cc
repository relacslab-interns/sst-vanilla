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

/* Author: Amro Awad
 * E-mail: aawad@sandia.gov
 */

#include <sst_config.h>
#include "PageTableWalker.h"
#include <sst/core/link.h>
#include "Samba_Event.h"
#include <iostream>

using namespace SST::SambaComponent;
using namespace SST::MemHierarchy;
using namespace SST;

int max(int a, int b)
{
    if ( a > b )
        return a;
    else
        return b;
}

int abs_int_Samba(int x)
{
    if (x<0)
        return -1*x;
    else
        return x;
}

PageTableWalker::PageTableWalker(ComponentId_t id, int Page_size, 
        int Assoc, PageTableWalker * Next_level, int Size) : ComponentExtension(id)
{

}

PageTableWalker::PageTableWalker(ComponentId_t id, int tlb_id, 
        PageTableWalker * Next_level, int level, SST::Params& params) : ComponentExtension(id)
{
    /* Set up state variables */
    stall = false; // no page faults (PF) initially
    fault_level = 0;
    hits=misses=0;
    to_mem = NULL; // memory for PTW; set thru Samba-TLBhierarchy::setPTWLink

    output = new SST::Output("SambaComponent[@f:@l:@p] ", 1, 0, SST::Output::STDOUT);

    /* Get parameters */
    coreId = tlb_id;
    os_page_size = ((uint32_t) params.find<uint32_t>("os_page_size", 4)); // KB
    self_connected = ((uint32_t) params.find<uint32_t>("self_connected", 0));

    page_walk_latency = ((uint32_t) params.find<uint32_t>("page_walk_latency", 50));
    latency = ((uint32_t) params.find<uint32_t>("latency_PTWC", 1));

    emulate_faults  = ((uint32_t) params.find<uint32_t>("emulate_faults", 0));
    ptw_confined  = ((uint32_t) params.find<uint32_t>("ptw_confined", 0));

    std::string LEVEL = std::to_string(level); // indicates current level
    parallel_mode = ((uint32_t) params.find<uint32_t>("parallel_mode_L"+LEVEL, 0));
    upper_link_latency = ((uint32_t) params.find<uint32_t>("upper_link_L"+LEVEL, 0));

    max_outstanding = ((uint32_t) params.find<uint32_t>("max_outstanding_PTWC", 4));
    max_width = ((uint32_t) params.find<uint32_t>("max_width_PTWC", 4));

    /* 
     * Page table (PT) constructions 
     * We assume a x86-64 system with 4-level PT 
     */
    pt_levels = 4;
    page_size = new uint64_t[pt_levels];
    page_size[0] = 1024*4; // offset of last-level PT
    page_size[1] = 512*1024*4;
    page_size[2] = (uint64_t) 512*512*1024*4;
    page_size[3] = (uint64_t) 512*512*512*1024*4;

    /* PTWC construction */
    size = new int[pt_levels];
    assoc = new int[pt_levels];
    sets = new int[pt_levels];
    tags = new Address_t**[pt_levels];
    valid = new bool**[pt_levels];
    lru = new int **[pt_levels];

    for (int i=0; i<pt_levels; i++)
    {
        size[i] =  ((uint32_t) params.find<uint32_t>("size"+std::to_string(i+1) + "_PTWC", 1));
        assoc[i] =  ((uint32_t) params.find<uint32_t>("assoc"+std::to_string(i+1) +  "_PTWC", 1));

        // We define the number of sets for that structure of page size number i
        sets[i] = size[i]/assoc[i];
    }

    for (int id=0; id<pt_levels; id++)
    {

        tags[id] = new Address_t*[sets[id]];
        valid[id] = new bool*[sets[id]];
        lru[id] = new int*[sets[id]];

        for (int i=0; i < sets[id]; i++)
        {
            tags[id][i]=new Address_t[assoc[id]];
            valid[id][i]=new bool[assoc[id]];
            lru[id][i]=new int[assoc[id]];
            for (int j=0; j<assoc[id];j++)
            {
                tags[id][i][j]=-1;
                valid[id][i][j]=false;
                lru[id][i][j]=j;
            }
        }
    }

    /* Stats-related */
    char* subID = (char*) malloc(sizeof(char) * 32);
    sprintf(subID, "Core%d_PTWC", tlb_id);
    statPageTableWalkerHits = registerStatistic<uint64_t>( "tlb_hits", subID);
    statPageTableWalkerMisses = registerStatistic<uint64_t>( "tlb_misses", subID );
}

/* 
 * Internal event handler 
 * SambaEvent is processed here via s_EventChan
 * For each level, we update MAPPED_PAGE_SIZE and PGD/PMD/PUD/PTE so actual PA is used later
 */
void PageTableWalker::handleEvent( SST::Event* e )
{
    /* Convert event type for processing */
    SambaEvent * temp_ptr =  dynamic_cast<SambaComponent::SambaEvent*> (e);
    if (temp_ptr==nullptr)
        std::cout<<" Error in Casting to SambaEvent "<<std::endl;

    if (temp_ptr->getType() == EventType::PAGE_FAULT)
    {
        /* Handle page fault from the first unmapped level (e.g., CR3 or L4) */
        if (!ptw_confined)
        {
            //if ((*CR3) == -1)
            if (!(*cr3_init))
                fault_level = 4;
            else if ((*PGD).find(temp_ptr->getAddress()/page_size[3]) == (*PGD).end())
                fault_level = 3;
            else if ((*PUD).find(temp_ptr->getAddress()/page_size[2]) == (*PUD).end())
                fault_level = 2;
            else if ((*PMD).find(temp_ptr->getAddress()/page_size[1]) == (*PMD).end())
                fault_level = 1;
            else if ((*PTE).find(temp_ptr->getAddress()/page_size[0]) == (*PTE).end())
                fault_level = 0;
            else
                output->fatal(CALL_INFO, -1, "MMU: DANGER!!\n");
        }
        else
        {
            uint64_t offset = (uint64_t)512*512*512*512;
            if (!(*cr3_init)) fault_level = 4;
            else if ((*PGD).find((temp_ptr->getAddress()/page_size[3])%512) == (*PGD).end()) fault_level = 3;
            else if ((*PUD).find((temp_ptr->getAddress()/page_size[2])%(512*512)) == (*PUD).end()) fault_level = 2;
            else if ((*PMD).find((temp_ptr->getAddress()/page_size[1])%(512*512*512)) == (*PMD).end()) fault_level = 1;
            else if ((*PTE).find((temp_ptr->getAddress()/page_size[0])%offset) == (*PTE).end()) fault_level = 0;
            else output->fatal(CALL_INFO, -1, "MMU: DANGER!!\n");
        }

        if (!(*cr3_init)) 
        {
            /* CR3 is unloaded yet */
            *cr3_init = 1;
            pageFaultHandler->allocatePage(coreId,fault_level,stall_addr,4096);
        }
        else
            pageFaultHandler->allocatePage(coreId,fault_level,stall_addr/page_size[fault_level],4096);

    }
    else if (temp_ptr->getType() == EventType::PAGE_FAULT_RESPONSE)
    {
        /* Handle response event from the handler - do next level walk */
        if (fault_level == 4)
        {
//            std::cout << getName().c_str() << " Core: " << coreId 
//                << " CR3 address: " << std::hex << temp_ptr->getPaddress() << std::endl;
            *cr3_init = 1; //TODO: remove?
            (*CR3) = temp_ptr->getPaddress();
            fault_level--;
            pageFaultHandler->allocatePage(coreId,fault_level,stall_addr/page_size[fault_level],4096);
        }
        else if (fault_level == 3)
        {
            if (!ptw_confined)
                (*PGD)[stall_addr/page_size[3]] = temp_ptr->getPaddress();
            else
            {
                if ((*PGD).find((stall_addr/page_size[3])%512) != (*PGD).end())
                    output->fatal(CALL_INFO, -1, "MMU: PTW DANGER.. same PGD!!\n");
                (*PGD)[(stall_addr/page_size[3])%512] = temp_ptr->getPaddress();
                (*PENDING_PAGE_FAULTS_PGD).erase((stall_addr/page_size[3])%(512));
            }
            fault_level--;
            pageFaultHandler->allocatePage(coreId,fault_level,stall_addr/page_size[fault_level],4096);

        }
        else if (fault_level == 2)
        {
            if (!ptw_confined)
                (*PUD)[stall_addr/page_size[2]] = temp_ptr->getPaddress();
            else
            {
                if ((*PUD).find((stall_addr/page_size[2])%(512*512)) != (*PUD).end())
                    output->fatal(CALL_INFO, -1, "MMU: PTW DANGER.. same PUD!!\n");
                (*PUD)[(stall_addr/page_size[2])%(512*512)] = temp_ptr->getPaddress();
                (*PENDING_PAGE_FAULTS_PUD).erase((stall_addr/page_size[2])%(512*512));
            }

//            if (temp_ptr->getSize() == page_size[2]) 
//            {
//              (*MAPPED_PAGE_SIZE1GB).insert(temp_ptr->getAddress()/page_size[2]);
//              fault_level = 0;
//              stall = false;
//              *hold = 0;
//
//            } 
//            else 
//            {
                fault_level--;
                pageFaultHandler->allocatePage(coreId,fault_level,stall_addr/page_size[fault_level],4096);
//            }

        }
        else if (fault_level == 1)
        {
            if (!ptw_confined)
                (*PMD)[stall_addr/page_size[1]] = temp_ptr->getPaddress();
            else
            {
                uint64_t offset = 512*512*512;
                if ((*PMD).find((stall_addr/page_size[1])%offset) != (*PMD).end())
                    output->fatal(CALL_INFO, -1, "MMU: PTW DANGER.. same PMD!!\n");
                (*PMD)[(stall_addr/page_size[1])%offset] = temp_ptr->getPaddress();
                (*PENDING_PAGE_FAULTS_PMD).erase((stall_addr/page_size[1])%offset);
            }

//            if (temp_ptr->getSize() == page_size[1]) 
//            {
//              (*MAPPED_PAGE_SIZE2MB).insert(temp_ptr->getAddress()/page_size[1]);
//              fault_level = 0;
//              stall = false;
//              *hold = 0;
//
//            } 
//            else 
//            {
                fault_level--;
                pageFaultHandler->allocatePage(coreId,fault_level,stall_addr/page_size[fault_level],4096);

//            }

        }
        else if (fault_level == 0)
        {
            if (!ptw_confined)
                (*PTE)[stall_addr/page_size[0]] = temp_ptr->getPaddress();
            else
            {
                uint64_t offset = (uint64_t)512*512*512*512;
                if ((*PTE).find((stall_addr/page_size[0])%offset) != (*PTE).end())
                    output->fatal(CALL_INFO, -1, "MMU: PTW DANGER.. same PTE!!\n");
                (*PTE)[(stall_addr/page_size[0])%offset] = temp_ptr->getPaddress();
            }
            SambaEvent * tse = new SambaEvent(EventType::PAGE_FAULT_SERVED);
            s_EventChan->send(tse);
        }
        else
            assert(0);
    }
    else if (temp_ptr->getType() == EventType::PAGE_FAULT_SERVED)
    {
        /* Event after final level walk - record mapping & delete pending */
        if (!ptw_confined)
        {
            (*MAPPED_PAGE_SIZE4KB).insert(stall_addr/page_size[0]);
            (*PENDING_PAGE_FAULTS).erase(stall_addr/page_size[0]);
        }
        else
        {
            uint64_t offset = (uint64_t)512*512*512*512;
            (*MAPPED_PAGE_SIZE4KB).insert((stall_addr/page_size[0])%offset);
            (*PENDING_PAGE_FAULTS_PTE).erase((stall_addr/page_size[0])%offset);
        }
    }
    else if (temp_ptr->getType()==EventType::SDACK)
    {
        //TODO
    }
    else if (temp_ptr->getType()==EventType::SHOOTDOWN)
    {
        //TODO
    }
    else
        assert(0);

    /* Delete event right after handling */
    delete temp_ptr;
}

bool PageTableWalker::recvPageFaultResp(PageFaultHandler::PageFaultHandlerPacket pkt)
{
    /* 
     * Handle event processed by PF handler
     * directly called from PageFaultHandler class 
     */
    switch (pkt.action) 
    {
        case PageFaultHandler::PageFaultHandlerAction::RESPONSE:
            {
                SambaEvent * tse = new SambaEvent(EventType::PAGE_FAULT_RESPONSE);
                tse->setResp(pkt.vAddress, pkt.pAddress,pkt.size);
                s_EventChan->send(10, tse);
            }
            break;
        default:
            output->fatal(CALL_INFO, -1, "PTW unknown request from page fault handler\n");
            break;
    }

    return true;
}

void PageTableWalker::recvResp(SST::Event * event)
{
    /* Handle PTW memory responses from memory subsystem */
    MemEvent * ev = static_cast<MemEvent*>(event);

    /* Get unique PTW ID generated by walker (tick) */
    long long int pw_id;
    if (!self_connected)
        pw_id = MEM_REQ[ev->getResponseToID()];
    else
        pw_id = MEM_REQ[ev->getID()];

    /* WID_Add[] is virtual address, WSR_PT_LEVEL[] is level of page table */
    insert_way(WID_Add[pw_id], find_victim_way(WID_Add[pw_id], WSR_PT_LEVEL[pw_id]), WSR_PT_LEVEL[pw_id]);
    WSR_READY[pw_id]=true;
    Address_t addr = WID_Add[pw_id];

    /* Avoid memory leak by deleting generated dummy requests once it returns */
    MEM_REQ.erase(ev->getID());
    delete ev;

    if (WSR_PT_LEVEL[pw_id]==0)
    {
        /* PTW is done ready to respond */
        ready_by[WID_EV[pw_id]] =  currTime + latency + 2*upper_link_latency;
        ready_by_size[WID_EV[pw_id]] = os_page_size; // FIXME: This hardcoded for now assuming the OS maps virtual pages to 4KB pages only
    }
    else
    {
        /* Generate dummy memory traffic for page table walk */
        Address_t dummy_add = rand()%10000000;

        if (emulate_faults)
        {
            /* Use actual page table base to start PTW if we have real page tables */
            if (!ptw_confined)
            {
                Address_t page_table_start = 0;
                if (WSR_PT_LEVEL[pw_id]==4)
                    page_table_start = (*PGD)[addr/page_size[3]];
                else if (WSR_PT_LEVEL[pw_id]==3)
                    page_table_start = (*PUD) [addr/page_size[2]];
                else if (WSR_PT_LEVEL[pw_id]==2)
                    page_table_start = (*PMD) [addr/page_size[1]];
                else if (WSR_PT_LEVEL[pw_id] == 1)
                    page_table_start = (*PTE) [addr/page_size[0]];

                dummy_add = page_table_start + (addr/page_size[WSR_PT_LEVEL[pw_id]-1])%512;
            }
            else
            {
                if (WSR_PT_LEVEL[pw_id]==4) {
                    dummy_add = (*CR3) + ((addr/page_size[3])%512)*8;
                }
                else if (WSR_PT_LEVEL[pw_id]==3) {
                    dummy_add = (*PGD)[(addr/page_size[3])%512] + ((addr/page_size[2])%512)*8;
                }
                else if (WSR_PT_LEVEL[pw_id]==2) {
                    dummy_add = (*PUD)[(addr/page_size[2])%(512*512)] + ((addr/page_size[1])%512)*8;}
                else if (WSR_PT_LEVEL[pw_id]==1) {
                    uint64_t offset = (uint64_t)512*512*512;
                    dummy_add = (*PMD)[(addr/page_size[1])%offset] + ((addr/page_size[0])%512)*8;
                }
                else
                    output->fatal(CALL_INFO, -1, "MMU: PTW DANGER!!\n");
            }
        }
        Address_t dummy_base_add = dummy_add & ~(line_size - 1);
        MemEvent *e = new MemEvent(getName(), dummy_add, dummy_base_add, Command::GetS);
        e->setVirtualAddress(addr);

        /* Send next PTW event to memory */
        WSR_PT_LEVEL[pw_id]--;
        MEM_REQ[e->getID()]=pw_id;
        to_mem->send(e);
    }
}

bool PageTableWalker::tick(SST::Cycle_t x)
{
    currTime = x;

    /* PTW is stalling due to fault handling, wait for completion */
    if (stall && emulate_faults)
    { 
        if (!ptw_confined)
        {
            if ((*PENDING_PAGE_FAULTS).find(stall_addr/page_size[0])==(*PENDING_PAGE_FAULTS).end()) {
                stall = false;
                *hold = 0;
            }
//            std::cout<< getName().c_str() << " Core: " << coreId << " stalled with stall address: " << stall_addr << std::endl;
        }
        else
        {
            uint64_t offset = (uint64_t)512*512*512*512;
            int release = 0;
            switch (stall_at_levels) 
            {
                case 4:
                    if ((*PENDING_PAGE_FAULTS_PGD).find((stall_addr/page_size[3])%(512))==(*PENDING_PAGE_FAULTS_PGD).end() &&
                        (*PENDING_PAGE_FAULTS_PUD).find((stall_addr/page_size[2])%(512*512))==(*PENDING_PAGE_FAULTS_PUD).end() &&
                        (*PENDING_PAGE_FAULTS_PMD).find((stall_addr/page_size[1])%(512*512*512))==(*PENDING_PAGE_FAULTS_PMD).end() &&
                        (*PENDING_PAGE_FAULTS_PTE).find((stall_addr/page_size[0])%(offset))==(*PENDING_PAGE_FAULTS_PTE).end())
                    {
                        release = 1;
                    }
                    break;

                case 3:
                    if ((*PENDING_PAGE_FAULTS_PUD).find((stall_addr/page_size[2])%(512*512))==(*PENDING_PAGE_FAULTS_PUD).end() &&
                        (*PENDING_PAGE_FAULTS_PMD).find((stall_addr/page_size[1])%(512*512*512))==(*PENDING_PAGE_FAULTS_PMD).end() &&
                        (*PENDING_PAGE_FAULTS_PTE).find((stall_addr/page_size[0])%(offset))==(*PENDING_PAGE_FAULTS_PTE).end())
                    {
                        release = 1;
                    }
                    break;

                case 2:
                    if ((*PENDING_PAGE_FAULTS_PMD).find((stall_addr/page_size[1])%(512*512*512))==(*PENDING_PAGE_FAULTS_PMD).end() &&
                        (*PENDING_PAGE_FAULTS_PTE).find((stall_addr/page_size[0])%(offset))==(*PENDING_PAGE_FAULTS_PTE).end())
                    {
                        release = 1;
                    }
                    break;

                case 1:
                    if (stall_at_PGD) 
                    {
                        if ((*PENDING_PAGE_FAULTS_PGD).find((stall_addr/page_size[3])%(512))==(*PENDING_PAGE_FAULTS_PGD).end()) 
                            release = 1;
                    }
                    else if (stall_at_PUD) 
                    {
                        if ((*PENDING_PAGE_FAULTS_PUD).find((stall_addr/page_size[2])%(512*512))==(*PENDING_PAGE_FAULTS_PUD).end()) 
                            release = 1;
                    }
                    else if (stall_at_PMD) 
                    {
                        if ((*PENDING_PAGE_FAULTS_PMD).find((stall_addr/page_size[1])%(512*512*512))==(*PENDING_PAGE_FAULTS_PMD).end()) 
                            release = 1;
                    }
                    else if (stall_at_PTE) 
                    {
                        if ((*PENDING_PAGE_FAULTS_PTE).find((stall_addr/page_size[0])%(offset))==(*PENDING_PAGE_FAULTS_PTE).end()) 
                            release = 1;
                    }
                    else 
                        output->fatal(CALL_INFO, -1, "MMU: PTW DANGER!!.. stall at level not recognized..\n");
                    break;

                default:
                    output->fatal(CALL_INFO, -1, "MMU: PTW DANGER!!.. stall at levels not recognized\n");
            }

            if (release) 
            {
                stall = false;
                *hold = 0;
//                std::cerr<< Owner->getName().c_str() << " Core: " << coreId 
//                    << " stalled address: " << stall_addr << " released: "<<stall_at_levels<< std::endl;
            }
        }
        return false;
    }


    /* Process PTW requests on the input side */
    int dispatched=0;

    std::vector<MemHierarchy::MemEventBase *>::iterator st_1,en_1;
    st_1 = not_serviced.begin();
    en_1 = not_serviced.end();
    for (;st_1!=not_serviced.end() && !(*shootdown) && !(*hold); st_1++)
    {
        dispatched++;

        /* Check availablility of PTWC port */
        if (dispatched > max_width)
            break;

        MemHierarchy::MemEventBase* ev = *st_1;
        Address_t addr = ((MemEvent*) ev)->getVirtualAddress();

        /* Check fault or not */
        if (emulate_faults==1)
        {
            bool fault = true;
            if (!ptw_confined)
            {
                /* Sneak-peek if access will be page fault or not */
                if ((*MAPPED_PAGE_SIZE4KB).find(addr/page_size[0])!=(*MAPPED_PAGE_SIZE4KB).end() || 
                    (*MAPPED_PAGE_SIZE2MB).find(addr/page_size[1])!=(*MAPPED_PAGE_SIZE2MB).end() || 
                    (*MAPPED_PAGE_SIZE1GB).find(addr/page_size[2])!=(*MAPPED_PAGE_SIZE1GB).end())
                    fault = false;

                /* If page fault, generate faulting event */
                if (fault)
                {
                    stall_addr = addr;

                    /* Check whether same faulting VA is already handling, if yes, wait */
                    if ((*PENDING_PAGE_FAULTS).find(addr/page_size[0])==(*PENDING_PAGE_FAULTS).end()) 
                    {
                        (*PENDING_PAGE_FAULTS).insert(addr/page_size[0]);
                        SambaEvent * tse = new SambaEvent(EventType::PAGE_FAULT);
                        tse->setResp(addr,0,4096);
                        s_EventChan->send(10, tse);

//                        std::cout<< getName().c_str() << " Core id: " << coreId << " Fault at address "<<addr<<std::endl;
                    }

                    stall = true;
                    *hold = 1;
                    return false;
                }
            }
            else
            {
                /* Flow similar to previous if-case */
                uint64_t offset = (uint64_t)512*512*512*512;
                if ((*MAPPED_PAGE_SIZE4KB).find((addr/page_size[0])%offset)!=(*MAPPED_PAGE_SIZE4KB).end() || 
                    (*MAPPED_PAGE_SIZE2MB).find((addr/page_size[1])%(512*512*512))!=(*MAPPED_PAGE_SIZE2MB).end() || 
                    (*MAPPED_PAGE_SIZE1GB).find((addr/page_size[2])%(512*512))!=(*MAPPED_PAGE_SIZE1GB).end())
                    fault = false;

                if (fault)
                {
                    stall_addr = addr;
                    if (to_mem!=NULL) 
                    {
                        if ((*PGD).find((addr/page_size[3])%512) == (*PGD).end()) 
                        {
                            stall_at_levels = 1;
                            stall_at_PGD = 1;
                            stall_at_PUD = 0;
                            stall_at_PMD = 0;
                            stall_at_PTE = 0;
                            if ((*PENDING_PAGE_FAULTS_PGD).find((addr/page_size[3])%(512))==(*PENDING_PAGE_FAULTS_PGD).end()) 
                            {
                                (*PENDING_PAGE_FAULTS_PGD).insert((addr/page_size[3])%512);
                                (*PENDING_PAGE_FAULTS_PUD).insert((addr/page_size[2])%(512*512));
                                (*PENDING_PAGE_FAULTS_PMD).insert((addr/page_size[1])%(512*512*512));
                                (*PENDING_PAGE_FAULTS_PTE).insert((addr/page_size[0])%(offset));
                                stall_at_levels += 3;
                                SambaEvent * tse = new SambaEvent(EventType::PAGE_FAULT);
                                tse->setResp(addr,0,4096);
                                s_EventChan->send(tse);
                            }
                        }
                        else if ((*PUD).find((addr/page_size[2])%(512*512)) == (*PUD).end()) 
                        {
                            stall_at_levels = 1;
                            stall_at_PGD = 0;
                            stall_at_PUD = 1;
                            stall_at_PMD = 0;
                            stall_at_PTE = 0;
                            if ((*PENDING_PAGE_FAULTS_PUD).find((addr/page_size[2])%(512*512))==(*PENDING_PAGE_FAULTS_PUD).end()) 
                            {
                                (*PENDING_PAGE_FAULTS_PUD).insert((addr/page_size[2])%(512*512));
                                (*PENDING_PAGE_FAULTS_PMD).insert((addr/page_size[1])%(512*512*512));
                                (*PENDING_PAGE_FAULTS_PTE).insert((addr/page_size[0])%(offset));
                                stall_at_levels += 2;
                                SambaEvent * tse = new SambaEvent(EventType::PAGE_FAULT);
                                tse->setResp(addr,0,4096);
                                s_EventChan->send(tse);
                            }
                        }
                        else if ((*PMD).find((addr/page_size[1])%(512*512*512)) == (*PMD).end()) 
                        {
                            stall_at_levels = 1;
                            stall_at_PGD = 0;
                            stall_at_PUD = 0;
                            stall_at_PMD = 1;
                            stall_at_PTE = 0;
                            if ((*PENDING_PAGE_FAULTS_PMD).find((addr/page_size[1])%(512*512*512))==(*PENDING_PAGE_FAULTS_PMD).end()) 
                            {
                                (*PENDING_PAGE_FAULTS_PMD).insert((addr/page_size[1])%(512*512*512));
                                (*PENDING_PAGE_FAULTS_PTE).insert((addr/page_size[0])%(offset));
                                stall_at_levels += 1;
                                SambaEvent * tse = new SambaEvent(EventType::PAGE_FAULT);
                                tse->setResp(addr,0,4096);
                                s_EventChan->send(tse);
                            }
                        }
                        else if ((*PTE).find((addr/page_size[0])%(offset)) == (*PTE).end()) 
                        {
                            stall_at_levels = 1;
                            stall_at_PGD = 0;
                            stall_at_PUD = 0;
                            stall_at_PMD = 0;
                            stall_at_PTE = 1;
                            if ((*PENDING_PAGE_FAULTS_PTE).find((addr/page_size[0])%(offset))==(*PENDING_PAGE_FAULTS_PTE).end()) 
                            {
                                (*PENDING_PAGE_FAULTS_PTE).insert((addr/page_size[0])%(offset));
                                SambaEvent * tse = new SambaEvent(EventType::PAGE_FAULT);
                                tse->setResp(addr,0,4096);
                                s_EventChan->send(tse);
                            }
                        }
                        else 
                        {
                            //TODO: check this case is possible or not by printing
                            //if it is impossible case assert it
                        }
                    }
                    else 
                    {
                        stall_at_levels = 1;
                        stall_at_PGD = 0;
                        stall_at_PUD = 0;
                        stall_at_PMD = 0;
                        stall_at_PTE = 1;
                        if ((*PENDING_PAGE_FAULTS_PTE).find((addr/page_size[0])%(offset))==(*PENDING_PAGE_FAULTS_PTE).end()) 
                        {
                            (*PENDING_PAGE_FAULTS_PTE).insert((addr/page_size[0])%(offset));
                            SambaEvent * tse = new SambaEvent(EventType::PAGE_FAULT);
                            tse->setResp(addr,0,4096);
                            s_EventChan->send(tse);
                        }
                    }
                    return false;
                }
            }
        }

        /* We check the PTWC-es in parallel to find if it hits */
        int hit_id=0; //TODO: remove?
        int k;
        for (k=0; k<pt_levels; k++)
        {
            if (check_hit(addr, k))
            {
                hit_id=k;
                break;
            }
        }

        /* Check if we found the entry in the PTWC of PTEs */
        if (k==0)
        {
            hits++;
            statPageTableWalkerHits->addData(1);
            
            update_lru(addr, hit_id);
            if (parallel_mode)
                ready_by[ev] = x;
            else
                ready_by[ev] = x + latency;

            ready_by_size[ev] = os_page_size; //page_size[hit_id]/1024;

            st_1 = not_serviced.erase(st_1);
        }
        else
        {
            /* Hack to reduce the number of walks for larges 2MB/1GB */ 
            if (os_page_size == 2048)
                k = max(k-1, 1);
            else if (os_page_size == 1024*1024)
                k = max(k-2, 1);

            /* Trigger PTWs for PTWC misses (recvResp will do remaining) */
            if ((int) pending_misses.size() < (int) max_outstanding)
            {
                misses++;
                statPageTableWalkerMisses->addData(1);
                
                pending_misses.push_back(*st_1);
                if (to_mem!=nullptr)
                {
                    /* Generate dummy memory traffic for page table walk */
                    WID_EV[++mmu_id] = (*st_1);
                    Address_t dummy_add = rand()%10000000;

                    if (emulate_faults)
                    {
                        /* Use actual page table base to start PTW if we have real page tables */
                        if (!ptw_confined)
                            dummy_add = (*CR3) + (addr/page_size[2])%512;
                        else
                        {
                            if (k==4) {
                                dummy_add = (*CR3) + ((addr/page_size[3])%512)*8;
                            }
                            else if (k==3) {
                                dummy_add = (*PGD)[(addr/page_size[3])%512] + ((addr/page_size[2])%512)*8;
                            }
                            else if (k==2) {
                                dummy_add = (*PUD)[(addr/page_size[2])%(512*512)] + ((addr/page_size[1])%512)*8;
                            }
                            else if (k==1) {
                                uint64_t offset = (uint64_t)512*512*512;
                                dummy_add = (*PMD)[(addr/page_size[1])%offset] + ((addr/page_size[0])%512)*8;
                            }
                            else
                                output->fatal(CALL_INFO, -1, "MMU: PTW DANGER!!\n");
                        }
                    }

                    Address_t dummy_base_add = dummy_add & ~(line_size - 1); //TODO-hklee: need to add offset for STU
                    MemEvent *e = new MemEvent(getName(), dummy_add, dummy_base_add, Command::GetS);
                    e->setVirtualAddress(addr);

                    /* Record this walk request into WSR_ and WID_ structs */
                    WSR_PT_LEVEL[mmu_id] = k-1;
//                    WID_EV[mmu_id] = e;
                    WID_Add[mmu_id] = addr;
                    WSR_READY[mmu_id] = false;

                    /* Send PTW event to memory */
//                    std::cout<<"Sending a new request with address "<<std::hex<<dummy_add<<std::endl;
                    MEM_REQ[e->getID()]=mmu_id;
                    to_mem->send(e);
                }
                else // (to_mem == nullptr)
                {
                    /* 
                     * Just wait for appropriate latency if there is not memeomry link 
                     * XXX: Upper link is substitued for sending miss request and recvin it
                     * This is hard-coded for last-level as memory access walk latency
                     */

                    ready_by[ev] = x + latency + 2*upper_link_latency + page_walk_latency;
                    ready_by_size[ev] = os_page_size; // FIXME: hardcoded for assuming 4KB pages only
                }

                st_1 = not_serviced.erase(st_1);
            }
        }

        /* Check emptiness */
        if (st_1 == not_serviced.end())
            break;
    }

    std::map<MemHierarchy::MemEventBase *, SST::Cycle_t, MemEventPtrCompare>::iterator st, en;
    st = ready_by.begin();
    en = ready_by.end();

    while(st!=en)
    {
        bool deleted=false;
        if (st->second <= currTime) // if this event is ready
        {
            Address_t addr = ((MemEvent*) st->first)->getVirtualAddress();

            // Double checking that we actually still don't have it inserted
            //std::cout<<"The address is"<<addr<<std::endl;
            if (!check_hit(addr, 0))
            {
                insert_way(addr, find_victim_way(addr, 0), 0);
                update_lru(addr, 0);
            }
            else
                update_lru(addr, 0);

            service_back->push_back(st->first);

            if (emulate_faults)
            {
                if (!ptw_confined)
                {
                    if ((*PTE).find(addr/4096)==(*PTE).end())
                    {
                        std::cout << "******* Major issue is in Page Table Walker **** " << std::endl;
                        std::cout << "The address is "<< hex << addr << " (" << addr / 4096 << ")" << std::endl;
                    }
                }
                else
                {
                    uint64_t offset = (uint64_t)512*512*512*512;
                    if ((*PTE).find((addr/4096)%offset)==(*PTE).end())
                    {
                        std::cout << "******* Major issue is in Page Table Walker **** " << std::endl;
                        std::cout << "The address is "<< hex << addr << " (" << addr / 4096 << ")" << std::endl;
                    }
                }
            }

            (*service_back_size)[st->first]=ready_by_size[st->first];

            ready_by_size.erase(st->first);

            deleted = true;

            // Deleting it from pending requests
            std::vector<MemHierarchy::MemEventBase *>::iterator st2, en2;
            st2 = pending_misses.begin();
            en2 = pending_misses.end();

            while(st2!=en2)
            {
                if (*st2 == st->first)
                {
                    pending_misses.erase(st2);
                    break;
                }
                st2++;
            }

            ready_by.erase(st);

        }
        if (deleted)
            st=ready_by.begin();
        else
            st++;

    }

    return false;
}

void PageTableWalker::initaitePageMigration(Address_t vaddress, Address_t paddress)
{

    /*
     * Check if any shootdon request with same address is pending.
     * If No, send a TLB shootdown request to page fault handler and stall
     * If Yes, Just stall.
     */
    //std::cout << getName().c_str() << " Core ID: " << coreId << " sending TLB shootdown with address: " << std::hex << vaddress << " new paddress: " << paddress << std::endl;
    stall_addr = vaddress;
    /*
    if ((*PENDING_SHOOTDOWN_EVENTS).find(vaddress/page_size[0]) == (*PENDING_SHOOTDOWN_EVENTS).end()) {
        (*PENDING_SHOOTDOWN_EVENTS).insert(vaddress/page_size[0]);
        (*PENDING_PAGE_FAULTS).insert(vaddress/page_size[0]);      //add to pending page faults list
        (*MAPPED_PAGE_SIZE4KB).erase(vaddress/page_size[0]);    //unmap the page
        SambaEvent * tse = new SambaEvent(EventType::SHOOTDOWN);
        tse->setResp(vaddress/page_size[0],paddress,4096);
        s_EventChan->send(10, tse);
    }

    //std::cout << getName().c_str() << " Core ID: " << coreId << " stall address " << std::hex << stall_addr << " vaddress index "<< vaddress/page_size[0] << std::endl;
    */
}

void PageTableWalker::insert_way(Address_t vaddr, int way, int struct_id)
{

    int set=abs_int_Samba((vaddr/page_size[struct_id])%sets[struct_id]);
    tags[struct_id][set][way]=vaddr/page_size[struct_id];
    valid[struct_id][set][way]=true;
}

/* Translate and update the statistics of miss/hit */
Address_t PageTableWalker::translate(Address_t vadd)
{
    return 1;
}


/* Invalidate TLB entries */
void PageTableWalker::invalidate(Address_t vadd, int id)
{
    for (int id=0; id<pt_levels; id++)
    {
        int set= abs_int_Samba((vadd*page_size[0]/page_size[id])%sets[id]);
        for (int i=0; i<assoc[id]; i++) {
            if (tags[id][set][i]==vadd*page_size[0]/page_size[id] && valid[id][set][i]) {
                valid[id][set][i] = false;
                break;
            }
        }
    }
}

/* done with shootdown, send shootdown ack */
void PageTableWalker::sendShootdownAck(int sd_delay, int page_swapping_delay)
{
    s_EventChan->send(sd_delay + (num_pages_migrated)*page_swapping_delay, new SambaEvent(SambaComponent::EventType::SDACK));
}

/* Find if VA exists in PTWC */
bool PageTableWalker::check_hit(Address_t vadd, int struct_id)
{
    int set= abs_int_Samba((vadd/page_size[struct_id])%sets[struct_id]);

    for (int i=0; i<assoc[struct_id];i++)
        if (tags[struct_id][set][i]==vadd/page_size[struct_id])
            return valid[struct_id][set][i];

    return false;
}

/* To insert the translaiton */
int PageTableWalker::find_victim_way(Address_t vadd, int struct_id)
{

    int set= abs_int_Samba((vadd/page_size[struct_id])%sets[struct_id]);

    for (int i=0; i<assoc[struct_id]; i++)
        if (lru[struct_id][set][i]==(assoc[struct_id]-1))
            return i;


    return 0;
}

/* This function updates the LRU policy for a given address */
void PageTableWalker::update_lru(Address_t vaddr, int struct_id)
{

    int lru_place=assoc[struct_id]-1;

    int set= abs_int_Samba((vaddr/page_size[struct_id])%sets[struct_id]);
    for (int i=0; i<assoc[struct_id];i++)
        if (tags[struct_id][set][i]==vaddr/page_size[struct_id])
        {
            lru_place = lru[struct_id][set][i];
            break;
        }
    for (int i=0; i<assoc[struct_id];i++)
    {
        if (lru[struct_id][set][i]==lru_place)
            lru[struct_id][set][i]=0;
        else if (lru[struct_id][set][i]<lru_place)
            lru[struct_id][set][i]++;
    }


}


