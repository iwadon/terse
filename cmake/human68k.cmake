set(CMAKE_SYSTEM_NAME Generic)          # macOS ではなく「組込み/汎用」扱いにする
set(CMAKE_SYSTEM_PROCESSOR m68k)

set(TERSE_PLATFORM_HUMAN68K ON CACHE BOOL "Building for Human68k" FORCE)

set(CMAKE_C_COMPILER   m68k-xelf-gcc)
set(CMAKE_CXX_COMPILER m68k-xelf-g++)

# macOS 固有の -arch やデプロイメント・ターゲット検査を避ける
set(CMAKE_OSX_ARCHITECTURES "")         # -arch の自動付与を無効化

# try-compile で実行形式のリンク/実行を要求しない（組込み向けは通常実行できない）
set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)

set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)

set(TERSE_USE_SYSTEM_ICONV OFF CACHE BOOL "Disabling system iconv support for Human68k" FORCE)
set(TERSE_BUILD_TESTING OFF CACHE BOOL "Disabling tests for Human68k" FORCE)
