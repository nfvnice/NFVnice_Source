fcount=2
if [ -n $2 ] ; then
  fcount=$2
fi
./ziperf_tcp_client.sh 60 $fcount | tee -a tcres_${1}.txt
#./ziperf_tcp_client.sh 60 | tee -a tcres_${1}.txt & 
