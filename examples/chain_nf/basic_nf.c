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
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 *
 * monitor.c - an example using onvm. Print a message each p package received
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

#include <rte_common.h>
#include <rte_mbuf.h>
#include <rte_ip.h>

#include "onvm_nflib.h"
#include "onvm_pkt_helper.h"

#define NF_TAG "basic_nf"

//#define FAKE_COMPUTE
#define FACT_VALUE 30
long factorial(int n);
long factorial(int n)
{
        long result;
        if (n == 0 || n == 1) {
                result = 1;
        }
        else {
                result = factorial(n-1) * n;
        }

        return result;
}

/* Struct that contains information about this NF */
struct onvm_nf_info *nf_info;

/* number of package between each print */
static uint32_t print_delay = 5000000;
static uint32_t destination = 0;
static uint16_t dst_flag = 0;
static uint32_t comp_cost_level = 0; // 0=No overhead, 1=Min, 2=Med, 3=Max
static uint64_t comp_cost_cycles =0;
static uint64_t avg_comp_cost=0;
/*
 * Print a usage message
 */
static void
usage(const char *progname) {
        printf("Usage: %s [EAL args] -- [NF_LIB args] -- -p <print_delay>\n\n", progname);
}

/*
 * Parse the application arguments.
 */
static int
parse_app_args(int argc, char *argv[], const char *progname) {
        int c;
        printf("Processing Args!!!!!!!!!!!1\n\n");
        while ((c = getopt (argc, argv, "d:p:c:")) != -1) {
                switch (c) {
                case 'p':
                        print_delay = strtoul(optarg, NULL, 10);
                        RTE_LOG(INFO, APP, "print_delay = %d\n", print_delay);
                        break;
                case 'd':
                        destination = strtoul(optarg, NULL, 10);
                        if (destination) dst_flag = 1;
                        printf("Destination_flag: %d, Destination: %d\n", dst_flag, destination); 
                        break;
                case 'c':
                        comp_cost_level = strtoul(optarg, NULL, 10);
                        //if (comp_cost_level) dst_flag = 1;
                        printf("comp_cos_level: %d\n", comp_cost_level); 
                        break;

                case '?':
                        usage(progname);
                        if (optopt == 'p')
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

static void
do_additional_stat_display(void) {
        static uint64_t last_cycles;
        static uint64_t cur_pkts = 0;
        static uint64_t last_pkts = 0;
        uint64_t cur_rate = 0;
        static uint64_t peak_rate = 0;
        static uint64_t min_rate  = 0;

        uint64_t cur_cycles = rte_get_tsc_cycles();
        cur_pkts += print_delay;

        cur_rate =  ((cur_pkts - last_pkts) * rte_get_timer_hz()) / (cur_cycles - last_cycles);
        peak_rate = (cur_rate > peak_rate)? (cur_rate):(peak_rate);
        min_rate = (min_rate == 0 || min_rate > cur_rate)? (cur_rate):(min_rate);        

        printf("Total packets: %9"PRIu64" \n", cur_pkts);
        printf("TX pkts per second: %9"PRIu64" \n", cur_rate);
        printf("TX pkts Max rate: %9"PRIu64" and  Min rate: %9"PRIu64" \n", peak_rate, min_rate);
        printf("Computation Level: %9"PRIu64" and  Computation Cost: %9"PRIu64" Avg. Compt Cost: %9"PRIu64"\n", (uint64_t)comp_cost_level, comp_cost_cycles, avg_comp_cost);
        last_pkts = cur_pkts;
        last_cycles = cur_cycles;

        printf("\n\n");
}
/*
 * This function displays stats. It uses ANSI terminal codes to clear
 * screen when called. It is called from a single non-master
 * thread in the server process, when the process is run with more
 * than one lcore enabled.
 */
static void
do_stats_display(struct rte_mbuf* pkt) {
        const char clr[] = { 27, '[', '2', 'J', '\0' };
        const char topLeft[] = { 27, '[', '1', ';', '1', 'H', '\0' };
        static int pkt_process = 0;
        struct ipv4_hdr* ip;

        pkt_process += print_delay;

        /* Clear screen and move to top left */
        printf("%s%s", clr, topLeft);

        printf("PACKETS\n");
        printf("-----\n");
        printf("Port : %d\n", pkt->port);
        printf("Size : %d\n", pkt->pkt_len);
        printf("Hash : %u\n", pkt->hash.rss);
        printf("N°   : %d\n", pkt_process);
        printf("\n\n");

        ip = onvm_pkt_ipv4_hdr(pkt);
        if (ip != NULL) {
                onvm_pkt_print(pkt);
        } else {
                printf("No IP4 header found\n");
        }
        do_additional_stat_display();
}

static inline int 
do_compute_at_cost( void ) {
        
        static int i=0,j=0,k=0;
        static uint64_t start_cycles = 0;
        if (0 == comp_cost_cycles) {
                start_cycles = rte_rdtsc_precise(); //rte_get_tsc_cycles();
                switch(comp_cost_level){
                default:
                break;
                case 1:
                        j=FACT_VALUE;
                        k=1;
                break;
                case 2:
                        j=FACT_VALUE*3/2;
                        k=2;
                break;
                case 3:
                        j=FACT_VALUE*2;
                        k=3;
                break;
                case 4:
                        j=FACT_VALUE*2*10;
                        k=1;
                break;
                case 5:
                        j=FACT_VALUE*2*10;
                        k=2;
                break;
                case 6:
                        j=FACT_VALUE*2*10;
                        k=3;
                break;
                case 7:
                        j=FACT_VALUE*2*10;
                        k=10;
                break;
                case 8:
                        j=FACT_VALUE*2*10;
                        k=50;
                break;
                case 9:
                        j=FACT_VALUE*2*10;
                        k=100;
                break;
                }
        }
        /* (Basic NF_V2)
         * Level=1                      Level=2                     Level=3                     Level=4                     Level=5                     Level=6
         * J=FACT_VALUE(30);            J=FACT_VALUE*3/2;           J=FACT_VALUE*2;             J=FACT_VALUE*2*10;          J=FACT_VALUE*2*10;          J=FACT_VALUE*2*10;
         * k=1                          k=2                         k=3                         k=1                         k=2                         k=3
         * PktRate=14.6Mpps             PktRate=9.48Mpps            PktRate=4.80Mpps            PktRate=1.18Mpps            PktRate=0.60Mpps            PktRate=0.41Mpps
         * BitRate=xxxxMbps             BitRate=4850Mbps            BitRate=2455Mbps            BitRate=0603Mbps            BitRate=0310Mbps            BitRate=0208Mbps
         */
        for (i = 0; i < k; i++) {
                factorial(j);
        }
        if (0 == comp_cost_cycles) {
                ////comp_cost_cycles = rte_get_tsc_cycles() - start_cycles;
                comp_cost_cycles =  rte_rdtsc_precise() - start_cycles;
                avg_comp_cost = (avg_comp_cost==0)?(comp_cost_cycles):((avg_comp_cost+comp_cost_cycles) >> 1);
        }
        return 0;
}

static int
packet_handler(struct rte_mbuf* pkt, struct onvm_pkt_meta* meta) {
        
        static uint32_t counter = 0;

       // if (comp_cost_level) {
                do_compute_at_cost();  
       // }

        if (++counter == print_delay) {
                do_stats_display(pkt);
                counter = 0;
                comp_cost_cycles=0;
        }

        // IF specified destination, then do this instead
        if (dst_flag) {      
                meta->action = ONVM_NF_ACTION_TONF;
                meta->destination = destination;
                meta->action = ONVM_NF_ACTION_NEXT;
                meta->destination = pkt->port;
        }
        // ELSE: Forward to next NF (as defined in SDN rule/ default service chain) on the same port
        else {
                meta->action = ONVM_NF_ACTION_NEXT;
                meta->destination = pkt->port;
        }
        return 0;
}


int main(int argc, char *argv[]) {
        int arg_offset;

        const char *progname = argv[0];
        arg_offset = onvm_nflib_init(argc, argv, NF_TAG);

        if (arg_offset < 0)
                return -1;
        argc -= arg_offset;
        argv += arg_offset;

        if (parse_app_args(argc, argv, progname) < 0)
                rte_exit(EXIT_FAILURE, "Invalid command-line arguments\n");

        onvm_nflib_run(nf_info, &packet_handler);
        printf("If we reach here, program is ending");
        return 0;
}
