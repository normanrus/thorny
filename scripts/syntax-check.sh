#!/usr/bin/env bash
# Local GCC syntax-check for the Thorny sources.
#
# The real build targets MSVC (Windows GD 2.2081 + Geode 5.11.1). This script
# only exists so the sources can be parsed on a Linux box without a full Geode
# toolchain; it stubs the handful of Windows-only headers the Geode/cocos
# headers pull in. It is NOT part of the build and NOT shipped.
#
# Usage: scripts/syntax-check.sh [file ...]   (defaults to src/*.cpp)
set -u

GEODE_SDK="${GEODE_SDK:-/tmp/sdk}"
CACHE="${THORNY_CACHE:-/tmp/cpmcache}"

inc() {
    ls -d "$1"/*/include 2>/dev/null | head -1
}
FMT_INC="$(inc "$CACHE/fmt")"
TULIP_INC="$(inc "$CACHE/tuliphook")"
RESULT_INC="$(inc "$CACHE/result")"
JSON_INC="$(inc "$CACHE/json")"
NTF_INC="$(inc "$CACHE/nontype_functional")"
ASP_INC="$(inc "$CACHE/asp2")"
ARC_INC="$(inc "$CACHE/arc")"

# The generated bindings live under two trees: a checkout dir and the build dir.
BINDING_INCS=()
for d in /tmp/modb/bindings/bindings /tmp/bindings/bindings/include; do
    [ -d "$d" ] && BINDING_INCS+=("-isystem" "$d")
done

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
STUB="${THORNY_STUB:-/tmp/winstub}"

FILES=("$@")
if [ "${#FILES[@]}" -eq 0 ]; then
    FILES=("$ROOT"/src/*.cpp)
fi

GXX="${GXX:-c++}"
STATUS=0
for f in "${FILES[@]}"; do
    echo "== $f"
    "$GXX" -std=gnu++23 -fsyntax-only -w \
        -include "$STUB/preinc.h" \
        -D_WIN32 -DWIN32 -D_WIN64 \
        -D_MSC_VER=1937 -D_MSC_FULL_VER=193732822 -D_MSVC_LANG=202302L \
        -DGEODE_IS_WINDOWS -DGEODE_IS_DESKTOP -DGEODE_COMP_GD_VERSION=22081 \
        -DGEODE_GD_VERSION=2.2081 -DGEODE_GD_VERSION_STRING='"2.2081"' \
        -DGEODE_MOD_ID='"norman.thorny"' -DMAT_JSON_DYNAMIC=1 \
        -DNOMINMAX -DFMT_HEADER_ONLY=1 -DARC_FEATURE_TIME=1 \
        -DPROJECT_NAME=Thorny -DThorny_EXPORTS \
        -I"$STUB" \
        -I"$GEODE_SDK/loader/include" \
        -I"$GEODE_SDK/loader/include/Geode/cocos/include" \
        -I"$GEODE_SDK/loader/include/Geode/cocos/extensions" \
        -I"$GEODE_SDK/loader/include/Geode/fmod" \
        "${BINDING_INCS[@]}" \
        ${FMT_INC:+-isystem "$FMT_INC"} \
        ${TULIP_INC:+-isystem "$TULIP_INC"} \
        ${RESULT_INC:+-isystem "$RESULT_INC"} \
        ${JSON_INC:+-isystem "$JSON_INC"} \
        ${NTF_INC:+-isystem "$NTF_INC"} \
        ${ASP_INC:+-isystem "$ASP_INC"} \
        ${ARC_INC:+-isystem "$ARC_INC"} \
        "$f" 2>&1 | grep -E "error:" | head -30
    rc=${PIPESTATUS[0]}
    if [ "$rc" -ne 0 ]; then STATUS=1; fi
done
exit "$STATUS"
