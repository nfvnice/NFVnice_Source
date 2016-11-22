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


void
onvm_stats_display_clients(unsigned difftime) {
        unsigned i;

        #ifdef INTERRUPT_SEM
        uint64_t vol_rate;
        uint64_t rx_drop_rate;
        uint64_t serv_rate;
        uint64_t serv_drop_rate;
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
        #else
        printf ("diff_time= 0x%4x", difftime);       
        #endif

        #ifdef INTERRUPT_SEM
        for (i = 0; i < ONVM_NUM_WAKEUP_THREADS; i++) {
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
        printf("OverflowFlag=[%d], Highest_DS_SID=[%d], Lowest_DS_SID=[%d], Num Throttles=[%"PRIu64"] \n", downstream_nf_overflow, highest_downstream_nf_service_id, lowest_upstream_to_throttle, throttle_count);
#endif  //ENABLE_NF_BACKPRESSURE

        printf("\nCLIENTS\n");
        printf("-------\n");
        for (i = 0; i < MAX_CLIENTS; i++) {
                if (!onvm_nf_is_valid(&clients[i]))
                        continue;
                const uint64_t rx = clients[i].stats.rx;
                const uint64_t rx_drop = clients[i].stats.rx_drop
                        #ifdef PRE_PROCESS_DROP_ON_RX
                        #ifdef DROP_APPROACH_1
                               + clients_stats->tx_predrop[i]
                        #endif
                        #endif
                                                           ;

                const uint64_t tx = clients_stats->tx[i];
                const uint64_t tx_drop = clients_stats->tx_drop[i];



                #ifndef INTERRUPT_SEM
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
                const uint64_t prev_rx = clients[i].stats.prev_rx;
                const uint64_t prev_rx_drop = clients[i].stats.prev_rx_drop;
                const uint64_t prev_tx = clients_stats->prev_tx[i];
                const uint64_t prev_tx_drop = clients_stats->prev_tx_drop[i];
                const uint64_t avg_wakeups = ( clients[i].stats.wakeup_count -  clients[i].stats.prev_wakeup_count);
                const uint64_t yields =  (clients_stats->wkup_count[i] - clients_stats->prev_wkup_count[i]);

                vol_rate = (rx - prev_rx) / difftime;
                rx_drop_rate = (rx_drop - prev_rx_drop) / difftime;
                serv_rate = (tx - prev_tx) / difftime;
                serv_drop_rate = (tx_drop - prev_tx_drop) / difftime;
                rx_qlen = rte_ring_count(clients[i].rx_q);
                tx_qlen = rte_ring_count(clients[i].tx_q);
                comp_cost = clients_stats->comp_cost[i];

                if (avg_wakeups > 0 ) {
                        avg_pkts_per_wakeup = (serv_rate+serv_drop_rate)/avg_wakeups;
                        good_pkts_per_wakeup = (serv_rate)/avg_wakeups;
                        if(yields) yield_rate = (serv_rate)/yields;
                }

                printf("Client %2u:[%d, %d],  comp_cost=%"PRIu64", avg_wakeups=%"PRIu64", yields=%"PRIu64", msg_flag(blocked)=%d, \n"
                "avg_ppw=%"PRIu64", avg_good_ppw=%"PRIu64",  pkts_per_yield=%"PRIu64"\n"
                "rx_rate=%"PRIu64", rx_drop=%"PRIu64", rx_drop_rate=%"PRIu64", rx_qlen=%"PRIu64"\n"
                "tx_rate=%"PRIu64", tx_drop=%"PRIu64", tx_drop_rate=%"PRIu64", tx_qlen=%"PRIu64"\n",
                clients[i].info->instance_id, i, clients[i].info->service_id, comp_cost, avg_wakeups, yields, rte_atomic16_read(clients[i].shm_server),
                avg_pkts_per_wakeup, good_pkts_per_wakeup, yield_rate,
                vol_rate, rx_drop, rx_drop_rate, rx_qlen, serv_rate, tx_drop, serv_drop_rate, tx_qlen);

                clients[i].stats.prev_rx = rx; //clients[i].stats.rx;
                clients[i].stats.prev_rx_drop = rx_drop; //clients[i].stats.rx_drop;
                clients_stats->prev_tx[i] = tx; //clients_stats->tx[i];
                clients_stats->prev_tx_drop[i] = tx_drop; //clients_stats->tx_drop[i];

                clients[i].stats.prev_wakeup_count = clients[i].stats.wakeup_count;
                clients_stats->prev_wkup_count[i] = clients_stats->wkup_count[i];
                #endif

        #ifdef ENABLE_NF_BACKPRESSURE
                //printf("rx_overflow=[%d], ThrottleNF_Flag=[%d], Highest_DS_SID=[%d] NF_Throttle_count=[%"PRIu64"], \n", clients[i].rx_buffer_overflow, clients[i].throttle_this_upstream_nf, clients[i].highest_downstream_nf_index_id, clients[i].throttle_count);
                #ifdef NF_BACKPRESSURE_APPROACH_1
                printf("bkpr_drop=%"PRIu64", bkpr_drop_rate=%"PRIu64"\n",clients[i].stats.bkpr_drop,(clients[i].stats.bkpr_drop - clients[i].stats.prev_bkpr_drop)/ difftime);
                clients[i].stats.prev_bkpr_drop = clients[i].stats.bkpr_drop;
                #endif //NF_BACKPRESSURE_APPROACH_1

                #ifdef NF_BACKPRESSURE_APPROACH_2
                printf("ThrottleNF_Flag=[%d], NF_Throttle_count=[%"PRIu64"], \n", clients[i].throttle_this_upstream_nf, clients[i].throttle_count);
                #endif //NF_BACKPRESSURE_APPROACH_2
        #endif  //ENABLE_NF_BACKPRESSURE

        #ifdef USE_CGROUPS_PER_NF_INSTANCE
                printf("NF_Core_Id [%d], NF_comp cost=[%d], NF_CPU_SHARE=[%d]\n", clients[i].info->core_id, clients[i].info->comp_cost, clients[i].info->cpu_share);
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
