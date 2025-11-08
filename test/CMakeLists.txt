# 设置测试可执行文件输出目录
set(TEST_OUTPUT_DIR ${CMAKE_SOURCE_DIR}/bin/test)
file(MAKE_DIRECTORY ${TEST_OUTPUT_DIR})

# 获取当前目录下所有的.cpp文件
file(GLOB TEST_SOURCES "*.cpp")

# 遍历所有源文件，为每个源文件创建一个可执行文件
foreach(TEST_SOURCE ${TEST_SOURCES})
    # 获取不带路径和扩展名的文件名作为可执行文件名
    get_filename_component(EXECUTABLE_NAME ${TEST_SOURCE} NAME_WE)
    
    # 添加可执行文件
    add_executable(${EXECUTABLE_NAME} ${TEST_SOURCE})
    
    # 设置输出路径
    set_target_properties(${EXECUTABLE_NAME} PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY ${TEST_OUTPUT_DIR}
    )
    #不知道为什么libswresample.so.5库连接不了，直接创建了一个新的 .conf
    # 设置运行时路径（仅动态编译需要）
    if(NOT USE_STATIC_LIBS)
        set_target_properties(${EXECUTABLE_NAME} PROPERTIES
            BUILD_WITH_INSTALL_RPATH TRUE
            INSTALL_RPATH "${RUNTIME_LIBRARY_PATHS}"
            INSTALL_RPATH_USE_LINK_PATH TRUE
        )
    endif()

    # 链接和主程序相同的库
    target_link_libraries(${EXECUTABLE_NAME}
        PRIVATE 
        ${FFMPEG_LIBRARIES} ${FFMPEG_LDFLAGS_OTHER}
        ${MANUAL_BOOST_LIBRARIES}
        ${MANUAL_JSONCPP_LIBRARIES}
        ${SYSTEM_LIBS}
    )
    
    # 添加测试
    add_test(NAME ${EXECUTABLE_NAME}
             COMMAND ${EXECUTABLE_NAME})
endforeach()