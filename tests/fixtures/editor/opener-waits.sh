#!/bin/sh
while [ ! -e "$1.release" ]; do
    sleep 0.01
done
exit 4
