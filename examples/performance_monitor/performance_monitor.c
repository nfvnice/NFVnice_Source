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

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netinet/in.h>

//#define _GNU_SOURCE
#include <sched.h>   //cpu_set_t , CPU_SET
#include <pthread.h> //pthread_t

/*
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
*/

#ifndef RTE_LOG
#define RTE_LOG

#endif
#define NF_TAG "perf_mon"

#define MIN(a,b) ((a) < (b)? (a):(b))
#define MAX(a,b) ((a) > (b)? (a):(b))

/* Struct that contains information about this NF */
struct onvm_nf_info *nf_info;

/* List of Global Command Line Arguments */
typedef struct globalArgs {
        uint32_t core_id;                   /* -c <pin program to core id> */
        uint32_t service_id;                /* -s <service id> */
        const char* log_file_path;          /* -l <path to dump the monitor traces> */
        uint32_t start_delay;               /* -d <delay before starting the monitor> */
        uint32_t interval_in_ms;            /* -i <monitoring interval> */
        uint32_t total_iterations;          /* -n <total number of iterations> */
        uint32_t core_monitor_mask;         /* -m <mask indicating the core numbsers to monitor> */
}globalArgs_t;

static const char *optString = "c:s:l:d:i:n:m";

static globalArgs_t globals = {
        .core_id = 15,
        .service_id = 15,
        .start_delay = 0,
        .log_file_path = "stats_log.csv",
        .interval_in_ms = 1000000,
        .total_iterations = 30,
};

typedef struct peformance_data_inputs {
        unsigned long int interval;
        unsigned long int repetition;
        char *core_list;
        char *outfile_path;
        char *nf_tags;
}pd_inputs_t;

const char *cmd = "pidstat -C \"bridge|forward|monitor|basic|speed|perf\" -lrsuwh 1 30 > pidstat_log.csv";
const char *cmd2 = "perf stat --cpu=0-14  -r 30 -o perf_log.csv sleep 1"; //-I 2000

//const char *cmd = "pidstat -C \"bridge|forward\" -lrsuwh 1 30 | tee pidstat_log.csv";
//const char *cmd2 = "perf stat --cpu=8,10 --per-core -r 30 -o pidstat2_log.csv sleep 1"; //-I 2000
//const char *cmd2 = "perf stat -B -e cache-references,cache-misses,cycles,instructions,branches,faults,migrations --cpu=8,10 --per-core -o pidstat2_log.csv sleep 10"; //-I 2000
//const char *cmd2 = "perf stat --cpu=8,10 --per-core -o pidstat2_log.csv sleep 30"; //-I 2000

static void
usage(const char *progname) {
        printf("Usage: %s -c <core_id>  -d <start_delay> -l <log_file>"
                "-s <service_chain_file> -r <IPv4_5tuple Rules file>"
                "-b <base_ip_address> -m <max_num_ips>        \n\n", progname);
}

static int
parse_app_args(int argc, char *argv[], const char *progname) {
        int c;

        while ((c = getopt(argc, argv, optString)) != -1) {
                switch (c) {
                case 'c':
                        globals.core_id = strtoul(optarg, NULL, 10);
                        break;
                case 's':
                        globals.service_id = strtoul(optarg, NULL, 10);
                        break;
                case 'l':
                        globals.log_file_path = optarg;
                        break;
                case 'd':
                        globals.start_delay = strtoul(optarg, NULL, 10);
                        break;
                case 'i':
                        globals.interval_in_ms = strtoul(optarg, NULL, 10);
                        break;
                case 'n':
                        globals.total_iterations = strtoul(optarg, NULL, 10);
                        break;
                case 'm':
                        globals.core_monitor_mask = strtoul(optarg, NULL, 10);
                        break;
                case '?':
                        usage(progname);
                        return -1;
                default:
                        usage(progname);
                        return -1;
                }
        }
        return optind;
}

int pin_this_thread_to_core(int core_id);
int pin_this_thread_to_core(int core_id) {
   int num_cores = sysconf(_SC_NPROCESSORS_ONLN);
   if (core_id < 0 || core_id >= num_cores)
      return EINVAL;

   cpu_set_t cpuset;
   __CPU_ZERO_S(sizeof(cpuset),&cpuset); //CPU_ZERO(&cpuset);
   __CPU_SET_S(core_id, sizeof(cpuset),&cpuset); //CPU_SET(core_id, &cpuset);

   pthread_t current_thread = pthread_self();
   return pthread_setaffinity_np(current_thread, sizeof(cpu_set_t), &cpuset);
}

int do_performance_log(void *pd_data);
#if 0
int do_performance_log(void *pd_data) {
        if (pd_data){}
        /* ls -al | grep '^d' */
        FILE *pp=NULL, *pp2=NULL;
        //pp = popen("ls -al", "r");
        pp = popen(cmd, "r");
        pp2 = popen(cmd2, "r");
        int done = 0;
        if (pp != NULL && pp2 !=NULL) {
                while (1) {
                        char *line;
                        char buf[1000];
                        line = fgets(buf, sizeof buf, pp);
                        if (line == NULL) done |= 0x01; //break;
                        //if (line[0] == 'd') printf("%s", line); // line includes '\n'
                        //printf("%s", line);

                        line = fgets(buf, sizeof buf, pp2);
                        if (line == NULL) done |= 0x02; //break;
                        //if (line[0] == 'd') printf("%s", line); // line includes '\n'
                        //printf("%s", line);
                        if(done == 0x03) break;
                }
                pclose(pp);
                pclose(pp2);
        }
        return 0;
}
#else
int do_performance_log(void *pd_data) {
        FILE *pp =NULL;
        pp = popen(cmd, "r");
        int done = 0;
        if (pp != NULL ) {
                while (1) {
                        done = system(cmd2); //this will block as it runs in the foreground;
                        done = 0x02;
                        done |=0x01;

                        if(done == 0x03) break;
                }
                pclose(pp);
        }
        return 0;
}
#endif

int main(int argc, char *argv[]) {
        //int arg_offset=0;

        const char *progname = argv[0];

        /*
        if ((arg_offset = onvm_nflib_init(argc, argv, NF_TAG)) < 0)
                return -1;
        argc -= arg_offset;
        argv += arg_offset;
        */

        if (parse_app_args(argc, argv, progname) < 0) {
               printf("Invalid command-line arguments\n");
               //rte_exit(EXIT_FAILURE, "Invalid command-line arguments\n");
        }
        /* pin to core */
        pin_this_thread_to_core(globals.core_id);
        
        /* continue performance monitoring */
        do_performance_log((void*)NULL);
        
        
        /*
         *
         * onvm_nflib_run(nf_info, &packet_handler);
           printf("If we reach here, program is ending");
         */

        return 0;
}
