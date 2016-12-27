bandwidth=$3
out_file=$4
num_streams=$2
duration=$1
if [ -z $duration ]; then
  duration=10
fi

if [ -z $num_streams ]; then
  num_streams=1
fi

if [ -z $bandwidth ]; then
  bandwidth=10240M
  #bandwidth=1M
fi

#-i = interval for output
#-f = format Mbps, Gbps
#-y = report format in csv
#-o = outfile

if [ -z $out_file ]; then
  out_file=report_iperf_udp_test.csv
fi



iperf -c 10.0.0.2 -B 10.10.1.4 -P $num_streams -t $duration  -u -b $bandwidth -i 1
#iperf -c 10.0.0.2 -B 10.10.1.4 -P $num_streams -t $duration  -u -b 20G -i 1 -f g -y C -o $out_file
