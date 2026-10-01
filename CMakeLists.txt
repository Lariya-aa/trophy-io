cmake_minimum_required(VERSION 3.20)
if(NOT DEFINED CMAKE_TOOLCHAIN_FILE)
  set(CMAKE_TOOLCHAIN_FILE "$ENV{VITASDK}/share/vita.toolchain.cmake" CACHE PATH "Vita toolchain")
endif()
project(trophy_io VERSION 0.1.2 LANGUAGES C)
include("${VITASDK}/share/vita.cmake" REQUIRED)
set(CMAKE_C_STANDARD 99)
set(VITA_MKSFOEX_FLAGS "-d PARENTAL_LEVEL=1 -d PSP2_SYSTEM_VER=0")
set(VITA_MAKE_FSELF_FLAGS "-a 0x2808000000000000")
add_compile_options(-Wall -Wextra -Wno-unused-parameter -O2 -fno-lto)
add_subdirectory(modules/kernel)
add_subdirectory(modules/user)
add_executable(trophy_io src/main.c src/screen.c src/core.c src/sha256.c src/storage.c src/catalog.c)
target_include_directories(trophy_io PRIVATE include)
target_compile_definitions(trophy_io PRIVATE TROPHY_APP_VERSION="${PROJECT_VERSION}")
add_dependencies(trophy_io trophy_user_stubs trophy_kernel.skprx-self trophy_user.suprx-self)
target_link_libraries(trophy_io
  ${CMAKE_CURRENT_BINARY_DIR}/modules/user/libTrophyIOUser_stub_weak.a
  taihen_stub SceAppMgr_stub SceCtrl_stub SceDisplay_stub ScePower_stub
  SceIofilemgr_stub SceVshBridge_stub SceLibKernel_stub)
vita_create_self(eboot.bin trophy_io UNSAFE)
vita_create_vpk(TrophyBatchManager-v${PROJECT_VERSION}.vpk TROPIO001 eboot.bin
  VERSION 00.12 NAME "Trophy IO ${PROJECT_VERSION}"
  FILE ${CMAKE_CURRENT_BINARY_DIR}/modules/kernel/trophy_kernel.skprx sce_module/trophy_kernel.skprx
  FILE ${CMAKE_CURRENT_BINARY_DIR}/modules/user/trophy_user.suprx sce_module/trophy_user.suprx
  FILE sce_sys/icon0.png sce_sys/icon0.png
  FILE sce_sys/livearea/contents/template.xml sce_sys/livearea/contents/template.xml
  FILE sce_sys/livearea/contents/bg.png sce_sys/livearea/contents/bg.png
  FILE sce_sys/livearea/contents/startup.png sce_sys/livearea/contents/startup.png
  FILE third_party/PSPSDK-LICENSE third_party/PSPSDK-LICENSE
  FILE LICENSE LICENSE
  FILE THIRD_PARTY.md THIRD_PARTY.md)
