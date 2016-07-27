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
 * onvm_mgr_nf.c - for all nf manager functions
 ********************************************************************/

#include "onvm_includes.h"
#include "onvm_mgr_nf.h"

uint16_t next_instance_id=0;

/**
 * Helper function to determine if an item in the clients array represents a valid NF
 * A "valid" NF consists of:
 *  - A non-null index that also contains an associated info struct
 *  - A status set to NF_RUNNING
 */
inline int
onvm_mgr_nf_is_valid_nf(struct client *cl)
{
        return cl && cl->info && cl->info->status == NF_RUNNING;
}

/**
 * Verifies that the next client id the manager gives out is unused
 * This lets us account for the case where an NF has a manually specified id and we overwrite it
 * This function modifies next_instance_id to be the proper value
 */
int onvm_mgr_nf_find_next_instance_id(void) {
        struct client *cl;
        while (next_instance_id < MAX_CLIENTS) {
                cl = &clients[next_instance_id];
                if (!onvm_mgr_nf_is_valid_nf(cl))
                        break;
                next_instance_id++;
        }
        return next_instance_id;

}

/**
 * Set up a newly started NF
 * Assign it an ID (if it hasn't self-declared)
 * Store info struct in our internal list of clients
 * Returns 1 (TRUE) on successful start, 0 if there is an error (ID conflict)
 */
inline int onvm_mgr_nf_start_new_nf(struct onvm_nf_info *nf_info)
{
        //TODO dynamically allocate memory here - make rx/tx ring
        // take code from init_shm_rings in init.c
        // flush rx/tx queue at the this index to start clean?

        // if NF passed its own id on the command line, don't assign here
        // assume user is smart enough to avoid duplicates
        uint16_t nf_id = nf_info->instance_id == (uint16_t)NF_NO_ID
                ? next_instance_id++
                : nf_info->instance_id;

        if (nf_id >= MAX_CLIENTS) {
                // There are no more available IDs for this NF
                nf_info->status = NF_NO_IDS;
                return 0;
        }

        if (onvm_mgr_nf_is_valid_nf(&clients[nf_id])) {
                // This NF is trying to declare an ID already in use
                nf_info->status = NF_ID_CONFLICT;
                return 0;
        }

        // Keep reference to this NF in the manager
        nf_info->instance_id = nf_id;
        clients[nf_id].info = nf_info;
        clients[nf_id].instance_id = nf_id;

        // Register this NF running within its service
        uint16_t service_count = nf_per_service_count[nf_info->service_id]++;
        service_to_nf[nf_info->service_id][service_count] = nf_id;

        // Let the NF continue its init process
        nf_info->status = NF_STARTING;
        return 1;
}

/**
 * Clean up after an NF has stopped
 * Remove references to soon-to-be-freed info struct
 * Clean up stats values
 */
inline void onvm_mgr_nf_stop_running_nf(struct onvm_nf_info *nf_info)
{
        uint16_t nf_id = nf_info->instance_id;
        uint16_t service_id = nf_info->service_id;
        int mapIndex;
        struct rte_mempool *nf_info_mp;

        /* Clean up dangling pointers to info struct */
        clients[nf_id].info = NULL;

        /* Reset stats */
        clients[nf_id].stats.rx = clients[nf_id].stats.rx_drop = 0;
        clients[nf_id].stats.act_drop = clients[nf_id].stats.act_tonf = 0;
        clients[nf_id].stats.act_next = clients[nf_id].stats.act_out = 0;

        /* Remove this NF from the service map.
         * Need to shift all elements past it in the array left to avoid gaps */
        nf_per_service_count[service_id]--;
        for(mapIndex = 0; mapIndex < MAX_CLIENTS_PER_SERVICE; mapIndex++) {
                if (service_to_nf[service_id][mapIndex] == nf_id) {
                        break;
                }
        }

        if (mapIndex < MAX_CLIENTS_PER_SERVICE) { // sanity error check
                service_to_nf[service_id][mapIndex] = 0;
                for (; mapIndex < MAX_CLIENTS_PER_SERVICE - 1; mapIndex++) {
                        // Shift the NULL to the end of the array
                        if (service_to_nf[service_id][mapIndex + 1] == 0) {
                                // Short circuit when we reach the end of this service's list
                                break;
                        }
                        service_to_nf[service_id][mapIndex] = service_to_nf[service_id][mapIndex + 1];
                        service_to_nf[service_id][mapIndex + 1] = 0;
                }
        }

        /* Free info struct */
        /* Lookup mempool for nf_info struct */
        nf_info_mp = rte_mempool_lookup(_NF_MEMPOOL_NAME);
        if (nf_info_mp == NULL)
                return;

        rte_mempool_put(nf_info_mp, (void*)nf_info);
}

void onvm_mgr_nf_do_check_new_nf_status(void) {
        int i;
        void *new_nfs[MAX_CLIENTS];
        struct onvm_nf_info *nf;
        int num_new_nfs = rte_ring_count(nf_info_queue);
        int dequeue_val = rte_ring_dequeue_bulk(nf_info_queue, new_nfs, num_new_nfs);

        if (dequeue_val != 0)
                return;

	num_clients=0;
        for (i = 0; i < num_new_nfs; i++) {
                nf = (struct onvm_nf_info *)new_nfs[i];

                // Sets next_instance_id variable to next available
                onvm_mgr_nf_find_next_instance_id();

                if (nf->status == NF_WAITING_FOR_ID) {
                        /* We're starting up a new NF.
                         * Function returns TRUE on successful start */
                        if (onvm_mgr_nf_start_new_nf(nf))
                                num_clients++;
                } else if (nf->status == NF_STOPPED) {
                        /* An existing NF is stopping */
                        onvm_mgr_nf_stop_running_nf(nf);
                        num_clients--;
                }
        }
}

/**
 * This function take a service id input and returns an instance id to route the packet to
 * This uses the packet's RSS Hash mod the number of available services to decide
 * Returns 0 (manager reserved ID) if no NFs are available from the desired service
 */
inline uint16_t onvm_mgr_nf_service_to_nf_map(uint16_t service_id, struct rte_mbuf *pkt) {
        uint16_t num_nfs_available = nf_per_service_count[service_id];

        if (num_nfs_available == 0)
                return 0;

        uint16_t instance_index = pkt->hash.rss % num_nfs_available;
        uint16_t instance_id = service_to_nf[service_id][instance_index];
        return instance_id;
}
