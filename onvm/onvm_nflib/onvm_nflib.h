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
 ********************************************************************/


/******************************************************************************

                                onvm_nflib.h


                           Header file for the API


******************************************************************************/


#ifndef _ONVM_NFLIB_H_
#define _ONVM_NFLIB_H_
#include <rte_mbuf.h>
//#ifdef INTERRUPT_SEM  //move maro to makefile, otherwise uncomemnt or need to include these after including common.h
#include <rte_cycles.h>
//#endif
#include "common.h"
/************************************API**************************************/
extern unsigned long max_nf_computation_cost;
/**
 * Initialize the OpenNetVM container Library.
 * This will setup the DPDK EAL as a secondary process, and notify the host
 * that there is a new NF.
 *
 * @argc
 *   The argc argument that was given to the main() function.
 * @argv
 *   The argv argument that was given to the main() function
 * @param tag
 *   A uniquely identifiable string for this NF.
 *   For example, can be the application name (e.g. "bridge_nf")
 * @return
 *   On success, the number of parsed arguments, which is greater or equal to
 *   zero. After the call to onvm_nf_init(), all arguments argv[x] with x < ret
 *   may be modified and should not be accessed by the application.,
 *   On error, a negative value .
 */
int
onvm_nflib_init(int argc, char *argv[], const char *nf_tag);


/**
 * Run the OpenNetVM container Library.
 * This will register the callback used for each new packet. It will then
 * loop forever waiting for packets.
 *
 * @param info
 *   an info struct describing this NF app. Must be from a huge page memzone.
 * @param handler
 *   a pointer to the function that will be called on each received packet.
 * @return
 *   0 on success, or a negative value on error.
 */
int
onvm_nflib_run(struct onvm_nf_info* info, int(*handler)(struct rte_mbuf* pkt, struct onvm_pkt_meta* action));


/**
 * Return a packet that has previously had the ONVM_NF_ACTION_BUFFER action
 * called on it.
 *
 * @param pkt
 *    a pointer to a packet that should now have a action other than buffer.
 * @return
 *    0 on success, or a negative value on error.
 */
int
onvm_nflib_return_pkt(struct rte_mbuf* pkt);


/* Function to perform Packet drop */
int
onvm_nflib_drop_pkt(struct rte_mbuf* pkt);

/**
 * Stop this NF
 * Sets the info to be not running and exits this process gracefully
 */
void
onvm_nflib_stop(void);

typedef int (*nf_explicit_callback_function)(void);
extern nf_explicit_callback_function nf_ecb;
void register_explicit_callback_function(nf_explicit_callback_function ecb);
void notify_for_ecb(void);

#define MSG_NOOP            (0)
#define MSG_STOP            (1)
#define MSG_NF_STARTING     (2)
#define MSG_NF_STOPPING     (3)
#define MSG_NF_UNBLOCK_SELF (4)
#define MSG_NF_TRIGGER_ECB  (5)
#define MSG_NF_REGISTER_ECB (6)
struct onvm_nf_msg {
        uint8_t msg_type; /* Constant saying what type of message is */
        void *msg_data; /* These should be rte_malloc'd so they're stored in hugepages */
};

int onvm_nflib_handle_msg(struct onvm_nf_msg *msg);
 
/* Interface for AIO abstraction from NF Lib: */
#define MAX_FILE_PATH_SIZE  PATH_MAX //(255)
#define AIO_OPTION_SYNC_MODE_RW   (0x01)    // Enable Synchronous Read/Writes
#define AIO_OPTION_BATCH_PROCESS  (0x02)    //applicable to aio_write
#define AIO_OPTION_PER_FLOW_QUEUE (0x04)    //applicable to both read/write
typedef struct nflib_aio_info {
        uint8_t file_path[MAX_FILE_PATH_SIZE];
        int mode;                       //read, read_write;
        uint32_t num_of_buffers;        //number of buffers to be setup for read/write
        uint32_t max_buffer_size;       //size of each buffer for read/writes
        uint32_t aio_options;           //Bitwise OR of AIO_OPTION_XXX*
        uint32_t wait_pkt_queue_len;    //Max size of pkts that can be put to wait for aio completion
}nflib_aio_info_t;
typedef struct onvm_nflib_aio_init_info {
        nflib_aio_info_t aio_read;      //read information
        nflib_aio_info_t aio_write;     //write information
        uint32_t max_worker_threads;    //number_of_worker_threads for r/w
        uint8_t aio_service_type;       //0=None; 1=Read_only; 2=Write_only; 3=Read_write;
}onvm_nflib_aio_init_info_t;
typedef struct nflib_aio_status {
        int32_t rw_status;      //completion status of read/write callback operation
        void *rw_buffer;        //buffer data read back, or to be written;  //can use rte_mbuf as well
        uint8_t rw_buf_len;     //len of buffer
        off_t rw_offset;        //File offset for read/write operation
}nflib_aio_status_t;

/* Callback handler for NF AIO EVENT COMPLETION NOTIFICATION *
 * Return Status: 0: NF processing is Success; -ve: Failure in NF to assert ??
 */
typedef int (*aio_notify_handler_cb)(struct rte_mbuf** pkt,  nflib_aio_status_t *status);

/* API to register/subscribe for AIO service
 * Must setup the callback handler if ASYNC IO is desired
 * Return Status: 0 succes; -ve value : Failures
 */
int nflib_aio_init(onvm_nflib_aio_init_info_t *info, aio_notify_handler_cb cb_handler);

/* API to initiate relevant AIO for the packet
 *  Note: Data to write will be setup by the NF; NFLib will only perform Write on NFs behalf.
 *        Read data file offset details will need to be specified by the NF; read data will be returned back in aio_status_t*
 *        Return Status: 0 Success; -ve value Failures; +ve value>0 (later extension ??);
 */
int nflib_pkt_aio(struct rte_mbuf* pkt, nflib_aio_status_t *status, uint32_t rw_options);   //per pkt rw_options: 0=read,1=write; 2=extend later..

#endif  // _ONVM_NFLIB_H_
