#!/bin/bash
rm -rf build
cmake -B build
cmake --build build
scp ./build/src/svp/sample_svp_npu_main root@192.168.1.168:/opt/my_sample