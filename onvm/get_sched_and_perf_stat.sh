#!/bin/bash
#usage: ./get_sched_and_perf_stat.sh >out.txt 2>&1

for sched in c b r f
do
        echo "****************************************************"
        echo "<START> For Scheduler type = $sched   <START> "
        ./set_sched_type.sh $sched
        #pidstat -C "aes|bridge|forward|monitor|basic|speed|perf|pidstat" -lrsuwh 1 5 &        
        pidstat -C "aes|bridge|forward|monitor|basic|speed|flow|chain" -lrsuwh 1 5 &        
        perf stat --cpu=8  -d -d -d -r 10 sleep 1
        sleep 3
        echo " <END> For Scheduler type = $sched  <END>"
        sleep 2
        echo "****************************************************"
done
        echo "reset to CFS Scheduler"
        ./set_sched_type.sh c
