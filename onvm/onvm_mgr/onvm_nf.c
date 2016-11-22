/*********************************************************************
 *                     openNetVM
 *              https://sdnfv.github.io
 *
 *   BSD LICENSE
 *
 *   Copyright(c)
 *            2015-2016 George Washington University
 *            2015-2016 University of California Riverside
 *            2010-2014 Intel Corporation. All rights reserved.
 *   All rights reserved.
 *
 *   Redistribution and use in source and binary forms, with or without
 *   modification, are permitted provided that the following conditions
 *   are met:
 *
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in
 *       the documentation and/or other materials provided with the
 *       distribution.
 *     * Neither the name of Intel Corporation nor the names of its
 *       contributors may be used to endorse or promote products derived
 *       from this software without specific prior written permission.
 *
 *   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 *   "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 *   LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 *   A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 *   OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 *   SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 *   LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 *   DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 *   THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 *   (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 *   OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 ********************************************************************/


/******************************************************************************

                              onvm_nf.c

       This file contains all functions related to NF management.

******************************************************************************/


#include "onvm_mgr.h"
#include "onvm_nf.h"
#include "onvm_stats.h"

uint16_t next_instance_id = 0;

#ifdef ENABLE_NF_BACKPRESSURE
// Global mode variables (default service chain without flow_Table entry: can support only 1 flow (i.e all flows have same NFs)
uint16_t downstream_nf_overflow = 0;
uint16_t highest_downstream_nf_service_id=0;
uint16_t lowest_upstream_to_throttle = 0;
uint64_t throttle_count = 0;
#endif // ENABLE_NF_BACKPRESSURE

void compute_and_assign_nf_cgroup_weight(void);
void monitor_nf_node_liveliness_via_pid_monitoring(void);
/********************************Interfaces***********************************/
void compute_and_assign_nf_cgroup_weight(void) {
#if defined (USE_CGROUPS_PER_NF_INSTANCE)
        typedef struct nf_core_and_cc_info {
                uint32_t total_comp_cost;   //overflow possible
                uint16_t total_nf_count;
        }nf_core_and_cc_info_t;

        static int update_rate = 0;
        if (update_rate != 10) {
                update_rate++;
                return;
        }
        update_rate = 0;

        //nf_core_and_cc_info_t nf_pool_per_core[rte_lcore_count()+1]; // = {{0,0},};
        nf_core_and_cc_info_t nf_pool_per_core[256]; // = {{0,0},};
        //uint16_t nf_pool_per_core[rte_lcore_count()+1] = {0,};

        uint16_t nf_id = 0;
        memset(nf_pool_per_core, 0, sizeof(nf_pool_per_core));

        //First build the total cost and contention info per core
        for (nf_id=0; nf_id < MAX_CLIENTS; nf_id++) {
                if (onvm_nf_is_valid(&clients[nf_id])){
                        nf_pool_per_core[clients[nf_id].info->core_id].total_comp_cost += clients[nf_id].info->comp_cost;
                        nf_pool_per_core[clients[nf_id].info->core_id].total_nf_count++;
                }
        }

        //evaluate and assign the cost of each NF
        for (nf_id=0; nf_id < MAX_CLIENTS; nf_id++) {
                if ((onvm_nf_is_valid(&clients[nf_id])) && (clients[nf_id].info->comp_cost) && ((nf_pool_per_core[clients[nf_id].info->core_id].total_comp_cost))) {
                        // share of NF = 1024* NF_comp_cost/Total_comp_cost
                        //Note: ideal share of NF is 100%(1024) so for N NFs sharing core => N*100 or (N*1024) then divide the cost proportionally
                        clients[nf_id].info->cpu_share = (uint32_t) ((1024*nf_pool_per_core[clients[nf_id].info->core_id].total_nf_count)*(clients[nf_id].info->comp_cost))/((nf_pool_per_core[clients[nf_id].info->core_id].total_comp_cost));


                        printf("\n ***** Client [%d] with cost [%d] on core [%d] with total_demand [%d] shared by [%d] NFs, got cpu share [%d]***** \n ", clients[nf_id].info->instance_id, clients[nf_id].info->comp_cost, clients[nf_id].info->core_id,
                                                                                                                                                   nf_pool_per_core[clients[nf_id].info->core_id].total_comp_cost,
                                                                                                                                                   nf_pool_per_core[clients[nf_id].info->core_id].total_nf_count,
                                                                                                                                                   clients[nf_id].info->cpu_share);

                        //set_cgroup_nf_cpu_share(clients[nf_id].info->instance_id, clients[nf_id].info->cpu_share);
                        set_cgroup_nf_cpu_share_from_onvm_mgr(clients[nf_id].info->instance_id, clients[nf_id].info->cpu_share);
                }
        }
#endif // #if defined (USE_CGROUPS_PER_NF_INSTANCE)
}

//#include <sys/types.h>
//#include <signal.h>
void monitor_nf_node_liveliness_via_pid_monitoring(void) {
        uint16_t nf_id = 0;

        for (; nf_id < MAX_CLIENTS; nf_id++) {
                if (onvm_nf_is_valid(&clients[nf_id])){
                        if (kill(clients[nf_id].info->pid, 0)) {
                                //clients[nf_id].info->status = NF_STOPPED;
                                printf("\n\n******* Moving NF with InstanceID:%d state %d to STOPPED\n\n",clients[nf_id].info->instance_id, clients[nf_id].info->status);
                                clients[nf_id].info->status = NF_STOPPED;
                                //**** TO DO: Take necessary actions here: It still doesn't clean-up until the new_nf_pool is populated by adding/killing another NF instance.
                                rte_ring_enqueue(nf_info_queue, clients[nf_id].info);
                                rte_mempool_put(nf_info_pool, clients[nf_id].info);
                                // Still the IDs are not recycled.. missing some additional changes:: found bug in the way the IDs are recycled-- fixed change in onvm_nf_next_instance_id()
                        }
                }
        }
}

inline int
onvm_nf_is_valid(struct client *cl) {
        return cl && cl->info && cl->info->status == NF_RUNNING;
}


uint16_t
onvm_nf_next_instance_id(void) {
        struct client *cl;
        /*
        while (next_instance_id < MAX_CLIENTS) {
                cl = &clients[next_instance_id];
                if (!onvm_nf_is_valid(cl))
                        break;
                next_instance_id++;
        }
        return next_instance_id;
        */

        if(next_instance_id >= MAX_CLIENTS) {
                next_instance_id = 1;   // don't know why the id=0 is reserved?
        }
        while (next_instance_id < MAX_CLIENTS) {
                cl = &clients[next_instance_id];
                if (!onvm_nf_is_valid(cl))
                        break;
                next_instance_id++;
        }
        return next_instance_id++;

}


void
onvm_nf_check_status(void) {
        int i;
        void *new_nfs[MAX_CLIENTS];
        struct onvm_nf_info *nf;
        int num_new_nfs = rte_ring_count(nf_info_queue);

        if (rte_ring_dequeue_bulk(nf_info_queue, new_nfs, num_new_nfs) != 0)
        #if !defined(USE_CGROUPS_PER_NF_INSTANCE) || !defined (ENABLE_DYNAMIC_CGROUP_WEIGHT_ADJUSTMENT)
                return;
        #else
        {}   // do nothing
        #endif //USE_CGROUPS_PER_NF_INSTANCE

        for (i = 0; i < num_new_nfs; i++) {
                nf = (struct onvm_nf_info *) new_nfs[i];

                if (nf->status == NF_WAITING_FOR_ID) {
                        if (!onvm_nf_start(nf))
                                num_clients++;
                } else if (nf->status == NF_STOPPED) {
                        if (!onvm_nf_stop(nf))
                                num_clients--;
                }
        }

        /* Add PID monitoring to assert active NFs (non crashed) */
        monitor_nf_node_liveliness_via_pid_monitoring();

        /* Ideal location to re-compute the NF weight */
        #if defined (USE_CGROUPS_PER_NF_INSTANCE) && defined(ENABLE_DYNAMIC_CGROUP_WEIGHT_ADJUSTMENT)
        compute_and_assign_nf_cgroup_weight();
        #endif //USE_CGROUPS_PER_NF_INSTANCE
}


inline uint16_t
onvm_nf_service_to_nf_map(uint16_t service_id, struct rte_mbuf *pkt) {
        uint16_t num_nfs_available = nf_per_service_count[service_id];

        if (num_nfs_available == 0)
                return 0;

        if (pkt == NULL)
                return 0;

        uint16_t instance_index = pkt->hash.rss % num_nfs_available;
        uint16_t instance_id = services[service_id][instance_index];
        return instance_id;
}


/******************************Internal functions*****************************/


inline int
onvm_nf_start(struct onvm_nf_info *nf_info) {
        // TODO dynamically allocate memory here - make rx/tx ring
        // take code from init_shm_rings in init.c
        // flush rx/tx queue at the this index to start clean?

        if(nf_info == NULL)
                return 1;

        // if NF passed its own id on the command line, don't assign here
        // assume user is smart enough to avoid duplicates
        uint16_t nf_id = nf_info->instance_id == (uint16_t)NF_NO_ID
                ? onvm_nf_next_instance_id()
                : nf_info->instance_id;

        if (nf_id >= MAX_CLIENTS) {
                // There are no more available IDs for this NF
                printf("\n Invalid NF_ID! Rejecting the NF Start!\n ");
                nf_info->status = NF_NO_IDS;
                return 1;
        }

        if (onvm_nf_is_valid(&clients[nf_id])) {
                // This NF is trying to declare an ID already in use
                printf("\n Invalid NF (conflicting ID!!) Rejecting the NF Start!\n ");
                nf_info->status = NF_ID_CONFLICT;
                return 1;
        }

        // Keep reference to this NF in the manager
        nf_info->instance_id = nf_id;
        clients[nf_id].info = nf_info;
        clients[nf_id].instance_id = nf_id;

        // Register this NF running within its service
        uint16_t service_count = nf_per_service_count[nf_info->service_id]++;
        services[nf_info->service_id][service_count] = nf_id;

        // Let the NF continue its init process
        nf_info->status = NF_STARTING;
        return 0;
}


inline int
onvm_nf_stop(struct onvm_nf_info *nf_info) {
        uint16_t nf_id;
        uint16_t service_id;
        int mapIndex;
        struct rte_mempool *nf_info_mp;

        if(nf_info == NULL){
                printf(" Null Entry for NF! Bad request for Stop!!\n ");
                return 1;
        }


        nf_id = nf_info->instance_id;
        service_id = nf_info->service_id;

        /* Clean up dangling pointers to info struct */
        clients[nf_id].info = NULL;

        /* Reset stats */
        onvm_stats_clear_client(nf_id);

        /* Remove this NF from the service map.
         * Need to shift all elements past it in the array left to avoid gaps */
        nf_per_service_count[service_id]--;
        for (mapIndex = 0; mapIndex < MAX_CLIENTS_PER_SERVICE; mapIndex++) {
                if (services[service_id][mapIndex] == nf_id) {
                        break;
                }
        }

        if (mapIndex < MAX_CLIENTS_PER_SERVICE) {  // sanity error check
                services[service_id][mapIndex] = 0;
                for (; mapIndex < MAX_CLIENTS_PER_SERVICE - 1; mapIndex++) {
                        // Shift the NULL to the end of the array
                        if (services[service_id][mapIndex + 1] == 0) {
                                // Short circuit when we reach the end of this service's list
                                break;
                        }
                        services[service_id][mapIndex] = services[service_id][mapIndex + 1];
                        services[service_id][mapIndex + 1] = 0;
                }
        }

        /* Free info struct */
        /* Lookup mempool for nf_info struct */
        nf_info_mp = rte_mempool_lookup(_NF_MEMPOOL_NAME);
        if (nf_info_mp == NULL) {
                printf(" Null Entry for NF_INFO_MAP in MEMPOOL! No pool available!!\n ");
                return 1;
        }
        rte_mempool_put(nf_info_mp, (void*)nf_info);
        printf(" NF reclaimed to NF pool!!! \n ");
        return 0;
}
