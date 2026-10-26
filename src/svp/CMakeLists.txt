# src/svp/CMakeLists.txt
file(GLOB NPU_SRCS 
    "svp_npu/*.c"
    "svp_npu/sample_svp_npu/*.c"
    "common/*.c"
)

add_executable(sample_svp_npu_main ${NPU_SRCS})

target_include_directories(sample_svp_npu_main PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/common
    ${CMAKE_CURRENT_SOURCE_DIR}/svp_npu/include
)

target_compile_options(sample_svp_npu_main PRIVATE -O3)

target_link_libraries(sample_svp_npu_main PRIVATE
    common_obj
    # 使用 PROJECT_SOURCE_DIR 直接从工程根目录往下找，极其精准
    ${PROJECT_SOURCE_DIR}/lib/hisilicon/libss_mpi_ive.a
    # ${PROJECT_SOURCE_DIR}/lib/hisilicon/libss_ivs_md.a
)

# 只需要这一句话！
# 它会自动把 apriltag_c_wrapper.h 的路径带过来，并完成链接
target_link_libraries(sample_svp_npu_main PRIVATE apriltag_algo)