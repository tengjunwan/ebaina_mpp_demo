Sample Usage:

Notice:
The compilation of this sample depends on the compilation of the OP-TEE OS and client.
For details about the OP-TEE dependency, please refer to its official documentation.
Step 1 is executed on the server, while other steps are executed on the single board.

Step 1: Compile sample code
make -j;

Execute the command make -j in the directory source/mpp/sample/optee. This generates sample_cipher in the sample/optee/security_subsys/host directory. 
It also generates TA files in the sample/optee/security_subsys/ta directory. Additionally, the open_source/optee/optee_client-3.20.0/out directory generates the libteec and tee-supplicant folders.

Step 2: Add library path
cd /path/optee_client/out/libteec
export LD_LIBRARY_PATH=pwd:$LD_LIBRARY_PATH

Step 3: Run tee-supplicant in background
cd /path/optee_client/out/tee-supplicant
./tee-supplicant &

Step 4: Copy TA
mkdir /lib/optee_armtz
cd /path/ta
cp * /lib/optee_armtz/

Step 5: Run sample
./sample_cipher <index>

For details about how to use OP-TEE, please refer to the document 《OP-TEE Development Guide》.
The index ranges from 1 to 11. When the index is 7, it fails. You need to add the public key pub_key and private key priv_key in the sample_rsa.c file.
When compiling the sample package, you must compile it using a version that has been compiled with a secure image.
After executing the sample via the serial port with no output, you need to enable logging. Open the directory:
open_source/optee/optee_os-3.20.0/core/arch/arm/plat-vendor/conf.mk,
and modify CFG_TEE_CORE_LOG_LEVEL and CFG_TEE_TA_LOG_LEVEL to 3.