./3iperf_tcp_client.sh 60 &
sleep 15
./nuttcp_udp_client.sh 30 10
#./iperf_udp_client.sh 30 10 100M
