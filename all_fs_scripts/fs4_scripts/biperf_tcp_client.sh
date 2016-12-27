out_file=$3
num_streams=$2
duration=$1
if [ -z $duration ]; then
  duration=10
fi

if [ -z $num_streams ]; then
  num_streams=1
fi

# -i = interval for output
#-f = format Mbps, Gbps
# -y = report format in csv
# -o = outfile

if [ -z $out_file ]; then
  out_file=report_iperf_tcp_test.csv
fi


#!/bin/bash

for ((i = 0; i <=100; i++))
do
time=$(($RANDOM%5+1))
pause=$(($RANDOM%5+1))

echo `iperf -c 10.0.0.2 -B 10.10.1.4 -t $time` -P $num_streams | tee $out_file
sleep $pause

done



#working line
#iperf -c 10.0.0.2 -B 10.10.1.4 -p 5901 -P $num_streams -t $duration  -m -f m -y | tee $out_file

#iperf -c 10.0.0.2 -B 10.10.1.4 -p 5901 -P $num_streams -t $duration  -m -f g -i 1 -y | tee $out_file
#iperf -c 10.0.0.2 -B 10.10.1.4 -P $num_streams -t $duration  -m -f g | tee $out_file
#iperf -c 10.0.0.2 -B 10.10.1.4 -P $num_streams -t $duration  -m -f m -i 1 | tee $out_file
