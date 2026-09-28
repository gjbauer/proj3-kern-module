#!/bin/sh

doas kldload ./myfs.ko

doas ./mount_myfs /dev/vtbd0 /mnt
