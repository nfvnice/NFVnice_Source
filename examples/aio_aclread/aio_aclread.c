/*********************************************************************
 *                     openNetVM
 *       https://github.com/sdnfv/openNetVM
 *
 *  Copyright 2015 George Washington University
 *            2015 University of California Riverside
 *
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the Li¡cense for the specific language governing permissions and
 *  limitations under the License.
 *
 *  schain_preload.c - NF that preloads FlowTable entries with service chains.
 *              -- List of service chains are parsed from "services.txt"
 *              Service chains are randomly assigned < Round Robin >
 *              -- List of IPv4 % Tuple Rules in the order
 *              <SRC_IP,DST_IP,SRC_PORT,DST_PORT,IP_PROTO>
 *              are parsed from ipv4rules.txt
 *              -- In addition the baseIP, MaxIPs and ports can be specified to
 *              pre-populate FlowTable rules.
 *              -- Can also dynamically set the SC on the missing FT entries.
 *              -- NF can be registered with any serviceID (preferably 1)
 *
 ********************************************************************/
#define _GNU_SOURCE     //for O_DIRECT
#include <unistd.h>
#include <stdint.h>
#include <stdio.h>
#include <inttypes.h>
#include <stdarg.h>
#include <errno.h>
#include <sys/queue.h>
#include <stdlib.h>
#include <getopt.h>
#include <string.h>
#include <assert.h>
#include <aio.h>
#include <signal.h>
#include <semaphore.h>

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netinet/in.h>

#include <rte_common.h>
#include <rte_mbuf.h>
#include <rte_ip.h>
#include <rte_malloc.h>
#include <rte_memzone.h>
#include <rte_memory.h>
#include <rte_cycles.h>

#include "onvm_nflib.h"
#include "onvm_pkt_helper.h"
#include "onvm_flow_table.h"
#include "onvm_sc_common.h"
#include "onvm_flow_dir.h"
#include "onvm_sc_mgr.h"


#define NF_TAG "pkt_logger"

#define MIN(a,b) ((a) < (b)? (a):(b))
#define MAX(a,b) ((a) > (b)? (a):(b))

#define MARK_PACKET_TO_RETAIN   (1)
#define MARK_PACKET_FOR_DROP    (2)

//NF specific Feature Options
#define ACT_AS_BRIDGE
//#define ENABLE_DEBUG_LOGS
//#define USE_SYNC_IO

#ifndef USE_SYNC_IO
#define PURE_ASYNC_MODE     (0)
#define PSEUDO_ASYNC_MODE   (1)
#define ASYNC_MODE  (PURE_ASYNC_MODE)
//#define ASYNC_MODE  (PSEUDO_ASYNC_MODE)
#if (ASYNC_MODE == PURE_ASYNC_MODE)
//#define USE_RTE_RING
#endif
#endif

#ifdef USE_SYNC_IO
//#define FD_OPEN_MODE (O_RDONLY|O_DIRECT|O_FSYNC)
//#define FD_OPEN_MODE (O_RDONLY|O_DIRECT) // (O_RDONLY|O_FSYNC)
#define FD_OPEN_MODE (O_RDONLY|O_FSYNC) // (O_RDONLY|O_FSYNC)
//#define FD_OPEN_MODE (O_RDONLY) // (O_RDONLY|O_FSYNC)
#else
//#define FD_OPEN_MODE (O_RDONLY|O_DIRECT|O_FSYNC) 
//#define FD_OPEN_MODE (O_RDONLY|O_DIRECT) // (O_RDONLY|O_FSYNC)
#define FD_OPEN_MODE (O_RDONLY|O_FSYNC) // (O_RDONLY|O_FSYNC)
//#define FD_OPEN_MODE (O_RDONLY)
#endif //USE_SYNC_IO


#define DISPLAY_AFTER_PACKETS   (1000000)     //(100000)

/* Struct that contains information about this NF */
struct onvm_nf_info *nf_info;

/* List of Global Command Line Arguments */
typedef struct globalArgs {
        uint32_t destination;               /* -d <destination_service ID> */
        uint32_t print_delay;               /* -p <print delay in num_pakets> */
        const char* pktlog_file;            /* -s <file name to save packet log> */
        const char* read_file;              /* -r <file name to read ACL entries> */
        const char* base_ip_addr;           /* -b <IPv45Tuple Base Ip Address> */
        uint32_t max_bufs;                  /* -m <Maximum number of Buffers> */
        uint32_t buf_size;                  /* -M <Maximum size of packet buffers> */
        int fd;                             /* .. file descriptor, internal to process to write/logging purpose*/
        uint32_t file_offset;               /* .. offset in file_location, internal '' */
        uint32_t cur_buf_index;             /* .. index of free buffer, internal '' */
        uint8_t is_blocked_on_sem;          /* .. status of nf_thread if blocked on sem for aio */
        uint64_t sem_block_count;           /* .. stat keeper for num_of_blocks */
        int fd_r;                           /* .. file descriptor, internal to process for reading (ACL??) entries */
        uint32_t read_offset;               /* .. offset in file_location, internal '' */
}globalArgs_t;
static const char *optString = "d:p:s:r:b:m:M";

static globalArgs_t globals = {
        .destination = 0,
        .print_delay = 1, //1000000,
        .pktlog_file = "logger_pkt.txt", // "/dev/null", // "pkt_logger.txt", //
        .read_file = "logger_pkt.txt",
        .base_ip_addr   = "10.0.0.1",
        .max_bufs   = 2, //1,
        .buf_size   = 4096, //4096, //128
        .fd = -1,
        .file_offset = 0,
        .cur_buf_index = 0,
        .is_blocked_on_sem=0,
        .sem_block_count=0,
};

#ifdef SDN_FT_ENTRIES
#define MAX_FLOW_TABLE_ENTRIES SDN_FT_ENTRIES
#else
#define MAX_FLOW_TABLE_ENTRIES 1024
#endif //SDN_FT_ENTRIES

//To keep track of all packets that are logged and that need not be re-checked again!
typedef struct flow_logged_data_t {
        uint32_t cur_entries;
        uint16_t ft_list[MAX_FLOW_TABLE_ENTRIES];
}flow_logged_data_t;
static flow_logged_data_t flow_logged_info;

#define MAX_PKT_BUFFERS (5)
int pktBufList[MAX_PKT_BUFFERS];
#define MAX_PKT_BUF_SIZE (64*1024)
#define BUF_SIZE MAX_PKT_BUF_SIZE
#define errExit(msg) do { perror(msg); exit(EXIT_FAILURE); } while (0)
#define errMsg(msg)  do { perror(msg); } while (0)

typedef enum AIOBufState {
        BUF_FREE = 0,
        BUF_IN_USE =1,
        BUF_SUBMITTED=2,
}AIOBufState_e;

typedef struct aio_buf_t {
        volatile AIOBufState_e state;
        void* buf;
        uint32_t buf_index;     //const <index of buffer initialized at the time of allocation>
        uint32_t max_size;      // const <max size of the buffer allocated at the time of initalization>
        uint32_t buf_len;       // varaible updated for each read/write operation
        struct rte_mbuf *pkt;    // pkt associated with the aio_buf for the read case
        struct aiocb *aiocb;    // <allocated at initialization and updated for each read/write>
        int req_status;         // <allocated at initialization and updated for each read/write>
}aio_buf_t;
static aio_buf_t *aio_buf_pool = NULL;
//static aio_buf_t *pkt_buf_pord = NULL;    //For read puropose */


#define IO_SIGNAL SIGUSR1   /* Signal used to notify I/O completion */
#define SEM_NAME "PKTLOGGER_SYNC_VAR"
sem_t *wait_mutex = NULL;
#define AIO_REQUEST_PRIO (0)

//AIO Operation type (Read or Write)
#define AIO_READ_OPERATION    (0)
#define AIO_WRITE_OPERATION   (1)

//mode=0=> from pkt_handler (can enqueue to wait), mode=1 => from wiat_queue ( cannot enqueue to wiat)
typedef enum pkt_log_mode {
        PKT_LOG_WAIT_ENQUEUE_ENABLED = 0,
        PKT_LOG_WAIT_ENQUEUE_DISABLED=1,
}pkt_log_mode_e;


/******************************************************************************
 *              FUNCTION DECLARATIONS
 ******************************************************************************/
int initialize_aio_buffers (void);
int clear_thread_start(void *pdata);
/* Handler for I/O completion signal */
#ifdef USE_SIGEV_SIGNAL
static void ioSigHandler(int sig, siginfo_t *si, void *ucontext);
#else
static void ioSigHandler(sigval_t sigval);
#endif
//static void aio_CompletionRoutine(sigval_t sigval);
int initialize_sync_variable(void);
int initialize_aiocb(aio_buf_t *pbuf);
int initialize_signal_action (void);
int initialize_log_file(void);
int initialize_aio_nf(void);
int deinitialize_aio_buffers (void);
int deinitialize_sync_variable(void);
int deinitialize_aiocb(aio_buf_t *pbuf);
int deinitialize_signal_action (void);
int deinitialize_log_file(void);
int deinitialize_aio_nf(void);
int nf_as_bridge(struct rte_mbuf* pkt,  __attribute__((unused)) struct onvm_pkt_meta* meta);
aio_buf_t* get_aio_buffer_from_aio_buf_pool(uint32_t aio_operation_mode);
int write_aio_buffer(aio_buf_t *pbuf);
int refresh_aio_buffer(aio_buf_t *pbuf);
int read_aio_buffer(aio_buf_t *pbuf);

int wait_for_buffer_ready(unsigned int timeout_ms);
int notify_io_rw_done(aio_buf_t *pbuf);
int packet_process_io(struct rte_mbuf* pkt,  __attribute__((unused)) struct onvm_pkt_meta* meta, struct onvm_flow_entry *flow_entry, __attribute__((unused)) pkt_log_mode_e mode);
int validate_packet_and_do_io(struct rte_mbuf* pkt,  __attribute__((unused)) struct onvm_pkt_meta* meta);


static int get_flow_entry( struct rte_mbuf *pkt, struct onvm_flow_entry **flow_entry);
#define PER_FLOW_Q_NAME "Per_flow_queue_%u"
static inline const char *
get_rte_ring_queue_name(unsigned id) {
        /* buffer for return value. Size calculated by %u being replaced
         * by maximum 4 digits (plus an extra byte for safety) */
        static char buffer[sizeof(PER_FLOW_Q_NAME) + 5];

        snprintf(buffer, sizeof(buffer) - 1, PER_FLOW_Q_NAME, id);
        return buffer;
}

/** Functions to maintain/enqueue/dequue Per Flow Wait Queue for packets that are yet to initiate I/O */
#define PERFLOW_QUEUE_RINGSIZE              (128)      // (32) (64) (128) (256) (512) (1024) (2048) (4096)
#define PERFLOW_QUEUE_RING_THRESHOLD_HIGH   (100)
#define PERFLOW_QUEUE_RING_THRESHOLD_LOW    (50)
#define PERFLOW_QUEUE_LOW_WATERMARK         (PERFLOW_QUEUE_RINGSIZE*PERFLOW_QUEUE_RING_THRESHOLD_LOW/100)
#define PERFLOW_QUEUE_HIGH_WATERMARK        (PERFLOW_QUEUE_RINGSIZE*PERFLOW_QUEUE_RING_THRESHOLD_HIGH/100)
typedef struct per_flow_ring_buffer {
        uint16_t pkt_count;         // num of entries in the r_buf[]
        uint16_t r_h;               // read_head in the r_buf[]
        uint16_t w_h;               // write head in the r_buf[]
        uint16_t max_len;           // Max size/count of r_buf[]
        #ifndef USE_RTE_RING
        struct rte_mbuf* pktbuf_ring[PERFLOW_QUEUE_RINGSIZE+1];
        #else
        struct rte_ring *pktbuf_rte_ring;   //alternative use the rte_ring buffer to enqueue/dequeue the packets
        #endif
}per_flow_ring_buffer_t;
//per_flow_ring_buffer_t pre_io_wait_ring[MAX_FLOW_TABLE_ENTRIES];
typedef struct pre_io_wait_queue {
        uint32_t wait_list_count;
        per_flow_ring_buffer_t flow_pkts[MAX_FLOW_TABLE_ENTRIES];      //indexed by flow_entry->entry_index
}pre_io_wait_queue_t;
pre_io_wait_queue_t pre_io_wait_ring;

int mark_flow_for_backpressure(struct rte_mbuf* pkt, struct onvm_flow_entry *flow_entry);
int clear_flow_for_backpressure(struct rte_mbuf* pkt, struct onvm_flow_entry *flow_entry);

/* Functions corresponding to Per flow Queue that holds enqueued packets that are waiting to perform I/O. */
int init_pre_io_wait_queue(void);
int is_flow_pkt_in_pre_io_wait_queue(__attribute__((unused)) struct rte_mbuf* pkt, struct onvm_flow_entry *flow_entry);
int add_flow_pkt_to_pre_io_wait_queue(struct rte_mbuf* pkt, struct onvm_flow_entry *flow_entry);
struct rte_mbuf* get_next_pkt_for_flow_entry_from_pre_io_wait_queue(struct onvm_flow_entry *flow_entry);
struct rte_mbuf* get_first_pkt_from_pre_io_wait_queue(struct onvm_flow_entry **flow_entry);

#define OFFSET_LIST_SIZE    (10)
static int offset_desc[OFFSET_LIST_SIZE] = { 24576, 4096, 8912, 12288, 16384, 20480, 24576, 28672, 16384, 32768 };
int get_read_file_offset(void);

#include <rte_ether.h>
#define MAX_PKT_HEADER_SIZE (68)
#define MAX_PKT_READ_SIZE   (1024)

uint16_t flow_bypass_list[] = {2,3}; //{0,1, 2,3, 6,7, 10,11, 14,15}; //{0,1, 4,5, 8,9, 12,13}; //{2,3, 6,7, 10,11, 14,15};
int check_in_flow_bypass_list(__attribute__((unused)) struct rte_mbuf* pkt, struct onvm_flow_entry *flow_entry);
int check_in_logged_flow_list(__attribute__((unused)) struct rte_mbuf* pkt, struct onvm_flow_entry *flow_entry);


int explicit_callback_function(void);
/******************************************************************************
 *              FUNCTION DEFINITIONS
 ******************************************************************************/


/* Handler for I/O completion signal */
#ifdef USE_SIGEV_SIGNAL
static void
ioSigHandler(int sig, siginfo_t *si, void *ucontext) {
        #ifdef ENABLE_DEBUG_LOGS
        printf("I/O completion signal received [%d]\n", sig);
        #endif
        //write(STDOUT_FILENO, "I/O completion signal received\n", 31);
        if(si != NULL && ucontext != NULL) {
                #ifdef ENABLE_DEBUG_LOGS
                printf("Received I/O signal [%d] with [%p, %p]", sig, si, ucontext);
                #endif //ENABLE_DEBUG_LOGS
        }

        aio_buf_t *pbuf = si->si_value.sival_ptr;

        if(pbuf) {
                notify_io_rw_done(pbuf);
        }
        else {
                #ifdef ENABLE_DEBUG_LOGS
                printf("Invalid Pbuf received with I/O signal [%d]", sig);
                #endif
        }
}
#else
/* Handler for SIGEV_THREAD */
static void
ioSigHandler(sigval_t sigval) {
//aio_CompletionRoutine(sigval_t sigval) {
        aio_buf_t *pbuf = sigval.sival_ptr;
        if(pbuf) {
                notify_io_rw_done(pbuf);
        }
        else {
                #ifdef ENABLE_DEBUG_LOGS
                printf("Invalid Pbuf received with I/O signal [%p]", sigval.sival_ptr);
                #endif
        }
        return;
}
#endif
static void
usage(const char *progname) {
        printf("Usage: %s [EAL args] -- [NF_LIB args] -- -d <destination> -p <print_delay>"
                "-s <service_chain_file> -r <IPv4_5tuple Rules file>"
                "-b <base_ip_address> -m <max_num_ips>        \n\n", progname);
}

static int
parse_app_args(int argc, char *argv[], const char *progname) {
        int c;

        while ((c = getopt(argc, argv, optString)) != -1) {
                switch (c) {
                case 'd':
                        globals.destination = strtoul(optarg, NULL, 10);
                        break;
                case 'p':
                        globals.print_delay = strtoul(optarg, NULL, 10);
                        break;
                case 's':
                        globals.pktlog_file = optarg;
                        break;
                case 'r':
                        globals.read_file = optarg;
                        break;
                case 'b':
                        globals.base_ip_addr = optarg;
                        break;
                case 'm':
                        globals.max_bufs = strtoul(optarg, NULL, 10);
                        break;
                case 'M':
                        globals.buf_size = strtoul(optarg, NULL, 10);
                        break;
                case '?':
                        usage(progname);
                        if (optopt == 'd')
                                RTE_LOG(INFO, APP, "Option -%c requires an argument.\n", optopt);
                        else if (optopt == 'p')
                                RTE_LOG(INFO, APP, "Option -%c requires an argument.\n", optopt);
                        else if (isprint(optopt))
                                RTE_LOG(INFO, APP, "Unknown option `-%c'.\n", optopt);
                        else
                                RTE_LOG(INFO, APP, "Unknown option character `\\x%x'.\n", optopt);
                        return -1;
                default:
                        usage(progname);
                        return -1;
                }
        }
        return optind;
}

int
initialize_aiocb(aio_buf_t *pbuf) {
       pbuf->aiocb->aio_buf     = (volatile void*)pbuf->buf;
       pbuf->aiocb->aio_fildes  = globals.fd;
       pbuf->aiocb->aio_nbytes  = pbuf->buf_len;
       pbuf->aiocb->aio_reqprio = AIO_REQUEST_PRIO;
       pbuf->aiocb->aio_offset  = globals.file_offset;
#ifdef USE_SIGEV_SIGNAL
       pbuf->aiocb->aio_sigevent.sigev_notify          = SIGEV_SIGNAL;
       pbuf->aiocb->aio_sigevent.sigev_signo           = IO_SIGNAL;
#else
       pbuf->aiocb->aio_sigevent.sigev_notify          = SIGEV_THREAD;
       pbuf->aiocb->aio_sigevent.sigev_notify_function = ioSigHandler;
#endif //USE_SIGEV_SIGNAL
       pbuf->aiocb->aio_sigevent.sigev_value.sival_ptr = pbuf;

       return 0;
}

int
initialize_aio_buffers (void) {
        int ret = 0;
        if(aio_buf_pool) {
                #ifdef ENABLE_DEBUG_LOGS
                printf("Already Allocated!!\n");
                #endif //ENABLE_DEBUG_LOGS
                return -1;
        }
        aio_buf_pool = rte_calloc("log_pktbuf_pool", globals.max_bufs, sizeof(*aio_buf_pool),0);
        if(NULL == aio_buf_pool) {
                rte_exit(EXIT_FAILURE, "Cannot allocate memory for log_pktbuf_pool\n");
        }
        uint8_t i = 0;
        uint32_t alloc_buf_size = MAX_PKT_BUF_SIZE;
        for(i=0; i< globals.max_bufs; i++) {
                alloc_buf_size              = MIN(globals.buf_size, MAX_PKT_BUF_SIZE);
                aio_buf_pool[i].buf         = rte_calloc("log_pktbuf_buf", alloc_buf_size, sizeof(uint8_t),0);
                aio_buf_pool[i].aiocb       = rte_calloc("log_pktbuf_aio", 1, sizeof(struct aiocb),0);
                aio_buf_pool[i].buf_index   = i;
                aio_buf_pool[i].pkt         = NULL;
                aio_buf_pool[i].max_size    = alloc_buf_size;
                aio_buf_pool[i].buf_len     = 0;
                aio_buf_pool[i].req_status  = 0;
                aio_buf_pool[i].state       = BUF_FREE;
                if(NULL == aio_buf_pool[i].buf || NULL == aio_buf_pool[i].aiocb) {
                        rte_exit(EXIT_FAILURE, "Cannot allocate memory for log_pktbuf_buf or log_pktbuf_aio \n");
                }
                else {
                        memset(aio_buf_pool[i].buf, 18, sizeof(uint8_t)*alloc_buf_size);

                        #ifdef ENABLE_DEBUG_LOGS
                        //for(ret=0; ret < 10; ret++) printf("%d", aio_buf_pool[i].buf[ret]);
                        printf("allocated buf [%d] of size [%d]\n ", (int)i, (int)alloc_buf_size);
                        #endif //ENABLE_DEBUG_LOGS
                }
                ret = initialize_aiocb(&aio_buf_pool[i]);
        }
        return ret;
}
int initialize_log_file(void) {
        globals.fd = open(globals.pktlog_file, FD_OPEN_MODE, 0666);
        if (-1 == globals.fd) {
                rte_exit(EXIT_FAILURE, "Cannot create file: %s \n", globals.pktlog_file);
        }
        globals.file_offset = 0;
        return globals.fd;
}

int
initialize_sync_variable(void) {
        wait_mutex = sem_open(SEM_NAME, O_CREAT, 06666, 0);
        if(wait_mutex == SEM_FAILED) {
                fprintf(stderr, "can not create semaphore!!\n");
                sem_unlink(SEM_NAME);
                exit(1);
        }
        return 0;
}

int
initialize_signal_action (void) {
#ifdef USE_SIGEV_SIGNAL
        struct sigaction sa;
        sigemptyset(&sa.sa_mask);
        sa.sa_flags = SA_RESTART | SA_SIGINFO;
        sa.sa_sigaction = ioSigHandler;
        if (sigaction(IO_SIGNAL, &sa, NULL) == -1)
                errExit("aio_sigaction");
#endif
        return 0;
}

int initialize_aio_nf(void) {
        int ret = 0;
        ret = initialize_log_file();
        ret = initialize_aio_buffers();
        ret = initialize_sync_variable();
        ret = initialize_signal_action();
        ret = init_pre_io_wait_queue();
        return ret;
}


int deinitialize_sync_variable(void) {
        if(wait_mutex) {
                sem_post(wait_mutex);
                sem_close(wait_mutex);
                sem_unlink(SEM_NAME);
                wait_mutex = NULL;
        }
        return 0;
}
int deinitialize_aiocb(aio_buf_t *pbuf) {
        return initialize_aiocb(pbuf);
}
int deinitialize_signal_action (void) {
        return 0;
}
int deinitialize_log_file(void) {
        if(globals.fd) {
                close(globals.fd);
                globals.fd = -1;
        }
        return 0;
}
int deinitialize_aio_buffers (void) {
        uint8_t i = 0;
        int ret = 0;
        if(aio_buf_pool) {
                for(i=0; i< globals.max_bufs; i++) {
                        //Address aio_cancel for pending requests and then free
                        ret = deinitialize_aiocb(&aio_buf_pool[i]);
                        rte_free(aio_buf_pool[i].aiocb);
                        aio_buf_pool[i].aiocb = NULL;
                        rte_free(aio_buf_pool[i].buf);
                        aio_buf_pool[i].buf = NULL;
                }
                rte_free(aio_buf_pool);
                aio_buf_pool = NULL;
        }
        return ret;
}
int deinitialize_aio_nf(void) {
        int ret = 0;
        ret = deinitialize_signal_action();
        ret = deinitialize_sync_variable();
        ret = deinitialize_log_file();
        ret = deinitialize_aio_buffers();

        return ret;
}

//#define USE_KEY_MODE_FOR_FLOW_ENTRY
static int get_flow_entry( struct rte_mbuf *pkt, struct onvm_flow_entry **flow_entry) {
        int ret = -1;
        if(flow_entry)*flow_entry = NULL;
#ifdef USE_KEY_MODE_FOR_FLOW_ENTRY
        struct onvm_ft_ipv4_5tuple fk;
        if ((ret = onvm_ft_fill_key(&fk, pkt))) {
                return ret;
        }
        ret = onvm_flow_dir_get_key(&fk, flow_entry);
#else  // #elif defined (USE_KEY_MODE_FOR_FLOW_ENTRY)
        ret = onvm_flow_dir_get_pkt(pkt, flow_entry);
#endif
        return ret;
}


int mark_flow_for_backpressure(struct rte_mbuf* pkt, struct onvm_flow_entry *flow_entry) {
        struct onvm_pkt_meta *meta = NULL;
        meta = onvm_get_pkt_meta(pkt);
        if(!(TEST_BIT(flow_entry->sc->highest_downstream_nf_index_id, meta->chain_index))) {
                SET_BIT(flow_entry->sc->highest_downstream_nf_index_id, meta->chain_index);
        }
        return 0 ; //exit(1);
}
int clear_flow_for_backpressure(struct rte_mbuf* pkt, struct onvm_flow_entry *flow_entry) {
        struct onvm_pkt_meta *meta = NULL;
        meta = onvm_get_pkt_meta(pkt);
        // Enable below line to skip the 1st NF in the chain Note: <=1 => skip Flow_rule_installer and the First NF in the chain; <1 => skip only the Flow_rule_installer NF
        //if(meta->chain_index < 1) continue;
        //Check the Flow Entry mark status and Add mark if not already done!
        if((TEST_BIT(flow_entry->sc->highest_downstream_nf_index_id, meta->chain_index))) {
                CLEAR_BIT(flow_entry->sc->highest_downstream_nf_index_id, meta->chain_index);
        }
    return 0;
}

int init_pre_io_wait_queue(void) {
        int i = 0;
        pre_io_wait_ring.wait_list_count=0;
        #ifdef USE_RTE_RING
        unsigned socket_id = SOCKET_ID_ANY;
        //unsigned socket_id =  rte_socket_id();
        const char * ring_name = NULL;
        const unsigned ring_size = PERFLOW_QUEUE_RINGSIZE;
        #endif
        for (i=0; i < MAX_FLOW_TABLE_ENTRIES; i++) {
                pre_io_wait_ring.flow_pkts[i].pkt_count =0;
                pre_io_wait_ring.flow_pkts[i].r_h =0;
                pre_io_wait_ring.flow_pkts[i].w_h =0;
                #ifndef USE_RTE_RING
                pre_io_wait_ring.flow_pkts[i].max_len =PERFLOW_QUEUE_RINGSIZE;
                #else
                ring_name = get_rte_ring_queue_name(i);
                pre_io_wait_ring.flow_pkts[i].pktbuf_rte_ring = rte_ring_create(ring_name, ring_size,socket_id,RING_F_SP_ENQ|RING_F_SC_DEQ);
                rte_ring_set_water_mark(pre_io_wait_ring.flow_pkts[i].pktbuf_rte_ring, PERFLOW_QUEUE_HIGH_WATERMARK);
                #endif
        }
        return 0;
}
int is_flow_pkt_in_pre_io_wait_queue(__attribute__((unused)) struct rte_mbuf* pkt, struct onvm_flow_entry *flow_entry) {
        if(!flow_entry) return 0;
        //return pre_io_wait_ring.flow_pkts[flow_entry->entry_index].pkt_count;
        #ifndef USE_RTE_RING
        if(pre_io_wait_ring.flow_pkts[flow_entry->entry_index].w_h ==  pre_io_wait_ring.flow_pkts[flow_entry->entry_index].r_h) return 0;
        return 1;
        #else
        if (rte_ring_empty(pre_io_wait_ring.flow_pkts[flow_entry->entry_index].pktbuf_rte_ring)) return 0;
        return 1;
        #endif
}
int add_flow_pkt_to_pre_io_wait_queue(struct rte_mbuf* pkt, struct onvm_flow_entry *flow_entry) {
        if(!flow_entry) return 0;
        
        #ifndef USE_RTE_RING       
        if(((pre_io_wait_ring.flow_pkts[flow_entry->entry_index].w_h+1)%pre_io_wait_ring.flow_pkts[flow_entry->entry_index].max_len) == pre_io_wait_ring.flow_pkts[flow_entry->entry_index].r_h) {
                #ifdef ENABLE_DEBUG_LOGS
                printf("\n***** OVERFLOW (Ring buffer Full!!) IN PRE_IO_WAIT_QUEUE!!****** \n");
                #endif
                //enable Backpressure on overflow
                mark_flow_for_backpressure(pkt,flow_entry);
                return 1;
        }
        if(0 == pre_io_wait_ring.flow_pkts[flow_entry->entry_index].pkt_count) {
            pre_io_wait_ring.wait_list_count++;
        }
        //pre_io_wait_ring.flow_pkts[flow_entry->entry_index].pktbuf_ring[pre_io_wait_ring.flow_pkts[flow_entry->entry_index].pkt_count++]=pkt;
        pre_io_wait_ring.flow_pkts[flow_entry->entry_index].pktbuf_ring[pre_io_wait_ring.flow_pkts[flow_entry->entry_index].w_h]=pkt; pre_io_wait_ring.flow_pkts[flow_entry->entry_index].pkt_count++;
        if((++(pre_io_wait_ring.flow_pkts[flow_entry->entry_index].w_h)) == pre_io_wait_ring.flow_pkts[flow_entry->entry_index].max_len) pre_io_wait_ring.flow_pkts[flow_entry->entry_index].w_h=0;
        
        //Check if Backpressure for this flow needs to be enabled !!
        if(pre_io_wait_ring.flow_pkts[flow_entry->entry_index].pkt_count >=PERFLOW_QUEUE_HIGH_WATERMARK) {
                //#ifdef ENABLE_DEBUG_LOGS
                printf("\n***** OVERFLOW (Exceeds High Water Mark!) IN PRE_IO_WAIT_QUEUE!!****** [r:%d, w:%d, c:%d]\n", pre_io_wait_ring.flow_pkts[flow_entry->entry_index].r_h, pre_io_wait_ring.flow_pkts[flow_entry->entry_index].w_h, pre_io_wait_ring.flow_pkts[flow_entry->entry_index].pkt_count);
                //#endif
                mark_flow_for_backpressure(pkt,flow_entry);
                return 0;
        }
        #else
        int sts = 0;
        if(rte_ring_full(pre_io_wait_ring.flow_pkts[flow_entry->entry_index].pktbuf_rte_ring)) {
                mark_flow_for_backpressure(pkt,flow_entry);
                #ifdef ENABLE_DEBUG_LOGS
                printf("\n Overflow ( Exceeds buffer size)! %d, %d\n", rte_ring_count(pre_io_wait_ring.flow_pkts[flow_entry->entry_index].pktbuf_rte_ring), (int)flow_entry->entry_index);
                #endif
                return 1;
        }
        sts = rte_ring_sp_enqueue(pre_io_wait_ring.flow_pkts[flow_entry->entry_index].pktbuf_rte_ring, pkt);
        if(sts) {
                #ifdef ENABLE_DEBUG_LOGS
                printf("\n Overflow ( Exceeds High Water mark)! %d, %d\n", rte_ring_count(pre_io_wait_ring.flow_pkts[flow_entry->entry_index].pktbuf_rte_ring), (int)flow_entry->entry_index);
                #endif
                mark_flow_for_backpressure(pkt,flow_entry);
        }
        else {
                if( 1 == rte_ring_count(pre_io_wait_ring.flow_pkts[flow_entry->entry_index].pktbuf_rte_ring)){
                pre_io_wait_ring.wait_list_count++;
                }
                #ifdef ENABLE_DEBUG_LOGS
                printf("\n added pkt to rte_ring: %d:%d:%d\n",rte_ring_count(pre_io_wait_ring.flow_pkts[flow_entry->entry_index].pktbuf_rte_ring), (int)flow_entry->entry_index, pre_io_wait_ring.wait_list_count);
                #endif
        }
        return 0;
        #endif

        return 0;
}
struct rte_mbuf* get_next_pkt_for_flow_entry_from_pre_io_wait_queue(struct onvm_flow_entry *flow_entry) {
        if(!flow_entry) return 0;
        struct rte_mbuf* pkt = NULL;
        
        #ifndef USE_RTE_RING
        if( pre_io_wait_ring.flow_pkts[flow_entry->entry_index].w_h == pre_io_wait_ring.flow_pkts[flow_entry->entry_index].r_h) return NULL; //empty
                
        pkt  = pre_io_wait_ring.flow_pkts[flow_entry->entry_index].pktbuf_ring[pre_io_wait_ring.flow_pkts[flow_entry->entry_index].r_h];
        if(pkt) {
                pre_io_wait_ring.flow_pkts[flow_entry->entry_index].pktbuf_ring[pre_io_wait_ring.flow_pkts[flow_entry->entry_index].r_h] = NULL;
                if((++(pre_io_wait_ring.flow_pkts[flow_entry->entry_index].r_h)) == pre_io_wait_ring.flow_pkts[flow_entry->entry_index].max_len) pre_io_wait_ring.flow_pkts[flow_entry->entry_index].r_h=0;
                pre_io_wait_ring.flow_pkts[flow_entry->entry_index].pkt_count--;
        }
        
        if(0 == pre_io_wait_ring.flow_pkts[flow_entry->entry_index].pkt_count) {
            pre_io_wait_ring.wait_list_count--;
            //if(pre_io_wait_ring.wait_list_count > 0)pre_io_wait_ring.wait_list_count--;
        }
        //Check if Backpressure for this flow needs to be disabled !!
        if(pre_io_wait_ring.flow_pkts[flow_entry->entry_index].pkt_count <= PERFLOW_QUEUE_LOW_WATERMARK) {
                clear_flow_for_backpressure(pkt,flow_entry);
        }
        #else
        int sts = 0;
        if(rte_ring_empty(pre_io_wait_ring.flow_pkts[flow_entry->entry_index].pktbuf_rte_ring)) return NULL;
        sts = rte_ring_sc_dequeue(pre_io_wait_ring.flow_pkts[flow_entry->entry_index].pktbuf_rte_ring, (void**)&pkt);
        if(sts) {
                return NULL; 
        } 
        else {
                if(rte_ring_count(pre_io_wait_ring.flow_pkts[flow_entry->entry_index].pktbuf_rte_ring) <= PERFLOW_QUEUE_LOW_WATERMARK) {
                        clear_flow_for_backpressure(pkt,flow_entry);
                }
                if(rte_ring_empty(pre_io_wait_ring.flow_pkts[flow_entry->entry_index].pktbuf_rte_ring)) {
                        pre_io_wait_ring.wait_list_count--;
                }
        }
        #endif
        return pkt;
}
struct rte_mbuf* get_first_pkt_from_pre_io_wait_queue(struct onvm_flow_entry **flow_entry) {
        if(0 ==pre_io_wait_ring.wait_list_count) return NULL;
        uint32_t i = 0;
        struct rte_mbuf* pkt = NULL;
        
        #ifndef USE_RTE_RING
        for ( i = 0; i < MAX_FLOW_TABLE_ENTRIES; i++ ) {
                    if(pre_io_wait_ring.flow_pkts[i].pkt_count) {
                        get_flow_entry(pre_io_wait_ring.flow_pkts[i].pktbuf_ring[pre_io_wait_ring.flow_pkts[i].r_h], flow_entry);
                        return get_next_pkt_for_flow_entry_from_pre_io_wait_queue(*flow_entry);
                        break;
                    }
        }
        #else
        for ( i = 0; i < MAX_FLOW_TABLE_ENTRIES; i++ ) {
                    if(rte_ring_count(pre_io_wait_ring.flow_pkts[i].pktbuf_rte_ring)) {
                        
                        if (0 == rte_ring_sc_dequeue(pre_io_wait_ring.flow_pkts[i].pktbuf_rte_ring, (void**)&pkt)) {
                                get_flow_entry(pkt, flow_entry);
                                if(rte_ring_count(pre_io_wait_ring.flow_pkts[i].pktbuf_rte_ring) <= PERFLOW_QUEUE_LOW_WATERMARK) {
                                        clear_flow_for_backpressure(pkt,*flow_entry);
                                }
                                if(rte_ring_empty(pre_io_wait_ring.flow_pkts[i].pktbuf_rte_ring)) {
                                        pre_io_wait_ring.wait_list_count--;
                                }
                                return pkt;
                        }
                        break;
                    }
        }
        #endif
        return pkt;
}

aio_buf_t* get_aio_buffer_from_aio_buf_pool(uint32_t aio_operation_mode) {
        if (AIO_READ_OPERATION == aio_operation_mode) {
                uint32_t i = 0;
                for (i=0; i < globals.max_bufs; i++) {
                        if (BUF_FREE == aio_buf_pool[i].state) {
                            //globals.cur_buf_index = i;
                            aio_buf_pool[i].state = BUF_IN_USE;
                            return &(aio_buf_pool[i]);
                        }
                }
                return NULL;
        }
        if (AIO_WRITE_OPERATION == aio_operation_mode) {
                if (BUF_IN_USE == aio_buf_pool[globals.cur_buf_index].state) {
                                return &(aio_buf_pool[globals.cur_buf_index]);
                }
                else if(BUF_FREE == aio_buf_pool[globals.cur_buf_index].state) {
                        aio_buf_pool[globals.cur_buf_index].state = BUF_IN_USE;
                        return &(aio_buf_pool[globals.cur_buf_index]);
                }
                else if (BUF_SUBMITTED == aio_buf_pool[globals.cur_buf_index].state) {
                        uint32_t i = 0;
                        for (i=0; i < globals.max_bufs; i++) {
                                if (aio_buf_pool[i].state != BUF_SUBMITTED) {
                                        globals.cur_buf_index = i;
                                        return &(aio_buf_pool[globals.cur_buf_index]);
                                }
                        }
                }
        }
                
        #ifdef ENABLE_DEBUG_LOGS
        printf("\nBuffer:[%p] state=%d, CurrentBufferIndex=%d", &aio_buf_pool[0], aio_buf_pool[0].state, globals.cur_buf_index);
        #endif //#ENABLE_DEBUG_LOGS
        return NULL;
}

int get_read_file_offset(void) {
    static int offset_index = 0;
    offset_index +=1; 
    if (offset_index >= OFFSET_LIST_SIZE) offset_index = 0;
    return offset_desc[offset_index];
}
int read_aio_buffer(aio_buf_t *pbuf) {
        int ret = 0;
        pbuf->aiocb->aio_nbytes = (size_t)pbuf->buf_len;
        pbuf->aiocb->aio_offset = (__off_t)get_read_file_offset();
        //globals.file_offset += 1; //pbuf->buf_len; //for now always read from offset 0; size 64 or 68 bytes based of pkt type.

#ifdef USE_SYNC_IO
        ret = pread(globals.fd, pbuf->buf, pbuf->aiocb->aio_nbytes, pbuf->aiocb->aio_offset);
        //globals.cur_buf_index = (((globals.cur_buf_index+1) % (globals.max_bufs))? (globals.cur_buf_index+1):(0));
        refresh_aio_buffer(pbuf);
        return ret;
#else
        pbuf->req_status = aio_read(pbuf->aiocb);
        if(-1 == pbuf->req_status) {
                printf("Error at aio_read(): %s\n", strerror(errno));
                ret = pbuf->req_status;
                return refresh_aio_buffer(pbuf);
                //exit(1);
        }
        pbuf->state = BUF_SUBMITTED;
#endif  //USE_SYNC_IO
        //globals.cur_buf_index = (((globals.cur_buf_index+1) % (globals.max_bufs))? (globals.cur_buf_index+1):(0));
        return ret;
}
int refresh_aio_buffer(aio_buf_t *pbuf) {
        int ret = 0;
        pbuf->aiocb->aio_nbytes = (size_t)0;
        pbuf->aiocb->aio_offset = (__off_t)0;
        pbuf->buf_len=0;
        pbuf->pkt = NULL;
        pbuf->state = BUF_FREE;
        #ifdef ENABLE_DEBUG_LOGS
        printf("\n Buffer [%p] state moved to FREE [%d]\n",pbuf, pbuf->state);
        #endif //ENABLE_DEBUG_LOGS
        return ret;
}

int wait_for_buffer_ready(unsigned int timeout_ms) {
        if(!wait_mutex) {
                //poll or return error
                return -1;
        }
        #ifdef ENABLE_DEBUG_LOGS
        printf("\n Waiting for Buffer Ready Notification. Block Count= [%d]!!\n", (int)globals.sem_block_count);
        #endif //ENABLE_DEBUG_LOGS

        if(!timeout_ms) {
                globals.is_blocked_on_sem = 1;
                globals.sem_block_count++;
                sem_wait(wait_mutex);
                globals.is_blocked_on_sem = 0;
        }
        else {
                struct timespec ts;
                if (clock_gettime(CLOCK_REALTIME, &ts) == -1) {
                        globals.is_blocked_on_sem = 1;
                        globals.sem_block_count++;
                        return sem_wait(wait_mutex);
                }
                ts.tv_nsec += timeout_ms*1000*1000;
                ts.tv_sec += ts.tv_nsec /1000000000;
                ts.tv_nsec %= 1000000000;
                globals.is_blocked_on_sem = 1;
                globals.sem_block_count++;
                int ret = sem_timedwait(wait_mutex,&ts);
                //what to do?
                if(ETIMEDOUT == ret) {
                        globals.is_blocked_on_sem = 0;
                }
                return ret;
        }
        #ifdef ENABLE_DEBUG_LOGS
        printf("\nWait Completed! [ block_state=%d, block_count=%d]!!\n", globals.is_blocked_on_sem, (int)globals.sem_block_count);
        #endif //ENABLE_DEBUG_LOGS
        return 0;
}

int notify_io_rw_done(aio_buf_t *pbuf) {

        pbuf->req_status = aio_error(pbuf->aiocb);
        
        if(0 != pbuf->req_status) {
                printf("\n Aio_read/write completed with error [ %d]\n", pbuf->req_status);
        }
        #ifdef ENABLE_DEBUG_LOGS
        else {
                printf("Aio_read/write completed Successfully [%d]!!\n", pbuf->req_status);
        }
        #endif //#ifdef ENABLE_DEBUG_LOGS

        if(pbuf->pkt) {
            #ifdef ACT_AS_BRIDGE
            struct onvm_pkt_meta *meta = onvm_get_pkt_meta(pbuf->pkt);
            nf_as_bridge(pbuf->pkt,meta);
            #endif
                onvm_nflib_return_pkt(pbuf->pkt);
        }
        
        int ret = refresh_aio_buffer(pbuf);
        
        #ifndef USE_SYNC_IO
        #if (PSEUDO_ASYNC_MODE == ASYNC_MODE)
        if(wait_mutex && globals.is_blocked_on_sem){
                sem_post(wait_mutex);
        }
        #else
        if(pre_io_wait_ring.wait_list_count) {   //packets in pre_io_wait_queue
                notify_for_ecb();
        }
        #endif
        #endif //#ifndef USE_SYNC_IO
        return ret;
}

int clear_thread_start(void *pdata) {
        printf("Waiting on clear signal: enter 999 to clear and 0 to exit \n:");
        int ret = 0;
        int input = 1;
        if(pdata){};
        while(input) {
                ret = scanf("%d",&input);
                if(input == 999) {
                       ret= 1;
                       printf("clear flow_rule_return_Status [%d]\n",ret);
                       printf("Waiting on clear signal: enter 999 to clear and 0 to exit \n:");
                }
        }
        return ret;
}

int check_in_flow_bypass_list(__attribute__((unused)) struct rte_mbuf* pkt, struct onvm_flow_entry *flow_entry) {
        
        //if(flow_entry && flow_entry->entry_index) {
        if(flow_entry) {
              uint16_t i = 0;
              for(i=0; i< sizeof(flow_bypass_list)/sizeof(uint16_t); i++) {
                      if(flow_bypass_list[i] == flow_entry->entry_index) return 1;
              }
        }
        return 0;
}

int check_in_logged_flow_list(__attribute__((unused)) struct rte_mbuf* pkt, struct onvm_flow_entry *flow_entry) {
        
        //if(flow_entry && flow_entry->entry_index) {
        if(flow_entry) {
              uint16_t i = 0;
              for(i=0; i< flow_logged_info.cur_entries; i++) {
                      if(flow_logged_info.ft_list[i] == flow_entry->entry_index) return 1;
              }
        }
        return 0;
}
int add_to_logged_flow_list(struct rte_mbuf* pkt);
int add_to_logged_flow_list(struct rte_mbuf* pkt) {
        if(MAX_FLOW_TABLE_ENTRIES == flow_logged_info.cur_entries) return 1;

        struct onvm_flow_entry *flow_entry = NULL;
        get_flow_entry(pkt, &flow_entry);
        if(flow_entry && flow_entry->entry_index) {
                flow_logged_info.ft_list[flow_logged_info.cur_entries++] = flow_entry->entry_index;
        }
        return 0;
}


/** Build Some Decision mode ACL logic that conditionally logs pkts from certain flows..
 * In simple case: it could be odd/even flow_entry, some src/dst port or hash.rss
 * **/
int validate_packet_and_do_io(struct rte_mbuf* pkt,  __attribute__((unused)) struct onvm_pkt_meta* meta) {
        struct onvm_flow_entry *flow_entry = NULL;
        get_flow_entry(pkt, &flow_entry);
        
        //if (pkt->hash.rss%3 == 0) return 0;
        if(check_in_flow_bypass_list(pkt, flow_entry)) return 0;
        
        //if(check_in_logged_flow_list(pkt, flow_entry)) return 0;
        
        //return 0;
        
        return packet_process_io(pkt, meta, flow_entry, PKT_LOG_WAIT_ENQUEUE_ENABLED);
}
int packet_process_io(struct rte_mbuf* pkt,  __attribute__((unused)) struct onvm_pkt_meta* meta, struct onvm_flow_entry *flow_entry, __attribute__((unused)) pkt_log_mode_e mode) {
#ifndef USE_SYNC_IO
        int ret = MARK_PACKET_TO_RETAIN;
        int queued_flow_flag = 0;
#else
        int ret = 0;
        int queued_flow_flag = 0;
#endif //USE_SYNC_IO

        if (NULL == flow_entry) {
                get_flow_entry(pkt, &flow_entry);
        }
        if(flow_entry) {
            #ifdef ENABLE_DEBUG_LOGS
            printf("Flow with Entry Index: %zu\n ", flow_entry->entry_index);
            #endif
        }
        else  {
                printf("\n Flow without any Flow Entry!!!:\n ");
                return 0;
        }

#ifndef USE_SYNC_IO
#if (PURE_ASYNC_MODE == ASYNC_MODE)
        queued_flow_flag = is_flow_pkt_in_pre_io_wait_queue(pkt, flow_entry);
        if(queued_flow_flag) {
                //Enqueue the packet to be processed later
                if((0 == add_flow_pkt_to_pre_io_wait_queue(pkt, flow_entry))){
                }
                //Failed to add pkt to the wait_queue ( indicates overflow.. mark to drop and setup the Flow OverFlow (backpressure)
                else {
                        #ifdef ENABLE_DEBUG_LOGS
                        printf("Dropping 1 Packets for the Flow with Entry Index: %zu\n ", flow_entry->entry_index);
                        #endif
                        return MARK_PACKET_FOR_DROP;
                }
        }
#endif
#endif
        aio_buf_t *pbuf = get_aio_buffer_from_aio_buf_pool(AIO_READ_OPERATION);
        if( (NULL == pbuf) ) {
#ifndef USE_SYNC_IO
                #if (PSEUDO_ASYNC_MODE == ASYNC_MODE) 
                wait_for_buffer_ready(0);
                pbuf = get_aio_buffer_from_aio_buf_pool(AIO_READ_OPERATION);
                if(pbuf == NULL){
                        #ifdef ENABLE_DEBUG_LOGS
                        printf("\n No empty Buffers!!\n");
                        #endif //#ifdef ENABLE_DEBUG_LOGS
                        return -1;
                }
                #else   //Pure Asynchronous Mode
                //Enqueue the packet to be processed later
                if((0 == add_flow_pkt_to_pre_io_wait_queue(pkt, flow_entry))){
                        return MARK_PACKET_TO_RETAIN;   //indicates the packet is held in the wait_queue and will be released later
                }
                //Failed to add pkt to the wait_queue ( indicates overflow.. mark to drop and setup the Flow OverFlow (backpressure)
                else {
                        #ifdef ENABLE_DEBUG_LOGS
                        printf("Dropping 1 Packets for the Flow with Entry Index: %zu\n ", flow_entry->entry_index);
                        #endif
                        return MARK_PACKET_FOR_DROP;
                }
                #endif
                
#endif //USE_SYNC_IO
        }

        if(pbuf != NULL) {
                pbuf->buf_len = MAX_PKT_READ_SIZE; //MIN(MAX_PKT_READ_SIZE, pkt->buf_len);
                #ifdef ENABLE_DEBUG_LOGS
                printf("\n reading ACL [%d] to Log Buffer after [%d] packets\n",pkt->buf_len, 1);
                #endif //#ifdef ENABLE_DEBUG_LOGS
                
#ifndef USE_SYNC_IO
                #if (PSEUDO_ASYNC_MODE != ASYNC_MODE)
                // To maintain the packet ordering: check if any of the packets are wait_enabled, then directly enqueue the packet
                if(queued_flow_flag) {
                        struct rte_mbuf* new_pkt = get_next_pkt_for_flow_entry_from_pre_io_wait_queue(flow_entry);
                        if( new_pkt != NULL) {
                                pkt = new_pkt;      // do io_on the first enqueued_packet()
                        }
                }
                #endif
#endif  //USE_SYNC_IO

                pbuf->pkt = pkt;
                read_aio_buffer(pbuf);
                //add_to_logged_flow_list(pkt);
        }
        return ret;
}

static void
do_stats_display(void) {
        //const char clr[] = { 27, '[', '2', 'J', '\0' };
        //const char topLeft[] = { 27, '[', '1', ';', '1', 'H', '\0' };
        static uint32_t pkt_process = 0;
        pkt_process+=0;

        /* Clear screen and move to top left */
        //printf("%s%s", clr, topLeft);

        //printf("AIO_ACLREADER STATS:\n");
        printf("-----\n");
        ////printf("Total Packets Serviced: %d\n", pkt_process);
        //printf("Total Bytes Written : %d\n", globals.file_offset);
        ////printf("Total Blocks on Sem : %d\n", (uint32_t)globals.sem_block_count);
        printf("Total Flows with pre_io_Wait: %d\n", pre_io_wait_ring.wait_list_count);
        printf("Total pkts in wait_list: %d, %d\n", pre_io_wait_ring.flow_pkts[0].w_h, pre_io_wait_ring.flow_pkts[0].pkt_count);
        //printf("N°   : %d\n", pkt_process);
        printf("\n\n");

}

int nf_as_bridge(struct rte_mbuf* pkt,  __attribute__((unused)) struct onvm_pkt_meta* meta) {
        if (pkt->port == 0) {
                meta->destination = 0;
        }
        else {
                meta->destination = 0;
        }
        meta->action = ONVM_NF_ACTION_OUT;
        meta->destination = pkt->port;
        return 0;
}

static int
packet_handler(struct rte_mbuf* __attribute__((unused)) pkt,  __attribute__((unused)) struct onvm_pkt_meta* meta) { // __attribute__((unused))
        static uint32_t counter = 0;
                if (++counter == DISPLAY_AFTER_PACKETS) {
                        do_stats_display();
                        counter = 0;
                }

        int ret=0;
        ret = validate_packet_and_do_io(pkt, meta); // packet_process_io(pkt, meta, NULL, PKT_LOG_WAIT_ENQUEUE_ENABLED);
        
//For time being act as bridge:
        if (ret == 0) {
#ifdef ACT_AS_BRIDGE
            nf_as_bridge(pkt,meta);
#else
                meta->action = ONVM_NF_ACTION_NEXT;
                meta->destination = pkt->port;
#endif
        }
        // Check if the packet is marked for DROP
        if(MARK_PACKET_FOR_DROP == ret) {
                meta->action = ONVM_NF_ACTION_DROP;
                ret = 0;
        }
        return ret;
}


int explicit_callback_function(void) {
        
        #ifdef ENABLE_DEBUG_LOGS
        printf("Inside NFs Explicit Callback Function\n");
        #endif
        //while(pbuf) keep processing packets from per_flow_pre_io_wait_queue
        int done = 0;
        int count = 0;
        aio_buf_t *pbuf = NULL;
        struct onvm_flow_entry *flow_entry = NULL;
        struct rte_mbuf* pkt = NULL;
        do {
                pbuf = get_aio_buffer_from_aio_buf_pool(AIO_READ_OPERATION);
                if( NULL == pbuf ) {
                        done = 1; break;//return 0;
                }
                else {
                        pkt = get_first_pkt_from_pre_io_wait_queue(&flow_entry);
                        if(pkt && flow_entry) {
                                pbuf->pkt = pkt;
                                pbuf->buf_len = MAX_PKT_READ_SIZE;
                                read_aio_buffer(pbuf);
                                pkt = NULL; flow_entry = NULL;
                        }
                        else {
                                done = 1; break;
                        }
                }
                count++;
        }while (!done);
         #ifdef ENABLE_DEBUG_LOGS
         printf("Total Packets processed in explicit_callback()= %d\n", count);
         #endif
        return done;
}

int main(int argc, char *argv[]) {
        int arg_offset;
        int ret = 0;
        const char *progname = argv[0];

        if ((arg_offset = onvm_nflib_init(argc, argv, NF_TAG)) < 0)
                return -1;
        argc -= arg_offset;
        argv += arg_offset;

        if (parse_app_args(argc, argv, progname) < 0)
                rte_exit(EXIT_FAILURE, "Invalid command-line arguments\n");
        
        if (0 == globals.destination || globals.destination == nf_info->service_id) {
                        globals.destination = nf_info->service_id + 1;
        }

        /* Map the sdn_ft table */
        onvm_flow_dir_nf_init();
        
        ret = initialize_aio_nf();
        if(ret) {
                rte_exit(EXIT_FAILURE, "Initialization failed!! error [%d] \n", ret);
        }
        
        register_explicit_callback_function(explicit_callback_function);
        onvm_nflib_run(nf_info, &packet_handler);
        printf("If we reach here, program is ending");
        
        ret = deinitialize_aio_nf();

        return 0;

        //return aiotest_main(argc, argv);
}
