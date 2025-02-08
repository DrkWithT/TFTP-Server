add_library(mybsock "")
target_include_directories(mybsock PUBLIC ${MY_INCS_DIR})
target_sources(mybsock PRIVATE netconfig.cpp PRIVATE sockets.cpp)
