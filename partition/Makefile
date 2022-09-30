
all:
	insmod ../ouichefs.ko
	mount -oloop -t ouichefs ../test.img partition_ouichefs/
	
clean:
	umount partition_ouichefs/
	rmmod ouichefs.ko
	

.PHONY: all 
