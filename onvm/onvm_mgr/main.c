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
                                   main.c

     File containing the main function of the manager and all its worker
     threads.

******************************************************************************/


#include "onvm_mgr.h"
#include "onvm_stats.h"
#include "onvm_pkt.h"
#include "onvm_nf.h"

#ifdef ONVM_MGR_ACT_AS_2PORT_FWD_BRIDGE
static int onv_pkt_send_on_alt_port(struct thread_info *rx, struct rte_mbuf *pkts[], uint16_t rx_count);
#endif //ONVM_MGR_ACT_AS_2PORT_FWD_BRIDGE

#ifdef INTERRUPT_SEM
struct wakeup_info *wakeup_infos;
#endif //INTERRUPT_SEM

#ifdef INTERRUPT_SEM
//#define ENABLE_PERFORMANCE_LOG

#ifdef ENABLE_PERFORMANCE_LOG
#ifdef USE_MQ
const char *cmd = "pidstat -C \"bridge|forward|monitor|basic_nf|\" -lrsuwh 1 30 > pidstat_log_MQ.csv";
const char *cmd2 = "perf stat --cpu=0-14  -r 30 -o perf_log_MQ.csv sleep 1"; //-I 2000
//-I 2000
#endif
#ifdef USE_SEMAPHORE
const char *cmd = "pidstat -C \"bridge|forward|monitor\" -lrsuwh 1 30 > pidstat_log_SEM.csv";
const char *cmd2 = "perf stat --cpu=0-14  -r 30 -o perf_log_SEM.csv sleep 1"; //-I 2000
#endif
#ifdef USE_SCHED_YIELD
const char *cmd = "pidstat -C \"bridge|forward|monitor|basic_nf|\" -lrsuwh 1 30 > pidstat_log_YLD.csv";
const char *cmd2 = "perf stat --cpu=0-14 -r 30 -o perf_log_YLD.csv sleep 1"; //-I 2000
#endif
#ifdef USE_SOCKET
const char *cmd = "pidstat -C \"bridge|forward|monitor|basic_nf|\" -lrsuwh 1 30 > pidstat_log_SOCK.csv";
const char *cmd2 = "perf stat --cpu=0-14 -r 30 -o perf_log_SOCK.csv sleep 1"; //-I 2000
#endif
#ifdef USE_SIGNAL
const char *cmd = "pidstat -C \"bridge|forward|monitor|basic_nf|\" -lrsuwh 1 30 > pidstat_log_SIG.csv";
const char *cmd2 = "perf stat --cpu=0-14 -r 30 -o perf_log_SIG.csv sleep 1"; //-I 2000
#endif
#ifdef USE_MQ2
const char *cmd = "pidstat -C \"bridge|forward|monitor|basic_nf|\" -lrsuwh 1 30 > pidstat_log_MQ2.csv";
const char *cmd2 = "perf stat --cpu=0-14 -r 30 -o perf_log_MQ2.csv sleep 1"; //-I 2000
#endif
#ifdef USE_ZMQ
const char *cmd = "pidstat -C \"bridge|forward|monitor|basic_nf|\" -lrsuwh 1 30 > pidstat_log_ZMQ.csv";
const char *cmd2 = "perf stat --cpu=0-14 -r 30 -o perf_log_ZMQ.csv sleep 1"; //-I 2000
#endif
#ifdef USE_FLOCK
const char *cmd = "pidstat -C \"bridge|forward|monitor|basic_nf|\" -lrsuwh 1 30 > pidstat_log_FLK.csv";
const char *cmd2 = "perf stat --cpu=0-14,10 --per-core -r 30 -o perf_log_FLK.csv sleep 1"; //-I 2000
#endif
#ifdef USE_NANO_SLEEP
const char *cmd = "pidstat -C \"bridge|forward|monitor|basic_nf|\" -lrsuwh 1 30 > pidstat_log_NNS.csv";
const char *cmd2 = "perf stat --cpu=0-14,10 --per-core -r 30 -o perf_log_NNS.csv sleep 1"; //-I 2000
#endif
#ifdef USE_POLL_MODE
const char *cmd = "pidstat -C \"bridge|forward|monitor|basic_nf|\" -lrsuwh 1 30 > pidstat_log_POLL.csv";
const char *cmd2 = "perf stat --cpu=0-14  -r 30 -o perf_log_POLL.csv sleep 1"; //-I 2000
#endif

//const char *cmd2 = "perf stat -B -e cache-references,cache-misses,cycles,instructions,branches,faults,migrations --cpu=0-14,10 --per-core -o perf_log.csv sleep 10"; //-I 2000
//const char *cmd2 = "perf stat --cpu=0-14,23 --per-core -o perf_log.csv sleep 30"; //-I 2000
//static int do_performance_log(void __attribute__((unused)) *pd_data) {
static int do_performance_log_2(void) {
        FILE *pp =NULL;
        //FILE *pp2 =NULL;
        //char buf[256];
        //return 0;
        pp = popen(cmd, "r");
        //pp2 = popen(cmd2, "r");
        int done = 0;
        //if (pp != NULL && pp2 != NULL) {
        if (pp != NULL ) {
                while (1) {
                        //char *line = NULL;

                        done = system(cmd2); //this will block as it runs in the foreground;
                        done |= 0x02;

                        /*
                        line = fgets(buf, sizeof buf, pp);
                        if (line == NULL) done |= 0x01; //break;
                        //printf("%s", line);

                        */

                        done |=0x01;

                        /* line = fgets(buf, sizeof buf, pp2);
                        if (line == NULL) done |= 0x02; //break;
                        //printf("%s", line);
                        */

                        if(done == 0x03) break;
                }
                //printf("\n perf_logger: %d\n", done);
                //sleep(10);
                pclose(pp);
                //pclose(pp2);
        }
        return 0;
}
static int
performance_log_thread(void *pdata) {
        if(!pdata) return 0;
        while (1) {
                if(pdata){;}

                if(!num_clients) continue;

                sleep(20);

                printf("********************* Starting to LOG PEFORMANCE COUNTERS!!!! *************\n\n");
                //do_performance_log(pdata);
                do_performance_log_2();
                printf("********************* END of LOG PEFORMANCE COUNTERS!!!! *************\n\n");

                sleep(20);

                break;
        }
        //return 0;
        while (1) {
                printf("********************* IDLING PEFORMANCE COUNTERS!!!! *************\n\n");
                sleep(10);
        }
        return 0;
}
#endif // ENABLE_PERFORMANCE_LOG
#endif // INTERRUPT_SEM

#ifdef ENABLE_USE_RTE_TIMER_MODE_FOR_MAIN_THREAD
#define NF_STATUS_CHECK_PERIOD_IN_MS    (500)       // 500ms or 0.5seconds
#define DISPLAY_STATS_PERIOD_IN_MS      (1000)      // 1000ms or Every second
#define NF_LOAD_EVAL_PERIOD_IN_MS       (1)         // 1ms
#define USLEEP_INTERVAL_IN_US           (500)       // 500 micro seconds

#define SECOND_TO_MICRO_SECOND          (1000*1000)
#define NANO_SECOND_TO_MICRO_SECOND     (double)(1/1000)
#define MICRO_SECOND_TO_SECOND          (double)(1/(SECOND_TO_MICRO_SECOND))

#define USE_THIS_CLOCK  CLOCK_MONOTONIC //CLOCK_THREAD_CPUTIME_ID     //CLOCK_PROCESS_CPUTIME_ID //CLOCK_MONOTONIC
typedef struct nf_stats_time_info {
        uint8_t in_read;
        struct timespec prev_time;
        struct timespec cur_time;
}nf_stats_time_info_t;
static nf_stats_time_info_t nf_stat_time;

struct rte_timer display_stats_timer;
struct rte_timer nf_status_check_timer;
struct rte_timer nf_load_eval_timer;

int initialize_master_timers(void);
static void display_stats_timer_cb(__attribute__((unused)) struct rte_timer *ptr_timer,
        __attribute__((unused)) void *ptr_data);
static void nf_status_check_timer_cb(__attribute__((unused)) struct rte_timer *ptr_timer,
        __attribute__((unused)) void *ptr_data);
static void nf_load_stats_timer_cb(__attribute__((unused)) struct rte_timer *ptr_timer,
        __attribute__((unused)) void *ptr_data);

int get_current_time(struct timespec *pTime);
unsigned long get_difftime_us(struct timespec *start, struct timespec *end);

#endif //ENABLE_USE_RTE_TIMER_MODE_FOR_MAIN_THREAD

#ifdef ENABLE_USE_RTE_TIMER_MODE_FOR_MAIN_THREAD
int get_current_time(struct timespec *pTime) {
        if (clock_gettime(USE_THIS_CLOCK, pTime) == -1) {
              perror("\n clock_gettime");
              return 1;
        }
        return 0;
}

unsigned long get_difftime_us(struct timespec *pStart, struct timespec *pEnd) {
        unsigned long delta = ( ((pEnd->tv_sec - pStart->tv_sec) * SECOND_TO_MICRO_SECOND) +
                     ((pEnd->tv_nsec - pStart->tv_nsec) /1000) );
        //printf("Delta [%ld], sec:[%ld]: nanosec [%ld]", delta, (pEnd->tv_sec - pStart->tv_sec), (pEnd->tv_nsec - pStart->tv_nsec));
        return delta;
}

static void
display_stats_timer_cb(__attribute__((unused)) struct rte_timer *ptr_timer,
        __attribute__((unused)) void *ptr_data) {

        static const unsigned diff_time_sec = (unsigned) (DISPLAY_STATS_PERIOD_IN_MS/1000);
        onvm_stats_display_all(diff_time_sec);
        return;
}

static void
nf_status_check_timer_cb(__attribute__((unused)) struct rte_timer *ptr_timer,
        __attribute__((unused)) void *ptr_data) {

        onvm_nf_check_status();
        return;
}

static void
nf_load_stats_timer_cb(__attribute__((unused)) struct rte_timer *ptr_timer,
        __attribute__((unused)) void *ptr_data) {

        if(nf_stat_time.in_read == 0) {
                if( get_current_time(&nf_stat_time.prev_time) == 0) {
                        nf_stat_time.in_read = 1;
                }
                return ;
        }

        if(0 == get_current_time(&nf_stat_time.cur_time)) {
                unsigned long difftime_us = get_difftime_us(&nf_stat_time.prev_time, &nf_stat_time.cur_time);
                if(difftime_us) {
                        onvm_nf_stats_update(difftime_us);
                }
                nf_stat_time.prev_time = nf_stat_time.cur_time;
        }
        return;
}

int
initialize_master_timers(void) {

        rte_timer_init(&nf_status_check_timer);
        rte_timer_init(&display_stats_timer);
        rte_timer_init(&nf_load_eval_timer);
        uint64_t ticks = 0;

        ticks = ((uint64_t)NF_STATUS_CHECK_PERIOD_IN_MS *(rte_get_timer_hz()/1000));
        rte_timer_reset_sync(&nf_status_check_timer,
                ticks,
                PERIODICAL,
                rte_lcore_id(), //timer_core
                &nf_status_check_timer_cb, NULL
                );

        ticks = ((uint64_t)DISPLAY_STATS_PERIOD_IN_MS *(rte_get_timer_hz()/1000));
        rte_timer_reset_sync(&display_stats_timer,
                ticks,
                PERIODICAL,
                rte_lcore_id(), //timer_core
                &display_stats_timer_cb, NULL
                );

        ticks = ((uint64_t)NF_LOAD_EVAL_PERIOD_IN_MS *(rte_get_timer_hz()/1000));
        rte_timer_reset_sync(&nf_load_eval_timer,
                ticks,
                PERIODICAL,
                rte_lcore_id(), //timer_core
                &nf_load_stats_timer_cb, NULL
                );
        return 0;
}
#endif //ENABLE_USE_RTE_TIMER_MODE_FOR_MAIN_THREAD
/*******************************Worker threads********************************/

/*
 * Stats thread periodically prints per-port and per-NF stats.
 */
static void
master_thread_main(void) {
        const unsigned sleeptime = 1;

        RTE_LOG(INFO, APP, "Core %d: Running master thread\n", rte_lcore_id());

        /* Longer initial pause so above printf is seen */
        sleep(sleeptime * 3);

#ifdef ENABLE_USE_RTE_TIMER_MODE_FOR_MAIN_THREAD
        if(initialize_master_timers() == 0) {
                while (usleep(USLEEP_INTERVAL_IN_US) == 0) {
                        rte_timer_manage();
                }
        } //else
#else

        /* Loop forever: sleep always returns 0 or <= param */
        while (sleep(sleeptime) <= sleeptime) {
                onvm_nf_check_status();
                onvm_stats_display_all(sleeptime);
        }
#endif //ENABLE_USE_RTE_TIMER_MODE_FOR_MAIN_THREAD
}

/*
 * Function to receive packets from the NIC
 * and distribute them to the default service
 */
static int
rx_thread_main(void *arg) {
        uint16_t i, rx_count;
        struct rte_mbuf *pkts[PACKET_READ_SIZE];
        //struct rte_mbuf *pkts[1024];
        struct thread_info *rx = (struct thread_info*)arg;

        RTE_LOG(INFO,
                APP,
                "Core %d: Running RX thread for RX queue %d\n",
                rte_lcore_id(),
                rx->queue_id);

        for (;;) {
                /* Read ports */
                for (i = 0; i < ports->num_ports; i++) {
                        rx_count = rte_eth_rx_burst(ports->id[i], rx->queue_id, \
                                        pkts, PACKET_READ_SIZE);
                        ports->rx_stats.rx[ports->id[i]] += rx_count;

                        /* Now process the NIC packets read */
                        if (likely(rx_count > 0)) {
                                // If there is no running NF, we drop all the packets of the batch.
                                if (!num_clients) {
                                        #ifdef ONVM_MGR_ACT_AS_2PORT_FWD_BRIDGE
                                        (void)onv_pkt_send_on_alt_port(rx, pkts, rx_count);
                                        #else
                                        onvm_pkt_drop_batch(pkts, rx_count);
                                        #endif //ONVM_MGR_ACT_AS_2PORT_FWD_BRIDGE

                                } else {
                                        onvm_pkt_process_rx_batch(rx, pkts, rx_count);
                                }
                        }
                }
        }

        return 0;
}

#define PACKET_READ_SIZE_TX ((uint16_t)(PACKET_READ_SIZE*4))
static int
tx_thread_main(void *arg) {
        struct client *cl;
        unsigned i, tx_count;
        struct rte_mbuf *pkts[PACKET_READ_SIZE];
        struct thread_info* tx = (struct thread_info*)arg;

        if (tx->first_cl == tx->last_cl - 1) {
                RTE_LOG(INFO,
                        APP,
                        "Core %d: Running TX thread for NF %d\n",
                        rte_lcore_id(),
                        tx->first_cl);
        } else if (tx->first_cl < tx->last_cl) {
                RTE_LOG(INFO,
                        APP,
                        "Core %d: Running TX thread for NFs %d to %d\n",
                        rte_lcore_id(),
                        tx->first_cl,
                        tx->last_cl-1);
        }

        for (;;) {
                /* Read packets from the client's tx queue and process them as needed */
                for (i = tx->first_cl; i < tx->last_cl; i++) {
                        tx_count = PACKET_READ_SIZE;
                        cl = &clients[i];
                        if (!onvm_nf_is_valid(cl))
                                continue;
                        /* try dequeuing max possible packets first, if that fails, get the
                         * most we can. Loop body should only execute once, maximum
                        while (tx_count > 0 &&
                                unlikely(rte_ring_dequeue_bulk(cl->tx_q, (void **) pkts, tx_count) != 0)) {
                                tx_count = (uint16_t)RTE_MIN(rte_ring_count(cl->tx_q),
                                                PACKET_READ_SIZE);
                        }
                        */
                        tx_count = rte_ring_dequeue_burst(cl->tx_q, (void **) pkts, tx_count);

                        /* Now process the Client packets read */
                        if (likely(tx_count > 0)) {

                                #ifdef ENABLE_NF_BACKPRESSURE
                                onvm_check_and_reset_back_pressure(pkts, tx_count, cl);
                                #endif // ENABLE_NF_BACKPRESSURE

                                onvm_pkt_process_tx_batch(tx, pkts, tx_count, cl);
                                //RTE_LOG(INFO,APP,"Core %d: processing %d TX packets for NF: %d \n", rte_lcore_id(),tx_count, i);
                        }
                        else continue;
                }

                /* Send a burst to every port */
                onvm_pkt_flush_all_ports(tx);

                /* Send a burst to every NF */
                onvm_pkt_flush_all_nfs(tx);
        }

        return 0;
}


/*******************************Main function*********************************/
//TODO: Move to apporpriate header or a different file for onvm_nf_wakeup_mgr/hdlr.c
#ifdef INTERRUPT_SEM
#include <signal.h>

unsigned nfs_wakethr[MAX_CLIENTS] = {[0 ... MAX_CLIENTS-1] = 1};

static void 
register_signal_handler(void);
static inline int
whether_wakeup_client(int instance_id);
static inline void
wakeup_client(int instance_id, struct wakeup_info *wakeup_info);
static int
wakeup_nfs(void *arg);

#endif

int
main(int argc, char *argv[]) {
        unsigned cur_lcore, rx_lcores, tx_lcores;
        unsigned clients_per_tx, temp_num_clients;
        unsigned i;

        /* initialise the system */
        #ifdef INTERRUPT_SEM
        unsigned wakeup_lcores;        
        register_signal_handler();
        #endif        

        /* Reserve ID 0 for internal manager things */
        next_instance_id = 1;
        if (init(argc, argv) < 0 )
                return -1;
        RTE_LOG(INFO, APP, "Finished Process Init.\n");

        /* clear statistics */
        onvm_stats_clear_all_clients();

        /* Reserve n cores for: 1 Stats, 1 final Tx out, and ONVM_NUM_RX_THREADS for Rx */
        cur_lcore = rte_lcore_id();
        rx_lcores = ONVM_NUM_RX_THREADS;

        #ifdef INTERRUPT_SEM
        wakeup_lcores = ONVM_NUM_WAKEUP_THREADS;
        tx_lcores = rte_lcore_count() - rx_lcores - wakeup_lcores - 1 -1;
        #else
        tx_lcores = rte_lcore_count() - rx_lcores - 1;
        #endif


        /* Offset cur_lcore to start assigning TX cores */
        cur_lcore += (rx_lcores-1);

        RTE_LOG(INFO, APP, "%d cores available in total\n", rte_lcore_count());
        RTE_LOG(INFO, APP, "%d cores available for handling manager RX queues\n", rx_lcores);
        RTE_LOG(INFO, APP, "%d cores available for handling TX queues\n", tx_lcores);
        #ifdef INTERRUPT_SEM
        RTE_LOG(INFO, APP, "%d cores available for handling wakeup\n", wakeup_lcores);        
        #endif 
        RTE_LOG(INFO, APP, "%d cores available for handling stats\n", 1);

        /* Evenly assign NFs to TX threads */

        /*
         * If num clients is zero, then we are running in dynamic NF mode.
         * We do not have a way to tell the total number of NFs running so
         * we have to calculate clients_per_tx using MAX_CLIENTS then.
         * We want to distribute the number of running NFs across available
         * TX threads
         */
        if (num_clients == 0) {
                clients_per_tx = ceil((float)MAX_CLIENTS/tx_lcores);
                temp_num_clients = (unsigned)MAX_CLIENTS;
        } else {
                clients_per_tx = ceil((float)num_clients/tx_lcores);
                temp_num_clients = (unsigned)num_clients;
        }
        //num_clients = temp_num_clients;
        for (i = 0; i < tx_lcores; i++) {
                struct thread_info *tx = calloc(1, sizeof(struct thread_info));
                tx->queue_id = i;
                tx->port_tx_buf = calloc(RTE_MAX_ETHPORTS, sizeof(struct packet_buf));
                tx->nf_rx_buf = calloc(MAX_CLIENTS, sizeof(struct packet_buf));

#ifdef ONVM_MGR_ACT_AS_2PORT_FWD_BRIDGE
                if ( i == 0) {
                        tx->first_cl = RTE_MIN(i * clients_per_tx, temp_num_clients);       //changed to read from NF[0]
                } else {
                        tx->first_cl = RTE_MIN(i * clients_per_tx + 1, temp_num_clients);
                }
#else
                tx->first_cl = RTE_MIN(i * clients_per_tx + 1, temp_num_clients);
#endif  //ONVM_MGR_ACT_AS_2PORT_FWD_BRIDGE
                tx->last_cl = RTE_MIN((i+1) * clients_per_tx + 1, temp_num_clients);
                cur_lcore = rte_get_next_lcore(cur_lcore, 1, 1);
                if (rte_eal_remote_launch(tx_thread_main, (void*)tx,  cur_lcore) == -EBUSY) {
                        RTE_LOG(ERR,
                                APP,
                                "Core %d is already busy, can't use for client %d TX\n",
                                cur_lcore,
                                tx->first_cl);
                        return -1;
                }
        }
       
        /* Launch RX thread main function for each RX queue on cores */
        for (i = 0; i < rx_lcores; i++) {
                struct thread_info *rx = calloc(1, sizeof(struct thread_info));
                rx->queue_id = i;
                rx->port_tx_buf = NULL;
                rx->nf_rx_buf = calloc(MAX_CLIENTS, sizeof(struct packet_buf));
                cur_lcore = rte_get_next_lcore(cur_lcore, 1, 1);
                if (rte_eal_remote_launch(rx_thread_main, (void *)rx, cur_lcore) == -EBUSY) {
                        RTE_LOG(ERR,
                                APP,
                                "Core %d is already busy, can't use for RX queue id %d\n",
                                cur_lcore,
                                rx->queue_id);
                        return -1;
                }
        }
        
        #ifdef INTERRUPT_SEM
        int clients_per_wakethread = ceil(temp_num_clients / wakeup_lcores);
        wakeup_infos = (struct wakeup_info *)calloc(wakeup_lcores, sizeof(struct wakeup_info));
        if (wakeup_infos == NULL) {
                printf("can not alloc space for wakeup_info\n");
                exit(1);
        }        
        for (i = 0; i < wakeup_lcores; i++) {
                wakeup_infos[i].first_client = RTE_MIN(i * clients_per_wakethread + 1, temp_num_clients);
                wakeup_infos[i].last_client = RTE_MIN((i+1) * clients_per_wakethread + 1, temp_num_clients);
                cur_lcore = rte_get_next_lcore(cur_lcore, 1, 1);
                rte_eal_remote_launch(wakeup_nfs, (void*)&wakeup_infos[i], cur_lcore);
                //printf("wakeup lcore_id=%d, first_client=%d, last_client=%d\n", cur_lcore, wakeup_infos[i].first_client, wakeup_infos[i].last_client);
                RTE_LOG(INFO, APP, "Core %d: Running wakeup thread, first_client=%d, last_client=%d\n", cur_lcore, wakeup_infos[i].first_client, wakeup_infos[i].last_client);
        }
        
        #ifdef INTERRUPT_SEM
        #ifdef ENABLE_PERFORMANCE_LOG
        cur_lcore = rte_get_next_lcore(cur_lcore, 1, 1);
        printf("performance_record_lcore=%u\n", cur_lcore);
        rte_eal_remote_launch(performance_log_thread,NULL, cur_lcore);
        #endif  // ENABLE_PERFORMANCE_LOG
        #endif  // INTERRUPT_SEM

        /* this change is Not needed anymore
        cur_lcore = rte_get_next_lcore(cur_lcore, 1, 1);
        printf("monitor_lcore=%u\n", cur_lcore);
        rte_eal_remote_launch(monitor, NULL, cur_lcore);
        */        
        #endif

        /* Master thread handles statistics and NF management */
        master_thread_main();
        
        return 0;
}

/*******************************Helper functions********************************/
#ifdef INTERRUPT_SEM
#define WAKEUP_THRESHOLD 1
static inline int
whether_wakeup_client(int instance_id)
{
        uint16_t cur_entries;
        if (clients[instance_id].rx_q == NULL) {
                return 0;
        }

        #ifdef ENABLE_NF_BACKPRESSURE
        #ifdef NF_BACKPRESSURE_APPROACH_2
        /* Block the upstream (earlier) NFs from getting scheduled, if there is NF at downstream that is bottlenecked! */
        if (downstream_nf_overflow) {
                if (clients[instance_id].info != NULL && is_upstream_NF(highest_downstream_nf_service_id,clients[instance_id].info->service_id)) {
                        throttle_count++;
                        return -1;
                }
        }
        //service chain case
        else if (clients[instance_id].throttle_this_upstream_nf) {
                clients[instance_id].throttle_count++;
                return -1;
        }
        #endif //NF_BACKPRESSURE_APPROACH_2
        #endif //ENABLE_NF_BACKPRESSURE

        cur_entries = rte_ring_count(clients[instance_id].rx_q);
        if (cur_entries >= nfs_wakethr[instance_id]) {
                return 1;
        }
        return 0;
}

static inline void 
notify_client(int instance_id)
{
        #ifdef USE_MQ
        static int msg = '\0';
        //struct timespec timeout = {.tv_sec=0, .tv_nsec=1000};
        //clock_gettime(CLOCK_REALTIME, &timeout);timeout..tv_nsec+=1000;
        //msg = (unsigned int)mq_timedsend(clients[instance_id].mutex, (const char*) &msg, sizeof(msg),(unsigned int)prio, &timeout);
        //msg = (unsigned int)mq_send(clients[instance_id].mutex, (const char*) &msg, sizeof(msg),(unsigned int)prio);
        msg = mq_send(clients[instance_id].mutex, (const char*) &msg,0,0);
        if (0 > msg) { perror ("mq_send failed!");}
        #endif
        
        #ifdef USE_FIFO
        unsigned msg = 1;
        msg = write(clients[instance_id].mutex, (void*) &msg, sizeof(msg));
        #endif
        

        #ifdef USE_SIGNAL
        //static int count = 0;
        //if (count < 100) { count++;
        int sts = sigqueue(clients[instance_id].info->pid, SIGUSR1, (const union sigval)0);        
        if (sts) perror ("sigqueue failed!!");        
        //}
        #endif

        #ifdef USE_SEMAPHORE 
        sem_post(clients[instance_id].mutex);
        #endif 

        #ifdef USE_SCHED_YIELD
        rte_atomic16_read(clients[instance_id].shm_server);
        #endif

        #ifdef USE_NANO_SLEEP
        rte_atomic16_read(clients[instance_id].shm_server);
        #endif

        #ifdef USE_SOCKET
        static char msg[2] = "\0";
        sendto(onvm_socket_id, msg, sizeof(msg), 0, (struct sockaddr *) &clients[instance_id].mutex, (socklen_t) sizeof(struct sockaddr_un));
        #endif

        #ifdef USE_FLOCK
        if (0 > (flock(clients[instance_id].mutex, LOCK_UN|LOCK_NB))) { perror ("FILE UnLock Failed!!");}
        #endif

        #ifdef USE_MQ2
        static unsigned long msg = 1;
        //static msgbuf_t msg = {.mtype = 1, .mtext[0]='\0'};
        //if (0 > msgsnd(clients[instance_id].mutex, (const void*) &msg, sizeof(msg.mtext), IPC_NOWAIT)) {
        if (0 > msgsnd(clients[instance_id].mutex, (const void*) &msg, 0, IPC_NOWAIT)) {
                perror ("Msgsnd Failed!!");
        }
        #endif

        #ifdef USE_ZMQ
        static char msg[2] = "\0";
        zmq_connect (onvm_socket_id,get_sem_name(instance_id));
        zmq_send (onvm_socket_id, msg, sizeof(msg), 0);
        #endif

        #ifdef USE_POLL_MODE
        rte_atomic16_read(clients[instance_id].shm_server);
        #endif
}
#ifdef SORT_EFFICEINCY_TET
static int rdata[MAX_CLIENTS];
void quickSort( int a[], int l, int r);
int partition( int a[], int l, int r);
void quickSort( int a[], int l, int r)
{
   int j;

   if( l < r )
   {
    // divide and conquer
        j = partition( a, l, r);
       quickSort( a, l, j-1);
       quickSort( a, j+1, r);
   }

}

int partition( int a[], int l, int r) {
   int pivot, i, j, t;
   pivot = a[l];
   i = l; j = r+1;

   while( 1)
   {
    do ++i; while( a[i] <= pivot && i <= r );
    do --j; while( a[j] > pivot );
    if( i >= j ) break;
    t = a[i]; a[i] = a[j]; a[j] = t;
   }
   t = a[l]; a[l] = a[j]; a[j] = t;
   return j;
}
void populate_and_sort_rdata(void);
void populate_and_sort_rdata(void) {
        unsigned i = 0;
        for (i=0; i< MAX_CLIENTS; i++) {
                uint16_t demand = rte_ring_count(clients[i].rx_q);
                uint16_t offload = rte_ring_count(clients[i].tx_q);
                uint16_t ccost   = clients[i].info->comp_cost;
                uint32_t prio = demand*ccost - offload;
                rdata[i] = prio;
        }
        quickSort(rdata, 0, MAX_CLIENTS-1);
}
#endif

static inline void
wakeup_client(int instance_id, struct wakeup_info *wakeup_info)  {
        int wkup_sts = whether_wakeup_client(instance_id);
        if ( wkup_sts == 1) {
                if (rte_atomic16_read(clients[instance_id].shm_server) ==1) {
                        wakeup_info->num_wakeups += 1;
                        //if(wakeup_info->num_wakeups) {}//populate_and_sort_rdata();}
                        clients[instance_id].stats.wakeup_count+=1;
                        rte_atomic16_set(clients[instance_id].shm_server, 0);
                        notify_client(instance_id);
                }
        }
        #ifdef ENABLE_NF_BACKPRESSURE
        #ifdef NF_BACKPRESSURE_APPROACH_2
        else if (-1 == wkup_sts) {
                /* Make sure to set the flag here and check for flag in nf_lib and block */
                rte_atomic16_set(clients[instance_id].shm_server, 1);
        }
        #endif //NF_BACKPRESSURE_APPROACH_2
        #endif //ENABLE_NF_BACKPRESSURE
}

static int
wakeup_nfs(void *arg) {
        struct wakeup_info *wakeup_info = (struct wakeup_info *)arg;
        unsigned i;

        /*
        if (wakeup_info->first_client == 1) {
                wakeup_info->first_client += ONVM_SPECIAL_NF;
        }
        */

        while (true) {
                //wakeup_info->num_wakeups += 1;    //count for debug
                for (i = wakeup_info->first_client; i < wakeup_info->last_client; i++) {
                        wakeup_client(i, wakeup_info);
                }
                //usleep(100);
        }

        return 0;
}

static void signal_handler(int sig, siginfo_t *info, void *secret) {
        int i;
        (void)info;
        (void)secret;
 
        //2 means terminal interrupt, 3 means terminal quit, 9 means kill and 15 means termination
        if (sig <= 15) {
                for (i = 1; i < MAX_CLIENTS; i++) {
                        
                        #ifdef USE_MQ
                        mq_close(clients[i].mutex);
                        mq_unlink(clients[i].sem_name);
                        #endif                      

                        #ifdef USE_FIFO
                        close(clients[i].mutex);
                        unlink(clients[i].sem_name);  
                        #endif

                        #ifdef USE_SIGNAL
                        #endif
                        
                        #ifdef USE_SOCKET
                        #endif             
                        
                        #ifdef USE_SEMAPHORE
                        sem_close(clients[i].mutex);
                        sem_unlink(clients[i].sem_name);
                        #endif

                        #ifdef USE_FLOCK
                        flock(clients[i].mutex, LOCK_UN|LOCK_NB);
                        close(clients[i].mutex);
                        #endif

                        #ifdef USE_MQ2
                        msgctl(clients[i].mutex, IPC_RMID, 0);
                        #endif

                        #ifdef USE_ZMQ
                        zmq_close(onvm_socket_id);
                        zmq_ctx_destroy(onvm_socket_ctx);
                        #endif

                }        
                #ifdef MONITOR
//                rte_free(port_stats);
//                rte_free(port_prev_stats);
                #endif
        }
        printf("Got Signal [%d]\n", sig);
        if(info) {
                printf("[signo: %d,errno: %d,code: %d]\n", info->si_signo, info->si_errno, info->si_code);
        }
        exit(10);
}
static void 
register_signal_handler(void) {
        unsigned i;
        struct sigaction act;
        memset(&act, 0, sizeof(act));        
        sigemptyset(&act.sa_mask);
        act.sa_flags = SA_SIGINFO;
        act.sa_handler = (void *)signal_handler;

        for (i = 1; i < 31; i++) {
                sigaction(i, &act, 0);
        }
}
#endif


#ifdef ONVM_MGR_ACT_AS_2PORT_FWD_BRIDGE
int send_direct_on_alt_port(struct rte_mbuf *pkts[], uint16_t rx_count);
int send_direct_on_alt_port(struct rte_mbuf *pkts[], uint16_t rx_count) {
        uint16_t i, sent_0,sent_1;
        volatile struct tx_stats *tx_stats;
        tx_stats = &(ports->tx_stats);

        struct rte_mbuf *pkts_0[PACKET_READ_SIZE];
        struct rte_mbuf *pkts_1[PACKET_READ_SIZE];
        uint16_t count_0=0, count_1=0;

        for (i = 0; i < rx_count; i++) {
                if (pkts[i]->port == 0) {
                        pkts_1[count_1++] = pkts[i];
                } else {
                        pkts_0[count_0++] = pkts[i];
                }
        }
#ifdef DELAY_BEFORE_SEND
        usleep(DELAY_PER_PKT*count_0);
#endif
        if(count_0) {
                sent_0 = rte_eth_tx_burst(0,
                                        0,//tx->queue_id,
                                        pkts_0,
                                        count_0);
                if (unlikely(sent_0 < count_0)) {
                        for (i = sent_0; i < count_0; i++) {
                                onvm_pkt_drop(pkts_0[i]);
                        }
                        tx_stats->tx_drop[0] += (count_0 - sent_0);
                }
                tx_stats->tx[0] += sent_0;
        }
#ifdef DELAY_BEFORE_SEND
        usleep(DELAY_PER_PKT*count_1);
#endif
        if(count_1) {
                sent_1 = rte_eth_tx_burst(1,
                                        0,//tx->queue_id,
                                        pkts_1,
                                        count_1);
                if (unlikely(sent_1 < count_1)) {
                        for (i = sent_1; i < count_1; i++) {
                                onvm_pkt_drop(pkts_1[i]);
                        }
                        tx_stats->tx_drop[1] += (count_1 - sent_1);
                }
                tx_stats->tx[1] += sent_1;
        }
        return 0;
}
static int onv_pkt_send_on_alt_port(struct thread_info *rx, struct rte_mbuf *pkts[], uint16_t rx_count) {

        int ret = 0;
        int i = 0;
        struct onvm_pkt_meta *meta = NULL;
        struct rte_mbuf *pkt = NULL;
        static struct client *cl = NULL; //&clients[0];

        if (rx == NULL || pkts == NULL || rx_count== 0)
                return ret;

#ifdef SEND_DIRECT_ON_ALT_PORT
        return send_direct_on_alt_port(pkts, rx_count);
#endif //SEND_DIRECT_ON_ALT_PORT

        for (i = 0; i < rx_count; i++) {
               meta = (struct onvm_pkt_meta*) &(((struct rte_mbuf*)pkts[i])->udata64);
               meta->src = 0;
               meta->chain_index = 0;
               pkt = (struct rte_mbuf*)pkts[i];
                if (pkt->port == 0) {
                        meta->destination = 1;
                }
                else {
                        meta->destination = 0;
                }
                meta->action = ONVM_NF_ACTION_OUT;
        }

        //Make use of the internal NF[0]
        cl = &clients[0];

        // DO ONCE: Ensure destination NF is running and ready to receive packets
        if (!onvm_nf_is_valid(cl)) {
                void *mempool_data = NULL;
                struct onvm_nf_info *info = NULL;
                struct rte_mempool *nf_info_mp = NULL;
                nf_info_mp = rte_mempool_lookup(_NF_MEMPOOL_NAME);
                if (nf_info_mp == NULL) {
                        printf("Failed to get NF_MEMPOOL");
                        return ret;
                }
                if (rte_mempool_get(nf_info_mp, &mempool_data) < 0) {
                        printf("Failed to get client info memory");
                        return ret;
                }
                if (mempool_data == NULL) {
                        printf("Client Info struct not allocated");
                        return ret;
                }

                info = (struct onvm_nf_info*) mempool_data;
                info->instance_id = 0;
                info->service_id = 0;
                info->status = NF_RUNNING;
                info->tag = "INTERNAL_BRIDGE";

                cl->info=info;
        }
        //return ret;

        //Push all packets directly to the NF[0]->tx_ring
        int enq_status = rte_ring_enqueue_bulk(cl->tx_q, (void **)pkts, rx_count);
        if (enq_status) {
                //printf("Enqueue to NF[0] Tx Buffer failed!!");
                onvm_pkt_drop_batch(pkts,rx_count);
                cl->stats.rx_drop += rx_count;
        }
        return ret;
}
#endif //ONVM_MGR_ACT_AS_2PORT_FWD_BRIDGE


