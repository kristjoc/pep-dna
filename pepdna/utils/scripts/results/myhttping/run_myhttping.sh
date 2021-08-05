# 1. First, move files to respective directories
#    The directory tree is organized as follows
#         incremental                      nonincremental
#         /         \                      /           \
#        p          np                    p            np
#      /   \      /   \                 /   \         /  \
#    tson tsoff tson tsoff            tson tsoff    tson tsoff

mv *with_tso_inc_p* 10gbps/inc/p/tson/
mv *with_tso_inc_np* 10gbps/inc/np/tson/
mv *with_tso_noninc_p* 10gbps/noninc/p/tson/
mv *with_tso_noninc_np* 10gbps/noninc/np/tson/


mv *without_tso_inc_p* 10gbps/inc/p/tsoff/
mv *without_tso_inc_np* 10gbps/inc/np/tsoff/
mv *without_tso_noninc_p* 10gbps/noninc/p/tsoff/
mv *without_tso_noninc_np* 10gbps/noninc/np/tsoff/

sleep 1
find . -iname "*_inc_*" | \
  while read I; do
    for body_size in 1024 8192 65536 524288 4194304 33554432 268435456; do
      cat "$I" | grep $body_size | awk -F "time" '{print $2}' >> "$I".$body_size.dat
    done
  done

filename=("pure_tcp" "user_pep" "pepdna_tcp2tcp" "pepdna_tcp2rina" "pepdna_tcp2ccn")
find "$(cd ..; pwd)" -iname "myhttping.py" | \
  while read I; do
    cd $(dirname `greadlink -f "$I"`)
    for i in ${!filename[@]}; do
      for size in 1024 8192 65536 524288 4194304 33554432 268435456; do
        yourfilenames=`ls | grep ${filename[$i]} | grep $size`
        for eachfile in $yourfilenames; do
          python3.9 welford.py $eachfile
        done
      done
    done
  done


find "$(cd ..; pwd)" -iname "myhttping.dat" | \
  while read I; do
    cd $(dirname `greadlink -f "$I"`)
    python3.9 myhttping.py &
  done

# Remove temporary .dat. files
find . -iname "*.dat.*" | \
  while read I; do
    rm "$I"
  done

# Remove .dat files
sleep 5
find . -iname "myhttping.dat" | \
  while read I; do
    rm "$I"
  done
