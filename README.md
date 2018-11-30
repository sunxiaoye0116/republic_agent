# republic_agent
Republic: Data Multicast Meets Hybrid Rack-Level Interconnections in Data Center

To run Republic Agent, the following packages should be installed on the Linux server.
- thrift-0.9.3
- zlog-latest
- apr-1.5.2
- apr-util-1.5.4
- netmap

To compile, run the following command
```
cd ~/github/republic_agent/eclipse_workspace/protocol/transceiver
make clean && make
```

To run agent

```
./protocol -e ${REPUBLIC_AGENT_NIC} -o ${REPUBLIC_AGENT_NIC} -s ${REPUBLIC_AGENT_DATA_CHANNEL_SCHEDULING_POLICY} -S ${REPUBLIC_AGENT_DATA_CHANNEL_SCHEDULING_PRIORITY} -d ${REPUBLIC_AGENT_DATA_CHANNEL_CORE} -c ${REPUBLIC_AGENT_CTRL_CHANNEL_CORE} -i ${REPUBLIC_AGENT_API_CORE} -q ${REPUBLIC_AGENT_NETMAP_QUEUE} -r ${REPUBLIC_AGENT_RATE_GBPS} -b ${REPUBLIC_AGENT_BATCH_SIZE} -a ${REPUBLIC_AGENT_ATTEMPT_RATE} -j ${REPUBLIC_AGENT_DATA_PAYLOAD_LEN}
```
