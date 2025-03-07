#!/bin/bash

UE_CLANG_DIR=$HOME/ue5/Engine/Extras/ThirdPartyNotUE/SDKs/HostLinux/Linux_x64/v22_clang-16.0.6-centos7/x86_64-unknown-linux-gnu
UE_CLANG_BIN=$UE_CLANG_DIR/bin/clang
UE_CLANG_PLUS_BIN=$UE_CLANG_DIR/bin/clang++
UE_LIBCPP_DIR=$HOME/ue5/Engine/Source/ThirdParty/Unix/LibCxx/lib/Unix/x86_64-unknown-linux-gnu

$UE_CLANG_BIN --version
$UE_CLANG_PLUS_BIN --version

export ac_cv_cxx_have_cxx11=yes
export CXXFLAGS="-std=c++11"

pushd /saturnv/protobuf

./autogen.sh

CC=$UE_CLANG_BIN CXX=$UE_CLANG_PLUS_BIN \
CXXFLAGS="-std=c++11 -stdlib=libc++ -I$UE_CLANG_DIR/include/c++/v1 -I$UE_CLANG_DIR/include" \
LDFLAGS="-stdlib=libc++ -L$UE_LIBCPP_DIR -Wl,-rpath,$UE_LIBCPP_DIR -lc++abi" \
./configure --prefix=/saturnv/protobuf/install_ue

popd