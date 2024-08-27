#!/bin/bash

export LD_LIBRARY_PATH=./build/service/simdeps

rm -f ./service-0.log
rm -f ./client-0.log
rm -rf ./client-1.log

./build/release/linux-unpacked/tadsim