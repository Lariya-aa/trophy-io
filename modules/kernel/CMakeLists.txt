add_executable(trophy_kernel main.c ../../src/core.c)
target_include_directories(trophy_kernel PRIVATE ../../include)
target_compile_options(trophy_kernel PRIVATE -nostdlib)
target_link_options(trophy_kernel PRIVATE -nostdlib -Wl,-q)
target_link_libraries(trophy_kernel SceSysclibForDriver_stub SceSysmemForDriver_stub
  SceModulemgrForDriver_stub SceThreadmgrForDriver_stub SceProcessmgrForDriver_stub
  taihenForKernel_stub taihenModuleUtils_stub)
vita_create_self(trophy_kernel.skprx trophy_kernel CONFIG exports.yml UNSAFE)
vita_create_stubs(trophy_kernel_stubs trophy_kernel exports.yml KERNEL)
