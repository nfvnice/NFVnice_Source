/*

 strace -c ./build/latency_profiler
 strace -T ./build/latency_profiler
 taskset 0x04 ./build/latency_profiler

*/
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <time.h>
#include <unistd.h>
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
#include <mqueue.h>

#include <fcntl.h>           /* For O_* constants */
#include <sys/stat.h>        /* For mode constants */
#include <semaphore.h>

#include <sys/time.h>
#include <sys/resource.h>


#define USE_THIS_CLOCK  CLOCK_THREAD_CPUTIME_ID //CLOCK_PROCESS_CPUTIME_ID //CLOCK_MONOTONIC

#ifdef USE_ZMQ
#include <zmq.h>
#endif

#if defined(_POSIX_TIMERS) && (_POSIX_TIMERS > 0) &&                           \
    defined(_POSIX_MONOTONIC_CLOCK)
#define HAS_CLOCK_GETTIME_MONOTONIC
#endif

#ifdef HAS_CLOCK_GETTIME_MONOTONIC
  struct timespec start, stop;
#else
  struct timeval start, stop;
#endif
int64_t delta = 0;

unsigned long int min_lat = 0;
unsigned long int max_lat = 0;
unsigned long int avg_lat = 0;

int get_cur_time(void* ct)
{
#ifdef HAS_CLOCK_GETTIME_MONOTONIC
    if (clock_gettime(USE_THIS_CLOCK,(struct timespec *)ct) == -1) {
      perror("clock_gettime");
      return 1;
    }
#else
    if (gettimeofday(&ct, NULL) == -1) {
      perror("gettimeofday");
      return 1;
    }
#endif
}

int get_start_time()
{
#ifdef HAS_CLOCK_GETTIME_MONOTONIC
    if (clock_gettime(USE_THIS_CLOCK, &start) == -1) {
      perror("clock_gettime");
      return 1;
    }
#else
    if (gettimeofday(&start, NULL) == -1) {
      perror("gettimeofday");
      return 1;
    }
#endif
}

int get_stop_time()
{
#ifdef HAS_CLOCK_GETTIME_MONOTONIC
    if (clock_gettime(USE_THIS_CLOCK, &stop) == -1) {
      perror("clock_gettime");
      return 1;
    }
#else
    if (gettimeofday(&stop, NULL) == -1) {
      perror("gettimeofday");
      return 1;
    }
#endif
}
#ifdef HAS_CLOCK_GETTIME_MONOTONIC
int64_t get_ttl_time(struct timespec start, struct timespec stop)
{
#else
int64_t get_ttl_time(struct timeval start, struct timeval stop)
#endif
#ifdef HAS_CLOCK_GETTIME_MONOTONIC
        delta = ((stop.tv_sec - start.tv_sec) * 1000000000 +
             (stop.tv_nsec - start.tv_nsec));
#else
        delta = (stop.tv_sec - start.tv_sec) * 1000000000 + 
             (stop.tv_usec - start.tv_usec) * 1000;
#endif
        return delta;
}

int64_t get_elapsed_time()
{
#ifdef HAS_CLOCK_GETTIME_MONOTONIC
        delta = ((stop.tv_sec - start.tv_sec) * 1000000000 +
             (stop.tv_nsec - start.tv_nsec));
#else
        delta = (stop.tv_sec - start.tv_sec) * 1000000000 + 
             (stop.tv_usec - start.tv_usec) * 1000;
#endif
        return delta;
}

void test_sched_yield()
{
        int64_t min = 0, max = 0, avg = 0, ttl_elapsed=0;
        int count = 1000, i =0;
        static struct timespec dur = {.tv_sec=0, .tv_nsec=200*1000}, rem = {.tv_sec=0, .tv_nsec=0};
        for (i = 0; i < count; i++)
        {
                get_start_time();
                sched_yield();
                get_stop_time();
                ttl_elapsed = get_elapsed_time();
                min = ((min == 0)? (ttl_elapsed): (ttl_elapsed < min ? (ttl_elapsed): (min)));
                max = ((ttl_elapsed > max) ? (ttl_elapsed):(max));
                avg += ttl_elapsed;
                //printf("Run latency: %li ns\n", delta);
        }
        printf("SCHED_YIELD() Min: %li, Max:%li and Avg latency: %li ns\n", min, max, avg/count);
        /* Remember the Mix, Max Avg include the overheads of time related calls: so substract the clock overheads as in test_clk_overhead() */
}

void test_nanosleep()
{
        int64_t min = 0, max = 0, avg = 0, ttl_elapsed=0;
        int count = 1000, i =0;
        static struct timespec dur = {.tv_sec=0, .tv_nsec=200*1000}, rem = {.tv_sec=0, .tv_nsec=0};
        for (i = 0; i < count; i++)
        {
                get_start_time();
                //nanosleep(&dur, &rem);
                clock_nanosleep(CLOCK_MONOTONIC, 0,  &dur, &rem);      // TIMER_ABSTIME
                get_stop_time();
                ttl_elapsed = get_elapsed_time();
                min = ((min == 0)? (ttl_elapsed): (ttl_elapsed < min ? (ttl_elapsed): (min)));
                max = ((ttl_elapsed > max) ? (ttl_elapsed):(max));
                avg += ttl_elapsed;
                //printf("Run latency: %li ns\n", delta);
        }
        printf("CLOCK_NANOSLEEP(): Min: %li, Max:%li and Avg latency: %li ns\n", min, max, avg/count);
        /* Remember the Mix, Max Avg include the Time specified for sleep: so substract these to arrive at call latency and avg oobtained for clock overheads as in test_clk_overhead() */
        
}

void test_clk_overhead()
{
        int64_t min = 0, max = 0, avg = 0, ttl_elapsed=0;
        int count = 1000, i =0;
        
        #ifdef HAS_CLOCK_GETTIME_MONOTONIC
          struct timespec str, stp;
        #else
          struct timeval str, stp;
        #endif
        
        get_cur_time(&str);
        for ( i = 0; i < count; i++) {
                get_start_time();
                get_stop_time();
                ttl_elapsed = get_elapsed_time();
                min = ((min == 0)? (ttl_elapsed): (ttl_elapsed < min ? (ttl_elapsed): (min)));
                max = ((ttl_elapsed > max) ? (ttl_elapsed):(max));
                avg += ttl_elapsed;
                //printf("Run latency: %li ns\n", delta);
        }
        get_cur_time(&stp);
        int64_t ttl = get_ttl_time(str,stp);
        delta = ttl/count;
        //printf("Run latency: %li ns\n", delta);
        printf("CLOCK_MEASUREMENT_OVERHEADS: Min: %li, Max:%li and Avg latency: %li , Overall Time: %li ns\n", min, max, avg/count, delta);
}

void test_mq()

{
        static char msg_t[256] = "\0";
        static unsigned long int msg_prio=0;
        //struct timespec timeout = {.tv_sec=0, .tv_nsec=1000};
        static ssize_t rmsg_len = 0;
        static mqd_t mutex;
        struct mq_attr attr = {.mq_flags=0, .mq_maxmsg=1050, .mq_msgsize=sizeof(int), .mq_curmsgs=0};
        mutex = mq_open("/mq_test", O_CREAT|O_RDWR, 0666, &attr);
        if (0 > mutex) {
                perror("Unable to open mqd");
                exit(1);
        }
        
        int64_t min = 0, max = 0, avg = 0, ttl_elapsed=0;
        int count = 1000, i =0;
        static int msg = '\0';
        // prepopulate the messages
        for ( i = 0; i < count; i++) {
                get_start_time();
                msg = mq_send(mutex, (const char*) &msg,0,0);
                get_stop_time();
                ttl_elapsed = get_elapsed_time();
                min = ((min == 0)? (ttl_elapsed): (ttl_elapsed < min ? (ttl_elapsed): (min)));
                max = ((ttl_elapsed > max) ? (ttl_elapsed):(max));
                avg += ttl_elapsed;

                //printf("Run latency: %li ns\n", delta);
        }
        printf("MQ_SEND(): Min: %li, Max:%li and Avg latency: %li ns\n", min, max, avg/count);
        avg = 0;
        //Now extract the messages one by one
        for ( i = 0; i < count; i++) {

                get_start_time();
                rmsg_len = mq_receive(mutex, msg_t,sizeof(msg_t), (unsigned int *)&msg_prio);
                get_stop_time();

                ttl_elapsed = get_elapsed_time();
                min = ((min == 0)? (ttl_elapsed): (ttl_elapsed < min ? (ttl_elapsed): (min)));
                max = ((ttl_elapsed > max) ? (ttl_elapsed):(max));
                avg += ttl_elapsed;

                //printf("Run latency: %li ns\n", delta);
                
        }
        printf("MQ_RECEIVE(): Min: %li, Max:%li and Avg latency: %li ns\n", min, max, avg/count);
        /* Remember the Mix, Max Avg include the overheads of time related calls: so substract the clock overheads as in test_clk_overhead() */
}

void test_mq_2()
{
        static char msg_t[256] = "\0";
        static unsigned long int msg_prio=0;
        //struct timespec timeout = {.tv_sec=0, .tv_nsec=1000};
        static ssize_t rmsg_len = 0;
        static mqd_t mutex;
        struct mq_attr attr = {.mq_flags=0, .mq_maxmsg=1050, .mq_msgsize=sizeof(int), .mq_curmsgs=0};
        mutex = mq_open("/mq_test", O_CREAT|O_RDWR, 0666, &attr);
        if (0 > mutex) {
                perror("Unable to open mqd");
                exit(1);
        }
        
        int64_t min = 0, max = 0, avg = 0, ttl_elapsed=0;
        int count = 1000, i =0;
        static int msg = '\0';
        pid_t pid = fork();
        if (pid > 0) {
                // prepopulate the messages
                for ( i = 0; i < count; i++) {
                        get_start_time();
                        msg = mq_send(mutex, (const char*) &msg,0,0);
                        get_stop_time();
                        ttl_elapsed = get_elapsed_time();
                        min = ((min == 0)? (ttl_elapsed): (ttl_elapsed < min ? (ttl_elapsed): (min)));
                        max = ((ttl_elapsed > max) ? (ttl_elapsed):(max));
                        avg += ttl_elapsed;

                        //printf("Run latency: %li ns\n", delta);
                }
                printf("MQ_SEND(2): Min: %li, Max:%li and Avg latency: %li ns\n", min, max, avg/count);
                avg = 0;
        }
        else if (pid == 0) {
                //Now extract the messages one by one
                for ( i = 0; i < count; i++) {

                        get_start_time();
                        rmsg_len = mq_receive(mutex, msg_t,sizeof(msg_t), (unsigned int *)&msg_prio);
                        get_stop_time();

                        ttl_elapsed = get_elapsed_time();
                        min = ((min == 0)? (ttl_elapsed): (ttl_elapsed < min ? (ttl_elapsed): (min)));
                        max = ((ttl_elapsed > max) ? (ttl_elapsed):(max));
                        avg += ttl_elapsed;

                        //printf("Run latency: %li ns\n", delta);
                        
                }
                printf("MQ_RECEIVE(2): Min: %li, Max:%li and Avg latency: %li ns\n", min, max, avg/count);
        }
        /* Remember the Mix, Max Avg include the overheads of time related calls: so substract the clock overheads as in test_clk_overhead() */
}
void test_sem()

{
        //struct timespec timeout = {.tv_sec=0, .tv_nsec=1000};
        sem_t* mutex = sem_open("/sem_test", O_CREAT, 0666, 0);
        if (0 > mutex) {
                perror("Unable to open mqd");
                exit(1);
        }
        
        int64_t min = 0, max = 0, avg = 0, ttl_elapsed=0;
        int count = 1000, i =0;
        
        // prepopulate the semaphore tokens
        for ( i = 0; i < count; i++) {
                get_start_time();
                sem_post(mutex);
                get_stop_time();
                ttl_elapsed = get_elapsed_time();
                min = ((min == 0)? (ttl_elapsed): (ttl_elapsed < min ? (ttl_elapsed): (min)));
                max = ((ttl_elapsed > max) ? (ttl_elapsed):(max));
                avg += ttl_elapsed;

                //printf("Run latency: %li ns\n", delta);
        }
        printf("SEM_POST(): Min: %li, Max:%li and Avg latency: %li ns\n", min, max, avg/count);
        avg = 0;
        //Now extract the tokens one by one
        for ( i = 0; i < count; i++) {

                get_start_time();
                sem_wait(mutex);
                get_stop_time();

                ttl_elapsed = get_elapsed_time();
                min = ((min == 0)? (ttl_elapsed): (ttl_elapsed < min ? (ttl_elapsed): (min)));
                max = ((ttl_elapsed > max) ? (ttl_elapsed):(max));
                avg += ttl_elapsed;

                //printf("Run latency: %li ns\n", delta);
                
        }
        printf("SEM_WAIT(): Min: %li, Max:%li and Avg latency: %li ns\n", min, max, avg/count);
        /* Remember the Mix, Max Avg include the overheads of time related calls: so substract the clock overheads as in test_clk_overhead() */
}


void test_sem_2()

{
        //struct timespec timeout = {.tv_sec=0, .tv_nsec=1000};
        sem_t* mutex = sem_open("/sem_test", O_CREAT, 0666, 0);
        if (0 > mutex) {
                perror("Unable to open mqd");
                exit(1);
        }
        
        int64_t min = 0, max = 0, avg = 0, ttl_elapsed=0;
        int count = 1000, i =0;
        pid_t pid = fork();
        if (pid > 0) {
                // prepopulate the semaphore tokens
                for ( i = 0; i < count; i++) {
                        get_start_time();
                        sem_post(mutex);
                        get_stop_time();
                        ttl_elapsed = get_elapsed_time();
                        min = ((min == 0)? (ttl_elapsed): (ttl_elapsed < min ? (ttl_elapsed): (min)));
                        max = ((ttl_elapsed > max) ? (ttl_elapsed):(max));
                        avg += ttl_elapsed;

                        //printf("Run latency: %li ns\n", delta);
                }
                printf("SEM_POST(2): Min: %li, Max:%li and Avg latency: %li ns\n", min, max, avg/count);
                avg = 0;
        }
        else if (pid == 0){
                //Now extract the tokens one by one
                for ( i = 0; i < count; i++) {

                        get_start_time();
                        sem_wait(mutex);
                        get_stop_time();

                        ttl_elapsed = get_elapsed_time();
                        min = ((min == 0)? (ttl_elapsed): (ttl_elapsed < min ? (ttl_elapsed): (min)));
                        max = ((ttl_elapsed > max) ? (ttl_elapsed):(max));
                        avg += ttl_elapsed;

                        //printf("Run latency: %li ns\n", delta);
                        
                }
                printf("SEM_WAIT(2): Min: %li, Max:%li and Avg latency: %li ns\n", min, max, avg/count);
        }
        /* Remember the Mix, Max Avg include the overheads of time related calls: so substract the clock overheads as in test_clk_overhead() */
}

#define MP_CLIENT_CGROUP_SET_CPU_SHARE_ONVM_MGR "/sys/fs/cgroup/cpu/nf_%u/cpu.shares"
void cgroup_update1(void) {
        FILE *fp = NULL;
        uint32_t shared_bw_val = 10240;  //when share_val is absolute bandwidth
        static char buffer[sizeof(MP_CLIENT_CGROUP_SET_CPU_SHARE_ONVM_MGR) + 20];
        snprintf(buffer, sizeof(buffer) - 1, MP_CLIENT_CGROUP_SET_CPU_SHARE_ONVM_MGR, 1);
        const char* cg_set_cmd = buffer;

        fp = fopen(cg_set_cmd, "w"); //optimize with mmap if that is allowed!!
        if (fp){
                fprintf(fp,"%d",shared_bw_val);
                fclose(fp);
        }
        return ;
}

void test_cgroup_update1() {
        int64_t min = 0, max = 0, avg = 0, ttl_elapsed=0;
        int count = 1000, i =0;
        for ( i = 0; i < count; i++) {
                get_start_time();
                cgroup_update1();
                get_stop_time();
                ttl_elapsed = get_elapsed_time();
                min = ((min == 0)? (ttl_elapsed): (ttl_elapsed < min ? (ttl_elapsed): (min)));
                max = ((ttl_elapsed > max) ? (ttl_elapsed):(max));
                avg += ttl_elapsed;

                //printf("Run latency: %li ns\n", delta);
        }
        printf("CGROUP_UPDATE(1): Min: %li, Max:%li and Avg latency: %li ns\n", min, max, avg/count);
}

#define MP_CLIENT_CGROUP_SET_CPU_SHARE "echo %u > /sys/fs/cgroup/cpu/nf_%u/cpu.shares"
void cgroup_update2(void) {
        FILE *fp = NULL;
        uint32_t shared_bw_val = 12420;  //when share_val is absolute bandwidth
        static char buffer[sizeof(MP_CLIENT_CGROUP_SET_CPU_SHARE) + 20];
        snprintf(buffer, sizeof(buffer) - 1, MP_CLIENT_CGROUP_SET_CPU_SHARE, shared_bw_val, 1);
        const char* cg_set_cmd = buffer;
        int ret = system(cg_set_cmd);
        return ;
}

void test_cgroup_update2() {
        int64_t min = 0, max = 0, avg = 0, ttl_elapsed=0;
        int count = 1000, i =0;
        for ( i = 0; i < count; i++) {
                get_start_time();
                cgroup_update2();
                get_stop_time();
                ttl_elapsed = get_elapsed_time();
                min = ((min == 0)? (ttl_elapsed): (ttl_elapsed < min ? (ttl_elapsed): (min)));
                max = ((ttl_elapsed > max) ? (ttl_elapsed):(max));
                avg += ttl_elapsed;
                //printf("Run latency: %li ns\n", delta);
        }
        printf("CGROUP_UPDATE(2): Min: %li, Max:%li and Avg latency: %li ns\n", min, max, avg/count);
}

void test_group_prio() {
        int my_proc_prio= getpriority(PRIO_PROCESS, 0);
        int my_grp_prio= getpriority(PRIO_PGRP, 0);
        int my_user_prio= getpriority(PRIO_USER, 0);
        printf("GetPriority: PROCESS [%d], GROUP [%d], USER [%d]\n", my_proc_prio,my_grp_prio,my_user_prio);
}
int main()
{
        #if defined(_POSIX_TIMERS) && (_POSIX_TIMERS > 0) && defined(_POSIX_MONOTONIC_CLOCK)
        printf("using POSIX MONOTONIC CLOCK \n");
        #else
        printf ("\n using Standard Time \n");
        #endif

        test_clk_overhead();
        test_cgroup_update1();
        test_cgroup_update2();
        test_mq();
        test_mq_2();
        test_sem();
        test_sem_2();
        test_nanosleep();
        test_sched_yield();
        test_group_prio();
}

