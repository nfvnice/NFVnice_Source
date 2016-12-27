./iperf_tcp_client.sh 60 &
sleep 15
./nuttcp_tcp_client.sh 30 1
#./iperf_udp_client.sh 30 10 100M
