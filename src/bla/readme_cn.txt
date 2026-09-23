编译指南：
1. 下载opencv4.7.0: 可以从https://github.com/opencv/opencv下载
2. 解压opencv4.7.0：下载压缩包的情况下需要解压opencv文件
3. 编译opencv4.7.0: 将该目录下build.sh拷贝至opencv4.7.0文件夹下，执行bulid.sh。bulid.sh中CMAKE_C_COMPILER和CMAKE_CXX_COMPILER的等号后面路径需要修改为版本的交叉编译工具链路径。
4. 拷贝二进制文件 ：在该目录下创建thirdparty文件夹，将opencv4.7.0/output/目录下的include和lib文件夹拷贝到thirdparty文件夹中
5. 编译sample     ：修改该目录下Makefile，根据Makefile中提示去除注释后，编译sample即可

sample指南：
1. 链接           ：export LD_LIBRARY_PATH=./thirdparty/lib:$LD_LIBRARY_PATH
2. 标定           ：标定命令sample_bla 0, 使用的标定数据文件名称，标定选点，下采样倍率等在代码中确定，按需修改
3. 保存数据       ：标定完成后自动保存标定结果到该目录下，标定结果文件名称在代码中确定，按需修改
4. 计算           ：计算命令sample_bla 1, 计算使用的标定结果文件名称在代码中确定，按需修改。计算需要传入x坐标和y坐标后得出结果，例如：224 50
