sudo modprobe 8021q
sudo modprobe rinarp

sudo ip link add link ens33 name ens33.120 type vlan id 120
sudo ip link set dev ens33 up
sudo ip link set dev ens33.120 up

sudo modprobe rina-irati-core irati_verbosity=7
sudo modprobe rina-default-plugin
sudo modprobe normal-ipcp
sudo modprobe shim-eth-vlan
sudo ipcm -c /etc/eg-pep-ipcmanager.conf
