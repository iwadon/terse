# toolchains/m68k-xelf.cmake
set(CMAKE_SYSTEM_NAME Generic)          # macOS ではなく「組込み/汎用」扱いにする
set(CMAKE_SYSTEM_PROCESSOR m68k)

# Human68k platform identification
set(TERSE_PLATFORM_HUMAN68K ON CACHE BOOL "Building for Human68k" FORCE)

# クロス用コンパイラ
set(CMAKE_C_COMPILER   m68k-xelf-gcc)
set(CMAKE_CXX_COMPILER m68k-xelf-g++)
# （必要なら）
# set(CMAKE_ASM_COMPILER m68k-xelf-as)
# set(CMAKE_AR          m68k-xelf-ar)
# set(CMAKE_RANLIB      m68k-xelf-ranlib)

# macOS 固有の -arch やデプロイメント・ターゲット検査を避ける
set(CMAKE_OSX_ARCHITECTURES "")         # -arch の自動付与を無効化

# try-compile で実行形式のリンク/実行を要求しない（組込み向けは通常実行できない）
set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)

# ターゲット探索系の既定（必要に応じて調整）
# set(CMAKE_FIND_ROOT_PATH /opt/m68k-xelf)  # プレフィックスがあれば
# set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
# set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
# set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)

# ターゲット固有フラグ（必要に応じて）
# set(CMAKE_C_FLAGS   "${CMAKE_C_FLAGS} -m68000 -ffreestanding -fno-builtin")
# set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -m68000 -ffreestanding -fno-builtin")
# set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -nostdlib")

set(TERSE_ENABLE_ICONV OFF)
set(TERSE_BUILD_TESTING OFF)
