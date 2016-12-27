cong_algo=$3
num_streams=$2
duration=$1
if [ -z $duration ]; then
  duration=10
fi

if [ -z $num_streams ]; then
  num_streams=1
fi

# -N = NO_DELAY disable NAGLEs algo
# -R = reverse mode (server sends data to client) 
if [ -z $cong_algo ] ; then
  iperf3 -c 10.0.0.2 -B 10.10.1.4 -P $num_streams -t $duration -O 5 -i 0 
else
  iperf3 -c 10.0.0.2 -B 10.10.1.4 -P $num_streams -t $duration -O 5 -C $cong_algo -i 0
fi 

