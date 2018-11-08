#!/bin/bash

make

./daemon start
sleep 30
./daemon stop
sleep 60

rngtest < ~/random/data

tail /var/log/syslog

