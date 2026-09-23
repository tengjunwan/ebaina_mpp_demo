Compilation Guide:
1. Download opencv4.7.0   : Download from https://github.com/opencv/opencv
2. Decompress opencv4.7.0 : Decompress the opencv file when downloading the compressed package.
3. Compile opencv4.7.0    : Copy build.sh from the directory to the opencv4.7.0 folder and run bulid.sh. Change the path following the equal sign (=) of CMAKE_C_COMPILER and CMAKE_CXX_COMPILER in the bulid.sh file to the path of the cross compilation tool chain of the version.
4. Copy the binary file   : Create the thirdparty folder in the directory and copy the include and lib folders in the opencv4.7.0/output/ directory to the thirdparty folder.
5. Compile the sample     : Modify the Makefile file in the directory, delete the comments as prompted, and compile the sample.

Sample Guide:
1. Linkage                ：export LD_LIBRARY_PATH=./thirdparty/lib:$LD_LIBRARY_PATH
2. Calibration            : The calibration command sample_bla 0 is used. The name of the calibration data file, calibration point selection, and downsampling rate are determined in the code and can be modified as required.
3. Save data              : After the calibration is complete, the calibration result is automatically saved to this directory. The calibration result file name is specified in the code and can be modified as required.
4. Calculation            : Run the sample_bla 1 command. The name of the calibration result file used for calculation is determined in the code and can be modified as required. The calculation result is obtained after the x and y coordinates are transferred, for example, 224 50.