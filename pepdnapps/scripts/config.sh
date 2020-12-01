# -*- bash -*-

#
# Written by: Kr1stj0n C1k0 <kristjoc@ifi.uio.no>
#

. param.sh

#---------------------------------------------
# Add delay for outgoing packets
#---------------------------------------------
set_out_delay() {
        local delay=$1

        if [ $nnodes == '2' ]; then
                hosts=($client_gw_host $server_gw_host)
                CMD_ADD_ARRAY=("sudo tc qdisc add dev ${client_gw_right_iface} root netem delay ${delay}ms;"
                               "sudo tc qdisc add dev ${server_gw_left_iface} root netem delay ${delay}ms;")
                CMD_DEL_ARRAY=("sudo tc qdisc del dev ${client_gw_right_iface} root 1> /dev/null;"
                               "sudo tc qdisc del dev ${server_gw_left_iface} root 1> /dev/null;")
        elif [ $nnodes == '3' ]; then
                hosts=($client_host $client_gw_host $server_gw_host)
                CMD_ADD_ARRAY=("sudo tc qdisc add dev ${client_iface} root netem delay ${delay}ms;"
                               "sudo tc qdisc add dev ${client_gw_left_iface} root netem delay ${delay}ms;
                                sudo tc qdisc add dev ${client_gw_right_iface} root netem delay ${delay}ms;"
                               "sudo tc qdisc add dev ${server_gw_left_iface} root netem delay ${delay}ms;")
                CMD_DEL_ARRAY=("sudo tc qdisc del dev ${client_iface} root 1> /dev/null;"
                               "sudo tc qdisc del dev ${client_gw_left_iface} root 1> /dev/null;
                                sudo tc qdisc del dev ${client_gw_right_iface} root 1> /dev/null;"
                               "sudo tc qdisc del dev ${server_gw_left_iface} root 1> /dev/null;")
        elif [ $nnodes == '4' ]; then
                hosts=($client_host $client_gw_host $server_gw_host $server_gw_host)
                CMD_ADD_ARRAY=("sudo tc qdisc add dev ${client_iface} root netem delay ${delay}ms;"
                               "sudo tc qdisc add dev ${client_gw_left_iface} root netem delay ${delay}ms;
                                sudo tc qdisc add dev ${client_gw_right_iface} root netem delay ${delay}ms;"
                               "sudo tc qdisc add dev ${server_gw_left_iface} root netem delay ${delay}ms;
                                sudo tc qdisc add dev ${server_gw_right_iface} root netem delay ${delay}ms;"
                                "sudo tc qdisc add dev ${server_iface} root netem delay ${delay}ms;")
                CMD_DEL_ARRAY=("sudo tc qdisc del dev ${client_iface} root 1> /dev/null;"
                               "sudo tc qdisc del dev ${client_gw_left_iface} root 1> /dev/null;
                                sudo tc qdisc del dev ${client_gw_right_iface} root 1> /dev/null;"
                               "sudo tc qdisc del dev ${server_gw_left_iface} root 1> /dev/null;
                                sudo tc qdisc del dev ${server_gw_right_iface} root 1> /dev/null;"
                               "sudo tc qdisc del dev ${server_iface} root 1> /dev/null;")
        else
                echo "Number of nodes not supported"
                return
        fi

        if [ $delay != '0' ]; then
                for i in ${!hosts[@]}; do
                        ssh ${hosts[$i]} ${CMD_ADD_ARRAY[$i]}
                done
        else
                for i in ${!hosts[@]}; do
                        ssh ${hosts[$i]} $CMD_DEL_ARRAY[$i]
                done
        fi

        sleep 3
}

#------------------------------
# Set ethernet interface speed
#------------------------------
set_eth_speed() {
        local speed=$1

        if [ $nnodes -eq '2' ]; then
                hosts=($client_gw_host $server_gw_host)
                CMD_ARRAY=("sudo ethtool -s ${client_gw_right_iface} speed $speed duplex full autoneg off 1> /dev/null"
                           "sudo ethtool -s ${server_gw_left_iface} speed $speed duplex full autoneg off 1> /dev/null")
        elif [ $nnodes -eq '3' ]; then
                hosts=($client_host $client_gw_host $server_gw_host)
                CMD_ARRAY=("sudo ethtool -s ${client_iface} speed $speed duplex full autoneg off 1> /dev/null"
                           "sudo ethtool -s ${client_gw_left_iface} speed $speed duplex full autoneg off 1> /dev/null;
                            sudo ethtool -s ${client_gw_right_iface} speed $speed duplex full autoneg off 1> /dev/null"
                           "sudo ethtool -s ${server_gw_left_iface} speed $speed duplex full autoneg off 1> /dev/null")
        elif [ $nnodes -eq '4' ]; then
                hosts=($client_host $client_gw_host $server_gw_host $server_host)
                CMD_ARRAY=("sudo ethtool -s ${client_iface} speed $speed duplex full autoneg off 1> /dev/null"
                           "sudo ethtool -s ${client_gw_left_iface} speed $speed duplex full autoneg off 1> /dev/null;
                            sudo ethtool -s ${client_gw_right_iface} speed $speed duplex full autoneg off 1> /dev/null"
                           "sudo ethtool -s ${server_gw_left_iface} speed $speed duplex full autoneg off 1> /dev/null;
                            sudo ethtool -s ${server_gw_right_iface} speed $speed duplex full autoneg off 1> /dev/null"
                           "sudo ethtool -s ${server_iface} speed $speed duplex full autoneg off 1> /dev/null")
        else
                echo "Number of nodes not supported"
                return
        fi

        for i in ${!hosts[@]}; do
                ssh ${hosts[$i]} ${CMD_ARRAY[$i]}
        done

        sleep 3
}

#----------------------------------------
# Set/Reset TCP Segmentation Offloading
#----------------------------------------
set_offloading() {
        local onoff=$1

        if [ $nnodes -eq '2' ]; then
                hosts=($client_gw_host $server_gw_host)
                CMD=("sudo ethtool -K ${client_gw_right_iface} gso ${onoff};
                      sudo ethtool -K ${client_gw_right_iface} gro ${onoff};
                      sudo ethtool -K ${client_gw_right_iface} tso ${onoff};
                      sudo ethtool -K ${client_gw_right_iface} tx ${onoff};
                      sudo ethtool -K ${client_gw_right_iface} rx ${onoff};
                      sudo ethtool -K ${client_gw_right_iface} tx-gso-partial ${onoff};
                      sudo ethtool -A ${client_gw_right_iface} autoneg off tx off rx off;
                      sudo ethtool -K ${client_gw_right_iface} sg ${onoff};
                      sudo ethtool -C ${client_gw_right_iface} rx-usecs 0 tx-usecs 0"

                     "sudo ethtool -K ${server_gw_left_iface} gso ${onoff};
                      sudo ethtool -K ${server_gw_left_iface} gro ${onoff};
                      sudo ethtool -K ${server_gw_left_iface} tso ${onoff};
                      sudo ethtool -K ${server_gw_left_iface} tx ${onoff};
                      sudo ethtool -K ${server_gw_left_iface} rx ${onoff};
                      sudo ethtool -K ${server_gw_left_iface} tx-gso-partial ${onoff};
                      sudo ethtool -A ${server_gw_left_iface} autoneg off tx off rx off;
                      sudo ethtool -K ${server_gw_left_iface} sg ${onoff};
                      sudo ethtool -C ${server_gw_left_iface} rx-usecs 0 tx-usecs 0")
        elif [ $nnodes -eq '3' ]; then
                hosts=($client_host $client_gw_host $server_gw_host)
                CMD=("sudo ethtool -K ${client_iface} gso ${onoff};
                      sudo ethtool -K ${client_iface} gro ${onoff};
                      sudo ethtool -K ${client_iface} tso ${onoff};
                      sudo ethtool -K ${client_iface} tx ${onoff};
                      sudo ethtool -K ${client_iface} rx ${onoff};
                      sudo ethtool -K ${client_iface} tx-gso-partial ${onoff};
                      sudo ethtool -A ${client_iface} autoneg off tx off rx off;
                      sudo ethtool -K ${client_iface} sg ${onoff};
                      sudo ethtool -C ${client_iface} rx-usecs 0 tx-usecs 0"

                     "sudo ethtool -K ${client_gw_left_iface} gso ${onoff};
                      sudo ethtool -K ${client_gw_left_iface} gro ${onoff};
                      sudo ethtool -K ${client_gw_left_iface} tso ${onoff};
                      sudo ethtool -K ${client_gw_left_iface} tx ${onoff};
                      sudo ethtool -K ${client_gw_left_iface} rx ${onoff};
                      sudo ethtool -K ${client_gw_left_iface} tx-gso-partial ${onoff};
                      sudo ethtool -A ${client_gw_left_iface} autoneg off tx off rx off;
                      sudo ethtool -K ${client_gw_left_iface} sg ${onoff};
                      sudo ethtool -C ${client_gw_left_iface} rx-usecs 0 tx-usecs 0;

                      sudo ethtool -K ${client_gw_right_iface} gso ${onoff};
                      sudo ethtool -K ${client_gw_right_iface} gro ${onoff};
                      sudo ethtool -K ${client_gw_right_iface} tso ${onoff};
                      sudo ethtool -K ${client_gw_right_iface} tx ${onoff};
                      sudo ethtool -K ${client_gw_right_iface} rx ${onoff};
                      sudo ethtool -K ${client_gw_right_iface} tx-gso-partial ${onoff};
                      sudo ethtool -A ${client_gw_right_iface} autoneg off tx off rx off;
                      sudo ethtool -K ${client_gw_right_iface} sg ${onoff};
                      sudo ethtool -C ${client_gw_right_iface} rx-usecs 0 tx-usecs 0"

                     "sudo ethtool -K ${server_gw_left_iface} gso ${onoff};
                      sudo ethtool -K ${server_gw_left_iface} gro ${onoff};
                      sudo ethtool -K ${server_gw_left_iface} tso ${onoff};
                      sudo ethtool -K ${server_gw_left_iface} tx ${onoff};
                      sudo ethtool -K ${server_gw_left_iface} rx ${onoff};
                      sudo ethtool -K ${server_gw_left_iface} tx-gso-partial ${onoff};
                      sudo ethtool -A ${server_gw_left_iface} autoneg off tx off rx off;
                      sudo ethtool -K ${server_gw_left_iface} sg ${onoff};
                      sudo ethtool -C ${server_gw_left_iface} rx-usecs 0 tx-usecs 0")
        elif [ $nnodes -eq '4' ]; then
                hosts=($client_host $client_gw_host $server_gw_host $server_host)
                CMD=("sudo ethtool -K ${client_iface} gso ${onoff};
                      sudo ethtool -K ${client_iface} gro ${onoff};
                      sudo ethtool -K ${client_iface} tso ${onoff};
                      sudo ethtool -K ${client_iface} tx ${onoff};
                      sudo ethtool -K ${client_iface} rx ${onoff};
                      sudo ethtool -K ${client_iface} tx-gso-partial ${onoff};
                      sudo ethtool -A ${client_iface} autoneg off tx off rx off;
                      sudo ethtool -K ${client_iface} sg ${onoff};
                      sudo ethtool -C ${client_iface} rx-usecs 0 tx-usecs 0"

                     "sudo ethtool -K ${client_gw_left_iface} gso ${onoff};
                      sudo ethtool -K ${client_gw_left_iface} gro ${onoff};
                      sudo ethtool -K ${client_gw_left_iface} tso ${onoff};
                      sudo ethtool -K ${client_gw_left_iface} tx ${onoff};
                      sudo ethtool -K ${client_gw_left_iface} rx ${onoff};
                      sudo ethtool -K ${client_gw_left_iface} tx-gso-partial ${onoff};
                      sudo ethtool -A ${client_gw_left_iface} autoneg off tx off rx off;
                      sudo ethtool -K ${client_gw_left_iface} sg ${onoff};
                      sudo ethtool -C ${client_gw_left_iface} rx-usecs 0 tx-usecs 0;

                      sudo ethtool -K ${client_gw_right_iface} gso ${onoff};
                      sudo ethtool -K ${client_gw_right_iface} gro ${onoff};
                      sudo ethtool -K ${client_gw_right_iface} tso ${onoff};
                      sudo ethtool -K ${client_gw_right_iface} tx ${onoff};
                      sudo ethtool -K ${client_gw_right_iface} rx ${onoff};
                      sudo ethtool -K ${client_gw_right_iface} tx-gso-partial ${onoff};
                      sudo ethtool -A ${client_gw_right_iface} autoneg off tx off rx off;
                      sudo ethtool -K ${client_gw_right_iface} sg ${onoff};
                      sudo ethtool -C ${client_gw_right_iface} rx-usecs 0 tx-usecs 0"

                     "sudo ethtool -K ${server_gw_left_iface} gso ${onoff};
                      sudo ethtool -K ${server_gw_left_iface} gro ${onoff};
                      sudo ethtool -K ${server_gw_left_iface} tso ${onoff};
                      sudo ethtool -K ${server_gw_left_iface} tx ${onoff};
                      sudo ethtool -K ${server_gw_left_iface} rx ${onoff};
                      sudo ethtool -K ${server_gw_left_iface} tx-gso-partial ${onoff};
                      sudo ethtool -A ${server_gw_left_iface} autoneg off tx off rx off;
                      sudo ethtool -K ${server_gw_left_iface} sg ${onoff};
                      sudo ethtool -C ${server_gw_left_iface} rx-usecs 0 tx-usecs 0;

                      sudo ethtool -K ${server_gw_right_iface} gso ${onoff};
                      sudo ethtool -K ${server_gw_right_iface} gro ${onoff};
                      sudo ethtool -K ${server_gw_right_iface} tso ${onoff};
                      sudo ethtool -K ${server_gw_left_iface} tx ${onoff};
                      sudo ethtool -K ${server_gw_right_iface} rx ${onoff};
                      sudo ethtool -K ${server_gw_right_iface} tx-gso-partial ${onoff};
                      sudo ethtool -A ${server_gw_right_iface} autoneg off tx off rx off;
                      sudo ethtool -K ${server_gw_right_iface} sg ${onoff};
                      sudo ethtool -C ${server_gw_right_iface} rx-usecs 0 tx-usecs 0"

                     "sudo ethtool -K ${server_iface} gso ${onoff};
                      sudo ethtool -K ${server_iface} gro ${onoff};
                      sudo ethtool -K ${server_iface} tso ${onoff};
                      sudo ethtool -K ${server_iface} tx ${onoff};
                      sudo ethtool -K ${server_iface} rx ${onoff};
                      sudo ethtool -K ${server_iface} tx-gso-partial ${onoff};
                      sudo ethtool -A ${server_iface} autoneg off tx off rx off;
                      sudo ethtool -K ${server_iface} sg ${onoff};
                      sudo ethtool -C ${server_iface} rx-usecs 0 tx-usecs 0")
        else
                echo "Number of nodes not supported"
                return
        fi

        for i in ${!hosts[@]}; do
                ssh ${hosts[$i]} ${CMD[$i]} > /dev/null 2>&1
        done

        sleep 3
}

#-----------------------
# Set sysctl variables
#-----------------------
set_sysctls() {
        local set_reset=$1
        local rate=$2

        if [ $nnodes -eq '2' ]; then
                hosts=($client_gw_host $server_gw_host)
        elif [ $nnodes -eq '3' ]; then
                hosts=($client_host $client_gw_host $server_gw_host)
        elif [ $nnodes -eq '4' ]; then
                hosts=($client_host $client_gw_host $server_gw_host $server_host)
        else
                echo "Number of nodes not supported"
                return
        fi

        calculate_bufsize $rate $global_delay

        if [ $set_reset == "set" ]; then
                for i in ${!hosts[@]}; do
                        CMD='sudo sysctl --system;
                        sudo sysctl -w net.ipv4.tcp_timestamps=0;
                        sudo sysctl -w net.ipv4.tcp_no_metrics_save=1;
                        sudo sysctl -w net.ipv4.tcp_low_latency=1;
                        sudo sysctl -w net.ipv4.tcp_autocorking=0;
                        sudo sysctl -w net.ipv4.tcp_fastopen=0;

                        sudo sysctl -w net.core.rmem_max='${global_bufsize}';
                        sudo sysctl -w net.core.wmem_max='${global_bufsize}';
                        sudo sysctl -w net.core.rmem_default='${global_bufsize}';
                        sudo sysctl -w net.core.wmem_default='${global_bufsize}';
                        sudo sysctl -w net.ipv4.tcp_rmem="'${global_bufsize}' '${global_bufsize}' '${global_bufsize}'";
                        sudo sysctl -w net.ipv4.tcp_wmem="'${global_bufsize}' '${global_bufsize}' '${global_bufsize}'";
                        sudo sysctl -w net.ipv4.tcp_mem="'${global_bufsize}' '${global_bufsize}' '${global_bufsize}'";
                        sudo sysctl -w net.ipv4.ip_local_port_range="20000 61000";
                        sudo sysctl -w net.ipv4.tcp_fin_timeout=20;
                        sudo sysctl -w net.ipv4.tcp_tw_reuse=1;
                        sudo sysctl -w net.core.somaxconn=8192;
                        sudo sysctl -w net.core.netdev_max_backlog=4096;
                        sudo sysctl -w net.ipv4.tcp_max_syn_backlog=8192;
                        sudo sysctl -w net.ipv4.tcp_syncookies=0;

                        sudo sysctl -w net.ipv4.tcp_syn_retries=1;
                        sudo sysctl -w net.ipv4.conf.all.route_localnet=1;
                        sudo sysctl -w net.ipv4.ip_nonlocal_bind=1;
                        sudo sysctl -w net.ipv4.ip_forward=1;
                        sudo sysctl -w net.ipv4.conf.all.forwarding=1'
                        ssh ${hosts[$i]} $CMD
                done
        else
                for i in ${!hosts[@]}; do
                        CMD="sudo sysctl --system;
                             sudo iptables -F;
                             sudo iptables -X;
                             sudo iptables -t mangle -F"
                        ssh ${hosts[$i]} $CMD
                done
        fi

        sleep 3
}

#----------------------------------------------------
# Set iptables (In cases where proxies are involved)
#----------------------------------------------------
set_iptables() {
        if [ $nnodes -eq '2' ]; then
                # Topology: client_gw_host --- server_gw_host
                # Scenarios TCP2TCP | TCP2RINA (with PEPDNA or UserSpace PEP)
                # In this case, the proxy is running at the same host with the server
                # and the communication needs to happen in localhost
                hosts=($server_gw_host)
                CMD=("sudo iptables -t mangle -I PREROUTING -m mark --mark ${global_mark} -m comment --comment proxy -j CONNMARK --save-mark;
                      sudo iptables -t mangle -I OUTPUT -m connmark --mark ${global_mark} -m comment --comment proxy -j CONNMARK --restore-mark;
                      sudo iptables -t mangle -N DIVERT;
                      sudo iptables -t mangle -A PREROUTING -p tcp -m socket -j DIVERT;
                      sudo iptables -t mangle -A DIVERT -j MARK --set-mark 1;
                      sudo iptables -t mangle -A DIVERT -j ACCEPT;

                      sudo ip rule add fwmark 1 lookup 100;
                      sudo ip route add local 0.0.0.0/0 dev lo table 100;
                      sudo ip rule add fwmark ${global_mark} lookup 200;
                      sudo ip route add local 0.0.0.0/0 dev lo table 200;

                      sudo iptables -A PREROUTING -t mangle -i ${server_gw_left_iface} -p tcp --dport ${global_lport} -j TPROXY --on-port ${global_proxy_port} --tproxy-mark 1")
        elif [ $nnodes -eq '3' ]; then
                # Topology: client_host --- client_gw_host --- server_gw_host
                # Scenarios: TCP2TCP | TCP2RINA (3 nodes in the network; Proxy runs in the middle node
                hosts=($client_gw_host)
                CMD=("sudo iptables -t mangle -N DIVERT;
                      sudo iptables -t mangle -A PREROUTING -p tcp -m socket -j DIVERT;
                      sudo iptables -t mangle -A DIVERT -j MARK --set-mark 1;
                      sudo iptables -t mangle -A DIVERT -j ACCEPT;

                      sudo ip rule add fwmark 1 lookup 100;
                      sudo ip route add local 0.0.0.0/0 dev lo table 100;

                      sudo iptables -A PREROUTING -t mangle -i ${client_gw_left_iface} -p tcp --dport ${global_lport} -j TPROXY --on-port ${global_proxy_port} --tproxy-mark 1")
        elif [ $nnodes -eq '4' ]; then
                # Topology: client_host --- client_gw_host --- server_gw_host --- server_host
                # Scenario: TCP2RINA2TCP
                hosts=($client_gw_host $server_gw_host)
                CMD=("sudo iptables -t mangle -N DIVERT;
                      sudo iptables -t mangle -A PREROUTING -p tcp -m socket -j DIVERT;
                      sudo iptables -t mangle -A DIVERT -j MARK --set-mark 1;
                      sudo iptables -t mangle -A DIVERT -j ACCEPT;

                      sudo ip rule add fwmark 1 lookup 100;
                      sudo ip route add local 0.0.0.0/0 dev lo table 100;

                      sudo iptables -A PREROUTING -t mangle -i ${client_gw_left_iface} -p tcp --dport ${global_lport} -j TPROXY --on-port ${global_proxy_port} --tproxy-mark 1"
                     "sudo iptables -t mangle -N DIVERT;
                      sudo iptables -t mangle -A PREROUTING -p tcp  -m socket -j DIVERT;
                      sudo iptables -t mangle -A DIVERT -j MARK --set-mark 1;
                      sudo iptables -t mangle -A DIVERT -j ACCEPT;

                      sudo ip rule add fwmark 1 lookup 100;
                      sudo ip route add local 0.0.0.0/0 dev lo table 100")
        fi

        for i in ${!hosts[@]}; do
                ssh ${hosts[$i]} ${CMD[$i]}
        done
}

#---------------------------------------
# Set TCP congestion control algorithm
#---------------------------------------
set_cc() {
        local set_reset=$1

        if [ $nnodes == '2' ]; then
                hosts=($client_gw_host $server_gw_host)
        elif [ $nnodes == '3' ]; then
                hosts=($client_host $client_gw_host $server_gw_host)
        elif [ $nnodes == '4' ]; then
                hosts=($client_host $client_gw_host $server_gw_host $server_host)
        else
                echo "Number of nodes not supported"
                return
        fi

        if [ $set_reset == "set" ]; then
                for i in ${!hosts[@]}; do
                        CMD="sudo sysctl -q -w net.ipv4.tcp_congestion_control='reno'"
                        ssh ${hosts[$i]} $CMD
                done
        elif [ $set_reset == "reset" ]; then
                for i in ${!hosts[@]}; do
                        CMD="sudo sysctl -q -w net.ipv4.tcp_congestion_control='cubic'"
                        ssh ${hosts[$i]} $CMD
                done
        else
                echo "Command not supported"
                return
        fi

        sleep 3
}

#------------------------------------
# Set TCP MSS for client and server
#------------------------------------
set_routes_mss() {
        local set_reset=$1
        local mss=$global_mss

        if [ $nnodes -eq '2' ]; then
                hosts=($client_gw_host $server_gw_host)
                CMD_ADD_ARRAY=("sudo ip route add ${server_gw_left_ip}/32 via ${server_gw_left_ip} dev ${client_gw_right_iface} advmss ${mss}"
                               "sudo ip route add ${client_gw_right_ip}/32 via ${client_gw_right_ip} dev ${server_gw_left_iface} advmss ${mss}")
                CMD_DEL_ARRAY=("sudo ip route del ${server_gw_left_ip}/32 via ${server_gw_left_ip} dev ${client_gw_right_iface} advmss ${mss}"
                               "sudo ip route del ${client_gw_right_ip}/32 via ${client_gw_right_ip} dev ${server_gw_left_iface} advmss ${mss}")
        elif [ $nnodes -eq '3' ]; then
                hosts=($client_host $server_gw_host)
                CMD_ADD_ARRAY=("sudo ip route add ${server_gw_left_ip}/32 via ${client_gw_left_ip} dev ${client_iface} advmss ${mss}"
                               "sudo ip route add ${client_ip}/32 via ${client_gw_right_ip} dev ${server_gw_left_iface} advmss ${mss}")
                CMD_DEL_ARRAY=("sudo ip route del ${server_gw_left_ip}/32 via ${client_gw_left_ip} dev ${client_iface} advmss ${mss}"
                               "sudo ip route del ${client_ip}/32 via ${client_gw_right_ip} dev ${server_gw_left_iface} advmss ${mss}")
        elif [ $nnodes -eq '4' ]; then
                hosts=($client_host $client_gw_host $server_gw_host $server_host)
                CMD_ADD_ARRAY=("sudo ip route add ${server_ip}/32 via ${client_gw_left_ip} dev ${client_iface} advmss ${mss}"
                               "sudo ip route add ${server_ip}/32 via ${server_gw_left_ip} dev ${client_gw_right_iface} advmss ${mss}"
                               "sudo ip route add ${client_ip}/32 via ${client_gw_right_ip} dev ${server_gw_left_iface} advmss ${mss}"
                               "sudo ip route add ${client_ip}/32 via ${server_gw_right_ip} dev ${server_iface} advmss ${mss}")

                CMD_DEL_ARRAY=("sudo ip route del ${server_ip}/32 via ${client_gw_left_ip} dev ${client_iface} advmss ${mss}"
                               "sudo ip route del ${server_ip}/32 via ${server_gw_left_ip} dev ${client_gw_right_iface} advmss ${mss}"
                               "sudo ip route del ${client_ip}/32 via ${client_gw_right_ip} dev ${server_gw_left_iface} advmss ${mss}"
                               "sudo ip route del ${client_ip}/32 via ${server_gw_right_ip} dev ${server_iface} advmss ${mss}")
        else
                echo "Number of nodes not supported"
                return
        fi

        if [ $set_reset == "set" ]; then
                for i in ${!hosts[@]}; do
                        ssh ${hosts[$i]} ${CMD_ADD_ARRAY[$i]}
                done
        elif [ $set_reset == "reset" ]; then
                for i in ${!hosts[@]}; do
                        ssh ${hosts[$i]} ${CMD_DEL_ARRAY[$i]}
                done
        else
                echo "Command not supported"
        fi

        sleep 3
}

#----------------------------------------------------------------
# Calculate buffer size using BDP. (We keep this static for now)
#----------------------------------------------------------------
calculate_bufsize() {
        # set TCP rcv/snd buffer size equal to BDP
        # BDP = (bandwidth * 1000 * 1000 / 8) * (delay / 1000) = bandwidth * 250 * delay
        local bandwidth=$1
        local delay=$2

        if [ $delay != "0" ]; then
                bs=$(echo "scale=2; $bandwidth * 500 * $delay * 2" | bc)
        else
                delay="1"
                bs=$(echo "scale=2; $bandwidth * 500 * $delay" | bc)
        fi

        global_bufsize=$bs
        global_bufsize=8388608
}

#--------------------------------------------
# Set cpufrew settings for up to 'proc' CPUs
#--------------------------------------------
set_cpufreq_scaling() {
        local onoff=$1
        local proc=$2

        if [ $nnodes == '2' ]; then
                hosts=($client_gw_host $server_gw_host)
        elif [ $nnodes == '3' ]; then
                hosts=($client_host $client_gw_host $server_gw_host)
        elif [ $nnodes == '4' ]; then
                hosts=($client_host $client_gw_host $server_gw_host $server_gw_host)
        else
                echo "Number of nodes not supported"
                return
        fi

        for i in ${!hosts[@]}; do
                if [ $onoff == "set" ]; then
                        CMD='for i in $(seq 1 '${proc}'); do
                                sudo cpufreq-set -c$i -r -g performance;
                            done'
                else
                        CMD='for i in $(seq 1 '${proc}'); do
                                sudo cpufreq-set -c$i -r -g powersave;
                            done'
                fi
                ssh ${hosts[$i]} $CMD > /dev/null 2>&1
        done
        sleep 1
}
