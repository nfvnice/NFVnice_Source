chain_len=$1
strt_from=$2
if [ -z $strt_from ]; then
strt_from=0
fi

mypwd=`pwd`
echo "mypwd=$mypwd"
if [ -z $chain_len ]
then
        chain_len=1
        seq_list=(2 4 6 8 10)
else
        seq_list=`seq $chain_len`
fi
echo "chain_len is set to $chain_len: $seq_list"
#for ch_len in `seq $chain_len`
for ch_len in ${seq_list[*]}
do
        if [ $ch_len -le $strt_from ] ; then
                echo "skipping chain $ch_len until $strt_from"
                continue
        fi
echo "Launching Script for Chain Len: $ch_len"
./script_var_chain_len_multi_core_updated.sh $ch_len
echo "Ended Script for Chain Len: $ch_len, Enter to continue.."
read text
tput clear
done
