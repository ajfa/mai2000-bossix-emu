#!/bin/sh
# Fetch the BOSS/IX operating system media and build a bootable disk image.
#
# The media is not redistributed here: it is MAI Basic Four software and it is
# published by the collector who owns the machine, so this script downloads it
# from the source. Needs curl or wget, tar and gzip.
#
# What it fetches:
#   2000_backup_micropolis.tar.gz   a raw saveset of a working BOSS/IX system
#                                   disk, taken from the machine itself
#   configrecord-nv-2000-97894.zip  the encrypted configuration record for that
#                                   machine's serial number, which BOSS/IX will
#                                   not boot without

set -e
cd "$(dirname "$0")"

BASE=http://www.ardiehl.de/basicfour/download/mai2000
MEDIA=media
mkdir -p "$MEDIA"

get () {
    if [ -f "$MEDIA/$2" ]; then
        echo "have $2"
        return
    fi
    echo "fetching $2 ..."
    if command -v curl >/dev/null 2>&1; then
        curl -fL -o "$MEDIA/$2" "$1/$2"
    else
        wget -O "$MEDIA/$2" "$1/$2"
    fi
}

get "$BASE" 2000_backup_micropolis.tar.gz
get "$BASE" configrecord-nv-2000-97894.zip

echo "unpacking ..."
tar xzf "$MEDIA/2000_backup_micropolis.tar.gz" -C "$MEDIA"
( cd "$MEDIA" && unzip -o -q configrecord-nv-2000-97894.zip )

SAVESET="$MEDIA/backup_micropolis/tsave/S0003"
CFGREC="$MEDIA/configrecord-nv-2000-97894/configrecord.bin"

[ -f "$SAVESET" ] || { echo "saveset not found at $SAVESET"; exit 1; }
[ -f "$CFGREC" ]  || { echo "config record not found at $CFGREC"; exit 1; }

echo "building wd0.img ..."
ARCHIVE=. SAVESET="$SAVESET" CFGREC="$CFGREC" OUT=wd0.img ./make-disk.sh

echo
echo "Now build the emulator and boot it:"
echo "    make"
echo "    ./eagleemu \"msg all -\" \"bus -\" \"dev wd image wd0.img\" g"
echo "Answer wd0 at the Boot device prompt. See README.md."
