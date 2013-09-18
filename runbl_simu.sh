if [ $# -lt 1 ]; then
	echo "Usage: $0 Mode[0=ext4 | 1=ext4-journal | 2=btrfs | 3=ramfs] [DevPath]"
	exit 1
fi

mode=$1
if [ $# = 2 ]; then
	dev=$2
	post="out"
else
	dev="/dev/block/mmcblk1p3"
	post="o"
fi

if [ $mode = 1 ]; then
	mopt="-o data=journal"
elif [ $mode = 3 ]; then
	dev="none"
fi

types=("ext4" "ext4" "btrfs" "ramfs")
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

./baseline-simu.$post mnt/baseline/tmp.data 2048 1 $mode > bls-$timestamp.data
echo "Test ends."
sleep 5
umount mnt

