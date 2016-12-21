Flow Rule Installer
==
Example NF that proactively sets up the Flow Table Entires with IPv4 5tuple rules and service chains
Differnt IPv4 5 Tuple rules can be specified in a text file, which can be passed as argument to the program.
Differnt service chain entries can be specified in a text file, which can be passed as argument to the program.
Each IPv4 5 tuple rule is mapped arbitrarily with the service chain and a flow table entry is created.

Also, the NF can reactively setup the flow table entry for the IPv4 flows.
Note: care needs to be taken that till the rule is inserted, subsequent packets should not be sent, so as to avoid the race condition and insertion of multiple rules. 

Compilation and Execution
--
```
cd examples
make
cd flow_rule_installer
./go.sh CORELIST SERVICE_ID DST [PRINT_DELAY]

OR

sudo ./flow_rule_installer/x86_64-native-linuxapp-gcc/flow_rule_installer -l CORELIST -n 3 --proc-type=secondary -- -r SERVICE_ID -- -d DST [-p PRINT_DELAY]
```

App Specific Arguments
--
  - `-d <dst>`: destination service ID to foward to
  - `-p <print_delay>`: number of packets between each print, e.g. `-p 1` prints every packets.
