ip route del default
ip route add default via 192.168.24.1 dev em1

echo "nameserver 169.235.30.11" >> /etc/resolv.conf
echo "nameserver 169.235.30.10" >> /etc/resolv.conf
echo "search cs.ucr.edu" >> /etc/resolv.conf
sudo resolvconf -u

