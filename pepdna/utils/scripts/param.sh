# -*- bash -*-

#
# Written by: Kr1stj0n C1k0 <kristjoc@ifi.uio.no>
#

readonly localhost="127.0.0.1"
# change global_count to 'n' to run the experiments 'n' times
readonly global_count="100"
readonly global_perf_rate_vec=("50" "100" "250" "500" "1000" "2500" "5000")
readonly global_perf_concon_vec=("100" "200" "500" "1000" "2000" "5000" "10000")
readonly global_mss="1451"
# global_lport is the TCP listening port of the server application
readonly global_lport="8080"
# PEP-DNA listens for incoming connections on port 9999
readonly global_proxy_port="9999"
# Sock Mark needed to achieve transparency when PEP-DNA is running at the same host
# as the server
readonly global_mark="333"
# For the FlowCompletionTime experiment to work, a 4GB file needs to be generated at
# /var/www/web/ at server side; Generate using dd
readonly global_filename="4g.bin"
# No delay emulated
readonly global_def_delay="0"
# Default ethernet speed 10000Mbps
readonly global_def_speed="10000"
readonly global_speed_vec=("10000")
# If you want to run the experiments for different ethernet speeds, uncomment the line below
# readonly global_speed_vec=("1000" "10000")
global_delay="0"
global_bufsize=0

vmware_variables() {
        # topology: client -- client_gateway -- server_gateway -- server
        readonly client_host="client"
        # readonly client_host="192.168.0.101"
        readonly client_ip="10.10.10.102"
        readonly client_iface="enp0s8"

        readonly client_gw_host="lrt"
        # readonly client_gw_host="192.168.0.102"
        readonly client_gw_right_ip="10.10.20.101"
        readonly client_gw_right_iface="enp0s9"
        readonly client_gw_left_ip="10.10.10.101"
        readonly client_gw_left_iface="enp0s8"

        readonly server_gw_host="rrt"
        # readonly server_gw_host="192.168.0.103"
        readonly server_gw_right_ip="10.10.30.101"
        readonly server_gw_right_iface="enp0s9"
        readonly server_gw_left_ip="10.10.20.102"
        readonly server_gw_left_iface="enp0s8"

        readonly server_host="server"
	# readonly server_host="192.168.0.104"
	readonly server_ip="10.10.30.102"
        readonly server_iface="enp0s8"

        readonly global_path="/home/kristjoc/"
}

testbed_variables() {
        # Supported topology: hylia --- midna
        readonly client_gw_host="hylia"
        readonly client_left_ip="10.100.120.3"
        readonly client_gw_left_iface="10Gd"

	readonly server_gw_host="midna"
	readonly server_gw_right_ip="10.100.120.6"
        readonly server_gw_right_iface="10Gd"

        readonly global_path="/home/ocarina/kristjoc/"
}

if [ $env == 'testbed' ]; then
        testbed_variables
elif [ $env == 'vmware' ]; then
        vmware_variables
else
        echo "Bad env variable"
fi
