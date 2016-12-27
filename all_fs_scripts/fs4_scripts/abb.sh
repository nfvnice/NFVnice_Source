#$1=size <32k, 128k,... >
#$2=name <bkpr, default.. >
#./abb.sh 32k default
#mkdir results/ab_results/nov_22/buf_size_512_128/$1  && ./do_ab_test.sh 10240k $1 results/ab_results/nov_22/buf_size_512_128/$1
#sub_dir="nov_27"

sub_dir=`date |  awk '{ print $2"_"$3}'`

if [ -z $1 ]; then
  fsize="32k"
else
  fsize=$1
fi

if [ -z $2 ]; then
  tname="default"
else
  tname=$2
fi

mkdir results/ab_results/$sub_dir/
mkdir results/ab_results/$sub_dir/buf_size_512_128
mkdir results/ab_results/$sub_dir/buf_size_512_128/${tname}_${fsize}
./do_ab_test.sh $fsize $tname results/ab_results/$sub_dir/buf_size_512_128/${tname}_${fsize}
