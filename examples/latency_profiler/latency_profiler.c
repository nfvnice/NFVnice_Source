/*

 strace -c ./build/latency_profiler
 strace -T ./build/latency_profiler
 taskset 0x04 ./build/latency_profiler

*/
#define _GNU_SOURCE //O_DIRECT
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
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <errno.h>
#include <aio.h>
#include <signal.h>

#include <rte_common.h>
#include <rte_mbuf.h>
#include <rte_ip.h>
#include <rte_malloc.h>
#include <rte_memzone.h>
#include <rte_memory.h>
#include <rte_cycles.h>

#include <sys/time.h>
#include <sys/resource.h>
#include <getopt.h>
#include <assert.h>
#include <aio.h>

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

#define rte_calloc(a,b,c,d) calloc(b,c)
#define rte_free(a) free(a)

#define SEM_NAME "AIO_SYNC_VAR"
sem_t *wait_mutex = NULL;
//#define WAIT_FOR_ASYNC_COMPLETION

int initialize_sync_variable(void);
int deinitialize_sync_variable(void);
int wait_on_sync_variable(void);
int post_on_sync_variable(void);
int initialize_sync_variable(void) {
        wait_mutex = sem_open(SEM_NAME, O_CREAT, 06666, 0);
        if(wait_mutex == SEM_FAILED) {
                fprintf(stderr, "can not create semaphore!!\n");
                sem_unlink(SEM_NAME);
                exit(1);
        }
        return 0;
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
int wait_on_sync_variable(void) {
    if(wait_mutex) {
                sem_wait(wait_mutex);

        }
        return 0;
}
int post_on_sync_variable(void) {
        if(wait_mutex) {
                sem_post(wait_mutex);
        }
        return 0;
}

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

#define AIO_REQUEST_PRIO (0)
#define OFFSET_LIST_SIZE 10
static int offset_desc[OFFSET_LIST_SIZE] = { 24576, 4096, 8912, 12288, 16384, 20480, 24576, 28672, 16384, 32768 };
#define OFFSET_MULTIPLIER   (1024*1024*5)  //(1024*1024)   //(1)
int get_read_file_offset(void);
int get_read_file_offset(void) {
    static int offset_index = 0;
    offset_index +=1; 
    if (offset_index >= OFFSET_LIST_SIZE) offset_index = 0;
    return offset_desc[offset_index];
}
//#define FD_RD_OPEN_MODE (O_RDONLY|O_DIRECT|O_FSYNC)
//#define FD_RD_OPEN_MODE (O_RDONLY|O_DIRECT) // (O_RDONLY|O_FSYNC)
//#define FD_RD_OPEN_MODE (O_RDONLY|O_FSYNC) // (O_RDONLY|O_FSYNC)
//#define FD_RD_OPEN_MODE (O_RDONLY) // (O_RDONLY|O_FSYNC)
#define FD_RD_OPEN_MODE (O_RDWR|O_FSYNC|O_RSYNC|O_DIRECT)
#define IO_BUF_SIZE 4096

#define USE_SYNC_PREAD
//#define DO_WRITE_BACK

#define MAX_READ_FLAG_OPTIONS (5)
int read_flag_options_list[MAX_READ_FLAG_OPTIONS] = {
        O_RDWR,
        (O_RDWR|O_FSYNC),
        (O_RDWR|O_DIRECT),
        (O_RDWR|O_DIRECT|O_FSYNC),
        (O_RDWR|O_FSYNC|O_RSYNC|O_DIRECT)
};
int get_next_read_flag(int index);
int get_next_read_flag(int index) {
        index = (index % (MAX_READ_FLAG_OPTIONS));
        return read_flag_options_list[index];
}

void test_sync_io_read(void) {
        int ret = 0;
        int64_t min = 0, max = 0, avg = 0, ttl_elapsed=0;
        int count = 1000, i =0;
        static struct timespec dur = {.tv_sec=0, .tv_nsec=200*1000}, rem = {.tv_sec=0, .tv_nsec=0};
        
        int open_flag = FD_RD_OPEN_MODE;
        int cur_open_options = 0;
        do {
                open_flag = get_next_read_flag(cur_open_options++);
                int fd = open("logger_pkt.txt", open_flag, 0666);
                if (-1 == fd) {
                    return ;
                }
                size_t aio_nbytes=IO_BUF_SIZE;
                __off_t aio_offset = 0;
                void* buf = rte_calloc("log_pktbuf_buf", IO_BUF_SIZE, sizeof(uint8_t),0);
                for (i = 0; i < count; i++)
                {
                        aio_offset = get_read_file_offset();
                        get_start_time();
                        #ifdef USE_SYNC_PREAD
                        ret = pread(fd, buf, aio_nbytes, aio_offset);
                        #else
                        ret = lseek(fd, aio_offset, SEEK_SET);
                        ret = read(fd, buf, aio_nbytes);
                        #endif
                        #ifdef DO_WRITE_BACK
                        if(pwrite(fd,buf, aio_nbytes, aio_offset)) {};
                        #endif
                        get_stop_time();
                        ttl_elapsed = get_elapsed_time();
                        min = ((min == 0)? (ttl_elapsed): (ttl_elapsed < min ? (ttl_elapsed): (min)));
                        max = ((ttl_elapsed > max) ? (ttl_elapsed):(max));
                        avg += ttl_elapsed;
                        //printf("Run latency: %li ns\n", delta);
                }
                close(fd);
                printf("sync_io_read(%d:%d) Min: %li, Max:%li and Avg latency: %li ns\n", cur_open_options, open_flag, min, max, avg/count);
                /* Remember the Mix, Max Avg include the overheads of time related calls: so substract the clock overheads as in test_clk_overhead() */
        } while( cur_open_options < MAX_READ_FLAG_OPTIONS);
}

//#define FD_WR_OPEN_MODE (O_RDWR|O_DIRECT|O_FSYNC)
//#define FD_WR_OPEN_MODE (O_RDWR|O_DIRECT) // (O_RDWR|O_FSYNC)
//#define FD_WR_OPEN_MODE (O_RDWR|O_FSYNC) // (O_RDWR|O_FSYNC)
//#define FD_WR_OPEN_MODE (O_RDWR) // (O_RDWR|O_FSYNC)
#define FD_WR_OPEN_MODE (O_RDWR|O_FSYNC|O_RSYNC|O_DIRECT)
#define MAX_WRITE_FLAG_OPTIONS (5)
int write_flag_options_list[MAX_WRITE_FLAG_OPTIONS] = {
        O_RDWR,
        (O_RDWR|O_FSYNC),
        (O_RDWR|O_DIRECT),
        (O_RDWR|O_DIRECT|O_FSYNC),
        (O_RDWR|O_FSYNC|O_RSYNC|O_DIRECT)
};
int get_next_write_flag(int index);
int get_next_write_flag(int index) {
        index = (index % (MAX_WRITE_FLAG_OPTIONS));
        return write_flag_options_list[index];
}


#define IO_BUF_SIZE 4096
void test_sync_io_write(void) {
        int ret = 0;
        int64_t min = 0, max = 0, avg = 0, ttl_elapsed=0;
        int count = 1000, i =0;
        static struct timespec dur = {.tv_sec=0, .tv_nsec=200*1000}, rem = {.tv_sec=0, .tv_nsec=0};
        
        int open_flag = FD_WR_OPEN_MODE;
        int cur_open_options = 0;
        do {
                open_flag = get_next_write_flag(cur_open_options++);
                int fd = open("logger_pkt.txt", open_flag, 0666);
                if (-1 == fd) {
                    return ;
                }
                size_t aio_nbytes=IO_BUF_SIZE;
                __off_t aio_offset = 0;
                void* buf = rte_calloc("log_pktbuf_buf", IO_BUF_SIZE, sizeof(uint8_t),0);
                for (i = 0; i < count; i++)
                {
                        aio_offset = get_read_file_offset();
                        get_start_time();
                        #ifdef USE_SYNC_PREAD
                        ret = pwrite(fd, buf, aio_nbytes, aio_offset);
                        #else
                        ret = lseek(fd, aio_offset, SEEK_SET);
                        ret = write(fd, buf, aio_nbytes);
                        #endif
                        get_stop_time();
                        ttl_elapsed = get_elapsed_time();
                        min = ((min == 0)? (ttl_elapsed): (ttl_elapsed < min ? (ttl_elapsed): (min)));
                        max = ((ttl_elapsed > max) ? (ttl_elapsed):(max));
                        avg += ttl_elapsed;
                        //printf("Run latency: %li ns\n", delta);
                }
                close(fd);
                printf("sync_io_write(%d:%d) Min: %li, Max:%li and Avg latency: %li ns\n", cur_open_options, open_flag, min, max, avg/count);
                /* Remember the Mix, Max Avg include the overheads of time related calls: so substract the clock overheads as in test_clk_overhead() */
        } while(cur_open_options < MAX_WRITE_FLAG_OPTIONS);
}

#define MAX_AIO_BUFFERS (5)
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
        //struct rte_mbuf *pkt;    // pkt associated with the aio_buf for the read case
        struct aiocb *aiocb;    // <allocated at initialization and updated for each read/write>
        int req_status;         // <allocated at initialization and updated for each read/write>
}aio_buf_t;
#ifdef DYNAMIC_RTE_POOL
static aio_buf_t *aio_buf_pool = NULL;
#else
static aio_buf_t aio_buf_pool[MAX_AIO_BUFFERS];    
#endif
static int aio_fd = 0;
int initialize_aio_buffers (void);
int deinitialize_aio_buffers(void);
int initialize_aiocb(aio_buf_t *pbuf);
aio_buf_t* get_aio_buffer_from_aio_buf_pool(uint32_t aio_operation_mode);
int notify_io_rw_done(aio_buf_t *pbuf);
static void ioSigHandler(sigval_t sigval);
int refresh_aio_buffer(aio_buf_t *pbuf);
int refresh_aio_buffer(aio_buf_t *pbuf) {
        int ret = 0;
        //pbuf->aiocb->aio_nbytes = (size_t)0;
        //pbuf->aiocb->aio_offset = (__off_t)0;
        //pbuf->buf_len=0;
        //pbuf->pkt = NULL;
        pbuf->state = BUF_FREE;
        #ifdef ENABLE_DEBUG_LOGS
        printf("\n Buffer [%p] state moved to FREE [%d]\n",pbuf, pbuf->state);
        #endif //ENABLE_DEBUG_LOGS
        return ret;
}

static void ioSigHandler(sigval_t sigval) {
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

int notify_io_rw_done(aio_buf_t *pbuf) {

        #ifdef ENABLE_DEBUG_LOGS
        int req_status = aio_error(pbuf->aiocb);       
        if(0 != req_status) {
                printf("\n Aio_read/write completed with error [ %d]\n", req_status);
        }
        else {
                printf("Aio_read/write completed Successfully [%d]!!\n", req_status);
        }
        #endif //#ifdef ENABLE_DEBUG_LOGS
        refresh_aio_buffer(pbuf);
        post_on_sync_variable();
        return 0;
}

int
initialize_aiocb(aio_buf_t *pbuf) {
       pbuf->aiocb->aio_buf     = (volatile void*)pbuf->buf;
       pbuf->aiocb->aio_fildes  = aio_fd;
       pbuf->aiocb->aio_nbytes  = pbuf->buf_len;
       pbuf->aiocb->aio_reqprio = AIO_REQUEST_PRIO;
       pbuf->aiocb->aio_offset  = 0;
       pbuf->aiocb->aio_sigevent.sigev_notify          = SIGEV_THREAD;
       pbuf->aiocb->aio_sigevent.sigev_notify_function = ioSigHandler;
       pbuf->aiocb->aio_sigevent.sigev_value.sival_ptr = pbuf;

       return 0;
}
int initialize_aio_buffers (void) {
        int ret = 0;
        #ifdef DYNAMIC_RTE_POOL
        if(aio_buf_pool) {
                #ifdef ENABLE_DEBUG_LOGS
                printf("Already Allocated!!\n");
                #endif //ENABLE_DEBUG_LOGS
                return -1;
        }
        #endif
        
        uint8_t i = 0;
        uint32_t alloc_buf_size = IO_BUF_SIZE;
        
        #ifdef DYNAMIC_RTE_POOL
        aio_buf_pool = rte_calloc("log_pktbuf_pool", MAX_AIO_BUFFERS, sizeof(*aio_buf_pool),0);
        if(NULL == aio_buf_pool) {
                rte_exit(EXIT_FAILURE, "Cannot allocate memory for log_pktbuf_pool\n");
        }
        #endif
        
        for(i=0; i< MAX_AIO_BUFFERS; i++) {
                alloc_buf_size              = IO_BUF_SIZE;
                aio_buf_pool[i].buf         = rte_calloc("log_pktbuf_buf", alloc_buf_size, sizeof(uint8_t),0);
                aio_buf_pool[i].aiocb       = rte_calloc("log_pktbuf_aio", 1, sizeof(struct aiocb),0);
                aio_buf_pool[i].buf_index   = i;
                //aio_buf_pool[i].pkt         = NULL;
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

int deinitialize_aio_buffers(void) {
        uint8_t i = 0;
        int ret = 0;
        if(aio_buf_pool) {
                for(i=0; i< MAX_AIO_BUFFERS; i++) {
                        //Address aio_cancel for pending requests and then free
                        //ret = deinitialize_aiocb(&aio_buf_pool[i]);
                        rte_free(aio_buf_pool[i].aiocb);
                        aio_buf_pool[i].aiocb = NULL;
                        rte_free(aio_buf_pool[i].buf);
                        aio_buf_pool[i].buf = NULL;
                }
                #ifdef DYNAMIC_RTE_POOL
                rte_free(aio_buf_pool);
                aio_buf_pool = NULL;
                #endif
        }
        return ret;
}
aio_buf_t* get_aio_buffer_from_aio_buf_pool(uint32_t aio_operation_mode) {
    if (0 == aio_operation_mode) {
                uint32_t i = 0;
                for (i=0; i < MAX_AIO_BUFFERS; i++) {
                        if (BUF_FREE == aio_buf_pool[i].state) {
                            //globals.cur_buf_index = i;
                            aio_buf_pool[i].state = BUF_IN_USE;
                            return &(aio_buf_pool[i]);
                        }
                }
        }
        return NULL;
}

void test_async_io_read(void) {
        int ret = 0;
        int64_t min = 0, max = 0, avg = 0, ttl_elapsed=0;
        int count = 1000, i =0;
        static struct timespec dur = {.tv_sec=0, .tv_nsec=200*1000}, rem = {.tv_sec=0, .tv_nsec=0};
        int open_flag = FD_RD_OPEN_MODE;
        int cur_open_options = 0;
        do {
                open_flag = get_next_read_flag(cur_open_options++);
                aio_fd = open("logger_pkt.txt", open_flag, 0666);
                if (-1 == aio_fd) {
                    return;
                }
                size_t aio_nbytes=IO_BUF_SIZE;
                if(initialize_aio_buffers()) {
                       // return ;
                }
                
                __off_t aio_offset = 0;
                aio_buf_t* pbuf = NULL;
                for (i = 0; i < count; i++)
                {
                        do { 
                                pbuf = get_aio_buffer_from_aio_buf_pool(0);
                                if(NULL == pbuf) {
                                    wait_on_sync_variable();
                                }
                        }while(pbuf == NULL);
                        aio_offset = get_read_file_offset();
                        
                        pbuf->aiocb->aio_offset = aio_offset;
                        pbuf->aiocb->aio_nbytes = aio_nbytes;
                        get_start_time();
                        ret = aio_read(pbuf->aiocb);
                        if(-1 ==ret) {
                                printf("Error at aio_read(): %s\n", strerror(errno));
                                refresh_aio_buffer(pbuf);
                                exit(1);
                        }
                        #ifdef WAIT_FOR_ASYNC_COMPLETION
                        while ((ret = aio_error (pbuf->aiocb)) == EINPROGRESS);
                        #endif
                        get_stop_time();
                        //refresh_aio_buffer(pbuf);
                        ttl_elapsed = get_elapsed_time();
                        min = ((min == 0)? (ttl_elapsed): (ttl_elapsed < min ? (ttl_elapsed): (min)));
                        max = ((ttl_elapsed > max) ? (ttl_elapsed):(max));
                        avg += ttl_elapsed;
                        //printf("Run latency: %li ns\n", delta);
                }
                close(aio_fd);
                deinitialize_aio_buffers();
                printf("Async_io_read(%d:%d) Min: %li, Max:%li and Avg latency: %li ns\n", cur_open_options, open_flag, min, max, avg/count);
                /* Remember the Mix, Max Avg include the overheads of time related calls: so substract the clock overheads as in test_clk_overhead() */
        } while(cur_open_options < MAX_READ_FLAG_OPTIONS);
}

void test_async_io_write(void) {
        int ret = 0;
        int64_t min = 0, max = 0, avg = 0, ttl_elapsed=0;
        int count = 1000, i =0;
        static struct timespec dur = {.tv_sec=0, .tv_nsec=200*1000}, rem = {.tv_sec=0, .tv_nsec=0};
        int open_flag = FD_WR_OPEN_MODE;
        int cur_open_options = 0;
        do {
                open_flag = get_next_write_flag(cur_open_options++);
                aio_fd = open("logger_wpkt.txt", open_flag, 0666);
                if (-1 == aio_fd) {
                    return;
                }
                size_t aio_nbytes=IO_BUF_SIZE;
                if(initialize_aio_buffers()) {
                        return;
                }
                
                __off_t aio_offset = 0;
                aio_buf_t* pbuf = NULL;
                for (i = 0; i < count; i++)
                {
                        do { 
                                pbuf = get_aio_buffer_from_aio_buf_pool(0);
                                if(NULL == pbuf) {
                                    wait_on_sync_variable();
                                }
                        }while(pbuf == NULL);
                        
                        aio_offset = get_read_file_offset();
                        
                        pbuf->aiocb->aio_offset = aio_offset;
                        pbuf->aiocb->aio_nbytes = aio_nbytes;
                        get_start_time();
                        ret = aio_write(pbuf->aiocb);
                        if(-1 ==ret) {
                                printf("Error at aio_read(): %s\n", strerror(errno));
                                refresh_aio_buffer(pbuf);
                                exit(1);
                        }
                        #ifdef WAIT_FOR_ASYNC_COMPLETION
                        while ((ret = aio_error (pbuf->aiocb)) == EINPROGRESS);
                        #endif
                        get_stop_time();
                        //refresh_aio_buffer(pbuf);
                        ttl_elapsed = get_elapsed_time();
                        min = ((min == 0)? (ttl_elapsed): (ttl_elapsed < min ? (ttl_elapsed): (min)));
                        max = ((ttl_elapsed > max) ? (ttl_elapsed):(max));
                        avg += ttl_elapsed;
                        //printf("Run latency: %li ns\n", delta);
                }
                close(aio_fd);
                deinitialize_aio_buffers();
                printf("Async_io_write(%d:%d) Min: %li, Max:%li and Avg latency: %li ns\n", cur_open_options, open_flag, min, max, avg/count);
                /* Remember the Mix, Max Avg include the overheads of time related calls: so substract the clock overheads as in test_clk_overhead() */
        } while(cur_open_options < MAX_WRITE_FLAG_OPTIONS);
}

#include <signal.h>
#include <sys/types.h>  /* Type definitions used by many programs */
#include <stdio.h>      /* Standard I/O functions */
#include <stdlib.h>     /* Prototypes of commonly used library functions,
                           plus EXIT_SUCCESS and EXIT_FAILURE constants */
#include <unistd.h>     /* Prototypes for many system calls */
#include <errno.h>      /* Declares errno and defines error constants */
#include <string.h>     /* Commonly used string-handling functions */
#define errExit(a) { printf(a); exit(1);}
#define TESTSIG SIGUSR1

static void
handler(int sig)
{
        static int64_t min = 0, max = 0, avg = 0, ttl_elapsed=0, count=0;
        get_stop_time();
        ttl_elapsed = get_elapsed_time();
        min = ((min == 0)? (ttl_elapsed): (ttl_elapsed < min ? (ttl_elapsed): (min)));
        max = ((ttl_elapsed > max) ? (ttl_elapsed):(max));
        avg += ttl_elapsed;
        count++;
        if(count%10 == 0) {
                printf("SIGNAL_HANDLER_RECV(%d): Min: %li, Max:%li and Avg latency: %li ns\n", getpid(), min, max, avg/count);
        }
}

int test_signal_latency()
{
    int64_t min = 0, max = 0, avg = 0, ttl_elapsed=0;
    int count = 1000, i =0;
                
    int numSigs, scnt;
    pid_t childPid;
    sigset_t blockedMask, emptyMask;
    struct sigaction sa;
    int sig_r;

    /*if (argc != 2 || strcmp(argv[1], "--help") == 0)
        usageErr("%s num-sigs\n", argv[0]);
    */

    numSigs = 10; //1000; //getInt(argv[1], GN_GT_0, "num-sigs");

    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sa.sa_handler = handler;
    if (sigaction(TESTSIG, &sa, NULL) == -1)
        errExit("sigaction");

    /* Block the signal before fork(), so that the child doesn't manage
       to send it to the parent before the parent is ready to catch it */

    sigemptyset(&blockedMask);
    sigaddset(&blockedMask, TESTSIG);
    if (sigprocmask(SIG_SETMASK, &blockedMask, NULL) == -1)
        errExit("sigprocmask");

    sigemptyset(&emptyMask);

    switch (childPid = fork()) {
    case -1: errExit("fork");

    case 0:     /* child */
        for (scnt = 0; scnt < numSigs; scnt++) {
             get_start_time();    

            if (kill(getppid(), TESTSIG) == -1)
                errExit("kill");
            if (sigsuspend(&emptyMask) == -1 && errno != EINTR)
            //if (sigwait(&blockedMask, &sig_r) == -1 && errno != EINTR)
                    errExit("sigsuspend");
        }
        exit(EXIT_SUCCESS);

    default: /* parent */
        for (scnt = 0; scnt < numSigs; scnt++) {
            //if (sigwait(&blockedMask, &sig_r) == -1 && errno != EINTR)
            if (sigsuspend(&emptyMask) == -1 && errno != EINTR)
                    errExit("sigsuspend");
            
            get_start_time();

            if (kill(childPid, TESTSIG) == -1)
                errExit("kill");
        }
        exit(EXIT_SUCCESS);
    }
}
int main()
{
        #if defined(_POSIX_TIMERS) && (_POSIX_TIMERS > 0) && defined(_POSIX_MONOTONIC_CLOCK)
        printf("using POSIX MONOTONIC CLOCK \n");
        #else
        printf ("\n using Standard Time \n");
        #endif

        #if 1
        test_clk_overhead();
        //#if 0        
        test_cgroup_update1();
        test_cgroup_update2();
        test_mq();
        test_mq_2();
        test_sem();
        test_sem_2();
        test_nanosleep();
        test_sched_yield();
        test_group_prio();
        //#endif
        test_signal_latency();
        #endif
       
#if 0
        initialize_sync_variable();
        test_sync_io_write();
        test_sync_io_read();
        test_async_io_write();
        test_async_io_read();
        sleep(5);
        deinitialize_sync_variable();
#endif
}

