#!/bin/sh

#sudo -i
#Check if already installed:
#if [ -f ~/drench_install.txt ]; then
#  echo " File: ~/drench_install.txt Exits!!"
#  exit 0
#fi

cwd=`pwd`

#cleanup the directories
do_cleanup() {
  if [ -d "/home/mininet/drench" ]; then
    sudo rm -rf /home/mininet/drench/*
  fi
}
#do_cleanup

#check if the drench is installed or not
drench_dir=$cwd/drench
do_install_drench() {
  if [ ! -d $drench_dir ]; then
    #cd /
    #sudo mv /drench /drench_bkp
    sudo git clone https://sameergk_ugoe@bitbucket.org/sameergk_ugoe/drench.github
    sudo chmod -R +x $drench_dir/*.sh
    sudo chmod -R +x $drench_dir/*.py
    sudo chmod -R +x $drench_dir/*.py
    
    #sudo python $drench_dir/scripts/cloudlab/node_configuration_scripts/configure_host_and_switch.py
    #. $drench_dir/scripts/mn_cluster/cluster_config_helper.sh
    #sudo cp -r /drench /home/mininet/
    #sudo chown -R mininet:mininet /home/mininet/drench
  fi
  
  #set exec permisions for /drench
  cd $drench_dir
  # update just in case
  sudo git pull
  sudo git fetch --all
  sudo git reset --hard origin/master
  sudo chmod -R 774 $drench_dir
  #sudo cp drench
  chmod u+x **/*.sh
  chmod u+x **/*.bash
  chmod u+x **/*.py
  #find ./ -name "*.sh" -exec chmod +x {} \;
  #sudo rm -rf /local/drench
  #sudo cp -r /drench /local/
}
#do_install_drench

cd $cwd

#change direcotry to local
#cd /local

#configure for silent updates
export DEBIAN_FRONTEND=noninteractive
sudo apt-get update -q
#apt-get install -q -y -o Dpkg::Options::="--force-confdef" -o Dpkg::Options::="--force-confold" apache2 mysql-server

install_linux_utils() {
    # Install git
    sudo apt-get install -y git

    #Install X11, Terminal and Screen tools
    sudo apt-get install xterm
    sudo apt-get install xorg openbox
    sudo apt-get install gnome-terminal
    sudo apt-get install screen


    #Install curl and wget
    sudo apt-get install -y curl
    sudo apt-get install -y wget
    sudo apt-get install -y unzip
}
install_linux_utils

install_python_utils() {
    # Install Python Dev Tools
    sudo apt-get install -y python-pip python-dev build-essential

    # Intall Scapy
    sudo apt-get install -y scapy

    # Upgrade PIP and Install netifaces
    sudo -H pip install --upgrade pip 
    sudo -H pip install --upgrade virtualenv 
    sudo -H pip install netifaces
    sudo -H pip install pcapy
    sudo -H pip install networkx 

    # Install maptplotlib numpy
    #sudo -H pip install numpy
    #sudo -H pip install scipy
    #sudo -H pip install matplotlib
}
install_python_utils

install_traffic_generators() {
    # Downlaod and Install D-ITG 
    sudo wget http://traffic.comics.unina.it/software/ITG/codice/D-ITG-2.8.1-r1023-src.zip
    sudo unzip D-ITG-2.8.1-r1023-src.zip
    cd D-ITG-2.8.1-r1023/src
    sudo make clean all
    sudo make install PREFIX=/usr/local
    cd ../../

    # Downlaod and Install Iperf
    sudo git clone https://github.com/esnet/iperf
    cd iperf
    sudo ./configure
    sudo make
    sudo make install
    cd ..
}
install_traffic_generators

#Downlaod and Install Traffic Generators tcpreplay, netsniff-ng, netmap
install_tcp_replay_with_netmap() {
    sudo apt-get install build-essential libpcap-dev
    #sudo git clone https://github.com/appneta/tcpreplay
    wget https://github.com/appneta/tcpreplay/releases/download/v4.1.1/tcpreplay-4.1.1.tar.gz && tar -xvzf tcpreplay-4.1.1.tar.gz
    cd tcpreplay-4.1.1
    ./configure
    make
    sudo make install
    cd ..
    
    git clone git://github.com/netsniff-ng/netsniff-ng.git
    #Dependencies
    sudo apt-get install ccache flex bison libnl-3-dev \
        libnl-genl-3-dev libnl-route-3-dev libgeoip-dev \
        libnetfilter-conntrack-dev libncurses5-dev liburcu-dev \
        libnacl-dev libpcap-dev zlib1g-dev libcli-dev libnet1-dev
    cd netsniff-ng
    git checkout v0.6.1
    make
    sudo make install
    cd ..
    
}

#install_tcp_replay_with_netmap

install_system_monitor_tools() {
    sudo apt-get install dstat
    #sudo apt-get install collectl
}
install_system_monitor_tools

#Install Mininet tools
mininet_install() {
  if [ -d "openflow" ]; then
      sudo rm -rf openflow
  fi
  if [ -d "mininet" ]; then
      echo "Copied Mininet"
  else
      #Download and Install Mininet and OpenvSwitch
      sudo git clone git://github.com/mininet/mininet
      cd mininet
      sudo git tag
      sudo git checkout -b 2.2.1 2.2.1
      cd ..
  fi
  sudo chmod -R +x *.sh *.py
  sudo chmod +x **/*.sh
  sudo chmod +x **/*.py
  sudo mininet/util/install.sh -a
  sudo mn --test pingall > ~/mn_pingout.txt 2>&1
}
mininet_install

install_ns2() {
ns2_ver="ns-allinone-2.35"
  #Prerequistes 1
  #sudo apt-get update
  #sudo apt-get dist-upgrade
  #sudo apt-get update
  #Prerequisites 2
  sudo apt-get install build-essential autoconf automake
  sudo apt-get install tcl8.5-dev tk8.5-dev
  sudo apt-get install perl xgraph libxt-dev libx11-dev libxmu-dev
  sudo apt-get install \
g++ autoconf automake libtool libxmu-dev patch xgraph
  #sudo apt-get install gcc-4.4
  #Download and exatract ns-2.3.5
  wget http://downloads.sourceforge.net/project/nsnam/allinone/$ns2_ver/$ns2_ver.tar.gz -O - | tar -xz
  cd $ns2_ver
  sudo ./install
  cd ..

}
install_ns2

sudo sh -c "echo 'DRENCH Installation Completed at `date` !' >> drench_install.txt"
#sudo cp -r /drench ~/

#Download and Install OpenvSwitch < Problem: ovs versions compatibility must match to that of linux kernel, how to match and verify?>
#Anyway it is part of mininet -a (no need to install again), it should already be setup and ready
ovs_install_1() {
    sudo apt-get update
    sudo apt-get install -y git automake autoconf gcc uml-utilities libtool build-essential git pkg-config linux-headers-`uname -r`
    sudo wget http://openvswitch.org/releases/openvswitch-1.10.0.tar.gz
    sudo tar zxvf openvswitch-1.10.0.tar.gz
    sudo cd openvswitch-1.10.0
    sudo ./boot.sh
    sudo ./configure --with-linux=/lib/modules/`uname -r`/build
    sudo make && sudo make install
    sudo insmod datapath/linux/openvswitch.ko
    sudo mkdir -p /usr/local/etc/openvswitch
    sudo ovsdb-tool create /usr/local/etc/openvswitch/conf.db vswitchd/vswitch.ovsschema
    sudo ovsdb-server -v --remote=punix:/usr/local/var/run/openvswitch/db.sock \
                     --remote=db:Open_vSwitch,manager_options \
                     --private-key=db:SSL,private_key \
                     --certificate=db:SSL,certificate \
                     --pidfile --detach --log-file
    sudo ovs-vsctl --no-wait init
    sudo ovs-vswitchd --pidfile --detach
    sudo ovs-vsctl show
    cd ..
}
ovs_install() {
    #sudo apt-get install linux-headers-`uname -r`
    #sudo apt-get install build-essential fakeroot
    sudo apt-get install -y git automake autoconf gcc uml-utilities libtool build-essential git pkg-config linux-headers-`uname -r`
    sudo wget http://openvswitch.org/releases/openvswitch-2.5.0.tar.gz
    sudo tar zxvf openvswitch-2.5.0.tar.gz
    sudo cd openvswitch-2.5.0
    sudo ./boot.sh
    sudo ./configure --with-linux=/lib/modules/`uname -r`/build
    sudo make 
    sudo make install
    #Load the OVS Kernel Module
    sudo insmod datapath/linux/openvswitch.ko
    sudo touch /usr/local/etc/ovs-vswitchd.conf
    sudo mkdir -p /usr/local/etc/openvswitch
    sudo ovsdb-tool create /usr/local/etc/openvswitch/conf.db vswitchd/vswitch.ovsschema
    sudo ovsdb-server /usr/local/etc/openvswitch/conf.db \
        --remote=punix:/usr/local/var/run/openvswitch/db.sock \
        --remote=db:Open_vSwitch,manager_options \
        --private-key=db:SSL,private_key \
        --certificate=db:SSL,certificate \
        --bootstrap-ca-cert=db:SSL,ca_cert --pidfile --detach --log-file
    sudo ovs-vsctl --no-wait init
    sudo ovs-vswitchd --pidfile --detach
    sudo insmod datapath/linux/openvswitch.ko
    sudo ovs-vsctl show
    sudo ovs-vsctl --version
    cd ..
}

#Revert to previous directory
cd $cwd

#specific to cloudlab cluster: move to some other script
create_user_mininet() {
    username=mininet
    password=mininet
    pass=$(perl -e 'print crypt($username, "password")'$password)
    sudo useradd -m -p $pass $username
} #Password setup Doestn work, need to figure out some other mechanism..


#Dowload and SYNC DRECN code from GIT Repo
#sudo git clone https://sameergk_ugoe@bitbucket.org/sameergk_ugoe/drench.git
#cd drench
#sudo chmod +x -R *.sh *.py
#cd ..
#sudo mkdir /home/mininet
#sudo cp -r drench /home/mininet/

install_compare_tools() {
  # Beyond comapre tool
  wget http://www.scootersoftware.com/bcompare-4.1.6.21095_amd64.deb
  sudo apt-get update
  sudo apt-get install gdebi-core
  sudo gdebi bcompare-4.1.6.21095_amd64.deb
  # Meld

  #Remove
  sudo apt-get remove bcompare
}
