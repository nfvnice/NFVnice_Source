#ls -l a9* | awk '{ print "./"$9 "\ &"}'  > run_9schain.sh

#For SVC1
core_id=8
svc_id=2
dst_id=3
inst_id=2
cost=3
./og.sh $core_id $svc_id $dst_id $inst_id $cost &
  sleep 2

#For SVC2
core_id=9
svc_id=3
dst_id=4
inst_id=3
cost=4
./og.sh $core_id $svc_id $dst_id $inst_id $cost &
  sleep 2

#For SVC3
core_id=10
svc_id=4
dst_id=5
inst_id=5
cost=6
./og.sh $core_id $svc_id $dst_id $inst_id $cost &
  sleep 2


#For SVC4
core_id=11
svc_id=5
dst_id=6
inst_id=6
cost=7
./og.sh $core_id $svc_id $dst_id $inst_id $cost &
  sleep 2

#For SVC5
core_id=12
svc_id=6
dst_id=7
inst_id=9
cost=8
./og.sh $core_id $svc_id $dst_id $inst_id $cost &
  sleep 2

#For SVC6=BRIDGE
core_id=13
svc_id=7
dst_id=8
inst_id=13
cost=0
../bridge/og.sh $core_id $svc_id $dst_id $inst_id $cost &
  sleep 2
