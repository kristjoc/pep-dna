# -*- bash -*-

#
# Written by: Kr1stj0n C1k0 <kristjoc@ifi.uio.no>
#

. utils.sh
. apps.sh

SCRIPT=`realpath $0`
SCRIPTPATH=`dirname $SCRIPT`

#---------------------#
clean_up() {
    unload_modules
    reset_config
    exit
}

#-----------------------------------------------------------------------------#
app_selector() {
    local client=$1
    local server=$2
    local dst_ip=$3
    local app=$4
    local scenario=$5

    # FLOW COMPLETION TIME
    if [ $app == "upload" ]; then
	upload $client $server $dst_ip $global_lport $scenario
    # HTTPING
    elif [ $app == "httping" ]; then
	httping $client $server $dst_ip $global_lport $scenario
    # MYHTTPING
    elif [ $app == "myhttping_noninc_np" ]; then
	myhttping $client $server $dst_ip $global_lport $scenario "noninc_np"
    elif [ $app == "myhttping_noninc_p" ]; then
	myhttping $client $server $dst_ip $global_lport $scenario "noninc_p"
    elif [ $app == "myhttping_inc_p" ]; then
	myhttping $client $server $dst_ip $global_lport $scenario "inc_p"
    elif [ $app == "myhttping_inc_np" ]; then
	myhttping $client $server $dst_ip $global_lport $scenario "inc_np"
    # HTTPERFPY
    elif [ $app == "httperf" ]; then
	httperf $client $server $dst_ip $global_lport $scenario $rate
    # CPUMEMPERF
    elif [ $app == "cpumemperf" ]; then
	cpumemperf $client $server $dst_ip $global_lport $scenario $concon
    # PING
    elif [ $app == "ping" ]; then
	ping $client $dst_ip
    # RINA_ECHO_TIME
    elif [ $app == "rina_echo_time" ]; then
	rina_echo_time $client $server
    fi
}

#-----------------------------------------------------------------------------#
run_pure_tcp() {
    local app=$1
    local tso=$2

    if [ $nnodes == '2' ]; then
	# client_gw_host --- server_gw_host #
	local client=$client_gw_host
	local server=$server_gw_host
	local dst_ip=$server_gw_left_ip
    elif [ $nnodes == '3' ]; then
	# client --- client_host --- server_gw_host #
	local client=$client_host
	local server=$server_gw_host
	local dst_ip=$server_gw_left_ip
    elif [ $nnodes == '4' ]; then
	# client --- client_host --- server_gw_host --- server_host #
	local client=$client_host
	local server=$server_host
	local dst_ip=$server_ip
    else
	echo "Number of nodes not supported"
	return
    fi
    unload_modules
    prepare_host $speed $tso

    if [ $tso == "on" ]; then
	app_selector $client $server $dst_ip $app "pure_tcp_with_tso"
    else
	app_selector $client $server $dst_ip $app "pure_tcp_without_tso"
    fi

    reset_config

    sleep 1
}

#-----------------------------------------------------------------------------#
run_pepdna() {
    local app=$1
    local mode=$2
    local tso=$3

    if [ $nnodes == '2' ]; then
	# client_gw_host --- server_gw_host #
	echo "PEPDNA should have been configured with CONFIG_PEPDNA_LOCALHOST"
	local client=$client_gw_host
	local server=$server_gw_host
	local dst_ip=$server_gw_left_ip
    elif [ $nnodes == '3' ]; then
	# client --- client_host --- server_gw_host #
	local client=$client_host
	local server=$server_gw_host
	local dst_ip=$server_gw_left_ip
    elif [ $nnodes == '4' ]; then
	# client --- client_host --- server_gw_host --- server_host #
	echo "ONLY for TCP2RINA2TCP scenario"
	local client=$client_host
	local server=$server_host
	local dst_ip=$server_ip
    else
	echo "Number of nodes not supported"
	return
    fi

    unload_modules

    #$1 - speed
    #$2 - tcp offloading
    prepare_host $speed $tso
    set_iptables
    load_rina_stuff $mode $app
    app_selector $client $server $dst_ip $app $mode
    unload_modules
    reset_config

    sleep 1
}


#-----------------------------------------------------------------------------#
run_user_pep() {
    local app=$1
    local tso=$2

    if [ $nnodes == '2' ]; then
	# Topology: client_gw_host --- server_gw_host
	local client=$client_gw_host
	local server=$server_gw_host
	local dst_ip=$server_gw_left_ip
    elif [ $nnodes == '3' ]; then
	# Topology: client --- client_gw_host --- server_gw_host
	local client=$client_host
	local server=$server_gw_host
	local dst_ip=$server_gw_left_ip
    elif [ $nnodes -gt '3' ]; then
	echo "Number of nodes not supported"
	return
    fi
    unload_modules
    prepare_host $speed $tso
    set_iptables
    load_user_pep

    if [ $tso == "on" ]; then
	app_selector $client $server $dst_ip $app "user_pep_with_tso"
    else
	app_selector $client $server $dst_ip $app "user_pep_without_tso"
    fi

    reset_config

    sleep 1
}

#-----------------------------------------------------------------------------#
run_pure_rina() {
    local app=$1

    if [ $nnodes == '2' ]; then
	# client_gw_host --- server_gw_host #
	local client=$client_gw_host
	local server=$server_gw_host
	local dst_ip=$server_gw_left_ip
    elif [ $nnodes == '3' ]; then
	echo "Pure RINA does not support more than 2 nodes"
	exit
    fi
    unload_modules

    # $1 - speed
    # $2 - tcp offloading
    prepare_host $speed "off"

    # $1 - listening IP for pepdna (not used here)
    # $2 - listening port for pepdna (not used here)
    # $3 - rina or pepdna
    load_rina_stuff "pure_rina" $app
    app_selector $client $server $dst_ip $app "pure_rina"
    unload_modules
    reset_config

    sleep 1
}

# Signal handling
trap clean_up SIGHUP SIGINT SIGTERM

#-----------------------------------------------------------------------------#
# Functions below are called at 'main.sh'
#-----------------------------------------------------------------------------#
run_ping_app() {
    for i in ${!global_speed_vec[@]}; do
	speed=${global_speed_vec[$i]}
	run_pure_tcp "ping" "off"
	sleep 3
    done

    # collect results
    if [ $nnodes -eq '2' ]; then
	ssh $client_gw_host "sudo gzip -f ${global_path}ping.dat"
	scp $client_gw_host:${global_path}ping.dat.gz $SCRIPTPATH/results/ping/
    elif [ $nnodes -gt '2' ]; then
	ssh $client_host "sudo gzip -f ${global_path}ping.dat"
	scp $client_host:${global_path}ping.dat.gz $SCRIPTPATH/results/ping/
    fi

    gunzip -d $SCRIPTPATH/results/ping/ping.dat.gz
    echo "PING app done"
}

#--------------------------------------------------------#
run_rina_echo_time_app() {
    for i in ${!global_speed_vec[@]}; do
	speed=${global_speed_vec[$i]}
	run_pure_rina "rina_echo_time"
	sleep 3
    done
    # collect results
    if [ $nnodes -eq '2' ]; then
	ssh $client_gw_host "sudo gzip -f ${global_path}rina_echo_time.dat"
	scp $client_gw_host:${global_path}rina_echo_time.dat.gz $SCRIPTPATH/results/ping/
    elif [ $nnodes -gt '2' ]; then
	ssh $client_host "sudo gzip -f ${global_path}rina_echo_time.dat"
	scp $client_host:${global_path}rina_echo_time.dat.gz $SCRIPTPATH/results/ping/
    fi

    gunzip -d $SCRIPTPATH/results/ping/rina_echo_time.dat.gz
    echo "RINA_ECHO_TIME app done"
}

#------------------------------------------------------#
run_httping_app() {
    local tso="off"
    # Note that a web server running on port $global_port
    # is required at server side. We assume apache2 is installed
    for i in ${!global_speed_vec[@]}; do
	speed=${global_speed_vec[$i]}
	run_pure_tcp "httping" $tso
	sleep 3
	run_user_pep "httping" $tso
	sleep 3
	run_pepdna "httping" "pepdna_tcp2tcp_without_tso" $tso
	sleep 3
	run_pepdna "httping" "pepdna_tcp2rina2tcp_without_tso" $tso
	sleep 3
    done
    # collect results
    if [ $nnodes -eq '2' ]; then
	ssh $client_gw_host "cd ${global_path} && sudo tar -cvzf httping.tar.gz httping_*"
	scp $client_gw_host:${global_path}httping.tar.gz $SCRIPTPATH/results/httping/
	ssh $client_gw_host "sudo rm -f ${global_path}/httping.tar.gz"
    elif [ $nnodes -gt '2' ]; then
	ssh $client_host "cd ${global_path} && sudo tar -cvzf httping.tar.gz httping_*"
	scp $client_host:${global_path}httping.tar.gz $SCRIPTPATH/results/httping/
	ssh $client_host "sudo rm -f ${global_path}/myhttping.tar.gz"
    fi

    tar -xvzf $SCRIPTPATH/results/httping/httping.tar.gz -C $SCRIPTPATH/results/httping/
    echo "HTTPING app done"
}

#--------------------------------------------------------------------------#
run_myhttping_app() {
    local whichapp=$1

    for i in ${!global_speed_vec[@]}; do
	speed=${global_speed_vec[$i]}

	# TCP scenario with|without TSO
	run_pure_tcp $whichapp "on"
	sleep 10
	run_pure_tcp $whichapp "off"
	sleep 10

	# TCP2TCP_U scenario with|without TSO
	run_user_pep $whichapp "on"
	sleep 10
	run_user_pep $whichapp "off"
	sleep 10

	# TCP-TCP scenario with|without TSO
	run_pepdna $whichapp "pepdna_tcp2tcp_with_tso" "on"
	sleep 10
	run_pepdna $whichapp "pepdna_tcp2tcp_without_tso" "off"
	sleep 10

	# TCP-RINA with|without TSO
	run_pepdna $whichapp "pepdna_tcp2rina_with_tso" "on"
	sleep 10
	run_pepdna $whichapp "pepdna_tcp2rina_without_tso" "off"
	sleep 3

	# TCP-RINA-TCP with|without TSO
	# run_pepdna $whichapp "pepdna_tcp2rina2tcp_with_tso" "on"
	# sleep 10
	# run_pepdna $whichapp "pepdna_tcp2rina2tcp_without_tso" "off"

	# RINA scenario
	# run_pure_rina $whichapp
	# sleep 10
    done

    # collect results
    if [ $nnodes -eq '2' ]; then
	ssh $client_gw_host "cd ${global_path} && sudo tar -cvzf myhttping.tar.gz myhttping_*"
	scp $client_gw_host:${global_path}myhttping.tar.gz $SCRIPTPATH/results/myhttping/
	ssh $client_gw_host "sudo rm -f ${global_path}/myhttping.tar.gz"
    elif [ $nnodes -gt '2' ]; then
	ssh $client_host "cd ${global_path} && sudo tar -cvzf myhttping.tar.gz myhttping_*"
	scp $client_host:${global_path}myhttping.tar.gz $SCRIPTPATH/results/myhttping/
	ssh $client_host "sudo rm -f ${global_path}/myhttping.tar.gz"
    fi

    tar -xvzf $SCRIPTPATH/results/myhttping/myhttping.tar.gz -C $SCRIPTPATH/results/myhttping/
    rm $SCRIPTPATH/results/myhttping/myhttping.tar.gz
    echo "myHTTPING app done"
}

#---------------------------------------------------------#
run_upload_app() {
    for i in ${!global_speed_vec[@]}; do
	speed=${global_speed_vec[$i]}
	delay=$global_def_speed

	# TCP scenario with|without TSO
	run_pure_tcp "upload" "on"
	sleep 10
	run_pure_tcp "upload" "off"

	# TCP2TCP_U scenario with|without TSO
	run_user_pep "upload" "on"
	sleep 10
	run_user_pep "upload" "off"
	sleep 10

	# TCP2TCP scenario with|without TSO
	run_pepdna "upload" "pepdna_tcp2tcp_with_tso" "on"
	sleep 10
	run_pepdna "upload" "pepdna_tcp2tcp_without_tso" "off"
	sleep 10

	# TCP2RINA scenario with|without TSO
	run_pepdna "upload" "pepdna_tcp2rina_with_tso" "on"
	sleep 10
	run_pepdna "upload" "pepdna_tcp2rina_without_tso" "off"
	sleep 10

	# TCP2RINA2TCP scenario with|without TSO
	# run_pepdna "upload" "pepdna_tcp2rina2tcp_with_tso" "on"
	# sleep 10
	# run_pepdna "upload" "pepdna_tcp2rina2tcp_without_tso" "off"
	# sleep 10

	# PURE RINA scenario
	# run_pure_rina "upload"
	# sleep 10
    done
    # collect Flow Completion Time results
    if [ $nnodes -eq '2' ]; then
	ssh $client_gw_host "cd ${global_path} && sudo tar -cvzf upload.tar.gz upload_*"
	scp $client_gw_host:${global_path}upload.tar.gz $SCRIPTPATH/results/fct/
	ssh $client_gw_host "sudo rm -f ${global_path}/upload.tar.gz"
    elif [ $nnodes -gt '2' ]; then
	ssh $client_host "cd ${global_path} && sudo tar -cvzf upload.tar.gz upload_*"
	scp $client_host:${global_path}upload.tar.gz $SCRIPTPATH/results/fct/
	ssh $client_host "sudo rm -f ${global_path}/upload.tar.gz"
    fi

    tar -xvzf $SCRIPTPATH/results/fct/upload.tar.gz -C $SCRIPTPATH/results/fct/
    rm $SCRIPTPATH/results/fct/upload.tar.gz
    echo "FlowCompletionTime app done"
}

#-----------------------------------------------------------------------------#
run_httperf_app() {
    for i in ${!global_perf_rate_vec[@]}; do
	global_delay=$global_def_delay
	speed=$global_def_speed
	rate=${global_perf_rate_vec[$i]}

	# TCP scenario with|without TSO
	# run_pure_tcp "httperf" "off"
	# sleep 30
	# run_pure_tcp "httperf" "on"
	# sleep 30

	# TCP2TCP_U scenario with|without TSO
	# run_user_pep "httperf" "off"
	# sleep 30
	# run_user_pep "httperf" "on"
	# sleep 30

	# TCP2TCP scenario with|without TSO
	# run_pepdna "httperf" "pepdna_tcp2tcp_with_tso" "on"
	# sleep 30
	# run_pepdna "httperf" "pepdna_tcp2tcp_without_tso" "off"
	# sleep 30

	# TCP2RINA scenario with|without TSO
	# run_pepdna "httperf" "pepdna_tcp2rina_with_tso" "on"
	# sleep 30
	# run_pepdna "httperf" "pepdna_tcp2rina_without_tso" "off"
	# sleep 30

	# TCP2RINA2TCP scenario with|without TSO
	# run_pepdna "httperf" "pepdna_tcp2rina2tcp_with_tso" "on"
	# sleep 30
	# run_pepdna "httperf" "pepdna_tcp2rina2tcp_without_tso" "off"
	# sleep 30
    done
}

#-----------------------------------------------------------------------------#
run_cpumemperf_app() {
    for i in ${!global_perf_concon_vec[@]}; do
	global_delay=$global_def_delay
	speed=$global_def_speed
	concon=${global_perf_concon_vec[$i]}

	# TCP scenario with|without TSO
	# run_pure_tcp "cpumemperf" "off"
	# sleep 30
	run_pure_tcp "cpumemperf" "on"
	sleep 30

	# TCP2TCP_U scenario with|without TSO
	# run_user_pep "cpumemperf" "off"
	# sleep 30
	run_user_pep "cpumemperf" "on"
	sleep 30

	# TCP2TCP scenario with|without TSO
	# run_pepdna "cpumemperf" "pepdna_tcp2tcp_with_tso" "on"
	# sleep 30
	run_pepdna "cpumemperf" "pepdna_tcp2tcp_without_tso" "off"
	sleep 30

	# TCP2RINA with|without TSO
	# run_pepdna "cpumemperf" "pepdna_tcp2rina_with_tso" "on"
	# sleep 30
	run_pepdna "cpumemperf" "pepdna_tcp2rina_without_tso" "off"
	sleep 30

	# TCP2RINA2TCP with|without TSO (nnodes should have been set 4 here)
	# run_pepdna "cpumemperf" "pepdna_tcp2rina2tcp_with_tso" "on"
	# sleep 10
	# run_pepdna "cpumemperf" "pepdna_tcp2rina2tcp_without_tso" "off"
	# sleep 10
    done
    # collect CPU and MEMORY usage results
    if [ $nnodes -eq '2' ]; then
	ssh $server_gw_host "cd ${global_path} && sudo tar -cvzf cpu.tar.gz cpu_*"
	ssh $server_gw_host "cd ${global_path} && sudo tar -cvzf mem.tar.gz mem_*"
	scp $server_gw_host:${global_path}cpu.tar.gz $SCRIPTPATH/results/cpumemperf/
	scp $server_gw_host:${global_path}mem.tar.gz $SCRIPTPATH/results/cpumemperf/
	ssh $server_gw_host "sudo rm -f ${global_path}/cpu.tar.gz"
	ssh $server_gw_host "sudo rm -f ${global_path}/mem.tar.gz"
    elif [ $nnodes -gt '2' ]; then
	ssh $client_gw_host "cd ${global_path} && sudo tar -cvzf cpu.tar.gz cpu_*"
	ssh $client_gw_host "cd ${global_path} && sudo tar -cvzf mem.tar.gz mem_*"
	scp $client_gw_host:${global_path}cpu.tar.gz $SCRIPTPATH/results/cpumemperf/
	scp $client_gw_host:${global_path}mem.tar.gz $SCRIPTPATH/results/cpumemperf/
	ssh $client_gw_host "sudo rm -f ${global_path}/cpu.tar.gz"
	ssh $client_gw_host "sudo rm -f ${global_path}/mem.tar.gz"
    fi

    tar -xvzf $SCRIPTPATH/results/cpumemperf/cpu.tar.gz -C $SCRIPTPATH/results/cpumemperf/
    rm $SCRIPTPATH/results/cpumemperf/cpu.tar.gz
    tar -xvzf $SCRIPTPATH/results/cpumemperf/mem.tar.gz -C $SCRIPTPATH/results/cpumemperf/
    rm $SCRIPTPATH/results/cpumemperf/mem.tar.gz
    echo "CPUMEMPERF app done"
}
