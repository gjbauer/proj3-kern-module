#!/bin/sh

doas umount /mnt

doas kldunload myfs.ko

doas mdconfig -d -u 0
