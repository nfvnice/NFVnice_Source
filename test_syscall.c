#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
int main() {
int r = system("echo 100 > /sys/fs/cgroup/cpu/nf_1/cpu.shares");
printf("cpu_share=%d\n", r);
long hostid=gethostid();
printf("HostID=%u\n",hostid);
return r;
}

