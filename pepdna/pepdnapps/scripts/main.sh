# -*- bash -*-

#
# Written by: Kr1stj0n C1k0 <kristjoc@ifi.uio.no>
#

# How to run: bash main.sh "testbed"|"vmware" 2|3|4

# env - experimenting environment (vmware | testbed)
export env=$1
# nnodes - number of nodes (2 | 4)
# if nnodes = 2 => client -- server
# if nnodes == 3 => client -- client_gw -- server_gw
# if nnodes == 4 => client -- client_gw -- server_gw -- server
export nnodes=$2

if [ -z "$env" ]; then
        echo "\$env is not set"
        echo "Usage: bash main.sh 'env' 'nnodes'"
        exit
fi

if [ -z "$nnodes" ]; then
        echo "\$nnodes is not set"
        echo "Usage: bash main.sh 'env' 'nnodes'"
        exit
fi

. run.sh

#-----------------------------------------------------------------------------#
# MAIN FUNCTION

# PING TESTS
# run_ping_app
# run_rina_echo_time_app
#---------------------------------------
# Legacy HTTPING tests
# run_httping_app
#---------------------------------------
# myHTTPING LATENCY tests
# Table 2 results: nonincremental persistent | nonincremental non-persistent
# Figure 5 results: incremental persistent | nonincremental non-persistent

run_myhttping_app "myhttping_noninc_p"
run_myhttping_app "myhttping_noninc_np"
run_myhttping_app "myhttping_inc_p"
run_myhttping_app "myhttping_inc_np"
#---------------------------------------
# FLOW COMPLETION TIME tests | Figure 6
run_upload_app
#---------------------------------------
# HTTPERF tests
# run_httperf_app
#---------------------------------------
# CPUMEMPERF tests | Figure 4
run_cpumemperf_app
#---------------------------------------
# EOF MAIN
