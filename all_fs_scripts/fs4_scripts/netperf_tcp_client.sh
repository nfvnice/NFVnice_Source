#netperf -L 10.0.0.2 -H 10.10.1.4 -t TCP_STREAM -v 10 -l 30 -D 5 -j
conf_level=$3
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
if [ -z $conf_level ] ; then
  netperf -H 10.0.0.2 -L 10.10.1.4 -t TCP_STREAM -v 10 -l $duration -D 5 -j
else
  netperf -H 10.0.0.2 -L 10.10.1.4 -t TCP_STREAM -v 10 -l $duration -D 5 -j
fi 

