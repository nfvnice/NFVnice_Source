if [ -z $1 ] 
then
    num_flows=1
else
    num_flows=$1
fi

if [ -z $2 ] 
then
    pkt_size=60
else
    pkt_size=$2
fi
/home/skulk901/MoonGen/build/MoonGen /home/skulk901/MoonGen/examples/l3-load-latency.lua 0 0 -f $num_flows -s $pkt_size
