#!/bin/bash
testcase=$1
if [ -z $testcase ] 
then
        testcase=1;
fi

mypwd=`pwd`
do_start_flow_rule_installer () {
cd ../flow_rule_installer
rm gfc_out.txt
#./gfc1.sh &
./gfc1.sh 2>&1 >> gfc_out.txt &
sleep 5
pid=`pgrep -a flow_rule_inst | awk '{print $1}'`
sudo kill -9 $pid
cd $mypwd
}

#Two chains: Chain 1: SVC1(cost=1)--> SVC2(cost=0)-->Bridge and Chain2: SVC1(cost=1)-->SVC3(cost=5)-->Bridge

do_launch_test () {

do_start_flow_rule_installer

cost1=1
cost2=0
cost3=5

#For SVC1
core_id=8
svc_id=2
dst_id=3
inst_id=2
cost=1
./og.sh $core_id $svc_id $dst_id $inst_id $cost &
  sleep 2

#For SVC2
core_id=9
svc_id=3
dst_id=4
#inst_id=6
inst_id=8
cost=6
cost=2
./og.sh $core_id $svc_id $dst_id $inst_id $cost2 &
  sleep 2

#For SVC3
core_id=10
svc_id=4
dst_id=5
#inst_id=10
inst_id=15
cost=8
cost=3
./og.sh $core_id $svc_id $dst_id $inst_id $cost3 &
  sleep 2

#For SVC4 ( Bridge or Basic Monitor)
core_id=11
svc_id=5
dst_id=6
#inst_id=14
inst_id=22
cost=8
cost=3
cd ../basic_monitor
rm monitor_out.txt
echo "Starting Basic Monitor on core: $core_id service id: $svc_id, InstanceId: $inst_id"
./lsc_x.sh $core_id $svc_id 2>&1 >> monitor_out.txt &
sleep 2

}

do_launch_test

