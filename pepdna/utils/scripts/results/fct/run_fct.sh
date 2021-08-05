#
# Run this bash script to plot Flow Completion Time graph
# Collected results will be automatically retrieved and stores in this directory.
#

# 1. First, move .dat files to temporary result directory
mv *.dat results/

filename=("fct_pure_tcp_with_tso" "fct_pure_tcp_without_tso" \
          "fct_user_pep_with_tso" "fct_user_pep_without_tso" \
          "fct_pepdna_tcp2tcp_with_tso" "fct_pepdna_tcp2tcp_without_tso" \
          "fct_pepdna_tcp2rina_with_tso" "fct_pepdna_tcp2rina_without_tso" \
          "fct_pepdna_tcp2ccn_with_tso" "fct_pepdna_tcp2ccn_without_tso")

cd "results"
rm "fct.dat"
for i in ${!filename[@]}; do
    for rate in 10000; do
        python3 ../welford.py ${filename[$i]}_$rate.dat
    done
done

cd ..

# Plot the graph. It will automatically be saved in /home/ dir
python3 plot_fct.py

# Remove the generated fc.dat file
rm results/fct.dat

open ~/10gbps.eps

# EOF
