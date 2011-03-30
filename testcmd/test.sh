#!/bin/sh
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
# CreateRequest.java
#
# Test script of iumfs filesystem
# After build iumfs, run this script.
# You will be prompted password for root user.
#
daemonpid=""
mnt="/var/tmp/iumfsmnt"
base="/var/tmp/iumfsbase"
CLASSPATH="\
./cmd/hdfsd.jar:${HADOOP_HOME}/conf:\
${HADOOP_HOME}/hadoop-common-0.21.0.jar:\
${HADOOP_HOME}/hadoop-hdfs-0.21.0.jar:\
${HADOOP_HOME}/lib/commons-logging-1.1.1.jar"

nobuild="$1"

init (){
       if [ ! -f "${HADOOP_HOME}/hadoop-common-0.21.0.jar" ]; then
           echo "Can't find hadoop-common-0.21.0.jar. HADOOP_HOME might not be set correctly."
           exit 1
       fi
       
        LOGFILE=testcmd/test-`date '+%Y%m%d-%H:%M:%S'`.log
        touch $LOGFILE
	# Just in case, umount ${mnt}
	while : 
	do
		mountexit=`mount |grep "${mnt} "`
		if [ -z "$mountexit" ]; then
			break
		fi
		sudo umount ${mnt} >> $LOGFILE 2>&1
	done

 	# kill iumfsd fstestd
	kill_daemon  >> $LOGFILE 2>&1

        echo "##"
        echo "## Preparing required directory for test."
        echo "##"

	# Just in case, remove existing test dir
	rm -rf ${base}   >> $LOGFILE 2>&1
        hdfs dfs -rmr /var/tmp/iumfsbase  >> $LOGFILE 2>&1
#	if [ "$?" -ne "0" ]; then
#	    echo "Can't remove existing directory on HDFS. See $LOGFILE" 
#	    fini 1	
#	fi

        # Create mount point and base directory 
	for dir in ${base} ${mnt}
	do
		if [ ! -d "${dir}" ]; then
			mkdir ${dir}  >> $LOGFILE 2>&1
			if [ "$?" -ne 0 ]; then
				echo "cannot create ${dir}"  >> $LOGFILE 2>&1
				fini 1
			fi
		fi
	done
        hdfs dfs -mkdir /var/tmp/iumfsbase  >> $LOGFILE 2>&1
	if [ "$?" -ne "0" ]; then
	    echo "Can't create new directory on HDFS. See $LOGFILE" 
	    fini 1	
	fi
        echo "Completed."
}

do_build(){
        echo ""  
        echo "##"
        echo "## Start building binaries ."
        echo "##"
	#./configure --enable-debug  >> $LOGFILE 2>&1
	./configure # >> $LOGFILE 2>&1
	sudo make uninstall # >> $LOGFILE 2>&1
	make clean  #>> $LOGFILE 2>&1
	make  #>> $LOGFILE 2>&1
	if [ "$?" -ne "0" ]; then
	    echo "Failed to build modules. See $LOGFILE" 
	    exit 1	
	fi
	sudo make install   #>> $LOGFILE 2>&1
	if [ "$?" -ne "0" ]; then
	    echo "Failed to install binaries. See $LOGFILE" 
	    exit 1
	fi
	(cd cmd ; ant )
        echo "Completed." 
}


do_mount () {
 	sudo mount -F hdfs ${base} ${mnt} >> $LOGFILE 2>&1
	return $?
}

do_umount() {
	sudo umount ${mnt}  >> $LOGFILE 2>&1
	return $?
}

start_fstestd() {
	./testcmd/fstestd -d 3 > testcmd/fstestd.log 2>&1 &
	if [ "$?" -eq 0 ]; then
		daemonpid=$! 
		return 0		
	fi
	return 1
}

start_hdfsd() {
        hadoop -Djava.util.logging.config.file=log.prop -cp $CLASSPATH  hdfsd > ./testcmd/hdfsd.log 2>&1 &
	if [ "$?" -eq 0 ]; then
		daemonpid=$! 
		return 0		
	fi
	return 1
}

kill_daemon(){
	pkill -KILL fstestd >> $LOGFILE 2>&1
        pid=""
        pid=`jps |grep hdfsd | awk '{print $1}'`
        if [ "$pid" -ne "" ]; then
             kill $pid >> $LOGFILE 2>&1
        fi
	daemonpid=""
	return 0
}

exec_mount_test () {
        for target in mount umount
        do
   	   cmd="do_${target}" 
	   $cmd  >> $LOGFILE 2>&1
	   if [ "$?" -eq "0" ]; then
		echo "${target} test: \tpass" 
	   else
		echo "${target} test: \tfail  See $LOGFILE" 
		fini 1	
	   fi
        done    
}

exec_fstest() {
	target=$1

	./testcmd/fstest $target >> $LOGFILE 2>&1
	if [ "$?" -eq "0" ]; then
		echo "${target} test: \tpass" 
	else
		echo "${target} test: \tfail  See $LOGFILE" 
	fi
}

fini() {
	do_umount
	kill_daemon
	if [ -z "$nobuild" ]; then
		sudo make uninstall >> $LOGFILE 2>&1
		rm -rf ${mnt} >> $LOGFILE 2>&1
		rm -rf ${base} >> $LOGFILE 2>&1
	fi
        echo ""
        echo "Finished."
        echo "See $LOGFILE, fstestd.log and hdfsd.log under testcmd/ for detail log."
        exit 0
}


cd ../

if [ -z "$nobuild" ]; then
	do_build
fi

init

echo ""
echo "##"
echo "## Start mount test."
echo "##"
exec_mount_test

echo ""
echo "##"
echo "## Start filesystem operation test with fstestd daemon."
echo "##"
do_mount
start_fstestd
exec_fstest "mkdir"
exec_fstest "open"
exec_fstest "write"
exec_fstest "read"
exec_fstest "getattr"
exec_fstest "readdir"
exec_fstest "remove"
exec_fstest "rmdir"
kill_daemon
do_umount

echo ""
echo "##"
echo "## Start filesystem operation test with hdfsd daemon."
echo "##"
do_mount
start_hdfsd
exec_fstest "mkdir"
exec_fstest "open"
exec_fstest "write"
exec_fstest "read"
exec_fstest "getattr"
exec_fstest "readdir"
exec_fstest "remove"
exec_fstest "rmdir"

fini

exit