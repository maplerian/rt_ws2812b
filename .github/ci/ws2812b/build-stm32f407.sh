#!/bin/sh
# SPDX-License-Identifier: Apache-2.0
# Real RT-Thread/STM32F407 SCons compile and link matrix for this package.
set -eu

profiles='static-minimal dynamic-auto timing-table-extended timing-search gamma-lut gamma-compute effect-minimal effect-all-msh sparse-slots effect-sequence-only'

fail()
{
    printf 'WS2812B_STM32_BUILD_FAIL: %s\n' "$*" >&2
    exit 1
}

# Emit the profile's compile-time macro values to stdout. This is not a
# substitute for compilation; --print-profile is only a local preflight aid.
set_profile()
{
    profile=$1
    case "$profile" in
        static-minimal|dynamic-auto|timing-table-extended|timing-search|gamma-lut|gamma-compute|\
        effect-minimal|effect-all-msh|sparse-slots|effect-sequence-only) ;;
        *) fail "unknown profile: $profile" ;;
    esac

    printf '#define PKG_USING_WS2812B\n'
    printf '#define RT_USING_SPI\n'
    case "$profile" in
        effect-all-msh)
            printf '#define WS2812B_MAX_DEVICES 3\n'
            printf '#define WS2812B_KCONFIG_SLOT_COUNT 3\n' ;;
        sparse-slots)
            printf '#define WS2812B_MAX_DEVICES 1\n'
            printf '#define WS2812B_KCONFIG_SLOT_COUNT 3\n' ;;
        *)
            printf '#define WS2812B_MAX_DEVICES 1\n'
            printf '#define WS2812B_KCONFIG_SLOT_COUNT 1\n' ;;
    esac
    printf '#define WS2812B_USING_KCONFIG_DEVICES\n'
    if [ "$profile" != dynamic-auto ]; then
        printf '#define WS2812B_CHECK_PARAMETERS\n'
    fi

    case "$profile" in
        dynamic-auto|gamma-compute|effect-all-msh)
            printf '#define WS2812B_MEMORY_DYNAMIC\n' ;;
        *)
            printf '#define WS2812B_MEMORY_STATIC\n' ;;
    esac

    case "$profile" in
        timing-table-extended)
            printf '#define WS2812B_TIMING_TABLE\n'
            printf '#define WS2812B_CHECK_EXTENDED\n' ;;
        timing-search|gamma-compute)
            printf '#define WS2812B_TIMING_SEARCH\n' ;;
        *)
            printf '#define WS2812B_TIMING_DIRECT\n' ;;
    esac

    case "$profile" in
        dynamic-auto|timing-search|gamma-compute)
            printf '#define WS2812B_ENCODER_AUTO\n'
            printf '#define WS2812B_ENCODER_SELF_CHECK\n' ;;
        *)
            printf '#define WS2812B_ENCODER_FAST_ONLY\n' ;;
    esac

    case "$profile" in
        effect-all-msh)
            printf '#define WS2812B_USING_EFFECT\n'
            printf '#define WS2812B_USING_MSH\n'
            printf '#define RT_USING_FINSH\n'
            printf '#define FINSH_USING_MSH\n'
            for method in SOLID BLINK BREATHE GRADIENT GRADATION CHASE SEQUENCE; do
                printf '#define WS2812B_EFFECT_%s\n' "$method"
            done
            printf '#define WS2812B_EFFECT_CUSTOM_SLOTS 2\n'
            ;;
        effect-minimal)
            printf '#define WS2812B_USING_EFFECT\n'
            printf '#define WS2812B_EFFECT_SOLID\n'
            printf '#define WS2812B_EFFECT_BLINK\n' ;;
        sparse-slots)
            printf '#define WS2812B_USING_EFFECT\n'
            printf '#define WS2812B_EFFECT_GRADATION\n' ;;
        effect-sequence-only)
            printf '#define WS2812B_USING_EFFECT\n'
            printf '#define WS2812B_EFFECT_SEQUENCE\n' ;;
    esac

    case "$profile" in
        gamma-lut|effect-all-msh)
            printf '#define WS2812B_USING_GAMMA\n'
            printf '#define WS2812B_GAMMA_22\n'
            printf '#define WS2812B_GAMMA_28\n' ;;
        gamma-compute)
            printf '#define WS2812B_USING_GAMMA\n'
            printf '#define WS2812B_GAMMA_COMPUTE\n' ;;
    esac

    case "$profile" in
        effect-all-msh)
            printf '#define WS2812B_DEV0_INITIAL_KIND 2\n'
            printf '#define WS2812B_DEV1_INITIAL_KIND 6\n'
            printf '#define WS2812B_DEV2_INITIAL_KIND 1\n' ;;
        effect-minimal)
            printf '#define WS2812B_DEV0_INITIAL_KIND 2\n' ;;
        sparse-slots)
            printf '#define WS2812B_DEV2_INITIAL_KIND 6\n' ;;
        effect-sequence-only)
            printf '#define WS2812B_DEV0_INITIAL_KIND 0\n' ;;
    esac

    case "$profile" in
        sparse-slots)
            printf '#define WS2812B_KCONFIG_DEVICE2\n'
            define_device 2 40 '"spi3"' '"spi30"' ;;
        *)
            printf '#define WS2812B_KCONFIG_DEVICE0\n'
            define_device 0 27 '"spi1"' '"spi10"'
            if [ "$profile" = effect-all-msh ]; then
                printf '#define WS2812B_KCONFIG_DEVICE1\n'
                printf '#define WS2812B_KCONFIG_DEVICE2\n'
                define_device 1 40 '"spi2"' '"spi20"'
                define_device 2 40 '"spi3"' '"spi30"'
            fi
            ;;
    esac
}

define_device()
{
    n=$1
    count=$2
    bus=$3
    name=$4
    printf '#define WS2812B_DEV%s_BUS %s\n' "$n" "$bus"
    printf '#define WS2812B_DEV%s_NAME %s\n' "$n" "$name"
    printf '#define WS2812B_DEV%s_LEDS %s\n' "$n" "$count"
    printf '#define WS2812B_DEV%s_HZ 9000000\n' "$n"
    printf '#define WS2812B_DEV%s_VARIANT 1\n' "$n"
    printf '#define WS2812B_DEV%s_RESET_US 300\n' "$n"
    printf '#define WS2812B_DEV%s_CLOCKS_PER_BIT 12\n' "$n"
    printf '#define WS2812B_DEV%s_T0H 3\n' "$n"
    printf '#define WS2812B_DEV%s_T1H 6\n' "$n"
    printf '#define WS2812B_DEV%s_FRAME_BYTES 0\n' "$n"
    printf '#define WS2812B_DEV%s_INITIAL_COLOR 0x501900\n' "$n"
    printf '#define WS2812B_DEV%s_INITIAL_BACKGROUND 0x000000\n' "$n"
    printf '#define WS2812B_DEV%s_INITIAL_PERIOD_TICKS 20\n' "$n"
}

case "${1:-}" in
    --list-profiles)
        printf '%s\n' $profiles
        exit 0 ;;
    --print-profile)
        [ "$#" -eq 2 ] || fail 'Usage: --print-profile PROFILE'
        set_profile "$2"
        exit 0 ;;
    '')
        profile=${WS2812B_CI_PROFILE:-static-minimal} ;;
    *)
        [ "$#" -eq 1 ] || fail 'Usage: build-stm32f407.sh [PROFILE]'
        profile=$1 ;;
esac

# Generate effective feature macros before SCons reads the BSP configuration.
ci_dir=${GITHUB_WORKSPACE:-$(pwd)}/_ci
mkdir -p "$ci_dir"
profile_defines="$ci_dir/ws2812b-profile-$profile.h"
set_profile "$profile" > "$profile_defines"
package_root=${GITHUB_WORKSPACE:-$(pwd)}
rtthread_dir="$ci_dir/rt-thread"
env_dir="$ci_dir/env"
bsp_rel=${RTTHREAD_BSP:-bsp/stm32/stm32f407-atk-explorer}
bsp_dir="$rtthread_dir/$bsp_rel"
ref=${RTTHREAD_REF:-master}
env_ref=${RTTHREAD_ENV_REF:-master}

printf 'CI profile: %s\nRT-Thread ref: %s\nBSP: %s\n' "$profile" "$ref" "$bsp_rel"
git clone --depth 1 --branch "$ref" https://github.com/RT-Thread/rt-thread.git "$rtthread_dir"
git clone --depth 1 --branch "$env_ref" https://github.com/RT-Thread/env.git "$env_dir"
test -f "$bsp_dir/SConstruct" || fail 'STM32F407 BSP SConstruct not found'
test -f "$env_dir/env.py" || fail 'RT-Thread Env missing env.py'

export RTT_ROOT="$rtthread_dir"
export RTT_ENV="$env_dir"
export RTT_CC=gcc
export RTT_EXEC_PATH="$(dirname "$(command -v arm-none-eabi-gcc)")"
export PATH="$HOME/.local/bin:$env_dir:$PATH"

cd "$bsp_dir"
scons --pyconfig-silent
# The official RT-Thread package index also contains a DIFFERENT ws2812b
# package under the same PKG_USING_WS2812B symbol. Never ask Env to fetch it.
# This CI tests ONLY the checked-out repository staged below.
if grep -Eq '^CONFIG_PKG_USING_WS2812B=y$' .config || \
   grep -Eq '^#define[[:space:]]+PKG_USING_WS2812B([[:space:]]|$)' rtconfig.h; then
    fail 'BSP already enables the official ws2812b package; CI must use only the local checkout'
fi
python "$env_dir/env.py" package --upgrade
python "$env_dir/env.py" package --update

# STM32F4 HAL/CMSIS drivers are installed beside the BSP, NOT under
# stm32f407-atk-explorer/packages/. CMSIS-Core is fetched separately there.
# Check the exact driver paths populated by RT-Thread Env package --update.
stm32_libraries="$rtthread_dir/bsp/stm32/libraries"
for relative in \
    stm32f4_cmsis_driver-latest/Include/stm32f4xx.h \
    stm32f4_hal_driver-latest/Inc/stm32f4xx_hal.h; do
    header="$stm32_libraries/$relative"
    [ -f "$header" ] || fail "STM32 dependency missing after Env update: $header"
    printf 'STM32 dependency ready: %s\n' "$header"
done

# Stage only actual package compilation inputs (not docs or binary media).
local_pkg="$bsp_dir/packages/ws2812b"
[ ! -e "$local_pkg" ] || fail 'destination package path is already occupied'
mkdir -p "$local_pkg"
cp "$package_root"/SConscript "$package_root"/ws2812.c "$package_root"/ws2812.h \
   "$package_root"/ws2812_config.h "$package_root"/ws2812_color.h \
   "$package_root"/ws2812_timing.c "$package_root"/ws2812b_msh.c "$local_pkg/"
cp -R "$package_root"/effect "$package_root"/gamma "$local_pkg/"

# RT-Thread Env generates BSP/packages/SConscript, which discovers package
# SConscript files under that directory. The BSP's own SConscript already
# loads this bridge. Adding the same package to SConstruct explicitly would
# link every ws2812b object twice (duplicate symbols in rt-thread.elf).
# Keep the package switch only in rtconfig.h, not in Env's .config, so
# Env cannot fetch the unrelated upstream package with the same symbol.
[ -f "$bsp_dir/packages/SConscript" ] || \
    fail 'RT-Thread Env packages/SConscript bridge missing after update'

rtconfig="$bsp_dir/rtconfig.h"
test -f "$rtconfig" || fail 'BSP rtconfig.h missing after pyconfig'
tail -n 1 "$rtconfig" | grep -Eq '^#endif([[:space:]]|$)' || fail 'rtconfig.h closing guard changed'
config_tmp="$ci_dir/rtconfig-$profile.h"
# Keep all defines inside rtconfig.h guard, visible to SCons GetDepend and C.
sed '$d' "$rtconfig" > "$config_tmp"
printf '\n/* CI-only effective C compilation profile: %s */\n' "$profile" >> "$config_tmp"
cat "$profile_defines" >> "$config_tmp"
printf '\n#endif\n' >> "$config_tmp"
cp "$config_tmp" "$rtconfig"

# Check that SCons and the C compiler will see the EXACT same profile.
while IFS= read -r macro; do
    grep -Fxq "$macro" "$rtconfig" || fail "effective rtconfig.h lost profile setting: $macro"
done < "$profile_defines"

revisions="$ci_dir/ws2812b-revisions-$profile.txt"
{
    printf 'source_head=%s\n' "${GITHUB_SHA:-local-checkout}"
    printf 'rtthread_sha=%s\n' "$(git -C "$rtthread_dir" rev-parse HEAD)"
    printf 'env_sha=%s\n' "$(git -C "$env_dir" rev-parse HEAD)"
    printf 'compiler=%s\n' "$(arm-none-eabi-gcc -dumpfullversion)"
    printf 'profile=%s\n' "$profile"
} | tee "$revisions"
printf '%s\n' 'CI effective profile defines:'
cat "$profile_defines"

# SCons compilation/link is the real CI gate. If the BSP, package, or linker
# fails then the job fails, regardless of any other source-selection check.
scons -j"$(nproc)"
elf="$bsp_dir/rt-thread.elf"
test -s "$elf" || fail 'real linked STM32F407 ELF was not generated'
arm-none-eabi-size "$elf"
elf_header=$(arm-none-eabi-readelf -h "$elf") || fail 'linked output is not a readable ELF file'
printf '%s\n' "$elf_header" | grep -Eq 'Machine:[[:space:]]*ARM([[:space:]]|$)' || \
    fail 'linked output is not an ARM ELF'

have_define()
{
    grep -Eq "^#define[[:space:]]+$1([[:space:]]|$)" "$profile_defines"
}

check_object()
{
    object=$1
    expected=$2
    matches=$(find "$bsp_dir/build" -type f -path '*/packages/ws2812b/*' -name "$object" -print)
    if [ "$expected" = present ]; then
        [ -n "$matches" ] || fail "compiled object missing: $object"
        # A duplicate object can hide accidentally double-included SConscript
        # and yield a falsely successful CI source-selection check.
        lines=$(printf '%s\n' "$matches" | wc -l | tr -d '[:space:]')
        [ "$lines" -eq 1 ] || fail "expected one compiled $object, found $lines: $matches"
    elif [ -n "$matches" ]; then
        fail "disabled option still compiled object: $object"
    fi
    printf '%s: %s\n' "$object" "$expected"
}

check_object ws2812.o present
if have_define WS2812B_TIMING_TABLE || have_define WS2812B_TIMING_SEARCH || have_define WS2812B_CHECK_EXTENDED; then
    check_object ws2812_timing.o present
else
    check_object ws2812_timing.o absent
fi
if have_define WS2812B_GAMMA_COMPUTE; then
    check_object ws2812_gamma_compute.o present
else
    check_object ws2812_gamma_compute.o absent
fi
if have_define WS2812B_USING_EFFECT; then
    check_object ws2812_effect.o present
else
    check_object ws2812_effect.o absent
fi
for method in solid blink breathe gradient gradation chase sequence; do
    switch=$(printf 'WS2812B_EFFECT_%s' "$method" | tr '[:lower:]' '[:upper:]')
    if have_define "$switch"; then
        check_object "ws2812_effect_$method.o" present
    else
        check_object "ws2812_effect_$method.o" absent
    fi
done
if have_define WS2812B_USING_MSH; then
    check_object ws2812b_msh.o present
else
    check_object ws2812b_msh.o absent
fi
printf 'WS2812B_STM32_BUILD_PASS profile=%s\n' "$profile"
