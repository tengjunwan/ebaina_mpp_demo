# 生成名为 apriltag_algo 的静态库 (STATIC 避免给板子拷运行时 .so 文件的麻烦)
add_library(apriltag_algo STATIC
    apriltag_algo.cpp
    apriltag_c_wrapper.cpp
)

# 向外暴露我们自己写的头文件路径 (PUBLIC 表示谁链接我，谁就能 include 这个路径)
target_include_directories(apriltag_algo PUBLIC 
    ${CMAKE_SOURCE_DIR}/include/apriltag_algo
)

# 建立对底层 apriltag 库的依赖 (PRIVATE 表示只供我内部实现使用，不向上传递给主程序)
# 注意：这里的 "apriltag" 是指你在 3rdparty/apriltag/CMakeLists.txt 里面定义出来的 target 名字
target_link_libraries(apriltag_algo PRIVATE apriltag)
