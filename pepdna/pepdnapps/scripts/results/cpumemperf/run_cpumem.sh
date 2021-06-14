#
# Run this bash script to plot CPU utilization
# and memory usage graphs
#

filename=("pure_tcp" "user_pep" "pepdna_tcp2tcp")

# 1. First, move .dat files to respective directories according to the folling tree
#            TCP                TCP-TCP         TCP-TCP_U
#            /\                    /\              /\
#        CPU   MEM             CPU   MEM        CPU   MEM

mv cpu_pure_tcp* tcp/cpu/
mv mem_pure_tcp* tcp/mem/

mv cpu_pepdna* tcp2tcp/cpu/
mv mem_pepdna* tcp2tcp/mem/

mv cpu_user_pep* user_pep/cpu/
mv user_pep* user_pep/mem/

# Generate perf_cpu.dat file
find "$(cd ..; pwd)" -iname "cpu_percentile.py" | \
    while read I; do
        cd $(dirname `greadlink -f "$I"`)
        for i in ${!filename[@]}; do
            for rate in 100 200 500 1000 2000 5000 10000; do
                yourfilenames=`ls | grep ${filename[$i]} | grep _${rate}.dat`
                for eachfile in $yourfilenames; do
                    python3 cpu_percentile.py $eachfile
                done
            done
        done
        cat cpu.dat >> ../../perf_cpu.dat
        rm cpu.dat
    done

# Generate perf_mem.dat file
find "$(cd ..; pwd)" -iname "mem_percentile.py" | \
    while read I; do
        cd $(dirname `greadlink -f "$I"`)
        for i in ${!filename[@]}; do
            for rate in 100 200 500 1000 2000 5000 10000; do
                yourfilenames=`ls | grep ${filename[$i]} | grep _${rate}.dat`
                for eachfile in $yourfilenames; do
                    python3 mem_percentile.py $eachfile
                done
            done
        done
        cat mem.dat >> ../../perf_mem.dat
        rm mem.dat
    done

# 
# Plot graphs using plot_cpumem.py matplotlib script
# Graphs will be saved in /home/directory.
# It is highly recommended to use the last Python3 version
#

python3 plot_cpumem.py
