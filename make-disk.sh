#!/bin/sh
# Build a bootable BOSS/IX disk image for the MAI 2000 emulator.
#
# Two pieces go into it:
#
#  1. The raw disk saveset. tsave/S0003 in the backup archive is one 512 byte
#     saveset header block followed by the raw contents of /dev/rwd0, so block 0
#     of the real disk is at byte offset 512. Block 0 of the saveset carries the
#     volume label VOL 30WD0 and the strings /dev/rwd0 and **FILESYSTEM**, and
#     the block after it starts with the disk loader signature, the same
#     "NN`..LD." that the installation tape loader begins with. That is how the
#     offset was established.
#
#  2. The configuration record. BOSS/IX will not come up without it: the serial
#     number in NVRAM has to agree with an encrypted record on the disk, and
#     without a match the kernel stops with "Invalid system configuration
#     record". The kernel reads it from block 84777, which is past the end of
#     the saveset because the saveset only covers the filesystem area. The
#     record for the machine this disk came from is published on
#     the collector's site. Run fetch-media.sh to download and unpack both,
#     then this script, or set SAVESET and CFGREC yourself.

set -e

# Both inputs can be overridden from the environment; fetch-media.sh does that.
SAVESET=${SAVESET:-media/backup_micropolis/tsave/S0003}
CFGREC=${CFGREC:-media/configrecord-nv-2000-97894/configrecord.bin}
OUT=${OUT:-wd0.img}

CFGBLOCK=84777
TOTALBLOCKS=84800

if [ ! -f "$SAVESET" ]; then
    echo "saveset not found: $SAVESET"
    echo "run ./fetch-media.sh first, or set SAVESET"
    exit 1
fi
if [ ! -f "$CFGREC" ]; then
    echo "configuration record not found: $CFGREC"
    exit 1
fi

echo "extracting raw disk from the saveset, skipping the header block"
dd if="$SAVESET" of="$OUT" bs=512 skip=1 status=none

echo "padding to $TOTALBLOCKS blocks so the reserved area exists"
truncate -s $((TOTALBLOCKS * 512)) "$OUT"

echo "writing the configuration record at block $CFGBLOCK"
dd if="$CFGREC" of="$OUT" bs=512 seek=$CFGBLOCK count=1 conv=notrunc status=none

ls -l "$OUT"
echo
echo "now run:  ./eagleemu \"msg all -\" \"bus -\" \"dev wd image $OUT\" g"
echo "answer wd0 at the Boot device prompt and press return at System file"
