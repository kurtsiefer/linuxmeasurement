#!/bin/sh
# script to generate device nodesfor PCI card drivers (dirty hack...)

dev1="dt30x"
dev1old="dt302one"
# lgf="/home/chris/programs/dt302/mknode.log"

# make ioboards directory
mkdir -p /dev/ioboards

# remove stale device files
rm -f /dev/ioboards/dt302*
rm -f /dev/dt30x*

major=$(awk "\$2~\"dt302\"  {print \$1}" /proc/devices)
# date >>$lgf
# echo "dt302 driver claims following major devices:" >>$lgf
# echo $major >>$lgf
cardindex=0 
for elm in $major 
do 
#    echo "creating device ${dev1}_$cardindex for major $elm">>$lgf
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
