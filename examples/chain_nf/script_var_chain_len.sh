chain_len=$1
cost=$2

if [ -z $chain_len ]
then
        chain_len=1
fi
echo "chain_len is set to $chain_len"

if [ -z $cost ]
then
        cost=0
fi
echo "cost is set to $cost"

nf=1
while [ $nf -le $chain_len ]
do
	echo "Launching a9NF_${nf}.sh with cost=$cost"
        ./a9NF_${nf}.sh $cost &
        sleep 2
        nf=$((nf+1))
done


#ls -l a9* | awk '{ print "./"$9 "\ &"}'  > run_9schain.sh
#./a9NF_1.sh 1 &
#  sleep 2
#./a9NF_2.sh 2 &
#  sleep 2
#./a9NF_3.sh 3 &
#  sleep 2
#./a9NF_4.sh 1 &
#  sleep 2
#./a9NF_5.sh 2 &
#  sleep 2
#./a9NF_6.sh 3 &
#  sleep 2
#./a9NF_7.sh 1 &
#  sleep 2
#./a9NF_8.sh 2 &
#  sleep 2
#./a9NF_9.sh 3 &
#  sleep 2
