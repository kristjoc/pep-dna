Automated testing framework
===========================

This document provides detailed information on the Automated testing framework
we use to evaluate the efficiency and performance of PEP-DNA of PEP-DNA.
The framework consists of 6 bash scripts:

* 'param.sh' contains all the necessary information about testing environments.
   The user can setup a physical or virtual testbed. Variables in this files store
   the information about the IP addresses of the nodes, connected interfaces, TCP
   listening port of PEP-DNA and applications, etc. This file contains the only input
   that the user should enter to run the experiments.

* 'config.sh' contains the functions that are called to configure Linux hosts in the network.
   Configurations include static routes, iptables, ethernet speeds, TCP MSS, buffer sizes,
   sysctl variables, etc.

* 'util.sh' prepares the hosts by calling the functions defined in 'config.sh',
   loads RINA modules, starts the enrollment phase if necessary, load PEP-DNA module, etc.

* 'apps.sh' contains functions that start the client and server applications.
   For the experiments in the paper, 3 main applications are used:
   - CPUMEMPERF(): measures the CPU Utilization and Memory Usage.
   - UPLOAD(): measures the throughput during a file transfer from the client to the server
   - MYHTTTPING(): measure all latency results.

* 'run.sh' contains the setup of the different scenarios used in the paper.
   - TCP
   - TCP - TCP  (using PEPDNA)
   - TCP - TCP  (using a User-Space Proxy)
   - TCP - RINA (using PEPDNA)
   - TCP - CCN  (using PEPDNA)

* 'main.sh' has the main functions that the user need to run. Enable or Disable an experiment
   by commenting or uncommenting a line in this file.


Testing environments and Topologies
-----------------------------------

There are two configured testing environments in the 'param.sh' file, (i) Vmware testbed and
(ii) the Physical testbed. The user can add additional testbeds by configuring different
variables and selecting the number of nodes in the network. So far, up to 4 nodes are suported
in the following topologies:

* 2 nodes
  client ------ server

* 3 nodes
  client ------ client_gw ------ server

* 4 nodes
  client ------ client_gw ------ server_gw ------ server

The experiments in the paper use the first topology where PEP-DNA runs at the same host as the server.
A scenario with 4 nodes for TCP - RINA - TCP case is also included in the scripts.

How to run the experiments
--------------------------

The main bash script needs to be executed from a management host which can access all nodes
via SSH. The SSH public key of the management host must have been first added to ~/.ssh/authorized_keys
files of the nodes, so that entering the passwords during SSH is not required.
   ```sh
   cd /pep-dna/pepdnapps/scripts/
   bash main.sh "testbed"|"vmware" 2|3|4
   ```

Plotting the graphs
-------------------

After all the experiments are finished, the results are automatically collected and stored in the
respective directories in https://github.com/kr1stj0n/pep-dna/tree/main/pepdnapps/scripts/results/
Initially, the directories contain the datasets which are reported in the paper. To plot the graphs,
go to the respective directory and run the bash scripts ```run_NameOfTheApp.sh```.
The script will process the data, call the matplotlib scripts and plot the graphs
which will be saved in /home/ directory. The latest version of Python3 is required in order to run
the matplotlib scripts.

For example, in order to plot the Figure 6 of the paper, run the following commands from the
management host.

   ```sh
   cd /pep-dna/pepdnapps/scripts/results/fct/
   bash run_fct.sh
   ```
