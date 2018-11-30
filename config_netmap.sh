#!/usr/bin/env bash

NMPATH="/home/[USERNAME]/github/netmap/"
IFACE="eth3"
IFACE2="eth2"
IP=`/sbin/ifconfig ${IFACE} | grep -o -E '([[:digit:]]{1,3}.){3}[[:digit:]]{1,3}' | head -n 1`
IP2=`/sbin/ifconfig ${IFACE2} | grep -o -E '([[:digit:]]{1,3}.){3}[[:digit:]]{1,3}' | head -n 1`
NETMASK="255.255.255.0"
GATEWAY="[RESEARCH_NETWORK_PREFIX].1"
MULTICAST_ADDRESS="224.0.0.0/4"

MTU=9000 # not including ethernet header
NETMAP_QUEUE=1
NETMAP_CPU=1
NQUEUE=12
NMSLOT=4096
RAMSIZE="20G"
RAMPATH="/mnt/ramdisk"

cd ${NMPATH}/LINUX
rmmod ixgbe
rmmod netmap
modprobe mdio
modprobe ptp
modprobe dca
modprobe vxlan
insmod ./netmap.ko
insmod ./ixgbe/ixgbe.ko #allow_unsupported_sfp=1
sleep 5

ifconfig ${IFACE} up
ifconfig ${IFACE} ${IP} netmask ${NETMASK} mtu ${MTU}
ifconfig ${IFACE2} ${IP2} netmask ${NETMASK}
route add default gw ${GATEWAY}
ip route add ${MULTICAST_ADDRESS} dev ${IFACE}
ip link set ${IFACE} promisc on
sleep 2

#LSO (large send offload), TSO (TCP segmentation offload), GSO (generic segmentation offload) are the same thing
#LRO (large receive offload)
ethtool -K ${IFACE} rx off tx off gso off gro off lro off
ethtool -A ${IFACE} autoneg off rx off tx off
ethtool -L ${IFACE} combined ${NQUEUE}
ethtool -G ${IFACE} rx ${NMSLOT} tx ${NMSLOT}
ethtool -K ${IFACE} ntuple on
#ethtool -C ${IFACE} rx-usecs 1
ethtool -N ${IFACE} flow-type udp4 dst-ip 238.238.238.0 m 0.0.0.255 action ${NETMAP_QUEUE}
sleep 2

# Receive-Side Scaling (RSS)
cd /sys/class/net/${IFACE}/device/msi_irqs/
CPU=0
LOOP_COUNTER=0
NETMAP_IRQ=$((${NETMAP_QUEUE}+114))
for IRQ in *
do
    # disable IRQ balance for nic queue
    echo "======= irq id: " ${IRQ} "<->" ${CPU} " ======="
    if [ ${IRQ} -eq ${NETMAP_IRQ} ]
    then
        echo netmap queue
        irqbalance --banirq=${IRQ}
        echo ${NETMAP_CPU} > /proc/irq/${IRQ}/smp_affinity_list;
    else
        irqbalance --banirq=${IRQ}
        echo ${CPU} > /proc/irq/${IRQ}/smp_affinity_list;
    fi
    cat /proc/irq/${IRQ}/smp_affinity
    cat /proc/irq/${IRQ}/smp_affinity_list
    cat /proc/irq/${IRQ}/affinity_hint
    let CPU=(CPU+1)%NQUEUE
    let LOOP_COUNTER=(LOOP_COUNTER+1)
    if [ ${LOOP_COUNTER} -eq ${NQUEUE} ]
    then
        break
    fi
done

# Receive Packet Steering (RPS)
QUEUE=0 # QUEUE is CPU
QUEUE_CPU=0 # QUEUE is CPU
while [ ${QUEUE} -lt ${NQUEUE} ];
do
    if [ ${QUEUE} -eq ${NETMAP_QUEUE} ]
    then
        NETMAP_CPUMAP=$((1 << ${NETMAP_CPU}))
        echo "======= queue id: " ${QUEUE} "<->" ${NETMAP_CPUMAP} "(netmap) ======="
        echo `printf '%08x\n' ${NETMAP_CPUMAP}` > /sys/class/net/${IFACE}/queues/tx-${NETMAP_QUEUE}/xps_cpus
        echo `printf '%08x\n' ${NETMAP_CPUMAP}` > /sys/class/net/${IFACE}/queues/rx-${NETMAP_QUEUE}/rps_cpus
    else
        CPUMAP=$((1 << ${QUEUE_CPU}))
        echo "======= queue id: " ${QUEUE} "<->" ${CPUMAP} " ======="
        echo `printf '%08x\n' ${CPUMAP}` > /sys/class/net/${IFACE}/queues/tx-${QUEUE}/xps_cpus
        echo `printf '%08x\n' ${CPUMAP}` > /sys/class/net/${IFACE}/queues/rx-${QUEUE}/rps_cpus
    fi
    cat /sys/class/net/${IFACE}/queues/rx-${QUEUE}/rps_cpus
    cat /sys/class/net/${IFACE}/queues/tx-${QUEUE}/xps_cpus
    let QUEUE_CPU=QUEUE_CPU+1
    let QUEUE=QUEUE+1
done


# Receive Flow Steering (RFS)
echo 32768 > /proc/sys/net/core/rps_sock_flow_entries
let RPS_PER_QUEUE=32768/2/NQUEUE
QUEUE=0 # QUEUE is CPU
while [ $QUEUE -lt ${NQUEUE} ];
do
    echo "======= queue id: " ${QUEUE} "<->" ${RPS_PER_QUEUE} " ======="
    echo ${RPS_PER_QUEUE} > /sys/class/net/${IFACE}/queues/rx-${QUEUE}/rps_flow_cnt
    cat /sys/class/net/${IFACE}/queues/rx-${QUEUE}/rps_flow_cnt
    let QUEUE=QUEUE+1
done

# netmap
#echo 9216 > /sys/module/netmap/parameters/buf_num # 18432
#echo 34816 > /sys/module/netmap/parameters/ring_size # 65784
echo 18432 > /sys/module/netmap/parameters/buf_num
echo 69632 > /sys/module/netmap/parameters/ring_size

echo 9216 > /sys/module/netmap/parameters/buf_size
echo 4 > /sys/module/netmap/parameters/ring_num

# emulated adapter mode
echo 0 > /sys/module/netmap/parameters/admode

mkdir -p ${RAMPATH}
mount -t tmpfs -o size=${RAMSIZE} tmpfs ${RAMPATH}

#for pppid in $(ps axjf | sed 's/|/ /' | awk '{print $2}' | grep -v PID); do
#    if [[ $(taskset -pc ${pppid}) == *"0-11"* ]]
#    then
#        taskset -p 0xffd ${pppid}
#    fi
#done

#
#commit 30da094081157b2318d94783940435bdba062b71
#Merge: 05c6808 8bd85e8
#Author: Vincenzo Maffione <v.maffione@gmail.com>
#Date:   Wed Aug 31 10:01:06 2016 +0200
#
#Merge branch 'master' into github-master