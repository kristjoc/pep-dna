sudo modprobe 8021q
sudo modprobe rinarp

sudo ip link add link ens32 name ens32.110 type vlan id 110
sudo ip link set dev ens32 up
sudo ip link set dev ens32.110 up

sudo modprobe rina-irati-core irati_verbosity=7
sudo modprobe rina-default-plugin
sudo modprobe normal-ipcp
sudo modprobe shim-eth-vlan
sudo ipcm -c /etc/ing-pep-ipcmanager.conf

