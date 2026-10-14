add_library(lhasa STATIC
  ${CMAKE_CURRENT_LIST_DIR}/lib/crc16.c
  ${CMAKE_CURRENT_LIST_DIR}/lib/ext_header.c
  ${CMAKE_CURRENT_LIST_DIR}/lib/lha_arch_unix.c
  ${CMAKE_CURRENT_LIST_DIR}/lib/lha_arch_win32.c
  ${CMAKE_CURRENT_LIST_DIR}/lib/lha_decoder.c
  ${CMAKE_CURRENT_LIST_DIR}/lib/lha_endian.c
  ${CMAKE_CURRENT_LIST_DIR}/lib/lha_file_header.c
  ${CMAKE_CURRENT_LIST_DIR}/lib/lha_input_stream.c
  ${CMAKE_CURRENT_LIST_DIR}/lib/null_decoder.c
  ${CMAKE_CURRENT_LIST_DIR}/lib/lh1_decoder.c
  ${CMAKE_CURRENT_LIST_DIR}/lib/lh5_decoder.c
  ${CMAKE_CURRENT_LIST_DIR}/lib/lh6_decoder.c
  ${CMAKE_CURRENT_LIST_DIR}/lib/lh7_decoder.c
  ${CMAKE_CURRENT_LIST_DIR}/lib/lhx_decoder.c
  ${CMAKE_CURRENT_LIST_DIR}/lib/lk7_decoder.c
  ${CMAKE_CURRENT_LIST_DIR}/lib/lz5_decoder.c
  ${CMAKE_CURRENT_LIST_DIR}/lib/lzs_decoder.c
  ${CMAKE_CURRENT_LIST_DIR}/lib/pm1_decoder.c
  ${CMAKE_CURRENT_LIST_DIR}/lib/pm2_decoder.c
)
target_include_directories(lhasa PUBLIC ${CMAKE_CURRENT_LIST_DIR}/lib)
