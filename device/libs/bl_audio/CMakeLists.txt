cmake_minimum_required(VERSION 3.10)
project(bl_audio C)

# alsa-lib 依赖: 主机构建直接找系统的; 交叉构建时由 README 的
# 工具链文件/PKG_CONFIG 环境变量指向 buildroot sysroot
find_package(PkgConfig REQUIRED)
pkg_check_modules(ALSA REQUIRED alsa)

add_library(bl_audio STATIC
	bl_mixer.c
	bl_sink.c
	bl_wav.c
)

target_include_directories(bl_audio PUBLIC
	${CMAKE_CURRENT_SOURCE_DIR}/include
	${ALSA_INCLUDE_DIRS}
)
target_link_libraries(bl_audio PUBLIC
	${ALSA_LIBRARIES}
	pthread
)

# 主机自测程序(无声卡环境下验证链接与错误路径)
add_executable(bl_audio_smoke test/smoke.c)
target_link_libraries(bl_audio_smoke PRIVATE bl_audio)
