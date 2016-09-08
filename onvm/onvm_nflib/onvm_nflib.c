/*********************************************************************
 *                     openNetVM
 *              https://sdnfv.github.io
 *
 *   BSD LICENSE
 *
 *   Copyright(c)
 *            2015-2016 George Wfashington University
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

                                  onvm_nflib.c


                  File containing all functions of the NF API


******************************************************************************/

#include "onvm_nflib_internal.h"
#include "onvm_nflib.h"

/************************************API**************************************/
#define USE_STATIC_IDS

int
onvm_nflib_init(int argc, char *argv[], const char *nf_tag) {
        const struct rte_memzone *mz;
        const struct rte_memzone *mz_scp;
        struct rte_mempool *mp;
        struct onvm_service_chain **scp;
        int retval_eal, retval_parse, retval_final;

        if ((retval_eal = rte_eal_init(argc, argv)) < 0)
                return -1;

        /* Modify argc and argv to conform to getopt rules for parse_nflib_args */
        argc -= retval_eal; argv += retval_eal;

        /* Reset getopt global variables opterr and optind to their default values */
        opterr = 0; optind = 1;

        if ((retval_parse = onvm_nflib_parse_args(argc, argv)) < 0)
                rte_exit(EXIT_FAILURE, "Invalid command-line arguments\n");

        /*
         * Calculate the offset that the nf will use to modify argc and argv for its
         * getopt call. This is the sum of the number of arguments parsed by
         * rte_eal_init and parse_nflib_args. This will be decremented by 1 to assure
         * getopt is looking at the correct index since optind is incremented by 1 each
         * time "--" is parsed.
         * This is the value that will be returned if initialization succeeds.
         */
        retval_final = (retval_eal + retval_parse) - 1;

        /* Reset getopt global variables opterr and optind to their default values */
        opterr = 0; optind = 1;

        /* Lookup mempool for nf_info struct */
        nf_info_mp = rte_mempool_lookup(_NF_MEMPOOL_NAME);
        if (nf_info_mp == NULL)
                rte_exit(EXIT_FAILURE, "No Client Info mempool - bye\n");

        /* Initialize the info struct */
        nf_info = onvm_nflib_info_init(nf_tag);

        mp = rte_mempool_lookup(PKTMBUF_POOL_NAME);
        if (mp == NULL)
                rte_exit(EXIT_FAILURE, "Cannot get mempool for mbufs\n");

        mz = rte_memzone_lookup(MZ_CLIENT_INFO);
        if (mz == NULL)
                rte_exit(EXIT_FAILURE, "Cannot get tx info structure\n");
        tx_stats = mz->addr;

        mz_scp = rte_memzone_lookup(MZ_SCP_INFO);
        if (mz_scp == NULL)
                rte_exit(EXIT_FAILURE, "Cannot get service chain info structre\n");
        scp = mz_scp->addr;
        default_chain = *scp;

        onvm_sc_print(default_chain);

        nf_info_ring = rte_ring_lookup(_NF_QUEUE_NAME);
        if (nf_info_ring == NULL)
                rte_exit(EXIT_FAILURE, "Cannot get nf_info ring");

        /* Put this NF's info struct onto queue for manager to process startup */
        if (rte_ring_enqueue(nf_info_ring, nf_info) < 0) {
                rte_mempool_put(nf_info_mp, nf_info); // give back mermory
                rte_exit(EXIT_FAILURE, "Cannot send nf_info to manager");
        }

        /* Wait for a client id to be assigned by the manager */
        RTE_LOG(INFO, APP, "Waiting for manager to assign an ID...\n");
        for (; nf_info->status == (uint16_t)NF_WAITING_FOR_ID ;) {
                sleep(1);
        }

        /* This NF is trying to declare an ID already in use. */
        if (nf_info->status == NF_ID_CONFLICT) {
                rte_mempool_put(nf_info_mp, nf_info);
                rte_exit(NF_ID_CONFLICT, "Selected ID already in use. Exiting...\n");
        } else if(nf_info->status == NF_NO_IDS) {
                rte_mempool_put(nf_info_mp, nf_info);
                rte_exit(NF_NO_IDS, "There are no ids available for this NF\n");
        } else if(nf_info->status != NF_STARTING) {
                rte_mempool_put(nf_info_mp, nf_info);
                rte_exit(EXIT_FAILURE, "Error occurred during manager initialization\n");
        }
        RTE_LOG(INFO, APP, "Using Instance ID %d\n", nf_info->instance_id);
        RTE_LOG(INFO, APP, "Using Service ID %d\n", nf_info->service_id);
        sleep(2);

        /* Now, map rx and tx rings into client space */
        rx_ring = rte_ring_lookup(get_rx_queue_name(nf_info->instance_id));
        if (rx_ring == NULL)
                rte_exit(EXIT_FAILURE, "Cannot get RX ring - is server process running?\n");

        tx_ring = rte_ring_lookup(get_tx_queue_name(nf_info->instance_id));
        if (tx_ring == NULL)
                rte_exit(EXIT_FAILURE, "Cannot get TX ring - is server process running?\n");

        /* Tell the manager we're ready to recieve packets */
        nf_info->status = NF_RUNNING;

        #ifdef INTERRUPT_SEM
        init_shared_cpu_info(nf_info->instance_id);
        #ifdef USE_SIGNAL
        nf_info->pid = getpid();
        #endif //USE_SIGNAL
        #endif

        RTE_LOG(INFO, APP, "Finished Process Init.\n");
        return retval_final;
}

#ifdef INTERRUPT_SEM
void onvm_nf_yeild(struct onvm_nf_info* info);
void onvm_nf_yeild(struct onvm_nf_info* info) {
        
        #ifdef USE_MQ
        static char msg_t[256] = "\0";
        static unsigned long int msg_prio=0;
        //struct timespec timeout = {.tv_sec=0, .tv_nsec=1000};
        static ssize_t rmsg_len = 0;
        #endif //USE_MQ

        #ifdef USE_SIGNAL
        //static sigset_t mask;
        //static struct timespec timeout;
	//static sigset_t orig_mask;  
        //struct sigaction  action;      
        #endif

        /* For now discard the special NF instance and put all NFs to wait */
        if ((!ONVM_SPECIAL_NF) || (info->instance_id != 1)) { }
        
        tx_stats->wkup_count[info->instance_id] += 1;
        rte_atomic16_set(flag_p, 1);
        
        #ifdef USE_MQ
        rmsg_len = mq_receive(mutex, msg_t,sizeof(msg_t), (unsigned int *)&msg_prio);
        //clock_gettime(CLOCK_REALTIME, &timeout);timeout.tv_nsec+=(10*1000*1000);
        //rmsg_len = mq_timedreceive(mutex, msg_t,sizeof(msg_t), (unsigned int *)&msg_prio, &timeout);
        if( 0 > rmsg_len) { 
                perror ("mq_timedreceive() failed!!");
                struct mq_attr attr = {0,};
                mq_getattr(mutex, &attr);
                printf (" Flags=%u, Max_MSG=%u, MSG_SIZE=%u, CURMSGS=%u",(unsigned int)attr.mq_flags, (unsigned int)attr.mq_maxmsg, (unsigned int)attr.mq_msgsize, (unsigned int)attr.mq_curmsgs);
               // exit(1);
        }
        #endif

        #ifdef USE_FIFO
        unsigned msg_t= 0;
        msg_t = read(mutex, (void*)&msg_t, sizeof(msg_t));                      
        #endif

        #ifdef USE_SIGNAL
        while(1) {
                //int sig_rcvd = sigtimedwait(&mask, NULL, &timeout);
                int sig_rcvd = 0;
                //static int counter = 0;
                int res = sigwait(&mutex, &sig_rcvd);
                if (res) { perror ("sigwait() Failed!!");  exit(1);}             
                //if (sig_rcvd == SIGINT) { printf("Recieved SIGINT: %d", sig_rcvd); exit(1); break;}
                if (sig_rcvd == SIGUSR1) { /* counter++; printf("[%d]", counter); printf("Recieved SIGUSR1: %d", sig_rcvd); */ break;}
                //if (sig_rcvd != SIGUSR1) { printf("Recieved other SIGNAL: %d", sig_rcvd); continue;}     
        }
        #endif
        
        #ifdef USE_SEMAPHORE
        sem_wait(mutex);
        #endif

        #ifdef USE_SCHED_YIELD
        sched_yield();
        #endif

        #ifdef USE_NANO_SLEEP
        static struct timespec dur = {.tv_sec=0, .tv_nsec=10}, rem = {.tv_sec=0, .tv_nsec=0};
        nanosleep(&dur, &rem); 
        if (rem.tv_nsec > 0) { printf ("nano sleep obstructed with time [%d]\n", (int)rem.tv_nsec);}
        #endif

        #ifdef USE_SOCKET
        //int onvm_mgr_fd = accept(mutex, NULL, NULL);
        static char msg_t[2] = "\0";
        static struct sockaddr_un  onvm_addr;
        static socklen_t addr_len = sizeof( struct sockaddr_un);     
        /* if (read(onvm_mgr_fd, &msg_t, sizeof(msg_t));
        */
        //int rlen = 
        recvfrom(mutex, msg_t, sizeof(msg_t), 0, (struct sockaddr *) &onvm_addr, &addr_len);
        //printf("%d ", rlen);
        //close(onvm_mgr_fd);
        #endif

        #ifdef USE_FLOCK
        flock(mutex, LOCK_EX);
        #endif
        
        #ifdef USE_MQ2
        static unsigned long msg_t = 1;
        //struct msgbuf { long mtype; char mtext[1];};
        //static struct msgbuf msg_t = {1,'\0'};
        //static msgbuf_t msg_t = {.mtype = 1, .mtext[0]='\0'};
        //msgrcv(mutex, msg_t, msg_len, msg_type, 0);      
        
        /*if(0 > msgrcv(mutex, NULL,0,0, IPC_NOWAIT)){ 
                if (errno == E2BIG) 
                        printf("Msg has arrived!!\n");
        } */
        if (0 > msgrcv(mutex, &msg_t, 0, 1, 0)) {        
        //if (0 > msgrcv(mutex, &msg_t, sizeof(msg_t.mtext), 1, 0)) {
                perror ("msgrcv Failed!!");
        }
        #endif

        #ifdef USE_ZMQ
        static char msg_t[2] = "\0";
        zmq_recv(mutex, msg_t, sizeof(msg_t), 0);
        #endif

}
#endif  //INTERRUPT_SEM

int
onvm_nflib_run(
        struct onvm_nf_info* info,
        int(*handler)(struct rte_mbuf* pkt, struct onvm_pkt_meta* meta)
        ) {
        void *pkts[PKT_READ_SIZE];
        struct onvm_pkt_meta* meta;
        
        #ifdef INTERRUPT_SEM
        // To account NFs computation cost (sampled over SAMPLING_RATE packets)
        uint64_t start_tsc = 0, end_tsc = 0;
        #endif
        
        printf("\nClient process %d handling packets\n", info->instance_id);
        printf("[Press Ctrl-C to quit ...]\n");

        /* Listen for ^C so we can exit gracefully */
        signal(SIGINT, onvm_nflib_handle_signal);
        

        for (; keep_running;) {
                uint16_t i, j, nb_pkts = PKT_READ_SIZE;
                void *pktsTX[PKT_READ_SIZE];
                int tx_batch_size = 0;
                int ret_act;

                /* try dequeuing max possible packets first, if that fails, get the
                 * most we can. Loop body should only execute once, maximum */
                while (nb_pkts > 0 &&
                                unlikely(rte_ring_dequeue_bulk(rx_ring, pkts, nb_pkts) != 0))
                        nb_pkts = (uint16_t)RTE_MIN(rte_ring_count(rx_ring), PKT_READ_SIZE);

                if(nb_pkts == 0) {
                        #ifdef INTERRUPT_SEM
                        onvm_nf_yeild(info);                     
                        #endif
                        continue;
                }
                /* Give each packet to the user proccessing function */
                for (i = 0; i < nb_pkts; i++) {
                        meta = onvm_get_pkt_meta((struct rte_mbuf*)pkts[i]);

                        #ifdef INTERRUPT_SEM
                        counter++;
                        meta = onvm_get_pkt_meta((struct rte_mbuf*)pkts[i]);
                        if (counter % SAMPLING_RATE == 0) {
                                start_tsc = rte_rdtsc();
                        }
                        #endif

                        ret_act = (*handler)((struct rte_mbuf*)pkts[i], meta);
                        
                        #ifdef INTERRUPT_SEM
                        if (counter % SAMPLING_RATE == 0) {
                                end_tsc = rte_rdtsc();
                                tx_stats->comp_cost[info->instance_id] = end_tsc - start_tsc;
                        }
                        #endif

                        /* NF returns 0 to return packets or 1 to buffer */
                        if(likely(ret_act == 0)) {
                                pktsTX[tx_batch_size++] = pkts[i];
                        }
                        else {
                                tx_stats->tx_buffer[info->instance_id]++;
                        }
                }

                if (unlikely(tx_batch_size > 0 && rte_ring_enqueue_bulk(tx_ring, pktsTX, tx_batch_size) == -ENOBUFS)) {
                        tx_stats->tx_drop[info->instance_id] += tx_batch_size;
                        for (j = 0; j < tx_batch_size; j++) {
                                rte_pktmbuf_free(pktsTX[j]);
                        }
                } else {
                        tx_stats->tx[info->instance_id] += tx_batch_size;
                }
        }

        nf_info->status = NF_STOPPED;

        /* Put this NF's info struct back into queue for manager to ack shutdown */
        nf_info_ring = rte_ring_lookup(_NF_QUEUE_NAME);
        if (nf_info_ring == NULL) {
                rte_mempool_put(nf_info_mp, nf_info); // give back mermory
                rte_exit(EXIT_FAILURE, "Cannot get nf_info ring for shutdown");
        }

        if (rte_ring_enqueue(nf_info_ring, nf_info) < 0) {
                rte_mempool_put(nf_info_mp, nf_info); // give back mermory
                rte_exit(EXIT_FAILURE, "Cannot send nf_info to manager for shutdown");
        }
        return 0;
}


int
onvm_nflib_return_pkt(struct rte_mbuf* pkt) {
        /* FIXME: should we get a batch of buffered packets and then enqueue? Can we keep stats? */
        if(unlikely(rte_ring_enqueue(tx_ring, pkt) == -ENOBUFS)) {
                rte_pktmbuf_free(pkt);
                tx_stats->tx_drop[nf_info->instance_id]++;
                return -ENOBUFS;
        }
        else tx_stats->tx_returned[nf_info->instance_id]++;
        return 0;
}


void
onvm_nflib_stop(void) {
        rte_exit(EXIT_SUCCESS, "Done.");
}


/******************************Helper functions*******************************/


static struct onvm_nf_info *
onvm_nflib_info_init(const char *tag)
{
        void *mempool_data;
        struct onvm_nf_info *info;

        if (rte_mempool_get(nf_info_mp, &mempool_data) < 0) {
                rte_exit(EXIT_FAILURE, "Failed to get client info memory");
        }

        if (mempool_data == NULL) {
                rte_exit(EXIT_FAILURE, "Client Info struct not allocated");
        }

        info = (struct onvm_nf_info*) mempool_data;
        info->instance_id = initial_instance_id;
        info->service_id = service_id;
        info->status = NF_WAITING_FOR_ID;
        info->tag = tag;

        return info;
}


static void
onvm_nflib_usage(const char *progname) {
        printf("Usage: %s [EAL args] -- "
#ifdef USE_STATIC_IDS
               "[-n <instance_id>]"
#endif
               "[-r <service_id>]\n\n", progname);
}


static int
onvm_nflib_parse_args(int argc, char *argv[]) {
        const char *progname = argv[0];
        int c;

        opterr = 0;
#ifdef USE_STATIC_IDS
        while ((c = getopt (argc, argv, "n:r:")) != -1)
#else
        while ((c = getopt (argc, argv, "r:")) != -1)
#endif
                switch (c) {
#ifdef USE_STATIC_IDS
                case 'n':
                        initial_instance_id = (uint16_t) strtoul(optarg, NULL, 10);
                        break;
#endif
                case 'r':
                        service_id = (uint16_t) strtoul(optarg, NULL, 10);
                        // Service id 0 is reserved
                        if (service_id == 0) service_id = -1;
                        break;
                case '?':
                        onvm_nflib_usage(progname);
                        if (optopt == 'n')
                                fprintf(stderr, "Option -%c requires an argument.\n", optopt);
                        else if (isprint(optopt))
                                fprintf(stderr, "Unknown option `-%c'.\n", optopt);
                        else
                                fprintf(stderr, "Unknown option character `\\x%x'.\n", optopt);
                        return -1;
                default:
                        return -1;
                }

        if (service_id == (uint16_t)-1) {
                /* Service ID is required */
                fprintf(stderr, "You must provide a nonzero service ID with -r\n");
                return -1;
        }
        return optind;
}


static void
onvm_nflib_handle_signal(int sig)
{
        if (sig == SIGINT) {
                keep_running = 0;
                #ifdef INTERRUPT_SEM
                if (/*(mutex) && */(rte_atomic16_read(flag_p) ==1)) {
                        rte_atomic16_set(flag_p, 0);
                        
                        #ifdef USE_MQ
                        mq_close(mutex);
                        #endif                        
                        
                        #ifdef USE_FIFO
                        close(mutex);                  
                        #endif
                        
                        #ifdef USE_SIGNAL
                        #endif
        
                        #ifdef USE_SEMAPHORE
                        sem_post(mutex);
                        #endif //USE_MQ
                        
                        #ifdef USE_SOCKET
                        shutdown(mutex, SHUT_RDWR);
                        close(mutex);
                        #endif

                        #ifdef USE_FLOCK
                        flock(mutex, LOCK_UN|LOCK_NB);
                        close(mutex);
                        #endif

                        #ifdef USE_MQ2
                        msgctl(mutex, IPC_RMID, 0);
                        #endif
                        
                        #ifdef USE_ZMQ
                        zmq_close(mutex);
                        zmq_ctx_destroy(mutex_ctx);
                        #endif
                }
                #endif
        }
        /* TODO: Main thread for INTERRUPT_SEM case: Must additionally relinquish SEM, SHM */
}


#ifdef INTERRUPT_SEM
static void 
init_shared_cpu_info(uint16_t instance_id) {
        const char *sem_name;
        int shmid;
        key_t key;
        char *shm;

        sem_name = get_sem_name(instance_id);
        fprintf(stderr, "sem_name=%s for client %d\n", sem_name, instance_id);

        #ifdef USE_MQ
        struct mq_attr attr = {.mq_flags=0, .mq_maxmsg=1, .mq_msgsize=sizeof(int), .mq_curmsgs=0}
;       mutex = mq_open(sem_name, O_RDONLY, 0666, &attr);
        //mutex = mq_open(sem_name, O_RDONLY);
        if (0 > mutex) {
                perror("Unable to open mqd");
                fprintf(stderr, "unable to execute semphore for client %d\n", instance_id);
                exit(1);
        }
        #endif

        #ifdef USE_FIFO
        mutex = mkfifo(sem_name, 0666);
        if (mutex) { perror ("MKFIFO failed!!");}        
        mutex = open(sem_name, O_RDONLY); 
        #endif

        #ifdef USE_SIGNAL
        sigemptyset (&mutex);
	sigaddset (&mutex, SIGUSR1);
        //sigaddset (&mutex, SIGINT);
        sigprocmask(SIG_BLOCK, &mutex, NULL);
        #endif
        
        #ifdef USE_SEMAPHORE
        mutex = sem_open(sem_name, 0, 0666, 0);
        if (mutex == SEM_FAILED) {
                perror("Unable to execute semaphore");
                fprintf(stderr, "unable to execute semphore for client %d\n", instance_id);
                sem_close(mutex);
                exit(1);
        }
        #endif //USE_MQ

        #ifdef USE_SOCKET
        mutex = socket (PF_LOCAL, SOCK_DGRAM, 0);
        struct sockaddr_un name = {.sun_family=AF_UNIX};
        strncpy(name.sun_path, sem_name, sizeof(name.sun_path) - 1);
        if ( 0 > bind(mutex, (const struct sockaddr *) &name,sizeof(struct sockaddr_un)) 
              /* || 0 > listen(mutex, 1) */ ) {
                perror ("Unable to bind or listen on AF_UNIX Socket!!");
                close(mutex);
                exit(1);
        }
        #endif

        #ifdef USE_FLOCK
        mutex = open(sem_name, O_CREAT|O_RDWR, 0666);
        #endif
        
        #ifdef USE_MQ2
        //struct mq_attr attr = {.mq_flags=0, .mq_maxmsg=1, .mq_msgsize=sizeof(int), .mq_curmsgs=0}
;       //mutex = open_queue(sem_name);
        key = get_rx_shmkey(instance_id); //ftok(sem_name,instance_id);
        mutex = msgget(key, IPC_CREAT|0666);
        if (0 > mutex) {
                perror("Unable to open msgqueue!");
                fprintf(stderr, "unable to execute semphore for client %d\n", instance_id);
                exit(1);
        }
        #endif
        
        #ifdef USE_ZMQ
        if (NULL == (zmq_ctx = zmq_init(1))){ perror("zmq_init()! failed!!");}
        if (NULL == (mutex_ctx = zmq_ctx_new ())){ perror("zmq_ctx_init()! failed!!");}
        if (NULL == (mutex = zmq_socket (mutex_ctx, ZMQ_REP))){ perror("zmq_socket()! failed!!");} //ZMQ_PULL
        if ( 0 != zmq_bind (mutex, sem_name)) {  //"tcp://*:5555" "ipc:///tmp/feeds/0"
                perror ("Failed to create ZMQ socket!!");
                exit(1);
        }
        #endif
        
        /* get flag which is shared by server */
        key = get_rx_shmkey(instance_id);
        if ((shmid = shmget(key, SHMSZ, 0666)) < 0) {
                perror("shmget");
                fprintf(stderr, "unable to Locate the segment for client %d\n", instance_id);
                exit(1);
        }

        if ((shm = shmat(shmid, NULL, 0)) == (char *) -1) {
                fprintf(stderr, "can not attach the shared segment to the client space for client %d\n", instance_id);
                exit(1);
        }

        flag_p = (rte_atomic16_t *)shm;
}
#endif

