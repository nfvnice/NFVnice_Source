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

                                 onvm_nf.h

     This file contains the prototypes for all functions related to packet
     processing.

******************************************************************************/


#ifndef _ONVM_NF_H_
#define _ONVM_NF_H_

extern uint16_t next_instance_id;
extern struct wakeup_info *wakeup_infos;

#define MAX_CORES_ON_NODE (64)
//Data structure to sort out all active NFs on each core
typedef struct nfs_per_core {
        uint16_t sorted;                    //status if the nf_ids list is sorted for wake-up
        uint16_t count;                     //count of nfs in the list of nf_ids
        uint32_t nf_ids[MAX_CLIENTS];       //id of the nf <populated in-order and sorted in order needed for wake-up
        uint64_t run_time[MAX_CLIENTS] ;    //run time of the each of the id, indexed by id itself(not sorted)
}nfs_per_core_t;

typedef struct nf_schedule_info {
        uint16_t sorted;
        nfs_per_core_t nf_list_per_core[MAX_CORES_ON_NODE];
}nf_schedule_info_t;
extern nf_schedule_info_t nf_sched_param;
//extern nfs_per_core_t nf_list_per_core[MAX_CORES_ON_NODE];


#ifdef ENABLE_NF_BACKPRESSURE
// Global mode variables (default service chain without flow_Table entry: can support only 1 flow (i.e all flows have same NFs)
extern uint8_t  global_bkpr_mode;
extern uint16_t downstream_nf_overflow;
extern uint16_t highest_downstream_nf_service_id;
extern uint16_t lowest_upstream_to_throttle;
extern uint64_t throttle_count;
#endif //ENABLE_NF_BACKPRESSURE

/********************************Interfaces***********************************/


/*
 * Interface checking if a given nf is "valid", meaning if it's running.
 *
 * Input  : a pointer to the nf
 * Output : a boolean answer 
 *
 */
inline int
onvm_nf_is_valid(struct client *cl);


/*
 * Interface giving the smallest unsigned integer unused for a NF instance.
 *
 * Output : the unsigned integer 
 *
 */
uint16_t
onvm_nf_next_instance_id(void);


/*
 * Interface looking through all registered NFs if one needs to start or stop.
 *
 */
void
onvm_nf_check_status(void);


/*
 * Interface giving a NF for a specific server id, depending on the flow.
 *
 * Inputs  : the service id
             a pointer to the packet whose flow help steer it. 
 * Output  : a NF instance id
 *
 */
inline uint16_t
onvm_nf_service_to_nf_map(uint16_t service_id, struct rte_mbuf *pkt);


/*
 * Interface to evaluate statistics relevant for nf_scheduling for all registered NFs.
 *
 */
void
onvm_nf_stats_update(unsigned long interval);


void compute_and_order_nf_wake_priority(void);



/* Enqueue NF to the bottleneck watch list */
int enqueu_nf_to_bottleneck_watch_list(uint16_t nf_id);
int dequeue_nf_from_bottleneck_watch_list(uint16_t nf_id);
int check_and_enqueue_or_dequeue_nfs_from_bottleneck_watch_list(void);
/****************************Internal functions*******************************/


/*
 * Function starting a NF.
 *
 * Input  : a pointer to the NF's informations
 * Output : an error code
 *
 */
inline int
onvm_nf_start(struct onvm_nf_info *nf_info);


/*
 * Function stopping a NF.
 *
 * Input  : a pointer to the NF's informations
 * Output : an error code
 *
 */
inline int
onvm_nf_stop(struct onvm_nf_info *nf_info);


#endif  // _ONVM_NF_H_
