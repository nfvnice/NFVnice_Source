duration=$1
num_streams=$2
bandwidth=$3
if [ -z $duration ]; then
  duration=10
fi

if [ -z $num_streams ]; then
  num_streams=1
fi

if [ -z $bandwidth ]; then
  bandwidth=10G
fi
echo "duration= $duration, num_streams = $num_streams, bandwidth=$bandwidth"
#iperf3 -c 10.0.0.2 -B 10.10.1.4 -P $num_streams -t $duration  -u -b 20480M 
iperf3 -c 10.0.0.2 -B 10.10.1.4 -P $num_streams -t $duration -u -b $bandwidth
