#!/bin/sh

doas kldload ./myfs.ko.full

doas ./mount_myfs /dev/vtbd0 /mnt

echo "exit=$?"
