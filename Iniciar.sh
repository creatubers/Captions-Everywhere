#!/bin/sh
set -eu
ROOT=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd -P)
if [ "$(uname -m)" != x86_64 ]; then
    printf '%s\n' 'Este paquete requiere Linux Intel/AMD de 64 bits (x86_64).' >&2
    exit 1
fi
STATE_DIR=${CAPTIONS_EVERYWHERE_STATE_DIR:-"$ROOT/state"}
mkdir -p "$STATE_DIR"
if [ ! -w "$STATE_DIR" ]; then
    printf '%s\n' 'La carpeta necesita permiso de escritura. Cópiala a una ubicación de tu usuario.' >&2
    exit 1
fi
# Preserve the desktop language before selecting the bundled runtime locale.
if [ "${CAPTIONS_EVERYWHERE_SYSTEM_LOCALE+x}" != x ]; then
    export CAPTIONS_EVERYWHERE_SYSTEM_LOCALE="${LANGUAGE:-${LC_ALL:-${LC_MESSAGES:-${LANG:-C}}}}"
fi
export LC_ALL=C.UTF-8
export LC_PORTABLE_ROOT="$ROOT"
export CAPTIONS_EVERYWHERE_STATE_DIR="$STATE_DIR"
export QT_PLUGIN_PATH="$ROOT/plugins"
export QT_QPA_PLATFORM_PLUGIN_PATH="$ROOT/plugins/platforms"
export QT_QPA_FONTDIR="$ROOT/resources"
XML_ROOT=$(printf '%s' "$ROOT" | sed 's/\&/\&amp;/g; s/</\&lt;/g; s/>/\&gt;/g; s/"/\&quot;/g')
XML_STATE=$(printf '%s' "$STATE_DIR" | sed 's/\&/\&amp;/g; s/</\&lt;/g; s/>/\&gt;/g; s/"/\&quot;/g')
mkdir -p "$STATE_DIR/font-cache"
printf '<?xml version="1.0"?><fontconfig><dir>%s/resources</dir><cachedir>%s/font-cache</cachedir></fontconfig>\n' "$XML_ROOT" "$XML_STATE" > "$STATE_DIR/fonts.conf"
export FONTCONFIG_FILE="$STATE_DIR/fonts.conf"
export FONTCONFIG_PATH="$ROOT/resources"
export XKB_CONFIG_ROOT="$ROOT/resources/xkb"
export QT_QUICK_BACKEND=software
export QT_OPENGL=software
# Qt Widgets uses the shared-memory Wayland backend; no GPU driver is needed.
if [ -z "${QT_QPA_PLATFORM:-}" ]; then
    if [ -n "${WAYLAND_DISPLAY:-}" ]; then export QT_QPA_PLATFORM=wayland; else export QT_QPA_PLATFORM=xcb; fi
fi
unset LD_PRELOAD LD_LIBRARY_PATH QT_QPA_PLATFORMTHEME
exec "$ROOT/lib/ld-linux-x86-64.so.2" --library-path "$ROOT/lib" "$ROOT/bin/captions-everywhere" "$@"
