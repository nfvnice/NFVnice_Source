/*********************************************************************
 *                     openNetVM
 *              https://sdnfv.github.io
 *
 *   BSD LICENSE
 *
 *   Copyright(c)
 *            2015-2016 George Washington University
 *            2015-2016 University of California Riverside
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
 * common.h - shared data between host and NFs
 ********************************************************************/

#ifndef _COMMON_H_
#define _COMMON_H_

#include <rte_mbuf.h>
#include <stdint.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <signal.h>
#include <sched.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <string.h>
#include <sys/file.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/time.h>
#include <sys/resource.h>
#include "onvm_sort.h"

#define MIN(a,b) ((a) < (b)? (a):(b))
#define MAX(a,b) ((a) > (b)? (a):(b))

#define ARBITER_PERIOD_IN_US            (100)       // 250 micro seconds or 100 micro seconds
/* Enable the ONVM_MGR to act as a 2-port bridge without any NFs */
#define ONVM_MGR_ACT_AS_2PORT_FWD_BRIDGE    // Work as bridge < without any NFs :: only testing purpose.. >
//#define SEND_DIRECT_ON_ALT_PORT
//#define DELAY_BEFORE_SEND
//#define DELAY_PER_PKT (5) //20micro seconds

#define ONVM_MAX_CHAIN_LENGTH 7   // the maximum chain length
#define MAX_CLIENTS 16            // total number of NFs allowed
#define MAX_SERVICES 16           // total number of unique services allowed
#define MAX_CLIENTS_PER_SERVICE 8 // max number of NFs per service.

#define ONVM_NF_ACTION_DROP 0   // drop packet
#define ONVM_NF_ACTION_NEXT 1   // to whatever the next action is configured by the SDN controller in the flow table
#define ONVM_NF_ACTION_TONF 2   // send to the NF specified in the argument field (assume it is on the same host)
#define ONVM_NF_ACTION_OUT 3    // send the packet out the NIC port set in the argument field

/* Note: Make the PACKET_READ_SIZE defined in onvm_mgr.h same as PKT_READ_SIZE defined in onvm_nflib_internal.h, better get rid of latter */
#define PRE_PROCESS_DROP_ON_RX  // Feature flag for addressing NF Local Back-pressure:: To lookup NF Tx queue occupancy and drop packets pro-actively before pushing to NFs Rx Ring.
////#define DROP_APPROACH_1       // (cleaned out) Handle inside NF_LIB:: After dequeue from NFs Rx ring, check for Tx Ring size and drop pkts before pushing packet to the NFs processing function. < Too late, only avoids packet processing by NF:: Discontinued.. Results are OK, but not preferred approach>
////#define DROP_APPROACH_2       // (cleaned out) Handle in onvm_mgr:: After segregation of Rx/Tx buffers to specific NF: Drop the packet before pushing the packets to the NFs Rx Ring buffer. < Early decision. Must also account for packets that could be currently under processing. < Results OK, but not preferred approach>
#define DROP_APPROACH_3         // Handle inside NF_LIB:: Make the NF to block until it cannot push the packets to the Tx Ring, Subsequent packets will be dropped in onvm_mgr context by Rx/Tx Threads < 3 options: Poll till Tx is free, Yield or Block on Semaphore>
////#define DROP_APPROACH_3_WITH_YIELD    //sub-option for approach 3: Results are good, but could result in lots of un-necessary context switches as one block might result in multiple yields. Thrpt is on par with block-approach.
////#define DROP_APPROACH_3_WITH_POLL     // (cleaned out) sub-option for approach 3: Results are good, but accounts to CPU wastage and hence not preferred.
#define DROP_APPROACH_3_WITH_SYNC       //sub-option for approach 3: Results are good, preferred approach.

#define INTERRUPT_SEM           // To enable NF thread interrupt mode wake.  Better to move it as option in Makefile

#define USE_SEMAPHORE           // Use Semaphore for IPC
//#define USE_MQ                // USe Message Queue for IPC between NFs and NF manager
//#define USE_FIFO              // Use Named Pipe (FIFO) -- cannot work in our model as Writer cannot be opened in nonblock
//#define USE_SIGNAL            // Use Signals (SIGUSR1) for IPC -- not reliable; makes the program exit after a while ( more pending singals??)..
//#define USE_SCHED_YIELD       // Use Explicit CPU Relinquish CPU, no explicit IPC other than shared mem read/write
//#define USE_NANO_SLEEP        // Use Sleep call to Reqlinqush CPU no explicit IPC other than shared mem read/write
//#define USE_SOCKET            // Use socket for IPC, NFs block on recv and mgr sends to ublock clients
//#define USE_FLOCK             // USE FILE_LOCK PREMITIVE for Blocking the NFs and mgr opens files in locked mode < Very expensive>
//#define USE_MQ2               // USE SYS_V5 Message Queue <good but relatively expensive than MQ >
//#define USE_ZMQ               // Use ZeroMQ sockets for communication < expensive as well, it doesn't seem to fit in our model>
#if (defined(INTERRUPT_SEM) && !defined(USE_SEMAPHORE) && !defined(USE_MQ) && !defined(USE_FIFO) && !defined(USE_SIGNAL) \
&& !defined(USE_SCHED_YIELD) && !defined(USE_NANO_SLEEP) && !defined(USE_SOCKET) && !defined(USE_FLOCK) && !defined(USE_MQ2) && !defined(USE_ZMQ))
#define USE_POLL_MODE
#endif

#ifdef USE_ZMQ
#include <zmq.h>
#endif

/* Enable Extra Debug Logs on all components */
//#define __DEBUG_LOGS__

/* Enable this flag to assign a distinct CGROUP for each NF instance */
#define USE_CGROUPS_PER_NF_INSTANCE                 // To create CGroup per NF instance
#define ENABLE_DYNAMIC_CGROUP_WEIGHT_ADJUSTMENT     // To dynamically evaluate and periodically adjust weight on NFs cpu share
#define USE_DYNAMIC_LOAD_FACTOR_FOR_CPU_SHARE       // Enable Load*comp_cost

/* For Bottleneck on Rx Ring; whether or not to Drop packets from Rx/Tx buf during flush_operation
 * Note: This is one of the likely cause of Out-of_order packets in the OpenNetVM (with Bridge) case: */
//#define DO_NOT_DROP_PKTS_ON_FLUSH_FOR_BOTTLENECK_NF   //Disable drop of existing packets -- may have caveats on when next flush would operate on that Tx/Rx buffer..
                                                        //Repercussions in onvm_pkt.c: onvm_pkt_enqueue_nf() to handle overflow and stop putting packet in full buffer and drop new ones instead.
                                                        //Observation: Good for TCP use cases, but with PktGen,Moongen dents line rate approx 0.3Mpps slow down

/* Enable watermark level NFs Tx and Rx Rings */
#define ENABLE_RING_WATERMARK // details on count in the onvm_init.h

/* Enable ECN CE FLAG : Feature Flag to enable marking ECN_CE flag on the flows that pass through the NFs with Rx Ring buffers exceeding the watermark level.
 * Dependency: Must have ENABLE_RING_WATERMARK feature defined. and HIGH and LOW Thresholds to be set. otherwise, marking may not happen at all.. Ideally, marking should be done after dequeue from Tx, to mark if Rx is overbudget..
 * On similar lines, even the back-pressure marking must be done for all flows after dequeue from the Tx Ring.. */
#define ENABLE_ECN_CE

/* Enable back-pressure handling to throttle NFs upstream */
//#define ENABLE_NF_BACKPRESSURE
#ifdef ENABLE_NF_BACKPRESSURE
#define NF_BACKPRESSURE_APPROACH_1    //Throttle enqueue of packets to the upstream NFs (handle in onvm_pkts_enqueue)
//#define NF_BACKPRESSURE_APPROACH_2      //Throttle upstream NFs from getting scheduled (handle in wakeup mgr)
//#define NF_BACKPRESSURE_APPROACH_3    //Throttle enqueue of packets to the upstream NFs (handle in NF_LIB with HOL blocking or pre-buffering of packets internally for bottlenecked chains)

// Extensions and sub-options for Back_Pressure handling
//#define DROP_PKTS_ONLY_AT_BEGGINING           // Extension to approach 1 to make packet drops only at the beginning on the chain (i.e only at the time to enqueue to first NF). (Note: can Enable)

//#define USE_BKPR_V2_IN_TIMER_MODE       //Use this flag if the Timer Thread can perform the Backpressure setting
#ifndef USE_BKPR_V2_IN_TIMER_MODE
#define ENABLE_SAVE_BACKLOG_FT_PER_NF           // save backlog Flow Entries per NF (Note: Enable)
#define BACKPRESSURE_USE_RING_BUFFER_MODE       // Use Ring buffer to store and delete backlog Flow Entries per NF  (Note: Enable, sub define  under ENABLE_SAVE_BACKLOG_FT_PER_NF)
#endif //USE_BKPR_V2_IN_TIMER_MODE

//#define RECHECK_BACKPRESSURE_MARK_ON_TX_DEQUEUE //Enable to re-check for back-pressure marking, at the time of packet dequeue from the NFs Tx Ring.
//#define BACKPRESSURE_EXTRA_DEBUG_LOGS           // Enable extra profile logs for back-pressure: Move all prints and additional variables under this flag (as optimization)

//Need Early bind to NF for the chain and determine the bottlneck status: Avoid passing first few packets of a flow till the chain, only to drop them later: Helps for TCP and issue with storing multiple flows at earlier NF
//#define ENABLE_EARLY_NF_BIND

//other test-variants and disregarded options: not to be used!!
//#define HOP_BY_HOP_BACKPRESSURE     //Option to enable [ON] = HOP by HOP propagation of back-pressure vs [OFF] = direct First NF to N-1 Discard(Drop)/block.
//#define ENABLE_NF_BKLOG_BUFFERING   //Extension to Approach 3 wherein each NF can pre-buffer internally the  packets for bottlenecked service chains. (Not Implemented!!)
//#define DUMMY_FT_LOAD_ONLY //Load Only onvm_ft and Bypass ENABLE_NF_BACKPRESSURE/NF_BACKPRESSURE_APPROACH_3

#endif //ENABLE_NF_BACKPRESSURE


/* Enable the Arbiter Logic to control the NFs scheduling and period on each core */
//#define ENABLE_ARBITER_MODE
//#define USE_ARBITER_NF_EXEC_PERIOD      //NFLib check for wake;/sleep state and Wakeup thread to put the the NFs to sleep after timer expiry (This feature is not working as expected..)

#define ENABLE_USE_RTE_TIMER_MODE_FOR_MAIN_THREAD
#define ENABLE_USE_RTE_TIMER_MODE_FOR_WAKE_THREAD





/* ENABLE TIMER BASED WEIGHT COMPUTATION IN NF_LIB */
#define ENABLE_TIMER_BASED_NF_CYCLE_COMPUTATION
#if defined(ENABLE_TIMER_BASED_NF_CYCLE_COMPUTATION) || defined (ENABLE_USE_RTE_TIMER_MODE_FOR_MAIN_THREAD)
#include <rte_timer.h>
#define STATS_PERIOD_IN_MS 1       //(use 1 or 10 or 100ms)
#endif //ENABLE_TIMER_BASED_NF_CYCLE_COMPUTATION

#define STORE_HISTOGRAM_OF_NF_COMPUTATION_COST  //Store the Histogram of NF Comp Cost   (in critical path, costing around 0.3 to 0.6Mpps)
#ifdef STORE_HISTOGRAM_OF_NF_COMPUTATION_COST
#include "histogram.h"                          //Histogra Library
#endif //STORE_HISTOGRAM_OF_NF_COMPUTATION_COST

#ifdef ENABLE_NF_BACKPRESSURE
//forward declaration either store reference of onvm_flow_entry or onvm_service_chain (latter may be sufficient)
struct onvm_flow_entry;
struct onvm_service_chain;
#endif  //ENABLE_NF_BACKPRESSURE


#define SET_BIT(x,bitNum) ((x)|=(1<<(bitNum-1)))
static inline void set_bit(long *x, unsigned bitNum) {
    *x |= (1L << (bitNum-1));
}

#define CLEAR_BIT(x,bitNum) ((x) &= ~(1<<(bitNum-1)))
static inline void clear_bit(long *x, unsigned bitNum) {
    *x &= (~(1L << (bitNum-1)));
}

#define TOGGLE_BIT(x,bitNum) ((x) ^= (1<<(bitNum-1)))
static inline void toggle_bit(long *x, unsigned bitNum) {
    *x ^= (1L << (bitNum-1));
}
#define TEST_BIT(x,bitNum) ((x) & (1<<(bitNum-1)))
static inline long test_bit(long x, unsigned bitNum) {
    return (x & (1L << (bitNum-1)));
}

static inline long is_upstream_NF(long chain_throttle_value, long chain_index) {
#ifndef HOP_BY_HOP_BACKPRESSURE
        long chain_index_value = 0;
        SET_BIT(chain_index_value, chain_index);
        CLEAR_BIT(chain_throttle_value, chain_index);
        return ((chain_throttle_value > chain_index_value)? (1):(0) );
#else
        long chain_index_value = 0;
        SET_BIT(chain_index_value, (chain_index+1));
        return ((chain_throttle_value & chain_index_value));
        //return is_immediate_upstream_NF(chain_throttle_value,chain_index);
#endif //HOP_BY_HOP_BACKPRESSURE
        //1 => NF component at chain_index is an upstream component w.r.t where the bottleneck is seen in the chain (do not drop/throttle)
        //0 => NF component at chain_index is an downstream component w.r.t where the bottleneck is seen in the chain (so drop/throttle)
}
static inline long is_immediate_upstream_NF(long chain_throttle_value, long chain_index) {
#ifdef HOP_BY_HOP_BACKPRESSURE
        long chain_index_value = 0;
        SET_BIT(chain_index_value, (chain_index+1));
        return ((chain_throttle_value & chain_index_value));
#else
        return is_upstream_NF(chain_throttle_value,chain_index);
#endif  //HOP_BY_HOP_BACKPRESSURE
        //1 => NF component at chain_index is an immediate upstream component w.r.t where the bottleneck is seen in the chain (do not drop/throttle)
        //0 => NF component at chain_index is an downstream component w.r.t where the bottleneck is seen in the chain (so drop/throttle)
}

static inline long get_index_of_highest_set_bit(long x) {
        long next_set_index = 0;
        //SET_BIT(chain_index_value, chain_index);
        //while ((1<<(next_set_index++)) < x);
        //for(; (x > (1<<next_set_index));next_set_index++)
        for(; (x >= (1<<next_set_index));next_set_index++);
        return next_set_index;
}

//#ifdef USE_MQ2
//typedef struct msgbuf { long mtype; char mtext[32];}msgbuf_t;
//#endif

//extern uint8_t rss_symmetric_key[40];
//size of onvm_pkt_meta cannot exceed 8 bytes, so how to add onvm_service_chain* sc pointer?
struct onvm_pkt_meta {
        uint8_t action; /* Action to be performed */
        uint8_t destination; /* where to go next */
        uint8_t src; /* who processed the packet last */
        uint8_t chain_index; /*index of the current step in the service chain*/
};
static inline struct onvm_pkt_meta* onvm_get_pkt_meta(struct rte_mbuf* pkt) {
        return (struct onvm_pkt_meta*)&pkt->udata64;
}

static inline uint8_t onvm_get_pkt_chain_index(struct rte_mbuf* pkt) {
        return ((struct onvm_pkt_meta*)&pkt->udata64)->chain_index;
}

/*
 * Define a structure with stats from the clients.
 */
struct client_tx_stats {
        /* these stats hold how many packets the manager will actually receive,
         * and how many packets were dropped because the manager's queue was full.
         */
        volatile uint64_t tx[MAX_CLIENTS];
        volatile uint64_t tx_drop[MAX_CLIENTS];
        volatile uint64_t tx_buffer[MAX_CLIENTS];
        volatile uint64_t tx_returned[MAX_CLIENTS];

        #ifdef INTERRUPT_SEM
        volatile uint64_t wkup_count[MAX_CLIENTS];
        volatile uint64_t prev_tx[MAX_CLIENTS];
        volatile uint64_t prev_tx_drop[MAX_CLIENTS];
        volatile uint64_t comp_cost[MAX_CLIENTS];
        volatile uint64_t prev_wkup_count[MAX_CLIENTS];
        #endif  //INTERRUPT_SEM

        /* FIXME: Why are these stats kept separately from the rest?
         * Would it be better to have an array of struct client_tx_stats instead
         * of putting the array inside the struct? How can we avoid cache
         * invalidations from different NFs updating these stats?
         */
};

extern struct client_tx_stats *clients_stats;

/*
 * Define a structure to describe one NF
 */
struct onvm_nf_info {
        uint16_t instance_id;
        uint16_t service_id;
        uint8_t status;
        const char *tag;

        pid_t pid;
        uint32_t comp_cost;     //indicates the computation cost of NF in num_of_cycles

#if defined (USE_CGROUPS_PER_NF_INSTANCE)
        //char cgroup_name[256];
        uint32_t cpu_share;     //indicates current share of NFs cpu
        uint32_t core_id;       //indicates the core ID the NF is running on
        uint32_t comp_pkts;     //[usage: TBD] indicates the number of pkts processed by NF over specific sampling period (demand (new pkts arrival) = Rx, better? or serviced (new pkts sent out) = Tx better?)
        uint32_t load;          //indicates instantaneous load on the NF ( = num_of_packets on the rx_queue + pkts dropped on Rx)
        uint32_t avg_load;      //indicates the average load on the NF
        uint32_t svc_rate;      //indicates instantaneous service rate of the NF ( = num_of_packets processed in the sampling period)
        uint32_t avg_svc;       //indicates the average service rate of the NF
        uint32_t drop_rate;     //indicates the drops observed within the sampled period.
        uint64_t exec_period;   //indicates the number_of_cycles/timeperiod alloted for execution in this epoch == normalized_load*comp_cost -- how to get this metric: (total_cycles_in_epoch)*(total_load_on_core)/(load_of_nf)
#endif

#ifdef ENABLE_TIMER_BASED_NF_CYCLE_COMPUTATION
        struct rte_timer stats_timer;
#endif //ENABLE_TIMER_BASED_NF_CYCLE_COMPUTATION

#ifdef STORE_HISTOGRAM_OF_NF_COMPUTATION_COST
        histogram_v2_t ht2;
#endif  //STORE_HISTOGRAM_OF_NF_COMPUTATION_COST

#ifdef ENABLE_ECN_CE
        histogram_v2_t ht2_q;
#endif  //ENABLE_ECN_CE
};

/*
 * Define a structure to describe a service chain entry
 */
struct onvm_service_chain_entry {
	uint16_t destination;
	uint8_t action;
};

struct onvm_service_chain {
	struct onvm_service_chain_entry sc[ONVM_MAX_CHAIN_LENGTH+1];
	uint8_t chain_length;
	uint8_t ref_cnt;
#ifdef ENABLE_NF_BACKPRESSURE
	volatile uint8_t highest_downstream_nf_index_id;     // bit index of each NF in the chain that is overflowing
//#ifdef NF_BACKPRESSURE_APPROACH_2
	uint8_t nf_instances_mapped; //set when all nf_instances are populated in the below array
	uint8_t nf_instance_id[ONVM_MAX_CHAIN_LENGTH+1];
//#endif //NF_BACKPRESSURE_APPROACH_2
#endif //ENABLE_NF_BACKPRESSURE
};

/* define common names for structures shared between server and client */
#define MP_CLIENT_RXQ_NAME "MProc_Client_%u_RX"
#define MP_CLIENT_TXQ_NAME "MProc_Client_%u_TX"
#define PKTMBUF_POOL_NAME "MProc_pktmbuf_pool"
#define MZ_PORT_INFO "MProc_port_info"
#define MZ_CLIENT_INFO "MProc_client_info"
#define MZ_SCP_INFO "MProc_scp_info"
#define MZ_FTP_INFO "MProc_ftp_info"

/* interrupt semaphore specific updates */
#ifdef INTERRUPT_SEM
#define SHMSZ 4                         // size of shared memory segement (page_size)
#define KEY_PREFIX 123                  // prefix len for key
#ifdef USE_MQ
#define MP_CLIENT_SEM_NAME "/MProc_Client_%u_SEM"
#elif defined USE_FIFO
#define MP_CLIENT_SEM_NAME "/MProc_Client_%u_SEM"
#elif defined USE_SOCKET
#define MP_CLIENT_SEM_NAME "/MProc_Client_%u_SEM"
#elif defined USE_ZMQ
#define MP_CLIENT_SEM_NAME "ipc:///MProc_Client_%u_SEM"
#else
#define MP_CLIENT_SEM_NAME "MProc_Client_%u_SEM"
#endif //USE_MQ
//1000003 1000033 1000037 1000039 1000081 1000099 1000117 1000121 1000133
//#define SAMPLING_RATE 1000000           // sampling rate to estimate NFs computation cost
#define SAMPLING_RATE 1000003           // sampling rate to estimate NFs computation cost
#define ONVM_SPECIAL_NF 0               // special NF for flow table entry management
#endif


#ifdef ENABLE_ARBITER_MODE
#define ONVM_NUM_WAKEUP_THREADS ((int)0)       //1 ( Must remove this as well)
#else
#define ONVM_NUM_WAKEUP_THREADS ((int)1)       //1 ( Must remove this as well)
#endif

/* common names for NF states */
#define _NF_QUEUE_NAME "NF_INFO_QUEUE"
#define _NF_MEMPOOL_NAME "NF_INFO_MEMPOOL"

#define NF_WAITING_FOR_ID 0     // First step in startup process, doesn't have ID confirmed by manager yet
#define NF_STARTING 1           // When a NF is in the startup process and already has an id
#define NF_RUNNING 2            // Running normally
#define NF_PAUSED  3            // NF is not receiving packets, but may in the future
#define NF_STOPPED 4            // NF has stopped and in the shutdown process
#define NF_ID_CONFLICT 5        // NF is trying to declare an ID already in use
#define NF_NO_IDS 6             // There are no available IDs for this NF

#define NF_NO_ID -1

/*
 * Given the rx queue name template above, get the queue name
 */
static inline const char *
get_rx_queue_name(unsigned id) {
        /* buffer for return value. Size calculated by %u being replaced
         * by maximum 3 digits (plus an extra byte for safety) */
        static char buffer[sizeof(MP_CLIENT_RXQ_NAME) + 2];

        snprintf(buffer, sizeof(buffer) - 1, MP_CLIENT_RXQ_NAME, id);
        return buffer;
}

/*
 * Given the tx queue name template above, get the queue name
 */
static inline const char *
get_tx_queue_name(unsigned id) {
        /* buffer for return value. Size calculated by %u being replaced
         * by maximum 3 digits (plus an extra byte for safety) */
        static char buffer[sizeof(MP_CLIENT_TXQ_NAME) + 2];

        snprintf(buffer, sizeof(buffer) - 1, MP_CLIENT_TXQ_NAME, id);
        return buffer;
}

#ifdef INTERRUPT_SEM
/*
 * Given the rx queue name template above, get the key of the shared memory
 */
static inline key_t
get_rx_shmkey(unsigned id)
{
        return KEY_PREFIX * 10 + id;
}

/*
 * Given the sem name template above, get the sem name
 */
static inline const char *
get_sem_name(unsigned id)
{
        /* buffer for return value. Size calculated by %u being replaced
         * by maximum 3 digits (plus an extra byte for safety) */
        static char buffer[sizeof(MP_CLIENT_SEM_NAME) + 2];

        snprintf(buffer, sizeof(buffer) - 1, MP_CLIENT_SEM_NAME, id);
        return buffer;
}
#endif
#ifdef USE_CGROUPS_PER_NF_INSTANCE
#define MP_CLIENT_CGROUP_NAME "nf_%u"
static inline const char *
get_cgroup_name(unsigned id)
{
        static char buffer[sizeof(MP_CLIENT_CGROUP_NAME) + 2];
        snprintf(buffer, sizeof(buffer) - 1, MP_CLIENT_CGROUP_NAME, id);
        return buffer;
}
#define MP_CLIENT_CGROUP_PATH "/sys/fs/cgroup/cpu/nf_%u/"
static inline const char *
get_cgroup_path(unsigned id)
{
        static char buffer[sizeof(MP_CLIENT_CGROUP_PATH) + 2];
        snprintf(buffer, sizeof(buffer) - 1, MP_CLIENT_CGROUP_PATH, id);
        return buffer;
}
#define MP_CLIENT_CGROUP_CREAT "mkdir /sys/fs/cgroup/cpu/nf_%u"
static inline const char *
get_cgroup_create_cgroup_cmd(unsigned id)
{
        static char buffer[sizeof(MP_CLIENT_CGROUP_CREAT) + 2];
        snprintf(buffer, sizeof(buffer) - 1, MP_CLIENT_CGROUP_CREAT, id);
        return buffer;
}
#define MP_CLIENT_CGROUP_ADD_TASK "echo %u > /sys/fs/cgroup/cpu/nf_%u/tasks"
static inline const char *
get_cgroup_add_task_cmd(unsigned id, pid_t pid)
{
        static char buffer[sizeof(MP_CLIENT_CGROUP_ADD_TASK) + 10];
        snprintf(buffer, sizeof(buffer) - 1, MP_CLIENT_CGROUP_ADD_TASK, pid, id);
        return buffer;
}
#define MP_CLIENT_CGROUP_SET_CPU_SHARE "echo %u > /sys/fs/cgroup/cpu/nf_%u/cpu.shares"
static inline const char *
get_cgroup_set_cpu_share_cmd(unsigned id, unsigned share)
{
        static char buffer[sizeof(MP_CLIENT_CGROUP_SET_CPU_SHARE) + 20];
        snprintf(buffer, sizeof(buffer) - 1, MP_CLIENT_CGROUP_SET_CPU_SHARE, share, id);
        return buffer;
}
#define MP_CLIENT_CGROUP_SET_CPU_SHARE_ONVM_MGR "/sys/fs/cgroup/cpu/nf_%u/cpu.shares"
static inline const char *
get_cgroup_set_cpu_share_cmd_onvm_mgr(unsigned id)
{
        static char buffer[sizeof(MP_CLIENT_CGROUP_SET_CPU_SHARE) + 20];
        snprintf(buffer, sizeof(buffer) - 1, MP_CLIENT_CGROUP_SET_CPU_SHARE_ONVM_MGR, id);
        return buffer;
}
#include <stdlib.h>
static inline int
set_cgroup_nf_cpu_share(uint16_t instance_id, uint32_t share_val) {
        /*
        unsigned long shared_bw_val = (share_val== 0) ?(1024):(1024*share_val/100); //when share_val is relative(%)
        if (share_val >=100) {
                shared_bw_val = shared_bw_val/100;
        }*/

        uint32_t shared_bw_val = (share_val== 0) ?(1024):(share_val);  //when share_val is absolute bandwidth
        const char* cg_set_cmd = get_cgroup_set_cpu_share_cmd(instance_id, shared_bw_val);
        //printf("\n CMD_TO_SET_CPU_SHARE: %s \n", cg_set_cmd);

        int ret = system(cg_set_cmd);
        return ret;
}
static inline int
set_cgroup_nf_cpu_share_from_onvm_mgr(uint16_t instance_id, uint32_t share_val) {
#ifdef SET_CPU_SHARE_FROM_NF
#else
        FILE *fp = NULL;
        uint32_t shared_bw_val = (share_val== 0) ?(1024):(share_val);  //when share_val is absolute bandwidth
        const char* cg_set_cmd = get_cgroup_set_cpu_share_cmd_onvm_mgr(instance_id);

        //printf("\n CMD_TO_SET_CPU_SHARE: %s \n", cg_set_cmd);
        fp = fopen(cg_set_cmd, "w");            //optimize with mmap if that is allowed!!
        if (fp){
                fprintf(fp,"%d",shared_bw_val);
                fclose(fp);
        }
        return 0;
#endif
}
#endif //USE_CGROUPS_PER_NF_INSTANCE

#define RTE_LOGTYPE_APP RTE_LOGTYPE_USER1

#ifdef ENABLE_NF_BACKPRESSURE
typedef struct per_core_nf_pool {
        uint16_t nf_count;
        uint32_t nf_ids[MAX_CLIENTS];
}per_core_nf_pool_t;
#endif //ENABLE_NF_BACKPRESSURE

#define SECOND_TO_MICRO_SECOND          (1000*1000)
#define NANO_SECOND_TO_MICRO_SECOND(x)  (double)((x)/(1000))
#define MICRO_SECOND_TO_SECOND(x)       (double)((x)/(SECOND_TO_MICRO_SECOND))
#define USE_THIS_CLOCK  CLOCK_MONOTONIC //CLOCK_THREAD_CPUTIME_ID //CLOCK_PROCESS_CPUTIME_ID //CLOCK_MONOTONIC
typedef struct stats_time_info {
        uint8_t in_read;
        struct timespec prev_time;
        struct timespec cur_time;
}nf_stats_time_info_t;
static inline int get_current_time(struct timespec *pTime);
static inline unsigned long get_difftime_us(struct timespec *start, struct timespec *end);
static inline int get_current_time(struct timespec *pTime) {
        if (clock_gettime(USE_THIS_CLOCK, pTime) == -1) {
              perror("\n clock_gettime");
              return 1;
        }
        return 0;
}
static inline unsigned long get_difftime_us(struct timespec *pStart, struct timespec *pEnd) {
        unsigned long delta = ( ((pEnd->tv_sec - pStart->tv_sec) * SECOND_TO_MICRO_SECOND) +
                     ((pEnd->tv_nsec - pStart->tv_nsec) /1000) );
        //printf("Delta [%ld], sec:[%ld]: nanosec [%ld]", delta, (pEnd->tv_sec - pStart->tv_sec), (pEnd->tv_nsec - pStart->tv_nsec));
        return delta;
}
#include <rte_cycles.h>
typedef struct stats_cycle_info {
        uint8_t in_read;
        uint64_t prev_cycles;
        uint64_t cur_cycles;
}stats_cycle_info_t;
static inline uint64_t get_current_cpu_cycles(void);
static inline uint64_t get_diff_cpu_cycles_in_us(uint64_t start, uint64_t end);
static inline uint64_t get_current_cpu_cycles(void) {
        return rte_rdtsc_precise();
}
static inline uint64_t get_diff_cpu_cycles_in_us(uint64_t start, uint64_t end) {
        if(end > start) {
                return (uint64_t) (((end -start)*SECOND_TO_MICRO_SECOND)/rte_get_tsc_hz());
        }
        return 0;
}

#ifdef ENABLE_NF_BACKPRESSURE
typedef struct sc_entries {
        struct onvm_service_chain *sc;
        uint16_t sc_count;
        uint16_t bneck_flag;
}sc_entries_list;

#define BOTTLENECK_NF_STATUS_WAIT_ENQUEUED   (0x01)
#define BOTTLENECK_NF_STATUS_DROP_MARKED     (0x02)
#define BOTTLENECK_NF_STATUS_RESET           (0x00)
typedef struct bottleneck_nf_entries {
        struct timespec s_time;
        uint16_t enqueue_status;
        uint16_t nf_id;
        uint16_t enqueued_ctr;
        uint16_t marked_ctr;
}bottleneck_nf_entries_t;
typedef struct bottlenec_nf_info {
        uint16_t entires;
        //struct rte_timer nf_timer[MAX_CLIENTS];   // not worth it, as it would still be called at granularity of invoking the rte_timer_manage()
        bottleneck_nf_entries_t nf[MAX_CLIENTS];
}bottlenec_nf_info_t;
bottlenec_nf_info_t bottleneck_nf_list;
#endif //ENABLE_NF_BACKPRESSURE

#define WAIT_TIME_BEFORE_MARKING_OVERFLOW_IN_US   (0*SECOND_TO_MICRO_SECOND)

int onvm_mark_all_entries_for_bottleneck(uint16_t nf_id);
int onvm_clear_all_entries_for_bottleneck(uint16_t nf_id);

#ifdef ENABLE_NF_BACKPRESSURE
/******************************** DATA STRUCTURES FOR FIPO SUPPORT *********************************
*     fipo_buf_node_t:      each rte_buf_node (packet) added to the fipo_per_flow_list -- Need basic Queue add/remove
*     fipo_per_flow_list:   Ordered list of buffers for each flow   -- Need Queue add/remove
*     nf_flow_list_t:       Priority List of Flows for each NF      -- Need Queue add/remove
*     Memory sharing Model is tedious to support this..
*     Rx/Tx should access fipo_buf_node to create a pkt entry, then fipo_per_flow_list to insert into
*
******************************** DATA STRUCTURES FOR FIPO SUPPORT *********************************/
typedef struct fipo_buf_node {
        void *pkt;
        struct fipo_buf_node *next;
        struct fipo_buf_node *prev;
}fipo_buf_node_t;

typedef struct fipo_list {
        uint32_t buf_count;
        fipo_buf_node_t *head;
        fipo_buf_node_t *tail;
}fipo_list_t;
typedef fipo_list_t fipo_per_flow_list;
//Each entry of the list must be shared with the NF, i.e. unique memzone must be created per NF per flow as FIPO_%NFID_%FID
//Fix the MAX_NUMBER_OF_FLOWS, cannot have dynamic memzones per NF, too expensive as it has to have locks..
#define MAX_NUM_FIPO_FLOWS  (16)
#define MAX_BUF_PER_FLOW  ((128)/(MAX_NUM_FIPO_FLOWS))//((CLIENT_QUEUE_RINGSIZE)/(MAX_NUM_FIPO_FLOWS))
typedef struct nf_flow_list {
        uint32_t flow_count;
        fipo_per_flow_list *head;
        fipo_per_flow_list *tail;
}nf_flow_list_t;
#endif //ENABLE_NF_BACKPRESSURE

#endif  // _COMMON_H_
