#!/bin/bash

LINUX_VERSION='4.13.0'
WORKSPACE='github'


# create workspace
cd ~
mkdir $WORKSPACE

# ========== thrift ========== 
cd ~
# install library for thrift
sudo apt-get install automake bison flex g++ git libboost1.58-all-dev libevent-dev libssl-dev libtool make pkg-config libglib2.0-dev python-all-dev python-all-dbg -y
sudo apt-get install python-all python-all-dev python-all-dbg libglib2.0-dev -y

# install thrift
cd ~/$WORKSPACE/
wget http://apache.cs.utah.edu/thrift/0.9.3/thrift-0.9.3.tar.gz
tar zxvf ./thrift-0.9.3.tar.gz && cd thrift-0.9.3/
./configure
make
sudo make install

# ========== ZLOG ========== 
# install ZLOG
cd ~/$WORKSPACE/
wget https://github.com/HardySimpson/zlog/archive/latest-stable.tar.gz
tar -zxvf ./latest-stable.tar.gz
cd ./zlog-latest-stable/
make
sudo make PREFIX=/usr/local/ install
sudo echo '/usr/local/lib' >> /etc/ld.so.conf
sudo ldconfig

# ========== apr & apr-util ========== 
# apr
cd ~/$WORKSPACE/
wget http://archive.apache.org/dist/apr/apr-1.5.2.tar.gz
tar zxvf ./apr-1.5.2.tar.gz
cd ./apr-1.5.2/
./configure --prefix=/usr/local/
make
make test
sudo make install

# apr-util
cd ~/$WORKSPACE/
wget http://archive.apache.org/dist/apr/apr-util-1.5.4.tar.gz
tar zxvf ./apr-util-1.5.4.tar.gz
cd ./apr-util-1.5.4/
./configure --prefix=/usr/local/ --with-apr=/usr/local/
make
make test
sudo make install

# install apr & apr-util package
sudo apt-get install libapr1-dev libaprutil1-dev -y


# ========== netmap ========== 
cd ~/$WORKSPACE/
# install netmap 
sudo apt-get install linux-headers-$(uname -r) -y
sudo apt-get install linux-source-${LINUX_VERSION} -y
cd /usr/src/
sudo tar xaf ./linux-source-${LINUX_VERSION}.tar.xz
cd ~/$WORKSPACE/
git clone https://github.com/luigirizzo/netmap.git
cd ./netmap/LINUX
./configure --kernel-sources=/usr/src/linux-source-${LINUX_VERSION} --drivers=ixgbe
make
sudo make install