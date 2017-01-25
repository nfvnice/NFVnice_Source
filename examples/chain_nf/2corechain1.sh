#Topology: chain of 6 NFs (M1-L1-M2-M3-H1-Bridge); M1,L1,M2 on core 8, M3H1 on core 9 and Bridge on core 10
#ls -l a9* | awk '{ print "./"$9 "\ &"}'  > run_9schain.sh

#For SVC1
core_id=9
svc_id=2
dst_id=3
inst_id=2
cost=2
cost=3
./og.sh $core_id $svc_id $dst_id $inst_id $cost &
  sleep 2

#For SVC2
core_id=9
svc_id=3
dst_id=4
inst_id=3
cost=1
cost=2
./og.sh $core_id $svc_id $dst_id $inst_id $cost &
  sleep 2

#For SVC3
core_id=9
svc_id=4
dst_id=5
inst_id=6
cost=2
cost=0
./og.sh $core_id $svc_id $dst_id $inst_id $cost &
  sleep 2


#For SVC4
core_id=10
svc_id=5
dst_id=6
inst_id=9
cost=2
./og.sh $core_id $svc_id $dst_id $inst_id $cost &
  sleep 2

#For SVC5
core_id=10
svc_id=6
dst_id=7
inst_id=12
cost=3
./og.sh $core_id $svc_id $dst_id $inst_id $cost &
  sleep 2

#For SVC6=BRIDGE
core_id=8
svc_id=7
dst_id=8
inst_id=13
cost=0
#../bridge/og.sh $core_id $svc_id $dst_id $inst_id $cost &
#  sleep 2
