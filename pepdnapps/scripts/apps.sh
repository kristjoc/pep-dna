# -*- bash -*-

#
# Written by: Kr1stj0n C1k0 <kristjoc@ifi.uio.no>
#

. param.sh

#-----------
# PING APP
#-----------
ping() {
        local client=$1
        local dst_ip=$2

        CMD="ping -c ${global_count} ${dst_ip} 2>&1 | grep statistics -A 2 >> ${global_path}ping.dat"
        ssh $client $CMD

        sleep 3
}

#--------------------
# RINA-ECHO-TIME APP
#--------------------
rina_echo_time() {
        local client=$1
        local server=$2

        CMD="sudo rina-echo-time -l -r -d normal.DIF > /dev/null 2>&1 &"
        ssh $server $CMD
        sleep 3
        CMD="sudo rina-echo-time -c ${global_count} -d normal.DIF 2>&1 | grep SDUs -A 3 >> ${global_path}rina_echo_time.dat"
        ssh $client $CMD
        sleep 1
}

#--------------------
# HTTPING APP
#--------------------
httping() {
        local client=$1
        local server=$2
        local dst_ip=$3
        local lport=$4
        local scenario=$5

        CMD="sudo service apache2 restart"
        ssh $server $CMD
        sleep 3

        CMD='httping -v -c '${global_count}' http://'${dst_ip}':'${lport}' 2>&1 | grep statistics -A 3 >> '${global_path}'httping_'${scenario}'_'${speed}'.dat'
        echo $CMD
        ssh $client $CMD

        sleep 1
}

#--------------------
# MYHTTPING APP
#--------------------
myhttping() {
        local client=$1
        local server=$2
        local dst_ip=$3
        local lport=$4
        local scenario=$5
        local mode=$6
        local cpu=0

        # TCP2RINA mode
        if [[ $scenario == *"pepdna_tcp2rina"* ]]; then
                if [ $mode == "noninc_np" ]; then
                        CMD='sudo service apache2 stop;
                             sudo service lighttpd stop;
                             sleep 1;
                             sudo taskset -c '${cpu}' rinahttping -s -p '${lport}' -a > /dev/null 2>&1'
                        ssh $server $CMD
                        sleep 1

                        CMD='sudo taskset -c '${cpu}' myhttping -c '${dst_ip}' -p '${lport}' -n '${global_count}' -a 2>&1 | grep statistics -A 2 >> '${global_path}'myhttping_'${scenario}'_'${mode}'_'${speed}'.dat;
                             sleep 5'
                        ssh $client $CMD
                        sleep 3
                        return
                elif [ $mode == "noninc_p" ]; then
                        CMD='sudo service apache2 stop;
                             sudo service lighttpd stop;
                             sleep 1;
                             sudo taskset -c '${cpu}' rinahttping -s -p '${lport}' > /dev/null 2>&1'
                        ssh $server $CMD
                        sleep 1

                        CMD='sudo taskset -c '${cpu}' myhttping -c '${dst_ip}' -p '${lport}' -n '${global_count}' 2>&1 | grep statistics -A 2 >> '${global_path}'myhttping_'${scenario}'_'${mode}'_'${speed}'.dat;
                             sleep 5'
                        ssh $client $CMD
                        sleep 3
                        return
                elif [ $mode == "inc_np" ]; then
                        for i in $(seq 1 ${global_count}); do
                                CMD='sudo service apache2 stop;
                                     sudo service lighttpd stop;
                                     sudo killall -2 rinahttping;
                                     sudo killall -9 rinahttping;
                                     sleep 1;
                                     sudo taskset -c '${cpu}' rinahttping -s -p '${lport}' -i -a > /dev/null 2>&1'
                                ssh $server $CMD
                                sleep 1

                                CMD='sudo taskset -c '${cpu}' myhttping -c '${dst_ip}' -p '${lport}' -i -a >> '${global_path}'myhttping_'${scenario}'_'${mode}'_'${speed}'.dat;
                                     sleep 10'
                                ssh $client $CMD
                                sleep 3
                        done
                        return
                elif [ $mode == "inc_p" ]; then
                        CMD='sudo service apache2 stop;
                             sudo service lighttpd stop;
                             sleep 1;
                             sudo taskset -c '${cpu}' rinahttping -s -p '${lport}' -i > /dev/null 2>&1'
                        ssh $server $CMD
                        sleep 1

                        CMD='for i in $(seq 1 '${global_count}'); do
                                 sudo taskset -c '${cpu}' myhttping -c '${dst_ip}' -p '${lport}' -i >> '${global_path}'myhttping_'${scenario}'_'${mode}'_'${speed}'.dat;
                                 sleep 10;
                             done'
                        ssh $client $CMD
                        sleep 3
                        return
                fi
        elif [[ $scenario == *"pepdna_tcp2tcp"* ]] || [[ $scenario == *"pep"* ]]; then
                if [ $mode == "noninc_np" ]; then
                        CMD='sudo service apache2 stop;
                             sudo service lighttpd stop;
                             sleep 1;
                             sudo taskset -c '${cpu}' myhttping -s -p '${lport}' -a > /dev/null 2>&1'
                        ssh $server $CMD
                        sleep 1

                        CMD='sudo taskset -c '${cpu}' myhttping -c '${dst_ip}' -p '${lport}' -n '${global_count}' -a 2>&1 | grep statistics -A 2 >> '${global_path}'myhttping_'${scenario}'_'${mode}'_'${speed}'.dat;
                             sleep 5'
                        ssh $client $CMD
                        sleep 3
                        return
                elif [ $mode == "noninc_p" ]; then
                        CMD='sudo service apache2 stop;
                             sudo service lighttpd stop;
                             sleep 1;
                             sudo taskset -c '${cpu}' myhttping -s -p '${lport}' > /dev/null 2>&1'
                        ssh $server $CMD
                        sleep 1

                        CMD='sudo taskset -c '${cpu}' myhttping -c '${dst_ip}' -p '${lport}' -n '${global_count}' 2>&1 | grep statistics -A 2 >> '${global_path}'myhttping_'${scenario}'_'${mode}'_'${speed}'.dat;
                             sleep 5'
                        ssh $client $CMD
                        sleep 3
                        return
                elif [ $mode == "inc_np" ]; then
                        for i in $(seq 1 ${global_count}); do
                                CMD='sudo service apache2 stop;
                                     sudo service lighttpd stop;
                                     sudo killall -2 myhttping;
                                     sudo killall -9 myhttping;
                                     sleep 1;
                                     sudo taskset -c '${cpu}' myhttping -s -p '${lport}' -i -a > /dev/null 2>&1'
                                ssh $server $CMD
                                sleep 1

                                CMD='sudo taskset -c '${cpu}' myhttping -c '${dst_ip}' -p '${lport}' -i -a >> '${global_path}'myhttping_'${scenario}'_'${mode}'_'${speed}'.dat;
                                     sleep 10'
                                ssh $client $CMD
                                sleep 3
                        done
                        return
                elif [ $mode == "inc_p" ]; then
                        CMD='sudo service apache2 stop;
                             sudo service lighttpd stop;
                             sleep 1;
                             sudo taskset -c '${cpu}' myhttping -s -p '${lport}' -i > /dev/null 2>&1'
                        ssh $server $CMD
                        sleep 1

                        CMD='for i in $(seq 1 '${global_count}'); do
                                sudo taskset -c '${cpu}' myhttping -c '${dst_ip}' -p '${lport}' -i >> '${global_path}'myhttping_'${scenario}'_'${mode}'_'${speed}'.dat;
                                sleep 10;
                             done'
                        ssh $client $CMD
                        sleep 3
                        return
                fi
        elif [ $scenario == "pure_rina" ]; then
                if [ $mode == "noninc_np" ]; then
                        CMD='sudo service apache2 stop;
                             sudo service lighttpd stop;
                             sleep 1;
                             sudo taskset -c '${cpu}' rinahttping -s -p '${lport}' -a > /dev/null 2>&1'
                        ssh $server $CMD
                        sleep 1

                        CMD='sudo taskset -c '${cpu}' rinahttping -c '${dst_ip}' -p '${lport}' -n '${global_count}' -a 2>&1 | grep statistics -A 2 >> '${global_path}'myhttping_'${scenario}'_'${mode}'_'${speed}'.dat;
                             sleep 5'
                        ssh $client $CMD
                        sleep 3
                        return
                elif [ $mode == "noninc_p" ]; then
                        CMD='sudo service apache2 stop;
                             sudo service lighttpd stop;
                             sleep 1;
                             sudo taskset -c '${cpu}' rinahttping -s -p '${lport}' > /dev/null 2>&1'
                        ssh $server $CMD
                        sleep 1

                        CMD='sudo taskset -c '${cpu}' rinahttping -c '${dst_ip}' -p '${lport}' -n '${global_count}' 2>&1 | grep statistics -A 2 >> '${global_path}'myhttping_'${scenario}'_'${mode}'_'${speed}'.dat;
                             sleep 5'
                        ssh $client $CMD
                        sleep 3
                        return
                elif [ $mode == "inc_np" ]; then
                        for i in $(seq 1 ${global_count}); do
                                CMD='sudo service apache2 stop;
                                     sudo service lighttpd stop;
                                     sudo killall -2 rinahttping;
                                     sudo killall -9 rinahttping;
                                     sleep 1;
                                     sudo taskset -c '${cpu}' rinahttping -s -p '${lport}' -i -a > /dev/null 2>&1'
                                ssh $server $CMD
                                sleep 1

                                CMD='sudo taskset -c '${cpu}' rinahttping -c '${dst_ip}' -p '${lport}' -i -a >> '${global_path}'myhttping_'${scenario}'_'${mode}'_'${speed}'.dat;
                                     sleep 10'
                                ssh $client $CMD
                                sleep 3
                        done
                        return
                elif [ $mode == "inc_p" ]; then
                        CMD='sudo service apache2 stop;
                             sudo service lighttpd stop;
                             sleep 1;
                             sudo taskset -c '${cpu}' rinahttping -s -p '${lport}' -i > /dev/null 2>&1'
                        ssh $server $CMD
                        sleep 1

                        CMD='for i in $(seq 1 '${global_count}'); do
                                sudo taskset -c '${cpu}' rinahttping -c '${dst_ip}' -p '${lport}' -i >> '${global_path}'myhttping_'${scenario}'_'${mode}'_'${speed}'.dat;
                                sleep 10;
                             done'
                        ssh $client $CMD
                        sleep 3
                        return
                fi
        else
                if [ $mode == "noninc_np" ]; then
                        CMD='sudo service apache2 stop;
                             sudo service lighttpd stop;
                             sleep 1;
                             sudo taskset -c '${cpu}' myhttping -s -p '${lport}' -a > /dev/null 2>&1'
                        ssh $server $CMD
                        sleep 1

                        CMD='sudo taskset -c '${cpu}' myhttping -c '${dst_ip}' -p '${lport}' -n '${global_count}' -a 2>&1 | grep statistics -A 2 >> '${global_path}'myhttping_'${scenario}'_'${mode}'_'${speed}'.dat;
                             sleep 5'
                        ssh $client $CMD
                        sleep 3
                        return
                elif [ $mode == "noninc_p" ]; then
                        CMD='sudo service apache2 stop;
                             sudo service lighttpd stop;
                             sleep 1;
                             sudo taskset -c '${cpu}' myhttping -s -p '${lport}' > /dev/null 2>&1'
                        ssh $server $CMD
                        sleep 1

                        CMD='sudo taskset -c '${cpu}' myhttping -c '${dst_ip}' -p '${lport}' -n '${global_count}' 2>&1 | grep statistics -A 2 >> '${global_path}'myhttping_'${scenario}'_'${mode}'_'${speed}'.dat;
                             sleep 5'
                        ssh $client $CMD
                        sleep 3
                        return
                elif [ $mode == "inc_np" ]; then
                        for i in $(seq 1 ${global_count}); do
                                CMD='sudo service apache2 stop;
                                     sudo service lighttpd stop;
                                     sudo killall -2 myhttping;
                                     sudo killall -9 myhttping;
                                     sleep 1;
                                     sudo taskset -c '${cpu}' myhttping -s -p '${lport}' -i -a > /dev/null 2>&1'
                                ssh $server $CMD
                                sleep 1

                                CMD='sudo taskset -c '${cpu}' myhttping -c '${dst_ip}' -p '${lport}' -i -a >> '${global_path}'myhttping_'${scenario}'_'${mode}'_'${speed}'.dat;
                                     sleep 10'
                                ssh $client $CMD
                                sleep 3
                        done
                        return
                elif [ $mode == "inc_p" ]; then
                        CMD='sudo service apache2 stop;
                             sudo service lighttpd stop;
                             sleep 1;
                             sudo taskset -c '${cpu}' myhttping -s -p '${lport}' -i > /dev/null 2>&1'
                        ssh $server $CMD
                        sleep 1

                        CMD='for i in $(seq 1 '${global_count}'); do
                                sudo taskset -c '${cpu}' myhttping -c '${dst_ip}' -p '${lport}' -i >> '${global_path}'myhttping_'${scenario}'_'${mode}'_'${speed}'.dat;
                                sleep 10;
                             done'
                        ssh $client $CMD
                        sleep 3
                        return
                fi
        fi
        sleep 3
}

#---------------------
# THROUGHPUT TEST
#---------------------
upload() {
        local client=$1
        local server=$2
        local dst_ip=$3
        local lport=$4
        local scenario=$5
        local cpu=0

        # TCP2RINA mode
        if [[ $scenario == *"pepdna_tcp2rina"* ]]; then
                CMD="sudo service apache2 stop;
                     sleep 1;
                     sudo taskset -c ${cpu} rinahttp -s > /dev/null 2>&1"
                ssh $server $CMD
                sleep 1

                CMD='for i in $(seq 1 '${global_count}'); do
                        sudo rm -f '${global_filename}';
                        sudo taskset -c '${cpu}' httpapp -u -c http://'${dst_ip}':'${lport}'/'${global_filename}' >> '${global_path}'fc.dat;
                        sleep 10;
                        sudo rm -f '${global_filename}';
                        sleep 5;
                     done;
                     tail --lines=+2 '${global_path}'fc.dat >> '${global_path}'upload_'${scenario}'_'${speed}'.dat;
                     sudo rm -f '${global_path}'fc.dat;
                     sleep 1'
                ssh $client $CMD
                sleep 3

                CMD="tail --lines=+2 '${global_path}'fs.dat >> '${global_path}'upload_'${scenario}'_'${speed}'.dat;
                     sudo rm -f '${global_path}'fs.dat"
                ssh $server $CMD
                sleep 1
                return
        # TCP2TCP (with PEPDNA or UserSpace PEP)
        elif [[ $scenario == *"pepdna_tcp2tcp"* ]] || [[ $scenario == *"pep"* ]]; then
                CMD="sudo service apache2 stop;
                     sleep 1;
                     sudo taskset -c ${cpu} httpapp -s -p ${lport} > /dev/null 2>&1"
                ssh $server $CMD
                sleep 1

                CMD='for i in $(seq 1 '${global_count}'); do
                        sudo rm -f '${global_filename}';
                        sudo taskset -c '${cpu}' httpapp -u -c http://'${dst_ip}':'${lport}'/'${global_filename}' >> '${global_path}'fc.dat;
                        sleep 10;
                        sudo rm -f '${global_filename}';
                        sleep 5;
                     done;
                     tail --lines=+2 '${global_path}'fc.dat >> '${global_path}'upload_'${scenario}'_'${speed}'.dat;
                     sudo rm -f '${global_path}'fc.dat;
                     sleep 1'
                ssh $client $CMD
                sleep 3

                CMD="tail --lines=+2 '${global_path}'fs.dat >> '${global_path}'upload_'${scenario}'_'${speed}'.dat;
                     sudo rm -f '${global_path}'fs.dat"
                ssh $server $CMD
                sleep 1
                return
        #RINA
        elif [ $scenario == "pure_rina" ]; then
                CMD="sudo service apache2 stop;
                     sleep 1;
                     sudo taskset -c ${cpu} rinahttp -s > /dev/null 2>&1"
                ssh $server $CMD
                sleep 1

                CMD='for i in $(seq 1 '${global_count}'); do
                        sudo rm -f '${global_filename}';
                        sudo taskset -c '${cpu}' rinahttp -u -c http://'${dst_ip}':'${lport}'/'${global_filename}' >> '${global_path}'fc.dat;
                        sleep 10;
                        sudo rm -f '${global_filename}';
                        sleep 5;
                     done;
                     tail --lines=+2 '${global_path}'fc.dat >> '${global_path}'upload_'${scenario}'_'${speed}'.dat;
                     sudo rm -f '${global_path}'fc.dat;
                     sleep 1'
                ssh $client $CMD
                sleep 3

                CMD="tail --lines=+2 '${global_path}'fs.dat >> '${global_path}'upload_'${scenario}'_'${speed}'.dat;
                     sudo rm -f '${global_path}'fs.dat"
                ssh $server $CMD
                sleep 1
                return
        #TCP2RINA2TCP
        else
                CMD="sudo service apache2 stop;
                     sleep 1;
                     sudo taskset -c ${cpu} httpapp -s -p ${lport} > /dev/null 2>&1"
                ssh $server $CMD
                sleep 1

                CMD='for i in $(seq 1 '${global_count}'); do
                        sudo rm -f '${global_filename}';
                        sudo taskset -c '${cpu}' httpapp -u -c http://'${dst_ip}':'${lport}'/'${global_filename}' >> '${global_path}'fc.dat;
                        sleep 10;
                        sudo rm -f '${global_filename}';
                        sleep 2;
                     done;
                     tail --lines=+2 '${global_path}'fc.dat >> '${global_path}'upload_'${scenario}'_'${speed}'.dat;
                     sudo rm -f '${global_path}'fc.dat;
                     sleep 1'
                ssh $client $CMD

                CMD="tail --lines=+2 '${global_path}'fs.dat >> '${global_path}'upload_'${scenario}'_'${speed}'.dat;
                     sudo rm -f '${global_path}'fs.dat"
                ssh $server $CMD
        fi
        sleep 3
}

#--------------------
# HTTPERF TEST
#-------------------
httperf() {
        local client=$1
        local server=$2
        local dst_ip=$3
        local lport=$4
        local scenario=$5
        local rate=$6
        local cpu1=0
        local cpu2=1

        # 20 seconds
        num_conn=$(echo "scale=2; $rate * 20" | bc)
        # 2 httperf instances
        total_rate=$(echo "scale=2; $rate * 2" | bc)
        num_calls=1

        # TCP2RINA mode
        if [[ $scenario == *"pepdna_tcp2rina"* ]]; then
                CMD='sudo service apache2 stop;
                     sudo service lighttpd stop;
                     sudo killall -2 rinahttping;
                     sudo killall -9 rinahttping;
                     sleep 1;
                     sudo taskset -c '${cpu1}' rinahttping -s -f -p '${lport}' > /dev/null 2>&1;
                     sleep 1;
                     sudo mycpuscr >> '${global_path}'cpu_'${scenario}'_'${total_rate}'.dat & disown;
                     sudo mymemscr >> '${global_path}'mem_'${scenario}'_'${total_rate}'.dat &'
                ssh $server $CMD
                sleep 3
                CMD="sudo taskset -c ${cpu1} python ${global_path}webapps/httperfpy/httperfpy/myHTTPerf.py \
                        -c ${num_conn} -r ${rate} -s ${num_calls} -t 30 http://${dst_ip}:${lport} >> ${global_path}httperpy1_${scenario}_${total_rate}.dat & disown;
                     sudo taskset -c ${cpu2} python ${global_path}webapps/httperfpy/httperfpy/myHTTPerf.py \
                             -c ${num_conn} -r ${rate} -s ${num_calls} -t 30 http://${dst_ip}:${lport} >> ${global_path}httperpy2_${scenario}_${total_rate}.dat"
                ssh $client $CMD

                CMD='sudo echo '---' >> '${global_path}'httperpy1_'${scenario}'_'${total_rate}'.dat;
                     sudo echo '---' >> '${global_path}'httperpy2_'${scenario}'_'${total_rate}'.dat'
                ssh $client $CMD
                sleep 3

                CMD='sudo killall -9 mycpuscr;
                     sudo killall -9 mymemscr'
                ssh $server $CMD
                sleep 10
                return
        elif [[ $scenario == *"pepdna_tcp2tcp"* ]] || [[ $scenario == *"pep"* ]]; then
                CMD='sudo service apache2 stop;
                     sudo service lighttpd stop;
                     sudo killall -2 rinahttping;
                     sudo killall -9 rinahttping;
                     sleep 1;
                     sudo taskset -c '${cpu1}' myhttping -s -f -p '${lport}' > /dev/null 2>&1;
                     sleep 1;
                     sudo mycpuscr >> '${global_path}'cpu_'${scenario}'_'${total_rate}'.dat & disown;
                     sudo mymemscr >> '${global_path}'mem_'${scenario}'_'${total_rate}'.dat &'
                ssh $server $CMD
                sleep 3

                CMD="sudo taskset -c ${cpu1} python ${global_path}webapps/httperfpy/httperfpy/myHTTPerf.py \
                        -c ${num_conn} -r ${rate} -s ${num_calls} -t 30 http://${dst_ip}:${lport} >> ${global_path}httperpy1_${scenario}_${total_rate}.dat & disown;
                     sudo taskset -c ${cpu2} python ${global_path}webapps/httperfpy/httperfpy/myHTTPerf.py \
                             -c ${num_conn} -r ${rate} -s ${num_calls} -t 30 http://${dst_ip}:${lport} >> ${global_path}httperpy2_${scenario}_${total_rate}.dat"
                ssh $client $CMD

                CMD='sudo echo '---' >> '${global_path}'httperpy1_'${scenario}'_'${total_rate}'.dat;
                     sudo echo '---' >> '${global_path}'httperpy2_'${scenario}'_'${total_rate}'.dat'
                ssh $client $CMD
                sleep 3

                CMD='sudo killall -9 mycpuscr;
                     sudo killall -9 mymemscr'
                ssh $server $CMD
                sleep 10
                return
        else    # TCP mode
                CMD='sudo service apache2 stop;
                     sudo service lighttpd stop;
                     sudo killall -2 rinahttping;
                     sudo killall -9 rinahttping;
                     sleep 1;
                     sudo taskset -c '${cpu1}' myhttping -s -f -p '${lport}' > /dev/null 2>&1;
                     sleep 1;
                     sudo mycpuscr >> '${global_path}'cpu_'${scenario}'_'${total_rate}'.dat & disown;
                     sudo mymemscr >> '${global_path}'mem_'${scenario}'_'${total_rate}'.dat &'
                     ssh $server $CMD
                     sleep 3

                CMD="sudo taskset -c ${cpu1} python ${global_path}webapps/httperfpy/httperfpy/myHTTPerf.py \
                        -c ${num_conn} -r ${rate} -s ${num_calls} -t 30 http://${dst_ip}:${lport} >> ${global_path}httperpy1_${scenario}_${total_rate}.dat & disown;
                     sudo taskset -c ${cpu2} python ${global_path}webapps/httperfpy/httperfpy/myHTTPerf.py \
                             -c ${num_conn} -r ${rate} -s ${num_calls} -t 30 http://${dst_ip}:${lport} >> ${global_path}httperpy2_${scenario}_${total_rate}.dat"
                ssh $client $CMD

                CMD='sudo echo '---' >> '${global_path}'httperpy1_'${scenario}'_'${total_rate}'.dat;
                     sudo echo '---' >> '${global_path}'httperpy2_'${scenario}'_'${total_rate}'.dat'
                ssh $client $CMD
                sleep 3

                CMD='sudo killall -9 mycpuscr;
                sudo killall -9 mymemscr'
                ssh $server $CMD
                sleep 10
                return
        fi
}

#---------------------------
# CPU and Memory Usage Test
#---------------------------
cpumemperf() {
        local client=$1
        local server=$2
        local dst_ip=$3
        local lport=$4
        local scenario=$5
        local concon=$6
        local cpu1=8
        local cpu2=9

        # TCP2RINA mode
        if [[ $scenario == *"pepdna_tcp2rina"* ]]; then
                CMD='sudo service apache2 stop;
                     sudo service lighttpd stop;
                     sudo killall -2 rinahttping;
                     sudo killall -9 rinahttping;
                     sudo killall -2 myhttping;
                     sudo killall -9 myhttping;
                     sleep 1;
                     sudo taskset -c '${cpu1}' rinahttping -s -m -p '${lport}' > /dev/null 2>&1;
                     sleep 1;
                     sudo mysar 35 >> '${global_path}'cpu_'${scenario}'_'${concon}'.dat & disown;
                     sudo mymemscr >> '${global_path}'mem_'${scenario}'_'${concon}'.dat &'
                ssh $server $CMD
                sleep 3

                CMD="sudo taskset -c ${cpu1} myhttping -c ${dst_ip} -p ${lport} -m -n ${concon} >> ${global_path}cpumemperf_${scenario}_${concon}.dat & disown;"
                ssh $client $CMD

                CMD='sudo killall -9 mycpuscr;
                     sudo killall -9 mymemscr;
                     sudo killall -9 mysar'
                ssh $server $CMD
                sleep 10
                return
        elif [[ $scenario == *"pepdna_tcp2tcp"* ]] || [[ $scenario == *"pep"* ]]; then
                CMD='sudo service apache2 stop;
                     sudo service lighttpd stop;
                     sudo killall -2 rinahttping;
                     sudo killall -9 rinahttping;
                     sudo killall -2 myhttping;
                     sudo killall -9 myhttping;
                     sleep 1;
                     sudo taskset -c '${cpu1}' myhttping -s -m -p '${lport}' > /dev/null 2>&1;
                     sleep 1;
                     sudo mysar 35 >> '${global_path}'cpu_'${scenario}'_'${concon}'.dat & disown;
                     sudo mymemscr >> '${global_path}'mem_'${scenario}'_'${concon}'.dat &'
                ssh $server $CMD
                sleep 1

                CMD="sudo taskset -c ${cpu1} myhttping -c ${dst_ip} -p ${lport} -m -n ${concon} >> ${global_path}cpumemperf_${scenario}_${concon}.dat"
                ssh $client $CMD

                CMD='sudo killall -9 mycpuscr;
                     sudo killall -9 mymemscr;
                     sudo killall -9 mysar'
                ssh $server $CMD
                sleep 10
                return
        else    # TCP mode - Same as TCP2TCP
                CMD='sudo service apache2 stop;
                     sudo service lighttpd stop;
                     sudo killall -2 rinahttping;
                     sudo killall -9 rinahttping;
                     sudo killall -2 myhttping;
                     sudo killall -9 myhttping;
                     sleep 1;
                     sudo taskset -c '${cpu1}' myhttping -s -m -p '${lport}' > /dev/null 2>&1;
                     sleep 1;
                     sudo mysar 35 >> '${global_path}'cpu_'${scenario}'_'${concon}'.dat & disown;
                     sudo mymemscr >> '${global_path}'mem_'${scenario}'_'${concon}'.dat &'
                ssh $server $CMD
                sleep 1

                CMD="sudo taskset -c ${cpu1} myhttping -c ${dst_ip} -p ${lport} -m -n ${concon} >> ${global_path}cpumemperf_${scenario}_${concon}.dat"
                ssh $client $CMD

                CMD='sudo killall -9 mycpuscr;
                     sudo killall -9 mymemscr;
                     sudo killall -9 mysar'
                ssh $server $CMD
                sleep 10
                return
        fi
}
