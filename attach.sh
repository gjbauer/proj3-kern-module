#!/bin/sh

doas kldload ./myfs.ko

doas mdconfig -a -t vnode -f test-image.img -u 0

doas mount -t myfs /dev/md0 /mnt
