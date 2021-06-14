# -*- bash -*-

#
# Written by: Kr1stj0n C1k0 <kristjoc@ifi.uio.no>
#

. config.sh

#-----------------------------------------------------
# Load RINA modules
#-----------------------------------------------------
load_modules() {
        local scenario=$1

        if [ $nnodes -eq '2' ]; then
                if [ $scenario != "pure_rina" ]; then
                        hosts=($server_gw_host)
                        CMD_ARRAY=("sudo modprobe 8021q;
                                    sudo modprobe rinarp;
                                    sudo modprobe rina-irati-core irati_verbosity=7;
                                    sudo modprobe rina-default-plugin;
                                    sudo modprobe normal-ipcp;
                                    sudo modprobe shim-eth-vlan;
                                    sudo ipcm -c /etc/ipcmanager.conf > /dev/null 2>&1 &")
                else
                        hosts=($client_gw_host $server_gw_host)
                        CMD_ARRAY=("sudo modprobe 8021q;
                                    sudo modprobe rinarp;
                                    sudo ip link add link ${client_gw_right_iface} name ${client_gw_right_iface}.110 type vlan id 110;
                                    sudo ip link set dev ${client_gw_right_iface} up;
                                    sudo ip link set dev ${client_gw_right_iface}.110 up;
                                    sudo modprobe rina-irati-core irati_verbosity=7;
                                    sudo modprobe rina-default-plugin;
                                    sudo modprobe normal-ipcp;
                                    sudo modprobe shim-eth-vlan;
                                    sudo ipcm -c /etc/ipcmanager.conf > /dev/null 2>&1 &"

                                   "sudo modprobe 8021q;
                                    sudo modprobe rinarp;
                                    sudo ip link add link ${server_gw_left_iface} name ${server_gw_left_iface}.110 type vlan id 110;
                                    sudo ip link set dev ${server_gw_left_iface} up;
                                    sudo ip link set dev ${server_gw_left_iface}.110 up;
                                    sudo modprobe rina-irati-core irati_verbosity=7;
                                    sudo modprobe rina-default-plugin;
                                    sudo modprobe normal-ipcp;
                                    sudo modprobe shim-eth-vlan;
                                    sudo ipcm -c /etc/ipcmanager.conf > /dev/null 2>&1 &")
                fi
        elif [ $nnodes -gt '2' ]; then
                hosts=($client_gw_host $server_gw_host)
                CMD_ARRAY=("sudo modprobe 8021q;
                            sudo modprobe rinarp;
                            sudo ip link add link ${client_gw_right_iface} name ${client_gw_right_iface}.110 type vlan id 110;
                            sudo ip link set dev ${client_gw_right_iface} up;
                            sudo ip link set dev ${client_gw_right_iface}.110 up;
                            sudo modprobe rina-irati-core irati_verbosity=7;
                            sudo modprobe rina-default-plugin;
                            sudo modprobe normal-ipcp;
                            sudo modprobe shim-eth-vlan;
                            sudo ipcm -c /etc/ipcmanager.conf > /dev/null 2>&1 &"

                           "sudo modprobe 8021q;
                            sudo modprobe rinarp;
                            sudo ip link add link ${server_gw_left_iface} name ${server_gw_left_iface}.110 type vlan id 110;
                            sudo ip link set dev ${server_gw_left_iface} up;
                            sudo ip link set dev ${server_gw_left_iface}.110 up;
                            sudo modprobe rina-irati-core irati_verbosity=7;
                            sudo modprobe rina-default-plugin;
                            sudo modprobe normal-ipcp;
                            sudo modprobe shim-eth-vlan;
                            sudo ipcm -c /etc/ipcmanager.conf > /dev/null 2>&1 &")
        else
                echo "Number of nodes not supported"
                return
        fi

        for i in ${!hosts[@]}; do
                ssh ${hosts[$i]} ${CMD_ARRAY[$i]}
        done

        sleep 3
}

#------------------------------------------------
# Enroll to DIF (applied only on the 'left' node
#------------------------------------------------
enroll_to_dif() {
        local scenario=$1

        if [[ $scenario == *"pure_tcp"* ]]; then
                return
        elif [[ $scenario == *"tcp2tcp"* ]]; then
                return
        elif [[ $scenario == *"tcp2rina_"* ]]; then
                return
        elif [ $nnodes == '2' ] && [ $scenario != "pure_rina" ]; then
                echo "If !PureRINA, no need to enroll when only 1 RINA node"
                return
        elif [ $nnodes -gt '1' ]; then
                hosts=($client_gw_host $server_gw_host)
        fi

        CMD="sudo irati-ctl enroll-to-dif 2 normal.DIF 110 > /dev/null 2>&1"

        for i in ${!hosts[@]}; do
                if [ ${hosts[$i]} == $client_gw_host ]; then
                        ssh ${hosts[$i]} $CMD
                fi
        done

        sleep 3
}

#--------------------------------------
# Load Flow-Allocator app in UserSpace
#--------------------------------------
load_flow_allocator() {
        local scenario=$1
        local app=$2

        if [[ $scenario == *"pure"* ]]; then
                return
        fi

        if [[ $scenario == *"tcp2tcp"* ]]; then
                return
        fi

        if [[ $scenario == *"tcp2rina_"* ]]; then
                # In this scenario, there will be 2 or 3 nodes. If 2 => fallocator app
                # will run always on the right node (server_gw_host)
                if [[ $app == "upload" ]]; then
                        CMD="sudo flow-allocator -r -d normal.DIF --server-apn rinahttp > /dev/null 2>&1 &"
                else
                        CMD="sudo flow-allocator -r -d normal.DIF --server-apn rinahttping > /dev/null 2>&1 &"
                fi
                if [ $nnodes == '2' ]; then
                        ssh $server_gw_host $CMD
                else
                        ssh $client_gw_host $CMD
                fi
                return
        fi

        if [[ $scenario == *"tcp2rina2tcp"* ]]; then
                hosts=($client_gw_host $server_gw_host)
                for i in ${!hosts[@]}; do
                        if [ ${hosts[$i]} == $client_gw_host ]; then
                                CMD="sudo flow-allocator -r -d normal.DIF > /dev/null 2>&1 &"
                        else
                                CMD="sudo flow-allocator -l -r -d normal.DIF > /dev/null 2>&1 &"
                        fi
                        ssh ${hosts[$i]} $CMD
                done
        fi

        sleep 3
}

#----------------------
# Use UserSpace Proxy
#----------------------
load_user_pep() {
        if [ $nnodes == '2' ]; then
                local dst_ip=$localhost
                local host=$server_gw_host
        elif [ $nnodes == '3' ]; then
                local dst_ip=$server_gw_left_ip
                local host=$client_gw_host
        elif [ $nnodes -gt '3' ]; then
                echo "Number of nodes not supported"
                return
        fi
        CMD="sudo tcprdr -4 -t -T -l $global_proxy_port $dst_ip $global_lport > /dev/null 2>&1 &"

        ssh $host $CMD
        sleep 3
}

#-----------------------
# Load PEPDNA module
#-----------------------
load_pepdna() {
        local scenario=$1

        if [ $nnodes == '2' ]; then
                # Topology: client_gw_host --- server_gw_host
                # Scenarios: TCP2TCP | TCP2RINA
                if [[ $scenario == *"tcp2tcp"* ]]; then
                        CMD="sudo modprobe pepdna port=$global_proxy_port pepdna_mode=0"
                elif [[ $scenario == *"tcp2rina_"* ]]; then
                        CMD="sudo modprobe pepdna port=$global_proxy_port pepdna_mode=1"
                fi
                ssh $server_gw_host $CMD
        elif [ $nnodes == '3' ]; then
                # Topology: client_host --- client_gw_host --- server_gw_host
                # Scenarios: TCP2TCP | TCP2RINA
                if [[ $scenario == *"tcp2tcp"* ]]; then
                        CMD="sudo modprobe pepdna port=$global_proxy_port pepdna_mode=0"
                elif [[ $scenario == *"tcp2rina_"* ]]; then
                        CMD="sudo modprobe pepdna port=$global_proxy_port pepdna_mode=1"
                fi
                ssh $client_gw_host $CMD
        elif [ $nnodes == '4' ]; then
                # Topology: client_host --- client_gw_host --- server_gw_host --- server_host
                # Scenarios: TCP2RINA2TCP
                hosts=($client_gw_host $server_gw_host)
                for i in ${!hosts[@]}; do
                        if [ ${hosts[$i]} == $server_gw_host ]; then
                                CMD="sudo modprobe pepdna port=$global_proxy_port pepdna_mode=2"
                        else
                                CMD="sudo modprobe pepdna port=$global_proxy_port pepdna_mode=1"
                        fi
                        ssh ${hosts[$i]} $CMD
                done
        fi
        sleep 3
}

#----------------------------
# Unload modules and teardown
#----------------------------
unload_modules() {
        if [ $nnodes == '2' ]; then
                hosts=($client_gw_host $server_gw_host)
        elif [ $nnodes == '3' ]; then
                hosts=($client $client_gw_host $server_gw_host)
        elif [ $nnodes == '4' ]; then
                hosts=($client $client_gw_host $server_gw_host $server)
        else
                echo "Number of nodes not supported"
                return
        fi

        for i in ${!hosts[@]}; do
                CMD="sudo killall -2 rinahttp;
                     sudo killall -2 rinahttping;
                     sudo killall -9 rinahttping;
                     sudo killall -2 myhttping;
                     sudo killall -9 myhttping;
                     sudo killall -2 httpapp;
                     sudo killall -9 httpapp;
                     sudo killall -2 mycpuscript;
                     sudo killall -2 mymemscript;
                     sudo killall -2 httping;
                     sudo killall -2 flow-allocator;
                     sudo killall -9 flow-allocator;
                     sudo killall -2 tcprdr;
                     sudo killall -9 tcprdr;
                     sudo killall -2 rina-echo-time;
                     sudo killall -9 rina-echo-time;
                     sudo killall -9 ipcm;
                     sudo killall -9 /bin/ipcp;
                     sudo pkill -9 /bin/ipcp;
                     sudo rmmod shim-eth-vlan;
                     sudo rmmod rinarp;
                     sudo rmmod 8021q;
                     sudo rmmod arp826;
                     sudo rmmod normal-ipcp;
                     sudo rmmod rina-default-plugin;
                     sudo rmmod pepdna;
                     sudo rmmod rina-irati-core;
                     sync; echo 3 | sudo tee /proc/sys/vm/drop_caches;"

                ssh ${hosts[$i]} $CMD > /dev/null 2>&1
        done

        sleep 3
}

#-----------------
# Prepare host
#-----------------
prepare_host() {
        local rate=$1
        local onoff=$2

        set_sysctls "set" $rate
        set_offloading $onoff
        set_routes_mss "set"
        set_cc "set"
        # set_out_delay $global_delay
        set_eth_speed $rate
        set_cpufreq_scaling "set" 10

        sleep 3
}

#-------------------
# Load RINA stuff
#-------------------
load_rina_stuff() {
        local scenario=$1
        local app=$2

        if [[ $scenario == *"rina"* ]]; then
                load_modules $scenario
        fi

        if [[ $scenario == *"pepdna"* ]]; then
                load_pepdna $scenario
        fi

        if [[ $scenario == *"tcp2rina_"* ]]; then
                enroll_to_dif $scenario
        fi

        if [[ $scenario == *"pure_rina"* ]]; then
                enroll_to_dif $scenario
        fi

        if [[ $scenario == *"tcp2rina2tcp"* ]]; then
                enroll_to_dif $scenario
        fi

        if [[ $scenario == *"tcp2rina"* ]]; then
                load_flow_allocator $scenario $app
        fi
}

#-------------------
# Reset host config
#-------------------
reset_config() {
        set_routes_mss "reset"
        set_sysctls "reset"
        set_offloading "on"
        # set_out_delay "0"
        set_eth_speed $global_def_speed
        set_cc "reset"
        set_cpufreq_scaling "reset" 10

        sleep 3
}
