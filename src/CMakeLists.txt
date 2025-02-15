add_subdirectory(mybsock)
add_subdirectory(mytftpd)

add_executable(tftpd "")
target_include_directories(tftpd PUBLIC ${MY_INCS_DIR})
target_link_directories(tftpd PRIVATE ${MY_LIBS_DIR})
target_sources(tftpd PRIVATE main.cpp)
target_link_libraries(tftpd PRIVATE mybsock PRIVATE mytftpd)
