#!/bin/bash
rm build -rf
rm output -rf
mkdir build
mkdir output
cd build
cmake -D CMAKE_BUILD_TYPE=Release -D CMAKE_C_FLAGS=-fPIC -D CMAKE_CXX_FLAGS=-fPIC -D CMAKE_C_COMPILER=/opt/linux/x86-arm/arm-v01c02-linux-musleabi-gcc/bin/arm-v01c02-linux-musleabi-gcc -D CMAKE_CXX_COMPILER=/opt/linux/x86-arm/arm-v01c02-linux-musleabi-gcc/bin/arm-v01c02-linux-musleabi-g++ -D CMAKE_INSTALL_PREFIX=../output -D BUILD_ZLIB=ON -D BUILD_SHARED_LIBS=ON -D WITH_OPENCL=OFF -D ENABLE_PRECOMPILED_HEADERS=OFF -D BUILD_opencv_wechat_qrcode=OFF -D BUILD_opencv_photo=OFF -D BUILD_opencv_highgui=OFF -D WITH_GTK=OFF -D WITH_1394=OFF -D WITH_ITT=ON -D WITH_IPP=OFF -D BUILD_opencv_python3=ON -D BUILD_opencv_dnn=OFF -D BUILD_opencv_imgcodecs=OFF -D WITH_JPEG=OFF -D WITH_ADE=OFF -D WITH_VA=OFF -DLAPACK_LIBRARIES=m_pic -D PYTHON_EXECUTABLE=/usr/bin/python3 ..
make -j64
make install
