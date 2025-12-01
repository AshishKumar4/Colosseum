#! /bin/bash

RPC_VERSION_FOLDER="rpclib-2.3.0"
folder_name="Release"
build_dir=build

# Try to detect Unreal Engine path
UE_ROOT=""
if [ -d "/ntfs-gen4-1tb/RoboticsProject/UnrealEngine" ]; then
    UE_ROOT="/ntfs-gen4-1tb/RoboticsProject/UnrealEngine"
elif [ -d "$HOME/Desktop/UMDCourseWork/MSML642/project/UnrealEngine" ]; then
    UE_ROOT="$HOME/Desktop/UMDCourseWork/MSML642/project/UnrealEngine"
elif [ -d "$SCRIPT_DIR/UnrealEngine" ]; then
    UE_ROOT="$SCRIPT_DIR/UnrealEngine"
fi

if [ -n "$UE_ROOT" ]; then
    echo "Found Unreal Engine at: $UE_ROOT"
    echo "Will build rpclib with UE's bundled toolchain to avoid symbol conflicts"
    
    # Use Unreal's bundled clang and libc++
    UE_CLANG="$UE_ROOT/Engine/Extras/ThirdPartyNotUE/SDKs/HostLinux/Linux_x64/v25_clang-18.1.0-rockylinux8/x86_64-unknown-linux-gnu/bin/clang"
    UE_CLANGXX="$UE_ROOT/Engine/Extras/ThirdPartyNotUE/SDKs/HostLinux/Linux_x64/v25_clang-18.1.0-rockylinux8/x86_64-unknown-linux-gnu/bin/clang++"
    UE_LIBCXX_INCLUDE="$UE_ROOT/Engine/Source/ThirdParty/Unix/LibCxx/include/c++/v1"
    UE_LIBCXX_LIB="$UE_ROOT/Engine/Source/ThirdParty/Unix/LibCxx/lib/Unix/x86_64-unknown-linux-gnu"
    
    if [ -f "$UE_CLANGXX" ] && [ -d "$UE_LIBCXX_INCLUDE" ]; then
        echo "Using Unreal's clang-18.1.0 and bundled libc++"
        CC="$UE_CLANG"
        CXX="$UE_CLANGXX"
        # Undefine _LIBCPP_HAS_COND_CLOCKWAIT since UE's libc++ library doesn't actually have pthread_cond_clockwait
        CMAKE_CXX_FLAGS="-nostdinc++ -isystem $UE_LIBCXX_INCLUDE -U_LIBCPP_HAS_COND_CLOCKWAIT -D_GLIBCXX_USE_CXX11_ABI=0"
        CMAKE_C_FLAGS="-D_GLIBCXX_USE_CXX11_ABI=0"
        CMAKE_EXE_LINKER_FLAGS="-L$UE_LIBCXX_LIB -nodefaultlibs -lc++ -lc++abi -lm -lc -lgcc_s -lgcc -lpthread"
        CMAKE_SHARED_LINKER_FLAGS="-L$UE_LIBCXX_LIB -nodefaultlibs -lc++ -lc++abi -lm -lc -lgcc_s -lgcc -lpthread"
    else
        echo "Warning: UE toolchain not found, falling back to system clang"
        CC=/usr/bin/clang
        CXX=/usr/bin/clang++
        CMAKE_CXX_FLAGS='-stdlib=libc++ -D_GLIBCXX_USE_CXX11_ABI=0'
        CMAKE_C_FLAGS='-D_GLIBCXX_USE_CXX11_ABI=0'
        CMAKE_EXE_LINKER_FLAGS=""
        CMAKE_SHARED_LINKER_FLAGS=""
    fi
else
    echo "Warning: Unreal Engine not found, using system clang"
    CC=/usr/bin/clang
    CXX=/usr/bin/clang++
    CMAKE_CXX_FLAGS='-stdlib=libc++ -D_GLIBCXX_USE_CXX11_ABI=0'
    CMAKE_C_FLAGS='-D_GLIBCXX_USE_CXX11_ABI=0'
    CMAKE_EXE_LINKER_FLAGS=""
    CMAKE_SHARED_LINKER_FLAGS=""
fi

mkdir -p build
cd build

CC="$CC" CXX="$CXX" cmake ../cmake \
  -DCMAKE_CXX_FLAGS="$CMAKE_CXX_FLAGS" \
  -DCMAKE_C_FLAGS="$CMAKE_C_FLAGS" \
  -DCMAKE_EXE_LINKER_FLAGS="$CMAKE_EXE_LINKER_FLAGS" \
  -DCMAKE_SHARED_LINKER_FLAGS="$CMAKE_SHARED_LINKER_FLAGS" \
  -DUE_ROOT="$UE_ROOT"

make -j$(nproc)

cd ..

mkdir -p AirLib/lib/x64/$folder_name
mkdir -p AirLib/deps/rpclib/lib
mkdir -p AirLib/deps/MavLinkCom/lib
cp $build_dir/output/lib/libAirLib.a AirLib/lib
cp $build_dir/output/lib/libMavLinkCom.a AirLib/deps/MavLinkCom/lib
cp $build_dir/output/lib/librpc.a AirLib/deps/rpclib/lib/librpc.a

# Update AirLib/lib, AirLib/deps, Plugins folders with new binaries
rsync -a --delete build/output/lib/ AirLib/lib/x64/$folder_name
rsync -a --delete external/rpclib/$RPC_VERSION_FOLDER/include AirLib/deps/rpclib
rsync -a --delete MavLinkCom/include AirLib/deps/MavLinkCom
rsync -a --delete AirLib Unreal/Plugins/AirSim/Source
rm -rf Unreal/Plugins/AirSim/Source/AirLib/src

# Update all environment projects 
for d in ~/Documents/Unreal\ Projects/*; do
    # Skip if not a directory
    [ -d "$d" ] || continue
    # Skip if symbolic link
    [ -L "${d%/}" ] && continue

    # Execute clean.sh if it exists and is executable
    if [ -x "$d/clean.sh" ]; then
        "$d/clean.sh"
    fi

    # Ensure Plugins directory exists
    mkdir -p "$d/Plugins"

    # Sync AirSim plugin into Plugins directory
    rsync -a --delete Unreal/Plugins/AirSim/ "$d/Plugins/AirSim/"
done

echo ""
echo ""
echo "=================================================================="
echo " Colosseum plugin is built! Here's how to build Unreal project."
echo "=================================================================="
echo "All environments under Unreal/Environments have been updated."
echo ""
echo "For further info see the docs:"
echo "https://codexlabsllc.github.io/Colosseum/build_linux/"
echo "=================================================================="