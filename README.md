# MAI 2000 emulator running BOSS/IX

Boots MAI Basic Four's **BOSS/IX 7.5B\*22** to an interactive shell on an
emulated MAI 2000: Motorola 68010 at 8 MHz, the board's custom segmented MMU,
Winchester disk, cartridge streamer and Z8530 console. BOSS/IX is Charles River
Data Systems UNOS licensed by MAI; the kernel in the reference disk image was
built on 4 January 1991.

```
System name: MAI 2000                         System serial number: 2000-97894
Operating System: EOS5B22, BOSS/IX release 7.5B*22 (Jan  4 1991 18:22)
<single user mode>

ADMIN>ls /
ATP PS bin boot dev doc etc games mnt s10 sys tmp tools usr util
ADMIN>basic
Business BASIC level BB90 07.05B*20.01

READY
>10 FOR I=1 TO 6
>20 PRINT I*I
>30 NEXT I
>RUN
```

## Relationship to the upstream emulator

This is a fork of Armin Diehl's
[MAI-Basic-Four-System-2000-emulator](https://github.com/ardiehl/MAI-Basic-Four-System-2000-emulator),
which is where the machine, the media, the boot PROM dump and the original
emulator all come from. Upstream reaches `Executing` and stops there. Eighteen
changes take it from that point to an interactive operating system. They are
described in [NOTES.md](NOTES.md), which also records the measurements behind
each one and several wrong hypotheses, so that nobody has to repeat them.

The short version of what was missing:

* the Winchester controller had no backing store at all and no DMA engine
* its SCSI phase model did not match what the driver polls for
* the MMU did not exist; `memory.c` said so in a comment
* the timer was modelled as an edge and forced exceptions past the interrupt
  mask, instead of asserting a level
* the disk raised its completion interrupt too late for a sleeping driver
* the cartridge streamer threw away the upper half of its command block
  address and wrote controller status over kernel text
* the CMB status registers read back as every fault on the board at once
* **the SCC had no interrupts**, which is why the console stayed silent: the
  console driver enables the transmit interrupt, queues a byte and sleeps

Three debugging facilities were added along the way and are useful in their own
right: write watchpoints, a ring of recently executed program counters, and a
system call tracer that decodes BOSS/IX's `TRAP #2` convention.

## Building

Needs a C compiler, make and readline.

```
sudo apt-get install -y gcc make libreadline-dev    # Debian, Ubuntu
make
```

## Getting the operating system

The BOSS/IX media is MAI Basic Four software and is not redistributed here. It
is published on the collector's site, and `fetch-media.sh` downloads it and
builds a bootable disk:

```
./fetch-media.sh
```

That fetches a raw saveset of a working system disk and the encrypted
configuration record for its serial number, which BOSS/IX will not boot
without, and assembles `wd0.img`.

## Running

```
./eagleemu "msg all -" "bus -" "dev wd image wd0.img" g
```

It needs a real terminal: the emulator puts stdin into raw mode and echoes the
emulated serial console to stderr, so redirecting stdin makes a working boot
look like a hang.

At the firmware prompts answer `wd0` to `Boot device:` and press return at
`System file:`. Press return at the clock prompt and ESC at the startup notice,
and you are at `ADMIN>`.

The very first boot of a freshly built image finds the filesystem mounted,
because the disk was imaged from a machine that was running. Press return when
it offers the automatic check and repair; it repairs, halts cleanly, and after
a reboot comes up clean.

### Shutting down

BOSS/IX marks its filesystems dirty while mounted, so quit in this order:

1. `CTRL+D` at `ADMIN>`
2. answer `shutdown` to `single, multi, shutdown?`
3. wait for `System shutdown.  Please reboot...`
4. `CTRL+X` for the emulator debugger, then `quit`

## Emulator commands

`CTRL+X` breaks in at any time; `g` continues, `quit` exits.

| command | what it does |
|---|---|
| `device wd image <file> [unit]` | attach a disk image |
| `device mmu registers` | dump the eight segment descriptors |
| `device fw registers` | show the 4-Way boards |
| `watch <n> <addr> [len]` | log every write into a range, DMA included |
| `history [count]` | recently executed instructions, marked S or U |
| `traptrace 1` | log system calls made from user mode |
| `msave [file]` | write all of RAM to a file |

## State of the machine

Single user works: shell, filesystem, Business Basic, disk read and write.
Multi user does not yet. `/etc/sys.log` shows why: this system had its terminals
on 4-Way serial boards, and although the boards now initialise, take command
blocks and raise completion interrupts, the driver still rejects the
transmission characteristics it tries to set on those ports. The 4-Way protocol
as read out of the service manual, and what the machine actually does with it,
are written up in NOTES.md for whoever picks it up.

## Credits and licence

Emulator groundwork, the machine itself, the media and a great deal of
documentation preservation: Armin Diehl, [ardiehl.de/basicfour](http://www.ardiehl.de/basicfour/).
The 68000 core is Musashi. Manuals from [bitsavers.org](http://bitsavers.org),
principally BFISD 8079 for the CMB and M8155A for the 4-Way controller.

This program is free software under the **GNU General Public License, version 2
or later**; see [LICENSE](LICENSE). BOSS/IX is copyright MAI Basic Four, Inc.;
UNOS is copyright Charles River Data Systems, Inc. This work exists for the
preservation and study of a machine out of production for roughly forty years.
