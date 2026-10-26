cmake_minimum_required(VERSION 3.10)


# 配置交叉编译器
set(CMAKE_C_COMPILER "/opt/linux/x86-arm/arm-v01c02-linux-musleabi-gcc/bin/arm-v01c02-linux-musleabi-gcc")
set(CMAKE_CXX_COMPILER "/opt/linux/x86-arm/arm-v01c02-linux-musleabi-gcc/bin/arm-v01c02-linux-musleabi-g++")

project(Hi3516CV610_Project LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 11)

# 全局编译选项
add_compile_options(
    -Wall -Wextra -Wno-unused-parameter -Wno-missing-field-initializers
    -fsigned-char -fstack-protector-strong -fPIC -ffunction-sections -fdata-sections
    -Os -mthumb -fno-aggressive-loop-optimizations
    -mcpu=cortex-a7 -mfloat-abi=softfp -mfpu=neon-vfpv4 -fPIE
)

# ==============================================================================
# 构建海思 SDK 现代化接口目标 (INTERFACE Target)
# ==============================================================================
add_library(hisi_sdk INTERFACE)

# 1. 封装：头文件搜索路径
target_include_directories(hisi_sdk INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}/include/hisilicon
    ${CMAKE_CURRENT_SOURCE_DIR}/include/hisilicon/exp_inc
    ${CMAKE_CURRENT_SOURCE_DIR}/include/hisilicon/module
    ${CMAKE_CURRENT_SOURCE_DIR}/include/3rdparty
    ${CMAKE_CURRENT_SOURCE_DIR}/src/common
)

# 2. 封装：编译宏定义
target_compile_definitions(hisi_sdk INTERFACE
    hi3516cv610 OT_XXXX ISP_V2 
    VER_X=1 VER_Y=0 VER_Z=0 VER_P=0 VER_B=10 
    USER_BIT_32 KERNEL_BIT_32 OT_RELEASE
    SENSOR0_TYPE=SC4336P_MIPI_4M_30FPS_10BIT
    SENSOR1_TYPE=SC4336P_MIPI_4M_30FPS_10BIT
    BOARD_TYPE=DMEB_QFN
    OT_ACODEC_TYPE_INNER
    OT_VQE_USE_STATIC_MODULE_REGISTER
)

# 3. 定义海思原厂库的绝对路径前缀
set(HISI_LIB_DIR "${CMAKE_CURRENT_SOURCE_DIR}/lib/hisilicon")
set(3RD_LIB_DIR "${CMAKE_CURRENT_SOURCE_DIR}/lib/3rdparty")

# 4. 封装：以绝对路径强制静态链接所有原厂库
# 去掉了晦涩的 -Bstatic，直白地告诉 CMake 每一个 .a 文件的物理位置
target_link_libraries(hisi_sdk INTERFACE
    -Wl,--start-group
    ${HISI_LIB_DIR}/libsecurec.a
    ${HISI_LIB_DIR}/libss_mpi.a
    ${HISI_LIB_DIR}/libss_mpi_sysbind.a
    ${HISI_LIB_DIR}/libss_mpi_sysmem.a
    ${HISI_LIB_DIR}/libss_mpi_ae.a
    ${HISI_LIB_DIR}/libss_mpi_isp.a
    ${HISI_LIB_DIR}/libot_mpi_isp.a
    ${HISI_LIB_DIR}/libss_mpi_awb.a
    ${HISI_LIB_DIR}/libdehaze.a
    ${HISI_LIB_DIR}/libextend_stats.a
    ${HISI_LIB_DIR}/libdrc.a
    ${HISI_LIB_DIR}/libldci.a
    ${HISI_LIB_DIR}/libbnr.a
    ${HISI_LIB_DIR}/libcalcflicker.a
    ${HISI_LIB_DIR}/libir_auto.a
    ${HISI_LIB_DIR}/libacs.a
    ${HISI_LIB_DIR}/libsns_hy006_3814_0011.a
    ${HISI_LIB_DIR}/libsns_sc4336p.a
    ${HISI_LIB_DIR}/libsns_sc450ai.a
    ${HISI_LIB_DIR}/libsns_sc500ai.a
    ${HISI_LIB_DIR}/libss_mpi_aibnr.a
    ${HISI_LIB_DIR}/libsvp_acl.a
    ${HISI_LIB_DIR}/libss_mpi_km.a
    ${HISI_LIB_DIR}/libprotobuf-c.a
    ${3RD_LIB_DIR}/libxoprtsp.a
    
    ${HISI_LIB_DIR}/libss_mpi_audio.a
    ${HISI_LIB_DIR}/libvoice_engine.a
    ${HISI_LIB_DIR}/libupvqe.a
    ${HISI_LIB_DIR}/libdnvqe.a
    ${HISI_LIB_DIR}/libss_mpi_audio_adp.a
    ${HISI_LIB_DIR}/libopus.a
    ${HISI_LIB_DIR}/libvqe_res.a
    ${HISI_LIB_DIR}/libvqe_record.a
    ${HISI_LIB_DIR}/libvqe_hpf.a
    ${HISI_LIB_DIR}/libvqe_anr.a
    ${HISI_LIB_DIR}/libvqe_aec.a
    ${HISI_LIB_DIR}/libvqe_agc.a
    ${HISI_LIB_DIR}/libvqe_eq.a
    ${HISI_LIB_DIR}/libvqe_talkv2.a
    ${HISI_LIB_DIR}/libaac_dec.a
    ${HISI_LIB_DIR}/libaac_enc.a
    ${HISI_LIB_DIR}/libaac_comm.a
    ${HISI_LIB_DIR}/libaac_sbr_dec.a
    ${HISI_LIB_DIR}/libaac_sbr_enc.a
    -Wl,--end-group
    
    stdc++ pthread m dl c
)

# ==============================================================================
# 构建底层公共静态库
# ==============================================================================
file(GLOB COMMON_SRCS 
    "${CMAKE_CURRENT_SOURCE_DIR}/src/common/sample_comm_sys.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/common/sample_comm_isp.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/common/sample_comm_vi.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/common/sample_comm_vpss.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/common/loadbmp.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/common/ot_confaccess.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/common/sample_comm_audio.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/common/sample_comm_venc.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/common/sample_comm_region.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/common/sample_comm_osd_drawline.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/common/sample_ipc.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/common/sns_type.c"
)
add_library(common_obj STATIC ${COMMON_SRCS})
target_link_libraries(common_obj PUBLIC hisi_sdk)

# 先加入第三方底层库
set(BUILD_PYTHON_WRAPPER OFF)
# 强制将后续未指明类型的 add_library 默认编译为静态库
set(BUILD_SHARED_LIBS OFF)
add_subdirectory(${CMAKE_SOURCE_DIR}/3rdparty/apriltag)

# 再加入你封装的算法模块
add_subdirectory(${CMAKE_SOURCE_DIR}/src/apriltag_algo)

# 添加子模块目录
add_subdirectory(${CMAKE_SOURCE_DIR}/src/svp)