Step 1 is executed on the server, while other steps are executed on the single board.

Step 1: Compile sample code
make -j;

Execute the command make -j in the directory source/mpp/sample/security_subsys to compile sample_cipher. 
This sample instance requires loading the source/out/ko file for execution.

Step 2: Load kernel module
cd ko/load3516cv610/
./load3516cv610_20g_debug -mmz_start 0x44000000 -i
cd ..
insmod ot_otp.ko

Step 3: Run sample
./sample_cipher <index>

Note: For details about how to use OP-TEE, please refer to the document 《OP-TEE Development Guide》.
The index ranges from 1 to 11. When the index is 7, it fails. You need to add the public and private keys priv_key and pub_key in the sample_rsa.c file.