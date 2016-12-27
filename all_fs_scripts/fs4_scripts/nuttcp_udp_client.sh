duration=$1
num_streams=$2
if [ -z $duration ]; then
  duration=10
fi

if [ -z $num_streams ]; then
  num_streams=1
fi

control_port=6000
data_port=6001
tx_rate_limit="10g"

#nuttcp -t -b -u -v -P $control_port -p $data_port -R $tx_rate_limit -N $num_streams -T $duration 10.0.0.2
nuttcp -t -u -vvv -P $control_port -p $data_port -R $tx_rate_limit -N $num_streams -T $duration 10.0.0.2

