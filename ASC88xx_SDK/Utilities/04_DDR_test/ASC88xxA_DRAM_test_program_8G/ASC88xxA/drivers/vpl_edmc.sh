###############################################
# Generated by Code Generator Version 2.2.0.3 #
###############################################
MODNAME="vpl_edmc"

rmmod vpl_edmc

rm /dev/$MODNAME

insmod vpl_edmc.ko dwMediaMemSize=192

MAJOR=`cat /proc/devices | grep $MODNAME | sed -e 's/vpl_edmc//' | sed -e 's/ //'`

if test -z $MAJOR; then
	echo "The device specified is not found !"
else
	echo "Let's make a node here for $MODNAME with major number $MAJOR"
	mknod /dev/$MODNAME c $MAJOR 0
fi