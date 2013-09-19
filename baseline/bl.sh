if [ $# -lt 1 ]; then
	echo "Usage: $0 Mode[ 0=ext4 | 1=ext4-adafs | 2=btrfs ] [DevPath]"
	exit 1
fi

mode=$1
if [ $# = 2 ]; then
	dev=$2
	post="out"
else
	if [ $mode = 2 ]; then
		dev="/dev/block/mmcblk1p4"
	elif [ $mode = 3 ]; then
		dev="none"
	else
		dev="/dev/block/mmcblk1p3"
	fi
	post="o"
fi

if [ $mode = 1 ]; then
	mopt="-o data=journal"
fi

types=("ext4" "adafs" "btrfs")
fs_type=${types[$mode]}

mount -t $fs_type $mopt $dev mnt
if [ $? != 0 ]; then
	exit -1;
fi
timestamp=`date +"%m%d-%H-%M-%S"`

echo 36000000 > /proc/sys/vm/dirty_writeback_centisecs
echo 90 > /proc/sys/vm/dirty_background_ratio

if [ $mode -ne 2 ]; then
	mkdir -p mnt/baseline
fi

sleep 2
echo "Test begins."

of="bl-$mode-$timestamp.data"
./baseline-bench.$post mnt/baseline/tmp.data 2048 1 > $of
sleep 1
umount mnt

echo "Test ends."
sleep 2
fsync $of 

