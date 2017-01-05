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

//NF specific Feature Options
//#define ENABLE_DEBUG_LOGS
//#define USE_SYNC_IO

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
        .read_file = "ipv4rules.txt",
        .base_ip_addr   = "10.0.0.1",
        .max_bufs   = 1, //1,
        .buf_size   = 128, //4096, //128
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

typedef enum PktBufState {
        BUF_FREE = 0,
        BUF_IN_USE =1,
        BUF_SUBMITTED=2,
}PktBufState_e;

typedef struct pkt_buf_t {
        volatile PktBufState_e state;
        void* buf;
        uint32_t max_size;
        uint32_t buf_len;

        struct aiocb *aiocb;
        int req_status;
}pkt_buf_t;
static pkt_buf_t *pkt_buf_pool = NULL;
//static pkt_buf_t *pkt_buf_pord = NULL;    //For read puropose */


#define IO_SIGNAL SIGUSR1   /* Signal used to notify I/O completion */
#define SEM_NAME "PKTLOGGER_SYNC_VAR"
sem_t *wait_mutex = NULL;
#define AIO_REQUEST_PRIO (0)

#define WAIT_PACKET_STORE_SIZE  (32)
typedef struct wait_packet_buf {
        struct rte_mbuf *buffer[WAIT_PACKET_STORE_SIZE];
        uint16_t count;
}wait_packet_buf_t;
static wait_packet_buf_t wait_pkts;
/******************************************************************************
 *              FUNCTION DECLARATIONS
 ******************************************************************************/
int initialize_log_buffers (void);
int clear_thread_start(void *pdata);
/* Handler for I/O completion signal */
#ifdef USE_SIGEV_SIGNAL
static void ioSigHandler(int sig, siginfo_t *si, void *ucontext);
#else
static void ioSigHandler(sigval_t sigval);
#endif
//static void aio_CompletionRoutine(sigval_t sigval);
int initialize_sync_variable(void);
int initialize_aiocb(pkt_buf_t *pbuf);
int initialize_signal_action (void);
int initialize_log_file(void);
int initialize_logger_nf(void);

int deinitialize_log_buffers (void);
int deinitialize_sync_variable(void);
int deinitialize_aiocb(pkt_buf_t *pbuf);
int deinitialize_signal_action (void);
int deinitialize_log_file(void);
int deinitialize_logger_nf(void);

pkt_buf_t* get_buffer_to_log(void);
int write_log_buffer(pkt_buf_t *pbuf);
int refresh_log_buffer(pkt_buf_t *pbuf);
int read_log_buffer(pkt_buf_t *pbuf);

int wait_for_buffer_ready(unsigned int timeout_ms);
int notify_io_write_done(pkt_buf_t *pbuf);

//mode=0=> from pkt_handler (can enqueue to wait), mode=1 => from wiat_queue ( cannot enqueue to wiat)
typedef enum pkt_log_mode {
        PKT_LOG_WAIT_ENQUEUE_ENABLED = 0,
        PKT_LOG_WAIT_ENQUEUE_DISABLED=1,
}pkt_log_mode_e;
int log_the_packet(struct rte_mbuf* pkt, pkt_log_mode_e mode);
int validate_and_log_the_packet(struct rte_mbuf* pkt);

int add_buf_to_wait_pkts(struct rte_mbuf* pkt);
int log_all_wait_buf_pkts(void);
/******************************************************************************
 *              FUNCTION DEFINITIONS
 ******************************************************************************/
/*Wait_buf enqueue and dequeue
 *
 */
int add_buf_to_wait_pkts(struct rte_mbuf* pkt) {
        if(wait_pkts.count < WAIT_PACKET_STORE_SIZE) {
                wait_pkts.buffer[wait_pkts.count++] = pkt;
                return 0;
        }
        return 1;
}

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

        pkt_buf_t *pbuf = si->si_value.sival_ptr;

        if(pbuf) {
                notify_io_write_done(pbuf);
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
        pkt_buf_t *pbuf = sigval.sival_ptr;
        if(pbuf) {
                notify_io_write_done(pbuf);
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
initialize_aiocb(pkt_buf_t *pbuf) {
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
initialize_log_buffers (void) {
        int ret = 0;
        if(pkt_buf_pool) {
                #ifdef ENABLE_DEBUG_LOGS
                printf("Already Allocated!!");
                #endif //ENABLE_DEBUG_LOGS
                return -1;
        }
        pkt_buf_pool = rte_calloc("log_pktbuf_pool", globals.max_bufs, sizeof(*pkt_buf_pool),0);
        if(NULL == pkt_buf_pool) {
                rte_exit(EXIT_FAILURE, "Cannot allocate memory for log_pktbuf_pool\n");
        }
        uint8_t i = 0;
        uint32_t alloc_buf_size = MAX_PKT_BUF_SIZE;
        for(i=0; i< globals.max_bufs; i++) {
                alloc_buf_size              = MIN(globals.buf_size, MAX_PKT_BUF_SIZE);
                pkt_buf_pool[i].buf         = rte_calloc("log_pktbuf_buf", alloc_buf_size, sizeof(uint8_t),0);
                pkt_buf_pool[i].aiocb       = rte_calloc("log_pktbuf_aio", 1, sizeof(struct aiocb),0);
                pkt_buf_pool[i].max_size    = alloc_buf_size;
                pkt_buf_pool[i].buf_len     = 0;
                pkt_buf_pool[i].req_status  = 0;
                pkt_buf_pool[i].state       = BUF_FREE;
                if(NULL == pkt_buf_pool[i].buf || NULL == pkt_buf_pool[i].aiocb) {
                        rte_exit(EXIT_FAILURE, "Cannot allocate memory for log_pktbuf_buf or log_pktbuf_aio \n");
                }
                else {
                        memset(pkt_buf_pool[i].buf, 18, sizeof(uint8_t)*alloc_buf_size);

                        #ifdef ENABLE_DEBUG_LOGS
                        for(ret=0; ret < 10; ret++) printf("%d", pkt_buf_pool[i].buf[ret]);
                        printf("allocated buf [%d] of size [%d]\n ", (int)i, (int)alloc_buf_size);
                        #endif //ENABLE_DEBUG_LOGS
                }
                ret = initialize_aiocb(&pkt_buf_pool[i]);
        }
        return ret;
}
int initialize_log_file(void) {
        globals.fd = open(globals.pktlog_file, O_RDONLY, 0);
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

int initialize_logger_nf(void) {
        int ret = 0;
        ret = initialize_log_file();
        ret = initialize_log_buffers();
        ret = initialize_sync_variable();
        ret = initialize_signal_action();
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
int deinitialize_aiocb(pkt_buf_t *pbuf) {
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
int deinitialize_log_buffers (void) {
        uint8_t i = 0;
        int ret = 0;
        if(pkt_buf_pool) {
                for(i=0; i< globals.max_bufs; i++) {
                        //Address aio_cancel for pending requests and then free
                        ret = deinitialize_aiocb(&pkt_buf_pool[i]);
                        rte_free(pkt_buf_pool[i].aiocb);
                        pkt_buf_pool[i].aiocb = NULL;
                        rte_free(pkt_buf_pool[i].buf);
                        pkt_buf_pool[i].buf = NULL;
                }
                rte_free(pkt_buf_pool);
                pkt_buf_pool = NULL;
        }
        return ret;
}
int deinitialize_logger_nf(void) {
        int ret = 0;
        ret = deinitialize_signal_action();
        ret = deinitialize_sync_variable();
        ret = deinitialize_log_file();
        ret = deinitialize_log_buffers();

        return ret;
}

pkt_buf_t* get_buffer_to_log(void) {
        if (BUF_IN_USE == pkt_buf_pool[globals.cur_buf_index].state) {
                        return &(pkt_buf_pool[globals.cur_buf_index]);
        }
        else if(BUF_FREE == pkt_buf_pool[globals.cur_buf_index].state) {
                pkt_buf_pool[globals.cur_buf_index].state = BUF_IN_USE;
                return &(pkt_buf_pool[globals.cur_buf_index]);
        }
        else if (BUF_SUBMITTED == pkt_buf_pool[globals.cur_buf_index].state) {
                uint32_t i = 0;
                for (i=0; i < globals.max_bufs; i++) {
                        if (pkt_buf_pool[i].state != BUF_SUBMITTED) {
                                globals.cur_buf_index = i;
                                return &(pkt_buf_pool[globals.cur_buf_index]);
                        }
                }
        }
        #ifdef ENABLE_DEBUG_LOGS
        printf("\nBuffer:[%p] state=%d, CurrentBufferIndex=%d", &pkt_buf_pool[0], pkt_buf_pool[0].state, globals.cur_buf_index);
        #endif //#ENABLE_DEBUG_LOGS
        return NULL;
}
int read_log_buffer(pkt_buf_t *pbuf) {
        int ret = 0;
        pbuf->aiocb->aio_nbytes = (size_t)pbuf->buf_len;
        pbuf->aiocb->aio_offset = (__off_t)globals.file_offset;
        //globals.file_offset += pbuf->buf_len; //for now always read from offset 0; size 64 or 68 bytes based of pkt type.

#ifdef USE_SYNC_IO
        ret = pread(globals.fd, pbuf->buf, pbuf->aiocb->aio_nbytes, pbuf->aiocb->aio_offset);
        globals.cur_buf_index = (((globals.cur_buf_index+1) % (globals.max_bufs))? (globals.cur_buf_index+1):(0));
        refresh_log_buffer(pbuf);
        return ret;
#endif
        pbuf->req_status = aio_read(pbuf->aiocb);
        if(-1 == pbuf->req_status) {
                printf("Error at aio_write(): %s\n", strerror(errno));
                ret = pbuf->req_status;
                return refresh_log_buffer(pbuf);
                //exit(1);
        }
        pbuf->state = BUF_SUBMITTED;
        //globals.cur_buf_index = ((globals.cur_buf_index+1) % (globals.max_bufs));
        globals.cur_buf_index = (((globals.cur_buf_index+1) % (globals.max_bufs))? (globals.cur_buf_index+1):(0));
        return ret;
}
int write_log_buffer(pkt_buf_t *pbuf) {
        int ret = 0;
        pbuf->aiocb->aio_nbytes = (size_t)pbuf->buf_len;
        pbuf->aiocb->aio_offset = (__off_t)globals.file_offset;
        globals.file_offset += pbuf->buf_len;

#ifdef USE_SYNC_IO
        ret = pwrite(globals.fd, pbuf->buf, pbuf->aiocb->aio_nbytes, pbuf->aiocb->aio_offset);
        globals.cur_buf_index = (((globals.cur_buf_index+1) % (globals.max_bufs))? (globals.cur_buf_index+1):(0));
        refresh_log_buffer(pbuf);
        return ret;
#endif
        pbuf->req_status = aio_write(pbuf->aiocb);
        if(-1 == pbuf->req_status) {
                printf("Error at aio_write(): %s\n", strerror(errno));
                ret = pbuf->req_status;
                return refresh_log_buffer(pbuf);
                //exit(1);
        }
        pbuf->state = BUF_SUBMITTED;
        //globals.cur_buf_index = ((globals.cur_buf_index+1) % (globals.max_bufs));
        globals.cur_buf_index = (((globals.cur_buf_index+1) % (globals.max_bufs))? (globals.cur_buf_index+1):(0));
        return ret;
}
int refresh_log_buffer(pkt_buf_t *pbuf) {
        int ret = 0;
        pbuf->aiocb->aio_nbytes = (size_t)0;
        pbuf->aiocb->aio_offset = (__off_t)0;
        pbuf->state = BUF_FREE;
        pbuf->buf_len=0;
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

int notify_io_write_done(pkt_buf_t *pbuf) {

        pbuf->req_status = aio_error(pbuf->aiocb);
        #ifdef ENABLE_DEBUG_LOGS
        if(0 != pbuf->req_status) {
                printf("\n Aio_write completed with error [ %d]\n", pbuf->req_status);
        }
        else {
                printf("Aio_write completed Successfully [%d]!!\n", pbuf->req_status);
        }
        #endif //#ifdef ENABLE_DEBUG_LOGS

        int ret = refresh_log_buffer(pbuf);
        if(wait_mutex && globals.is_blocked_on_sem){
                sem_post(wait_mutex);
        }
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
#include <rte_ether.h>
#define MAX_PKT_HEADER_SIZE (68)

int log_all_wait_buf_pkts(void) {
        if(wait_pkts.count) {
               uint32_t i = 0;
               for(i=0; i< wait_pkts.count; i++) {
                       log_the_packet(wait_pkts.buffer[i], PKT_LOG_WAIT_ENQUEUE_DISABLED);
                       onvm_nflib_return_pkt(wait_pkts.buffer[i]);
               }
               wait_pkts.count = 0;
        }
        return 0;
}
#define USE_KEY_MODE_FOR_FLOW_ENTRY
static int get_flow_entry( struct rte_mbuf *pkt, struct onvm_flow_entry **flow_entry);
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

uint16_t flow_bypass_list[] = {0,1, 4,5, 8,9, 12,13};
int check_in_flow_bypass_list(struct rte_mbuf* pkt);
int check_in_flow_bypass_list(struct rte_mbuf* pkt) {
        struct onvm_flow_entry *flow_entry = NULL;
        get_flow_entry(pkt, &flow_entry);
        if(flow_entry && flow_entry->entry_index) {
              uint16_t i = 0;
              for(i=0; i< sizeof(flow_bypass_list)/sizeof(uint16_t); i++) {
                      if(flow_bypass_list[i] == flow_entry->entry_index) return 1;
              }
        }
        return 0;
}
int check_in_logged_flow_list(struct rte_mbuf* pkt);
int check_in_logged_flow_list(struct rte_mbuf* pkt) {
        struct onvm_flow_entry *flow_entry = NULL;
        get_flow_entry(pkt, &flow_entry);
        if(flow_entry && flow_entry->entry_index) {
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
int validate_and_log_the_packet(struct rte_mbuf* pkt) {
        //if (pkt->hash.rss%3 == 0) return 0;
        if(check_in_flow_bypass_list(pkt)) return 0;
        if(check_in_logged_flow_list(pkt)) return 0;
        return log_the_packet(pkt,PKT_LOG_WAIT_ENQUEUE_ENABLED);
}
int log_the_packet(struct rte_mbuf* pkt, pkt_log_mode_e mode) {
        int ret = 0;

        //check if there are prior enqueued packets, before loggin this input packet from pkt_handler
        if(PKT_LOG_WAIT_ENQUEUE_ENABLED == mode) {
                log_all_wait_buf_pkts();
        }

        pkt_buf_t *pbuf = get_buffer_to_log();
        if( NULL == pbuf) {
                if((PKT_LOG_WAIT_ENQUEUE_ENABLED == mode)  && (0 == add_buf_to_wait_pkts(pkt))){
                        return 1;   //indicates the packet is held in the wait_queue and will be released later
                } //else: enqueue to wiat_bufs has failed, block till bufs are released..
                wait_for_buffer_ready(0);

                pbuf = get_buffer_to_log();
                if(pbuf == NULL){
                        #ifdef ENABLE_DEBUG_LOGS
                        printf("\n No empty Buffers!!\n");
                        #endif //#ifdef ENABLE_DEBUG_LOGS
                        return -1;
                }
        }

        if(pbuf != NULL) {
                log_all_wait_buf_pkts();

                pbuf->buf_len = MIN(MAX_PKT_HEADER_SIZE, pkt->buf_len);

                #ifdef ENABLE_DEBUG_LOGS
                printf("\n reading ACL [%d] to Log Buffer after [%d] packets\n",pkt->buf_len, 1);
                #endif //#ifdef ENABLE_DEBUG_LOGS
                read_log_buffer(pbuf);
                add_to_logged_flow_list(pkt);
        }

        return ret;
}

static void
do_stats_display(void) {
        //const char clr[] = { 27, '[', '2', 'J', '\0' };
        //const char topLeft[] = { 27, '[', '1', ';', '1', 'H', '\0' };
        static uint32_t pkt_process = 0;
        pkt_process+=100000;

        /* Clear screen and move to top left */
        //printf("%s%s", clr, topLeft);

        printf("PKT_LOGGER STATS:\n");
        printf("-----\n");
        printf("Total Packets Serviced: %d\n", pkt_process);
        printf("Total Bytes Written : %d\n", globals.file_offset);
        printf("Total Blocks on Sem : %d\n", (uint32_t)globals.sem_block_count);
        //printf("N°   : %d\n", pkt_process);
        printf("\n\n");
}

static int
packet_handler(struct rte_mbuf* __attribute__((unused)) pkt, struct onvm_pkt_meta* __attribute__((unused)) meta) { // __attribute__((unused))
        static uint32_t counter = 0;
                if (++counter == 100000) {
                        do_stats_display();
                        counter = 0;
                }

        int ret=0;
        ret = validate_and_log_the_packet(pkt); // log_the_packet(pkt,PKT_LOG_WAIT_ENQUEUE_ENABLED);

        //For time being act as bridge:
//#define ACT_AS_BRIDGE
#ifdef ACT_AS_BRIDGE
        if (pkt->port == 0) {
                meta->destination = 1;
        }
        else {
                meta->destination = 0;
        }
        meta->action = ONVM_NF_ACTION_OUT;
#else
        meta->action = ONVM_NF_ACTION_NEXT;
        meta->destination = pkt->port;
#endif
        return ret;
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

        ret = initialize_logger_nf();
        if(ret) {
                rte_exit(EXIT_FAILURE, "Initialization failed!! error [%d] \n", ret);
        }

        onvm_nflib_run(nf_info, &packet_handler);
        printf("If we reach here, program is ending");
        
        ret = deinitialize_logger_nf();

        return 0;

        //return aiotest_main(argc, argv);
}
/*
 *
 * ADIO READ FROM STDIN EXAMPLE
 * include <sys/types.h>
#include <aio.h>
#include <fcntl.h>
#include <errno.h>
#include <iostream>

using namespace std;

const int SIZE_TO_READ = 100;

int main()
{
    // open the file
    int file = open("blah.txt", O_RDONLY, 0);

    if (file == -1)
    {
        cout << "Unable to open file!" << endl;
        return 1;
    }

    // create the buffer
    char* buffer = new char[SIZE_TO_READ];

    // create the control block structure
    aiocb cb;

    memset(&cb, 0, sizeof(aiocb));
    cb.aio_nbytes = SIZE_TO_READ;
    cb.aio_fildes = file;
    cb.aio_offset = 0;
    cb.aio_buf = buffer;

    // read!
    if (aio_read(&cb) == -1)
    {
        cout << "Unable to create request!" << endl;
        close(file);
    }

    cout << "Request enqueued!" << endl;

    // wait until the request has finished
    while(aio_error(&cb) == EINPROGRESS)
    {
        cout << "Working..." << endl;
    }

    // success?
    int numBytes = aio_return(&cb);

    if (numBytes != -1)
        cout << "Success!" << endl;
    else
        cout << "Error!" << endl;

    // now clean up
    delete[] buffer;
    close(file);

    return 0;
}
 */
