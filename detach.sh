#!/bin/sh

doas umount /mnt

doas kldunload myfs.ko
