#!/bin/bash
# Patch Unreal's libc++ to not use pthread_cond_clockwait
UE_CONFIG="/ntfs-gen4-1tb/RoboticsProject/UnrealEngine/Engine/Source/ThirdParty/Unix/LibCxx/include/c++/v1/__config"

if [ -f "$UE_CONFIG" ]; then
    # Check if already patched
    if grep -q "// PATCHED: Disable pthread_cond_clockwait" "$UE_CONFIG"; then
        echo "Already patched"
    else
        echo "Patching $UE_CONFIG to disable pthread_cond_clockwait"
        # Comment out the lines that define _LIBCPP_HAS_COND_CLOCKWAIT
        sudo sed -i '/^#      define _LIBCPP_HAS_COND_CLOCKWAIT$/s/^#/\/\/ PATCHED: Disable pthread_cond_clockwait\n\/\/ #/' "$UE_CONFIG"
        echo "Patch applied"
    fi
else
    echo "Error: $UE_CONFIG not found"
fi
