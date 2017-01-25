#!/bin/bash
testcase=$1
if [ -z $testcase ] then
        testcase=1
fi

#ls -l a9* | awk '{ print "./"$9 "\ &"}'  > run_9schain.sh

do_launch_test () {
cost1=0
cost2=2
cost3=3
        case $testcase in
        "1")    #"lmh"
                cost1=0;
                cost2=2;
                cost3=3;
        ;;
        "2")    #"lhm"
                cost1=0;
                cost2=3;
                cost3=2;
        ;;
        "3")    #"mlh"
                cost1=2;
                cost2=0;
                cost3=3;
        ;;
        "4")    #"mhl"
                cost1=2;
                cost2=3;
                cost3=0;
        ;;
        "5")    #"hlm"
                cost1=3;
                cost2=0;
                cost3=2;
        ;;
        "6")    #"hml"
                cost1=3;
                cost2=2;
                cost3=0;
        ;;
        esac
#Reset all to MMM
cost1=1
cost2=2
cost3=3
core1=8
core2=9
core3=10
#For SVC1
core_id=8
svc_id=2
dst_id=3
inst_id=2


./og.sh $core1 $svc_id $dst_id $inst_id $cost1 &
  sleep 2

#For SVC2
core_id=8
svc_id=3
dst_id=4
inst_id=6
cost=6
cost=2
./og.sh $core2 $svc_id $dst_id $inst_id $cost2 &
  sleep 2

#For SVC3
core_id=8
svc_id=4
dst_id=5
inst_id=10
cost=8
cost=3
./og.sh $core3 $svc_id $dst_id $inst_id $cost3 &
  sleep 2
}

do_launch_test


