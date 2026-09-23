步骤1在服务器上执行，其他步骤在单板上执行。

步骤1：编译示例代码
make -j;

在source/mpp/sample/security_subsys执行命令make -j编译出sample_cipher，
该sample实例运行需要加载source/out/ko文件。

步骤2：加载内核模块
cd ko/load3516cv610/
./load3516cv610_20g_debug -mmz_start 0x44000000 -i
cd ..
insmod ot_otp.ko

步骤3：运行示例
./sample_cipher <index>

注：有关如何使用OP-TEE的详细信息，请参考文档《OP-TEE开发指南》。
index为1-11，index为7时会失败，需要在sample_rsa.c文件中，添加公私钥priv_key与pub_key。