# MAI 2000 emulator running BOSS/IX

> **Development has moved. Go to
> [ardiehl/MAI-Basic-Four-System-2000-emulator](https://github.com/ardiehl/MAI-Basic-Four-System-2000-emulator).**
>
> Everything in this fork has been merged there, and that repository is now the
> single place where the emulator is built, fixed and released. Clone that one.
> This fork stays online as the record of how the machine was brought up, not as
> something to build.

This tree is the fork in which MAI Basic Four's **BOSS/IX 7.5B\*22** was first
booted to an interactive shell on an emulated MAI 2000: Motorola 68010 at 8 MHz,
the board's custom segmented MMU, Winchester disk, cartridge streamer and Z8530
console. BOSS/IX is Charles River Data Systems UNOS licensed by MAI; the kernel
in the reference disk image was built on 4 January 1991.

```
System name: MAI 2000                         System serial number: 2000-97894
Operating System: EOS5B22, BOSS/IX release 7.5B*22 (Jan  4 1991 18:22)
<single user mode>

ADMIN>basic
Business BASIC level BB90 07.05B*20.01

READY
>10 FOR I=1 TO 6
>20 PRINT I*I
>30 NEXT I
>RUN
```

## Status

The machine is usable, and the work continues upstream rather than here.

What has been measured on the current upstream tree, with a fresh copy of the
reference disk image on every run:

| | |
|---|---|
| single user shell at `ADMIN>` | about six seconds from the boot prompt |
| filesystem check | no repairs needed, `/etc/sys.log` empty |
| multi user mode | starts, login as `admin`, `ps` lists the running processes |
| Business Basic | BB90 runs programs |
| disk | read and write through the Winchester controller and its DMA engine |
| firmware self test | passes every stage, `cmb` through `cs` |
| shutdown | clean, filesystems marked clean |
| clock | tracks real time, having previously stood still |
| console typing | about 50 ms per keystroke, and the same on a starved host |

## What was wrong, and where it is written down

Upstream reached `Executing` and stopped. The changes that take it from there to
an interactive operating system are described in [NOTES.md](NOTES.md), which
also records the measurement behind each one and several hypotheses that turned
out to be wrong, so that nobody has to spend time on them again.

The short version of what was missing:

* the Winchester controller had no backing store at all and no DMA engine
* its SCSI phase model did not match what the driver polls for
* the MMU did not exist; `memory.c` said so in a comment
* the timer was modelled as an edge and forced exceptions past the interrupt
  mask, instead of asserting a level
* the disk raised its completion interrupt too late for a sleeping driver
* the cartridge streamer threw away the upper half of its command block address
  and wrote controller status over kernel text
* the CMB status registers read back as every fault on the board at once
* the SCC had no interrupts, which is why the console stayed silent: the console
  driver enables the transmit interrupt, queues a byte and sleeps
* the SCC drove its interrupt line with the chip's master interrupt enable
  clear, which killed the firmware self test on an unhandled level 5

Three debugging facilities came out of the work and are useful in their own
right: write watchpoints, a ring of recently executed program counters, and a
system call tracer that decodes the BOSS/IX `TRAP #2` convention.

## Open items

Tracked upstream, listed here so that a reader arriving at this fork knows what
is and is not solved. Nothing on this list is being worked on in this
repository.

**Blocking other work**

* The checksum of the encrypted configuration record. The kernel reads it from a
  fixed block and refuses to boot when it does not match the serial number held
  in NVRAM. Until it can be recomputed, the machine cannot be reconfigured for
  its full memory, a second Winchester controller, or a floppy drive usable
  outside the boot partition.

**Machine and media**

* Second Winchester controller, and a disk image carrying the field diagnostics.
* Floppy support beyond the boot partition, which depends on the record above.
* Any newly published disk image needs the previous owner's business records
  removed from it first, while leaving the Basic programs runnable.

**Serial and terminals**

* The 4-Way controller needs character input, reset and its register set, so
  that the extra terminal ports have somewhere to appear. The login banner
  already reaches them.
* Outgoing key translation for the MAI keyboard, including the MB function keys
  and backspace, which BOSS/IX expects as `^H` while it treats `0x7f` as line
  kill.
* Serial ports and local sockets as endpoints.

**Toolchain**

* Whether the on-disk assembler runs. That answer decides everything after it.
* A linker, C compiler, libraries and headers that match this release rather
  than a later UNOS.
* Failing that, the object file format, and whether an old binutils can target
  it.
* Finishing the installation tape, and whether a utility compiled for the 68020
  can be patched to run here.
* Identifying the filesystem, which may be close to System V.

## Notes for anyone reading the code

Two mistakes are easy to make in this emulator and both cost real time.

Interrupts must not be paced by counting instructions. A 68000 sitting in `stop`
executes none, so a counter never advances and the interrupt never arrives.

The polling counters in the device tick come down once per pass round the loop,
not once per instruction. Putting a sleep in the idle path turns every poll that
hangs off a counter into one poll every several seconds. Nothing fails to boot
when that happens; the machine merely becomes impossible to type at, which is
why it is easy to ship by accident. Measuring the echo one key at a time, with a
pause between keys, is what catches it.

## Credits and licence

Emulator groundwork, the machine itself, the media and a great deal of
documentation preservation: Armin Diehl,
[ardiehl.de/basicfour](http://www.ardiehl.de/basicfour/). The 68000 core is
Musashi. Manuals from [bitsavers.org](http://bitsavers.org), principally BFISD
8079 for the CMB and M8155A for the 4-Way controller.

This program is free software under the **GNU General Public License, version 2
or later**; see [LICENSE](LICENSE). BOSS/IX is copyright MAI Basic Four, Inc.;
UNOS is copyright Charles River Data Systems, Inc. This work exists for the
preservation and study of a machine out of production for roughly forty years.
