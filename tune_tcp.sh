sysctl -w net.ipv4.tcp_window_scaling=1
sysctl -w net.ipv4.tcp_rmem=4096  87830  16777216
sysctl -w net.ipv4.tcp_wmem=4096  87830  16777216
sysctl -w net.ipv4.tcp_slow_start_after_idle=0
