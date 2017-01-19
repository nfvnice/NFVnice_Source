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
                          onvm_stats.c

   This file contain the implementation of all functions related to
   statistics display in the manager.

******************************************************************************/


#include "onvm_mgr.h"
#include "onvm_stats.h"
#include "onvm_nf.h"


/****************************Interfaces***************************************/

void
onvm_stats_display_all(unsigned difftime) {
        onvm_stats_clear_terminal();
        onvm_stats_display_ports(difftime);
        onvm_stats_display_clients(difftime);
        onvm_stats_display_chains(difftime);
}


void
onvm_stats_clear_all_clients(void) {
        unsigned i;

        for (i = 0; i < MAX_CLIENTS; i++) {
                clients[i].stats.rx = clients[i].stats.rx_drop = 0;
                clients[i].stats.act_drop = clients[i].stats.act_tonf = 0;
                clients[i].stats.act_next = clients[i].stats.act_out = 0;
        }
}

void
onvm_stats_clear_client(uint16_t id) {
        clients[id].stats.rx = clients[id].stats.rx_drop = 0;
        clients[id].stats.act_drop = clients[id].stats.act_tonf = 0;
        clients[id].stats.act_next = clients[id].stats.act_out = 0;
}


/****************************Internal functions*******************************/
#define USE_EXTENDED_PORT_STATS
#ifdef USE_EXTENDED_PORT_STATS
static int 
get_port_stats_rate(double period_time)
{
        int i;
        uint64_t port_rx_rate=0, port_rx_err_rate=0, port_imissed_rate=0, \
                port_rx_nombuf_rate=0, port_tx_rate=0, port_tx_err_rate=0;

        struct rte_eth_stats port_stats[ports->num_ports];
        static struct rte_eth_stats port_prev_stats[5];
        for (i = 0; i < ports->num_ports; i++){
                rte_eth_stats_get(ports->id[i], &port_stats[i]);
                printf("Port:%"PRIu8", rx:%"PRIu64", rx_err:%"PRIu64", rx_imissed:%"PRIu64" "
                //        "ibadcrc:%"PRIu64", ibadlen:%"PRIu64", illerrc:%"PRIu64", errbc:%"PRIu64" "
                        "rx_nombuf:%"PRIu64", tx:%"PRIu64", tx_err:%"PRIu64"\n",
                        ports->id[i], port_stats[i].ipackets, port_stats[i].ierrors, port_stats[i].imissed,
                        //port_stats[i].ibadcrc, port_stats[i].ibadlen, port_stats[i].illerrc, port_stats[i].errbc,
                        port_stats[i].rx_nombuf, port_stats[i].opackets,
                        port_stats[i].oerrors);

                port_rx_rate = (port_stats[i].ipackets - port_prev_stats[i].ipackets) / period_time;
                port_rx_err_rate = (port_stats[i].ierrors - port_prev_stats[i].ierrors) / period_time;
                port_imissed_rate = (port_stats[i].imissed - port_prev_stats[i].imissed) / period_time;
                port_rx_nombuf_rate = (port_stats[i].rx_nombuf - port_prev_stats[i].rx_nombuf) / period_time;
                port_tx_rate = (port_stats[i].opackets - port_prev_stats[i].opackets) / period_time;
                port_tx_err_rate = (port_stats[i].oerrors - port_prev_stats[i].oerrors) / period_time;

                printf("Port:%"PRIu8", rx_rate:%"PRIu64", rx_err_rate:%"PRIu64", rx_imissed_rate:%"PRIu64" "
                        "rx_nombuf_rate:%"PRIu64", tx_rate:%"PRIu64", tx_err_rate:%"PRIu64"\n",
                        ports->id[i], port_rx_rate, port_rx_err_rate, port_imissed_rate,
                        port_rx_nombuf_rate, port_tx_rate, port_tx_err_rate);

                port_prev_stats[i].ipackets = port_stats[i].ipackets;
                port_prev_stats[i].ierrors = port_stats[i].ierrors;
                port_prev_stats[i].imissed = port_stats[i].imissed;
                port_prev_stats[i].rx_nombuf = port_stats[i].rx_nombuf;
                port_prev_stats[i].opackets = port_stats[i].opackets;
                port_prev_stats[i].oerrors = port_stats[i].oerrors;
        }

        return 0;
}
#endif

void
onvm_stats_display_ports(unsigned difftime) {
        unsigned i;

        printf("PORTS\n");
        printf("-----\n");
        for (i = 0; i < ports->num_ports; i++)
                printf("Port %u: '%s'\t", (unsigned)ports->id[i],
                                onvm_stats_print_MAC(ports->id[i]));
        printf("\n\n");

        //#ifndef USE_EXTENDED_PORT_STATS
        /* Arrays to store last TX/RX count to calculate rate */
        static uint64_t tx_last[RTE_MAX_ETHPORTS];
        static uint64_t tx_drop_last[RTE_MAX_ETHPORTS];
        static uint64_t rx_last[RTE_MAX_ETHPORTS];

        for (i = 0; i < ports->num_ports; i++) {
                printf("Port %u - rx: %9"PRIu64"  (%9"PRIu64" pps)\t"
                                "tx: %9"PRIu64"  (%9"PRIu64" pps) \t"
                                "tx_drop: %9"PRIu64"  (%9"PRIu64" pps)\n",
                                (unsigned)ports->id[i],
                                ports->rx_stats.rx[ports->id[i]],
                                (ports->rx_stats.rx[ports->id[i]] - rx_last[i])
                                        /difftime,
                                        ports->tx_stats.tx[ports->id[i]],
                                (ports->tx_stats.tx[ports->id[i]] - tx_last[i])
                                        /difftime,
                                        ports->tx_stats.tx_drop[ports->id[i]],
                                (ports->tx_stats.tx_drop[ports->id[i]] - tx_drop_last[i])
                                        /difftime
                                );

                rx_last[i] = ports->rx_stats.rx[ports->id[i]];
                tx_last[i] = ports->tx_stats.tx[ports->id[i]];
                tx_drop_last[i] = ports->tx_stats.tx_drop[ports->id[i]];
        }
        //#else
        get_port_stats_rate(difftime);
        //#endif
}


int get_onvm_nf_stats_snapshot_v2(unsigned nf_index, onvm_stats_snapshot_t *snapshot, unsigned difftime) {

#ifdef INTERRUPT_SEM
        static stats_cycle_info_t  interval_cycels[MAX_CLIENTS];
        static onvm_stats_snapshot_t last_snapshot[MAX_CLIENTS];

        //input has time interval defined, then just copy the items stored in last_snapshot
        if(difftime) {
                if(interval_cycels[nf_index].cur_cycles) {
                        *snapshot = last_snapshot[nf_index];
                        return 0;
                }
                return 1;
        }
        if(interval_cycels[nf_index].in_read == 0) {
                interval_cycels[nf_index].prev_cycles = get_current_cpu_cycles();
                interval_cycels[nf_index].in_read = 1;
                //return 1;
        }
        else {
                interval_cycels[nf_index].cur_cycles = get_current_cpu_cycles();
                unsigned long difftime_us = get_diff_cpu_cycles_in_us(interval_cycels[nf_index].prev_cycles, interval_cycels[nf_index].cur_cycles);
                if(difftime_us) {
                        difftime = difftime_us; //MICRO_SECOND_TO_SECOND(difftime_us);
                        interval_cycels[nf_index].prev_cycles = interval_cycels[nf_index].cur_cycles;
                }
        }
        snapshot->rx_delta = (clients[nf_index].stats.rx - clients[nf_index].stats.prev_rx);
        snapshot->rx_drop_delta = (clients[nf_index].stats.rx_drop - clients[nf_index].stats.prev_rx_drop);
        snapshot->tx_delta = (clients_stats->tx[nf_index] - clients_stats->prev_tx[nf_index]);
        snapshot->tx_drop_delta = (clients_stats->tx_drop[nf_index] - clients_stats->prev_tx_drop[nf_index]);


        if(difftime) {
                clients[nf_index].stats.prev_rx = clients[nf_index].stats.rx;
                clients[nf_index].stats.prev_rx_drop = clients[nf_index].stats.rx_drop;
                clients_stats->prev_tx[nf_index] = clients_stats->tx[nf_index];
                clients_stats->prev_tx_drop[nf_index] = clients_stats->tx_drop[nf_index];

                snapshot->rx_rate       = (snapshot->rx_delta*SECOND_TO_MICRO_SECOND)/difftime;
                snapshot->serv_rate     = (snapshot->tx_delta*SECOND_TO_MICRO_SECOND)/difftime;
                snapshot->arrival_rate  = ((snapshot->rx_delta + snapshot->rx_drop_delta)*SECOND_TO_MICRO_SECOND)/difftime;
                snapshot->tx_rate       = ((snapshot->tx_delta + snapshot->tx_drop_delta)*SECOND_TO_MICRO_SECOND)/difftime;
                snapshot->rx_drop_rate  = (snapshot->rx_drop_delta*SECOND_TO_MICRO_SECOND)/difftime;
                snapshot->tx_drop_rate  = (snapshot->tx_drop_delta*SECOND_TO_MICRO_SECOND)/difftime;

                last_snapshot[nf_index] = *snapshot;
        }
#else
        (void)difftime;
        snapshot->rx_delta = (clients[nf_index].stats.rx);
        snapshot->rx_drop_delta = (clients[nf_index].stats.rx_drop);
        snapshot->tx_delta = (clients_stats->tx[nf_index]);
        snapshot->tx_drop_delta = (clients_stats->tx_drop[nf_index]);
#endif
        return 0;
}

int get_onvm_nf_stats_snapshot(unsigned nf_index, onvm_stats_snapshot_t *snapshot, unsigned difftime) {

#ifdef INTERRUPT_SEM
        static nf_stats_time_info_t nf_stat_time;


        snapshot->rx_delta = (clients[nf_index].stats.rx - clients[nf_index].stats.prev_rx);
        snapshot->rx_drop_delta = (clients[nf_index].stats.rx_drop - clients[nf_index].stats.prev_rx_drop);
        snapshot->tx_delta = (clients_stats->tx[nf_index] - clients_stats->prev_tx[nf_index]);
        snapshot->tx_drop_delta = (clients_stats->tx_drop[nf_index] - clients_stats->prev_tx_drop[nf_index]);


        if(difftime) {
                if(nf_stat_time.in_read == 0) {
                        if(get_current_time(&nf_stat_time.prev_time) == 0) {
                                nf_stat_time.in_read = 1;
                        }
                        //difftime=0;
                }
                else if(0 == get_current_time(&nf_stat_time.cur_time)) {
                        unsigned long difftime_us = get_difftime_us(&nf_stat_time.prev_time, &nf_stat_time.cur_time);
                        if(difftime && difftime_us) {
                                difftime = difftime_us; //MICRO_SECOND_TO_SECOND(difftime_us);
                                nf_stat_time.prev_time = nf_stat_time.cur_time;
                        }
                }
                clients[nf_index].stats.prev_rx = clients[nf_index].stats.rx;
                clients[nf_index].stats.prev_rx_drop = clients[nf_index].stats.rx_drop;
                clients_stats->prev_tx[nf_index] = clients_stats->tx[nf_index];
                clients_stats->prev_tx_drop[nf_index] = clients_stats->tx_drop[nf_index];

                snapshot->rx_rate       = (snapshot->rx_delta*SECOND_TO_MICRO_SECOND)/difftime;
                snapshot->serv_rate     = (snapshot->tx_delta*SECOND_TO_MICRO_SECOND)/difftime;
                snapshot->arrival_rate  = ((snapshot->rx_delta + snapshot->rx_drop_delta)*SECOND_TO_MICRO_SECOND)/difftime;
                snapshot->tx_rate       = ((snapshot->tx_delta + snapshot->tx_drop_delta)*SECOND_TO_MICRO_SECOND)/difftime;
                snapshot->rx_drop_rate  = (snapshot->rx_drop_delta*SECOND_TO_MICRO_SECOND)/difftime;
                snapshot->tx_drop_rate  = (snapshot->tx_drop_delta*SECOND_TO_MICRO_SECOND)/difftime;
        }
#else
        (void)difftime;
        snapshot->rx_delta = (clients[nf_index].stats.rx);
        snapshot->rx_drop_delta = (clients[nf_index].stats.rx_drop);
        snapshot->tx_delta = (clients_stats->tx[nf_index]);
        snapshot->tx_drop_delta = (clients_stats->tx_drop[nf_index]);
#endif
        return 0;
}

void
onvm_stats_display_clients(unsigned difftime) {
        unsigned i;

        #ifdef INTERRUPT_SEM
        uint64_t rx_qlen;
        uint64_t tx_qlen;
        uint64_t comp_cost;
        uint64_t num_wakeups = 0;
        uint64_t prev_num_wakeups = 0;
        uint64_t wakeup_rate = 0;
        uint64_t avg_pkts_per_wakeup = 0;
        uint64_t good_pkts_per_wakeup = 0;
        uint64_t yield_rate = 0;

        /* unsigned sleep_time = 1; // This info is not availble anymore: 
                // must move entire wakeup to separate new function  onvm_stats_display_client_wakeup_info(difftime)
        */
        printf("INTERRUPT_SEM MODE!");

        #if defined (USE_CGROUPS_PER_NF_INSTANCE)
        printf(" CGROUP_ENABLED:");
                #if defined (ENABLE_DYNAMIC_CGROUP_WEIGHT_ADJUSTMENT)
                printf(" With Dynamic Weight ");
                #endif
                #if defined (USE_DYNAMIC_LOAD_FACTOR_FOR_CPU_SHARE)
                printf(" and With Dynamic Load ");
                #endif
        #else
        printf(" CGROUP DISABLED!");
        #endif

        #if defined(ENABLE_NF_BACKPRESSURE)
        printf(" BACKPRESURE ON: ");
                #if defined (NF_BACKPRESSURE_APPROACH_1)
                printf("APPROACH #1");
                #endif
                #if defined (NF_BACKPRESSURE_APPROACH_2)
                printf("APPROACH #2");
                #endif

        #else
        printf(" BACKPRESSURE OFF!");
        #endif

        #if defined (ENABLE_ECN_CE)
        printf(" ECN MARKING ON!");
        #else
        printf(" ECN MARKING OFF!");
        #endif

        #if defined (STORE_HISTOGRAM_OF_NF_COMPUTATION_COST)
        printf(" HISTOGRAM ON!");
        #else
        printf(" HISTOGRAM OFF!");
        #endif

        #else
        printf ("POLL MODE! diff_time=0x%4x", difftime);
        #endif

        #ifdef INTERRUPT_SEM
        for (i = 0; ((int)i < ONVM_NUM_WAKEUP_THREADS); i++) {
                //avg_wakeups = (wakeup_infos[i].num_wakeups-wakeup_infos[i].prev_num_wakeups);
                num_wakeups += wakeup_infos[i].num_wakeups;
                prev_num_wakeups += wakeup_infos[i].prev_num_wakeups;
                wakeup_infos[i].prev_num_wakeups = wakeup_infos[i].num_wakeups;
                //if(avg_wakeups)printf("instance_id=%d, wakeup_rate=%"PRIu64"\n", i, avg_wakeups);
        }

        wakeup_rate = (num_wakeups - prev_num_wakeups) / difftime; 
        printf("num_wakeups=%"PRIu64", wakeup_rate=%"PRIu64"\n", num_wakeups, wakeup_rate);
        #endif

#ifdef ENABLE_NF_BACKPRESSURE
        printf("BkprMode=[%d], OverflowFlag=[%d], Highest_DS_SID=[%d], Lowest_DS_SID=[%d], Num Throttles=[%"PRIu64"] \n", global_bkpr_mode, downstream_nf_overflow, highest_downstream_nf_service_id, lowest_upstream_to_throttle, throttle_count);
#endif  //ENABLE_NF_BACKPRESSURE

        printf("\nCLIENTS\n");
        printf("-------\n");
        for (i = 0; i < MAX_CLIENTS; i++) {
                if (!onvm_nf_is_valid(&clients[i]))
                        continue;

                const uint64_t rx_drop = clients[i].stats.rx_drop;
                const uint64_t tx_drop = clients_stats->tx_drop[i];


                #ifndef INTERRUPT_SEM
                const uint64_t rx = clients[i].stats.rx;
                const uint64_t tx = clients_stats->tx[i];
                const uint64_t act_drop = clients[i].stats.act_drop;
                const uint64_t act_next = clients[i].stats.act_next;
                const uint64_t act_out = clients[i].stats.act_out;
                const uint64_t act_tonf = clients[i].stats.act_tonf;
                const uint64_t act_buffer = clients_stats->tx_buffer[i];
                const uint64_t act_returned = clients_stats->tx_returned[i];


                printf("Client %2u - rx: %9"PRIu64" rx_drop: %9"PRIu64" next: %9"PRIu64" drop: %9"PRIu64" ret: %9"PRIu64"\n"
                                    "tx: %9"PRIu64" tx_drop: %9"PRIu64" out:  %9"PRIu64" tonf: %9"PRIu64" buf: %9"PRIu64"\n",
                                clients[i].info->instance_id,
                                rx, rx_drop, act_next, act_drop, act_returned,
                                tx, tx_drop, act_out, act_tonf, act_buffer);
        
                #endif

                #ifdef INTERRUPT_SEM
                /* periodic/rate specific statistics of NF instance */
                static onvm_stats_snapshot_t st = {0,};
                //get_onvm_nf_stats_snapshot(i, &st, difftime*SECOND_TO_MICRO_SECOND);
#ifndef USE_CGROUPS_PER_NF_INSTANCE
                get_onvm_nf_stats_snapshot_v2(i, &st, 0);
#else
                if(get_onvm_nf_stats_snapshot_v2(i, &st, difftime*SECOND_TO_MICRO_SECOND)) continue;
#endif

                const uint64_t avg_wakeups = ( clients[i].stats.wakeup_count -  clients[i].stats.prev_wakeup_count);
                const uint64_t yields =  (clients_stats->wkup_count[i] - clients_stats->prev_wkup_count[i]);

                rx_qlen = rte_ring_count(clients[i].rx_q);
                tx_qlen = rte_ring_count(clients[i].tx_q);
                comp_cost = clients_stats->comp_cost[i];

                if (avg_wakeups > 0 ) {
                        avg_pkts_per_wakeup = (st.tx_rate)/avg_wakeups;
                        good_pkts_per_wakeup = (st.serv_rate)/avg_wakeups;
                        if(yields) yield_rate = (st.serv_rate)/yields;
                }
                clients[i].stats.prev_wakeup_count = clients[i].stats.wakeup_count;
                clients_stats->prev_wkup_count[i] = clients_stats->wkup_count[i];

                printf("Client %2u:[%d, %d],  comp_cost=%"PRIu64", avg_wakeups=%"PRIu64", yields=%"PRIu64", msg_flag(blocked)=%d, \n"
                "avg_ppw=%"PRIu64", avg_good_ppw=%"PRIu64",  pkts_per_yield=%"PRIu64"\n"
                "rx_rate=%"PRIu64", rx_drop=%"PRIu64", rx_drop_rate=%"PRIu64", rx_qlen=%"PRIu64"\n"
                "tx_rate=%"PRIu64", tx_drop=%"PRIu64", tx_drop_rate=%"PRIu64", tx_qlen=%"PRIu64"\n",
                clients[i].info->instance_id, i, clients[i].info->service_id, comp_cost, avg_wakeups, yields, rte_atomic16_read(clients[i].shm_server),
                avg_pkts_per_wakeup, good_pkts_per_wakeup, yield_rate,
                (uint64_t)st.rx_rate, rx_drop, (uint64_t)st.rx_drop_rate, rx_qlen, (uint64_t)st.serv_rate, tx_drop, (uint64_t)st.tx_drop_rate, tx_qlen);

                #endif

        #ifdef ENABLE_NF_BACKPRESSURE
                //printf("rx_overflow=[%d], ThrottleNF_Flag=[%d], Highest_DS_SID=[%d] NF_Throttle_count=[%"PRIu64"], \n", clients[i].rx_buffer_overflow, clients[i].throttle_this_upstream_nf, clients[i].highest_downstream_nf_index_id, clients[i].throttle_count);
                //#ifdef NF_BACKPRESSURE_APPROACH_1
                #if defined (NF_BACKPRESSURE_APPROACH_1) && defined (BACKPRESSURE_EXTRA_DEBUG_LOGS)
                printf(" bottlenec_status=[%d], bottleneck_flows=[%d], bkpr_count [%d], max_rx_q_len=[%d], max_tx_q_len=[%d], bkpr_drop=%"PRIu64", bkpr_drop_rate=%"PRIu64"\n",clients[i].is_bottleneck, clients[i].bft_list.bft_count, clients[i].stats.bkpr_count,clients[i].stats.max_rx_q_len, clients[i].stats.max_tx_q_len, clients[i].stats.bkpr_drop,(clients[i].stats.bkpr_drop - clients[i].stats.prev_bkpr_drop)/ difftime);
                clients[i].stats.prev_bkpr_drop = clients[i].stats.bkpr_drop;
                clients[i].stats.max_rx_q_len=0;
                clients[i].stats.max_tx_q_len=0;
                //clients[i].stats.bkpr_count=0;
                #endif //NF_BACKPRESSURE_APPROACH_1

                #ifdef NF_BACKPRESSURE_APPROACH_2
                printf("ThrottleNF_Flag=[%d], NF_Throttle_count=[%"PRIu64"], \n", clients[i].throttle_this_upstream_nf, clients[i].throttle_count);
                #endif //NF_BACKPRESSURE_APPROACH_2
        #endif  //ENABLE_NF_BACKPRESSURE

        #ifdef USE_CGROUPS_PER_NF_INSTANCE
                //printf("Pid:[%d], CoreId:[%d], cpu_share:[%d], compcost:[%d], load:[%d,%d], svc_rate:[%d,%d] prio:[%d,%d,%d] \n", clients[i].info->pid, clients[i].info->core_id, clients[i].info->cpu_share, clients[i].info->comp_cost, clients[i].info->load, clients[i].info->avg_load, clients[i].info->svc_rate, clients[i].info->avg_svc, sched_getscheduler(clients[i].info->pid), getpriority(PRIO_PROCESS, clients[i].info->pid), nice(0));
                //printf("Pid:[%d], CoreId:[%d], cpu_share:[%d], compcost:[%d], load:[%d,%d], svc_rate:[%d,%d] prio:[%d,%d,%d] bkpr:[%d,%d,%d]\n", clients[i].info->pid, clients[i].info->core_id, clients[i].info->cpu_share, clients[i].info->comp_cost, clients[i].info->load, clients[i].info->avg_load, clients[i].info->svc_rate, clients[i].info->avg_svc, sched_getscheduler(clients[i].info->pid), getpriority(PRIO_PROCESS, clients[i].info->pid), nice(0), bottleneck_nf_list.nf[clients[i].instance_id].enqueue_status, bottleneck_nf_list.nf[clients[i].instance_id].enqueued_ctr, bottleneck_nf_list.nf[clients[i].instance_id].marked_ctr);
                printf("Pid:[%d], CoreId:[%d], cpu_share:[%d], compcost:[%d], load:[%d,%d], svc_rate:[%d,%d] prio:[%zu,%d,%d] ", clients[i].info->pid, clients[i].info->core_id, clients[i].info->cpu_share, clients[i].info->comp_cost, clients[i].info->load, clients[i].info->avg_load, clients[i].info->svc_rate, clients[i].info->avg_svc, clients[i].info->exec_period, getpriority(PRIO_PROCESS, clients[i].info->pid), nice(0));
                #ifdef ENABLE_NF_BACKPRESSURE
                printf("bkpr:[%d,%d,%d]", bottleneck_nf_list.nf[clients[i].instance_id].enqueue_status, bottleneck_nf_list.nf[clients[i].instance_id].enqueued_ctr, bottleneck_nf_list.nf[clients[i].instance_id].marked_ctr);
                #endif

                #ifdef STORE_HISTOGRAM_OF_NF_COMPUTATION_COST
                printf("\n Histogram: TtlCnt[%d], Min:[%d], Max:[%d], RAvg:[%d] Median:[%d] PT25:[%d], PT99:[%d] \n", clients[i].info->ht2.histogram.total_count, clients[i].info->ht2.min_val, clients[i].info->ht2.max_val, clients[i].info->ht2.running_avg, clients[i].info->ht2.median_val, hist_percentile(&clients[i].info->ht2.histogram, VAL_TYPE_25_PERCENTILE),hist_percentile(&clients[i].info->ht2.histogram, VAL_TYPE_99_PERCENTILE) );
                #endif
        #endif //USE_CGROUPS_PER_NF_INSTANCE
                printf("\n");

        }

        printf("\n");
}

void onvm_stats_display_chains(unsigned difftime) {
        printf("\nCHAINS\n");
        printf("-------\n");
        (void)difftime;
        onvm_flow_dir_print_stats();
}
/***************************Helper functions**********************************/


void
onvm_stats_clear_terminal(void) {
        const char clr[] = { 27, '[', '2', 'J', '\0' };
        const char topLeft[] = { 27, '[', '1', ';', '1', 'H', '\0' };

        printf("%s%s", clr, topLeft);
}


const char *
onvm_stats_print_MAC(uint8_t port) {
        static const char err_address[] = "00:00:00:00:00:00";
        static char addresses[RTE_MAX_ETHPORTS][sizeof(err_address)];

        if (unlikely(port >= RTE_MAX_ETHPORTS))
                return err_address;
        if (unlikely(addresses[port][0] == '\0')) {
                struct ether_addr mac;
                rte_eth_macaddr_get(port, &mac);
                snprintf(addresses[port], sizeof(addresses[port]),
                                "%02x:%02x:%02x:%02x:%02x:%02x\n",
                                mac.addr_bytes[0], mac.addr_bytes[1],
                                mac.addr_bytes[2], mac.addr_bytes[3],
                                mac.addr_bytes[4], mac.addr_bytes[5]);
        }
        return addresses[port];
}
