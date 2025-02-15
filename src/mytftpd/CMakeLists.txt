add_library(mytftpd "")
target_include_directories(mytftpd PUBLIC ${MY_INCS_DIR})
target_sources(mytftpd PUBLIC transaction.cpp PUBLIC driver.cpp)
