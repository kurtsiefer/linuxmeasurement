#!/bin/sh
# script to generate device nodesfor PCI card drivers (dirty hack...)

dev1="dt340"
dev1old="dt340test1"

# make ioboards directory
mkdir -p /dev/ioboards

# remove stale device files
rm -f /dev/ioboards/dt340*
rm -f /dev/dt340*

major=$(awk "\$2~\"dt340\"  {print \$1}" /proc/devices)
# echo "dt340 driver claims following major devices:"
# echo $major
cardindex=0 
for elm in $major
  do 
    # echo "creating device ${dev1}_$cardindex for major $elm"
    # create  file entries
    mknod /dev/${dev1}_${cardindex} c $elm 1
    chgrp users /dev/${dev1}_${cardindex}
    chmod 664 /dev/${dev1}_${cardindex}
    ln -s /dev/${dev1}_${cardindex} /dev/ioboards/
    
    cardindex=$(($cardindex+1))
done

# legacy link
if [ -a /dev/${dev1}_0 ]
then
    ln -s /dev/${dev1}_0 /dev/ioboards/${dev1old}
fi
