#!/bin/ksh
#
# Copyright (C) 2010 Kazuyoshi Aizawa. All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
#
# 1. Redistributions of source code must retain the above copyright
#   notice, this list of conditions and the following disclaimer.
# 2. Redistributions in binary form must reproduce the above copyright
#   notice, this list of conditions and the following disclaimer in the
#   documentation and/or other materials provided with the distribution.
#
# THIS SOFTWARE IS PROVIDED BY AUTHOR AND CONTRIBUTORS ``AS IS'' AND
# ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
# IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
# ARE DISCLAIMED.  IN NO EVENT SHALL AUTHOR OR CONTRIBUTORS BE LIABLE
# FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
# DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
# OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
# HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
# LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
# OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
# SUCH DAMAGE.
#

#
# Test script of iumfs filesystem
# After build iumfs, run this script.
# You will be prompted password for root user.
#
procs=3 # number of processes for stress test
stresstime=30 # number of seconds for stress test
maxumountretries=3 # number of try of umount
daemonpid=""
mnt="/var/tmp/iumfsmnt"
base="/var/tmp/iumfsbase"
CLASSPATH="\
./cmd/hdfsd.jar:${HADOOP_HOME}/conf:\
./cmd/lib/iumfs-daemon-core.jar:\
${HADOOP_HOME}/hadoop-core-1.0.2.jar:\
${HADOOP_HOME}/lib/commons-lang-2.4.jar:\
${HADOOP_HOME}/lib/commons-configuration-1.6.jar:\
${HADOOP_HOME}/lib/commons-logging-1.1.1.jar"


init (){
        LOGDIR=$PWD/logs
        TESTLOGFILE=$LOGDIR/test-`date '+%Y%m%d-%H:%M:%S'`.log
        DAEMONLOGFILE=$LOGDIR/hdfsd-`date '+%Y%m%d-%H:%M:%S'`.log
        # Create log directory
        if [ ! -d "${LOGDIR}" ]; then
                mkdir ${LOGDIR}
        fi

	# Just in case, umount ${mnt}
	do_umount

 	# kill iumfsd fstestd
	kill_daemon  >> $TESTLOGFILE 2>&1

        # Create mount point directory 
	if [ ! -d "${mnt}" ]; then
	    mkdir ${mnt}  >> $TESTLOGFILE 2>&1
	    if [ "$?" -ne 0 ]; then
		echo "cannot create ${mnt}"  >> $TESTLOGFILE 2>&1
		fini 1
	    fi
	fi
}

init_hdfs (){
       if [ ! -f "${HADOOP_HOME}/hadoop-core-1.0.2.jar" ]; then
           echo "Can't find hadoop-core-1.0.2.jar. HADOOP_HOME might not be set correctly."
           exit 1
       fi
        echo "##"
        echo "## Preparing required directory for test."
        echo "##"
	# Just in case, remove existing test dir
        hadoop dfs -rmr ${base}  >> $TESTLOGFILE 2>&1
        # Create mount base directory 
        hadoop dfs -mkdir ${base}  >> $TESTLOGFILE 2>&1
	if [ "$?" -ne "0" ]; then
	    echo "Can't create new directory on HDFS. See $TESTLOGFILE" 
	    fini 1	
	fi
        echo "Completed."
}

do_build(){
        echo "##"
        echo "## Start building binaries ."
        echo "##"
	#./configure --enable-debug  >> $TESTLOGFILE 2>&1
	./configure # >> $TESTLOGFILE 2>&1
	sudo make uninstall # >> $TESTLOGFILE 2>&1
	make clean  #>> $TESTLOGFILE 2>&1do_umount() {
	sudo umount ${mnt}  >> $TESTLOGFILE 2>&1
	return $?
}

do_mount () {
    sudo mount -F iumfs ${1}${base} ${mnt} >> $TESTLOGFILE 2>&1
    return $?
}

do_umount() {
        cnt=0;
        while [ $cnt -lt $maxumountretries ]
        do
                sudo umount ${mnt} >> $TESTLOGFILE 2>&1
                cnt=`expr $cnt + 1`
                mountexit=`mount |grep "${mnt} "`
                if [ -z "$mountexit" ]; then
                        return 0
                fi
                sleep 1
        done
        echo "cannot umount ${mnt}"  | tee >> $TESTLOGFILE 2>&1
        return 1
}

start_hdfsd() {
        hadoop -Djava.util.logging.config.file=cmd/log.prop -cp $CLASSPATH  iumfs.hdfs.Main > $DAEMONLOGFILE 2>&1 &
	if [ "$?" -eq 0 ]; then
		daemonpid=$! 
		return 0		
	fi
	return 1
}

kill_daemon(){
        pid=`jps 2>/dev/null |grep hdfs | awk '{print $1}'`
        if [ "$pid" -ne "" ]; then
             sudo kill $pid >> $TESTLOGFILE 2>&1
        fi
	daemonpid=""
	return 0
}

exec_mount_test () {

        for target in mount umount
        do
   	   cmd="do_${target}" 
	   $cmd  $1 >> $TESTLOGFILE 2>&1
	   if [ "$?" -eq "0" ]; then
		echo "${target} test: \tpass" 
	   else
		echo "${target} test: \tfail  See $TESTLOGFILE" 
		fini 1	
	   fi
        done    
}

exec_fstest() {
	target=$1

	/usr/local/bin/fstest $target >> $TESTLOGFILE 2>&1
	if [ "$?" -eq "0" ]; then
		echo "${target} test: \tpass" 
	else
		echo "${target} test: \tfail  See $TESTLOGFILE" 
	fi
}

fini() {
        ## Kill unfinished processes
        for pid in $pids
        do
            kill $pid > /dev/null 2>&1
        done
	do_umount
	kill_daemon
        #rm -rf ${mnt} >> $TESTLOGFILE 2>&1
        echo "##"
        echo "## Finished."
        echo "##"
        echo "See log files for detail."
        echo "$TESTLOGFILE"
        echo "$DAEMONLOGFILE"
        exit 0
}

do_basic_test(){
    echo "##"
    echo "## Start filesystem operation test with $1 daemon."
    echo "##"
    exec_fstest "mkdir"
    exec_fstest "open"
    exec_fstest "write"
    exec_fstest "read"
    exec_fstest "getattr"
    exec_fstest "readdir"
    exec_fstest "remove"
    exec_fstest "rmdir"
}

do_create_and_delete(){
     filename=$RANDOM
     start=`get_second`
     count=0
     cd ${mnt}
     while :
     do
         count=`expr $count + 1`
         current=`get_second`
         elapsed=`expr $current - $start`
         throughput=`echo "$count/$elapsed" | bc -l 2>/dev/null` 
         echo "$count $elapsed $throughput" > $LOGDIR/throughput.$filename
         echo $filename > $filename
	 if [ "$?" -ne 0 ]; then
	     echo "do_create_and_delete: cannot create $filenme." | tee >> $TESTLOGFILE 2>&1
             continue
	 fi
         /bin/ls -aF > /dev/null 2>&1
         rm $filename
	 if [ "$?" -ne 0 ]; then
	     echo "do_create_and_delete: cannot remove $filenme." | tee >> $TESTLOGFILE 2>&1
             continue
	 fi
     done
}

get_second (){
    date '+%H %M %S' | read hour minute second
    echo "$hour*60*60+$minute*60+$second" | bc
}

do_stress_test(){
    echo "##"
    echo "## Start stress test"
    echo "##"
    pids=""
    cnt=0

    trap fini 2 

    while [ $cnt -lt $procs ]
    do
        do_create_and_delete &
        pids="$pids $!"
        cnt=`expr $cnt + 1`
    done

    echo "$pids started"

    ## Sleep for complete
    echo "Sleep $stresstime sec..."
    sleep $stresstime

    echo "stress test: \tpass" 
}

main() { 
    cd ../

    init

    printf "Please enter NameNode: "
    read nodename

    echo "##"
    echo "## Start mount test."
    echo "##"

    exec_mount_test "hdfs://$nodename"
    init_hdfs
    do_mount "hdfs://$nodename"
    start_hdfsd

    do_basic_test
#    do_stress_test

    fini $1
}

main
