示例用法：

注意事项：
此示例的编译依赖于OP-TEE OS和客户端的编译。
有关OP-TEE依赖项的详细信息，请参阅其官方文档。
步骤1在服务器上执行，其他步骤在单板上执行。


步骤1：编译示例代码
make -j;

source/mpp/sample/optee执行命令make -j，sample/optee/security_subsys/host生成sample_cipher，sample/optee/security_subsys/ta生成ta文件，
open_source/optee/optee_client-3.20.0/out生成libteec与tee-supplicant文件夹。

步骤2：添加库路径
cd /path/optee_client/out/libteec
export LD_LIBRARY_PATH=pwd:$LD_LIBRARY_PATH

步骤3：在后台运行tee-supplicant
cd /path/optee_client/out/tee-supplicant
./tee-supplicant &

步骤4：拷贝ta
mkdir /lib/optee_armtz
cd /path/ta
cp * /lib/optee_armtz/

步骤5：运行示例
./sample_cipher <index>

有关如何使用OP-TEE的详细信息，请参阅文档《OP-TEE开发指南》。
index范围从1到11。当索引为7时，操作失败。需要在sample_rsa.c文件中添加公钥pub_key和私钥priv_key。
在编译sample版本包时，必须使用已编译为安全映像的版本进行编译。
通过串口执行示例但无输出时，需要启用日志记录。打开目录：
open_source/optee/optee_os-3.20.0/core/arch/arm/plat-vendor/conf.mk，
并将CFG_TEE_CORE_LOG_LEVEL和CFG_TEE_TA_LOG_LEVEL修改为3。