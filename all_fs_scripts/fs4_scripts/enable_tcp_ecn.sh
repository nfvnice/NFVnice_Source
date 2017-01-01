echo "CURRENT ECN STATUS"
cat /proc/sys/net/ipv4/tcp_ecn
sysctl net.ipv4.tcp_ecn

echo 1 > /proc/sys/net/ipv4/tcp_ecn
sysctl -w net.ipv4.tcp_ecn=1
#sysctl -a

echo "NEW ECN STATUS"
cat /proc/sys/net/ipv4/tcp_ecn
sysctl net.ipv4.tcp_ecn
