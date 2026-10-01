add_executable(trophy_user main.c)
target_include_directories(trophy_user PRIVATE ../../include)
target_compile_options(trophy_user PRIVATE -nostdlib)
target_link_options(trophy_user PRIVATE -nostdlib -Wl,-q)
add_dependencies(trophy_user trophy_kernel_stubs)
target_link_libraries(trophy_user
  ${CMAKE_CURRENT_BINARY_DIR}/../kernel/libTrophyIOKernel_stub.a SceLibKernel_stub)
vita_create_self(trophy_user.suprx trophy_user CONFIG exports.yml UNSAFE)
vita_create_stubs(trophy_user_stubs trophy_user exports.yml)
