if [ -z $1 ] 
then
    num_flows=2
else
    num_flows=$1
fi
/home/skulk901/MoonGen/build/MoonGen /home/skulk901/MoonGen/examples/l3-load-latency.lua 0 1 -f $num_flows
