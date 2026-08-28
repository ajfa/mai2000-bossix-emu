# What was changed, and where it got to

This is a fork of Armin Diehl's MAI 2000 emulator, upstream commit
`ac9d906c74bd252468e02347c3c119ed7bc77dc6`, with the changes described here.

Upstream reaches `Executing` and then hangs. The patched tree boots BOSS/IX
7.5B\*22 from disk to a **fully interactive single user system**: the startup
banner prints, the filesystem check runs and repairs, the clock prompt answers,
the ADMIN> shell executes commands, and Business Basic BB90 07.05B\*20.01
enters, runs and exits programs. Console output and keyboard input both work,
over the CMB SCC with real interrupts. What remains for multi user operation is
written up at the end.

## Result

```
System name: MAI 2000                         System serial number: 2000-97894
Operating System: EOS5B22, BOSS/IX release 7.5B*22 (Jan  4 1991 18:22)
07:40 am, 11/16/11.  Update clock: hhmmssxx mmddyy
<single user mode>

ADMIN>date
Wed Nov 16 2011 07:40:19
ADMIN>ls /
ATP PS bin boot dev doc etc games mnt s10 sys tmp tools usr util
ADMIN>sysinfo
System Serial Number: 2000-97894
 Console:   SCC     0    19200  ...  edt
ADMIN>basic
Business BASIC level BB90 07.05B*20.01

READY
>10 LET A=0
>20 FOR I=1 TO 6
>30 LET A=A+I*I
>40 PRINT "SUMA DE CUADRADOS HASTA",I,"ES",A
>50 NEXT I
>RUN
SUMA DE CUADRADOS HASTA 6ES 91
READY
>RELEASE
ADMIN>
```

The NVRAM clock reads November 2011, which is when the machine this disk was
imaged from last ran. On the first boot the filesystem is dirty, exactly as a
real machine left running would be: the kernel repairs it, shuts down cleanly
and asks for a reboot, and the second boot comes up clean. A session killed at
the shell leaves the filesystem dirty again, so the driver scripts break into
the emulator debugger on the shutdown message, reset the machine and boot
again, which makes every session self healing.

## Changes

### 1. fd.c, uninitialised buffer

`fd_write_cmd` builds a `char params[255]` in a `switch (type)` that has cases
1, 2 and 4 but no case 3. Type 3 covers Read Address, Read Track and Write
Track, so for those commands `params` was passed to a `%s` while still holding
stack garbage, and glibc aborted with `*** buffer overflow detected ***` as soon
as any message class was enabled. This made the emulator unusable with logging
on, which is the only way to debug it. Fixed by initialising the buffer and
adding the missing case.

### 2. wd.c and wd.h, the disk

Upstream models the controller registers and part of the SCSI handshake but has
no backing store at all, no `fopen` anywhere, and no way to attach an image.
Added:

* a raw 512 byte per block image per unit, with `device wd image <file> [unit]`
  to attach one, and the image shown in `device wd registers`
* a DMA engine. The three DMA address registers hold the ones complement of the
  system address shifted right once, so the byte address is
  `((~dmaAddress) << 1) & 0xffffff` and the register is decremented after each
  word. The existing DMA loopback test already relied on that, the transfer
  routines follow the same rule so the two agree.
* the commands BOSS/IX actually issues: TEST UNIT READY, REZERO, REQUEST SENSE,
  SEEK, START/STOP, VERIFY, READ and READ(10), WRITE, WRITE(10) and
  WRITE AND VERIFY, SEND DIAGNOSTIC, RECEIVE DIAGNOSTIC, MODE SENSE,
  MODE SELECT, READ CAPACITY and a FORMAT stub
* the logical block address is now taken from the CDB. Upstream read it out of
  `replyBuffer`, which had just been zeroed, so every read was of block 0.
* an image survives a CPU reset instead of being dropped

### 3. wd.c, the phase machine

The driver's completion loop is

```
00024c00  cmpi.b  #-$34, (A0)      ; A0 = cc0009, -$34 = 0xcc
```

so it spins until the status register reads `0xcc`. Watching it poll gives the
whole model:

| status | phase |
|---|---|
| `0x48` | data in, SBUSY + INPFULL, a data byte is ready |
| `0xcc` | status, SCMD + SBUSY + OPCOMP + INPFULL, the status byte is ready |
| `0xe8` | message, SCMD + SMSG + SBUSY + INPFULL |
| `0x00` | bus free, controller deselected |

A command whose data moved by DMA has to land on `0xcc` directly without
passing through the data in phase, which upstream did not do. The phase ladder
was rewritten around those four values, a `SCSI_S_STATUS` state was added, the
SCSI status byte was split out of the reply buffer into its own field, and the
`Invalid state 3` message that fired on every command is gone.

### 4. mmu.c and mmu.h, new

`memory.c` says "MMU not included here" and there was no MMU anywhere. Once the
disk worked the kernel got as far as pid 1 and died with
`68010 memory management trap`, writing to `0x00800000` from `0x1b02a`.

Section 3.2.16 of the service manual describes the unit and the addresses match
what the kernel touches:

* base descriptors are written in the `8XXXXX` window, signal MMBWE-
* limit descriptors are written in the `AXXXXX` window, signal MMLWE-
* logical A09 through A20 are translated, A01 through A08 pass through, which is
  what sets the 512 byte minimum segment size
* the segment is selected by A01 A02 A03 in supervisor mode and by A21 A22 A23
  in user mode, so supervisor accesses are physical and user accesses translate
* base descriptor is `R X TYPE A20..A09`, limit descriptor is `0000 A20..A09`
* physical A09..A20 is base plus logical A09..A20, three 4 bit adders
* TYPE decodes to four limit policies: absent, error if addr >= limit, error if
  addr < limit, error if addr <= limit. The last two are for stacks.
* four status bits per segment: written, execute error, write error, limit error

All of that is implemented except the execute error, because Musashi's read
callbacks do not distinguish a program fetch from a data read. A violation is
reported to the CPU as a bus error, which is what MMERR- does on the real board.
`device mmu registers` dumps the eight descriptors. The layout above is borne
out in practice: once the kernel gets far enough to program real descriptors it
translates a whole boot without a single fault.

### 5. scc.c, baud rate latches

`BAUD_ADDR 0x640000` is inside the SCC address range but had no handler, so
every write produced a bus error message. The boot PROM never writes it, the
BOSS/IX tty driver writes it for both ports. Added a write only latch per port,
readable back.

### 6. sim.c, write watchpoints

The existing breakpoints trigger on the program counter, which is useless when
the question is "who writes this variable". `watch <n> <address> [len]` logs
every store into a range, with the storing instruction, and catches controller
DMA as well as CPU stores because it sits in `sys_write_byte` and
`sys_write_word`. This is what identified the idle loop's wait condition.

### 7. pit.c, the timer is level sensitive and autovectored

Two separate faults were tangled together here.

`m68k_pulse_interrupt` calls `m68ki_exception_interrupt` directly, which **takes
the exception immediately and never consults the interrupt mask in the SR**. The
68230 timer request is a level, not an edge: ZDS, bit 0 of the timer status
register, is set when the counter reaches zero and stays set until software
writes a one to it, and the request is asserted for as long as ZDS is set and
the TOUT control selects an interrupt mode. The old model forced an exception
the moment the counter hit zero, so the boot PROM's brief timer register test
raised an interrupt that could never be withdrawn and that the PROM could not
refuse. Now `pit_update_irq` asserts and negates a real request line through the
new `m68k_set_int_line`, and the CPU takes it when its own mask allows.

The timer is **autovectored**, and an earlier attempt to hand back the
programmed vector was wrong. Three independent things say so: only PC3/TOUT is
wired, to INT6, and TIACK is not connected, so the 68230 can never supply a
vector however TOUT control is programmed; the boot PROM installs its handler at
the level 6 autovector while it programs the timer; and the only writes the PROM
ever makes to the vector register are a read back register test pattern, which
is exactly what an earlier "only trust a written vector register" heuristic
mistook for a real vector. `src/pit.c.vectored` preserves that dead end.

### 8. wd.c, the operation complete interrupt

This is what actually had the kernel idling. The phase ladder only advanced when
the host read a status or message byte, and the completion interrupt was raised
at the very end of the message phase. That works while the boot loader is
polling and is useless once a real driver sleeps: BOSS/IX writes its vector to
the controller, sets INTEN, issues one read of block 8198 and blocks in a wait
loop at `0x0b76`. Nothing ever walked the phases, so the interrupt was never
reached and the flag the loop tests at `0x34b8c` was never written by anyone.
The controller now raises the interrupt itself the moment a command completes
and its status byte is ready, modelled as a level asserted on completion and
negated on the acknowledge cycle, handing back the vector the kernel programmed.

### 9. cmb.c, status registers that read as every fault at once

`CMBR_MEMPAR_HI`, `CMBR_MEMPAR_LO` and `CMBR_GENSTATUS` were recognised in the
switch but fell through to the unhandled path, which returns all ones, and a
byte read of the odd half of the status register was not decoded at all. Per
table 3-1 of the service manual the CMB status bits include the memory
management error flag, the main memory parity error flag, the power fail flag
and the stack overflow flag, so all ones reads as every fault on the board being
active simultaneously. The boot PROM never looks. The kernel does, immediately,
and took a memory management trap the moment it did. These now return zero, no
fault latched, and the odd byte of the status register returns the low half.

### 10. sim.c, a ring of recently executed program counters

The instruction hook already receives the PC of every instruction about to run,
so recording it into a 256 entry ring costs almost nothing and answers the one
question a watchpoint cannot: how did control get here. `history [count]` dumps
it after a breakpoint, marking every entry that is not a straight line
continuation. This is what found the bug below.

### 11. cs.c, the IOPB address low half threw away the high half

This was the root cause of the `pc = 2BCC` crash, and it is a one line bug with
a comment attached admitting the guess.

The cartridge streamer takes the address of its I/O parameter block as two
writes to the status register, each carrying 14 bits, and the byte address is
the assembled value shifted left once. The high half merges correctly, keeping
the low 14 bits and replacing the upper ones. The low half did

```c
cs.IOPB_addr = value;   /* assume this overwrites upper */
```

The boot loader never noticed, because the addresses it uses fit in the low
half. The BOSS/IX kernel sends `4007` then `15db`, which should assemble to
`0001d5db` and a byte address of `0003abb6` in kernel data. Dropping the upper
half gave `000015db` and `00002bb6`, which is inside kernel **text**. So when
the command completed, the controller wrote its completion status through that
pointer and landed on `00002bca`, turning

```
2bc8: 2480        move.l D0,(A2)
2bca: 486e fff0   pea    (-$10,A6)
2bce: 2f0a        move.l A2,-(A7)
```

into `move.l D0,D0` followed by an orphaned `fff0`, a line F trap, which is what
killed the kernel a few instructions later. The low half now merges.

The chain that found it is worth recording, because none of the individual steps
guessed: a watchpoint on `2bca` showed the word being written twice, once by the
loader with the correct `486e` and once from kernel code with `2000`; correlating
that against the device log put the second write immediately after
`READ lba 84777`, the configuration record; and the streamer log then showed the
IOPB address being assembled and the high half vanishing.

### 12. sim.c, a breakpoint slot that was never set still fired

An unused breakpoint slot holds address 0 and the match test did not exclude it,
so the emulator halted every time the CPU reached location 0. Harmless until the
kernel legitimately went there, at which point it looked like a breakpoint
nobody had set.

### 13. sim.c, a message buffer too small for its own longest prefix

`msgout` builds its prefix in `char classSrc[30]` out of three pieces: the
class, the source and the routine. `"NOT YET IMPLEMENTED "` is 20 characters,
and with a source and a routine after it, for example `"fw: "` and `"write16 "`,
the total is 32. Nothing in the existing code combined that class with a
routine, so it had never blown. The first message from the new 4-Way code did,
and glibc aborted the emulator. Buffer widened to 64. This is the third
buffer bug of the same family found in this codebase.

### 14. fourway.c and fourway.h, the 4-Way controller

New, and the Z80 on the board is not emulated: like `cs.c` does for the
streamer, the board is modelled at the level of its host protocol.

The header that shipped with the emulator had guessed the addresses as
`D0A000`, `D1A000`, `D2A000`, `D3A000`, which cannot be right because `D00000`
is the cartridge streamer. Measured, the boards sit every `0x20000` from
`0xd20000`, which makes the controller ID in the manual's `D(x)A000` notation
2, 4, 6, 8, A and C. Its `fw_pulse_reset` call in `sim.c` was also commented
out, so nothing was ever initialised.

The manual only documents the Instruction Register address. Logging every
access produced the rest:

| offset | what |
|---|---|
| `+0x0000` | control or reset latch, PROM writes `ff`, kernel writes `1` |
| `+0x2000` | status, bit 0 is BUSY, which the kernel polls with `btst #0` |
| `+0xa000` | Instruction Register, the one the manual gives |
| `+0xc000` | read by both PROM and kernel, purpose still unknown |

Reads are byte wide at the odd address of each word register.

The initialisation handshake now completes on both boards. One correction to
the manual's account: it says the exchange for the base interrupt vector is a
repeat of the two step one for the command block address, but the machine only
ever writes the vector once. Measured, the kernel hands over command block
`0x3bcc2` and vector `0x6a` to one board and `0x3bd12` and vector `0x7a` to the
other, then flags port A ready.

Command blocks are read and executed, and a completion interrupt is raised with
the board's own vector and acknowledged. The interrupt table in section 3.2.6
of the service manual settles the level: levels 2 and 4 both carry "System I/O
bus peripheral device controllers" and are the only two that can be bus
vectored, which is what a board supplying its own vector needs. The Winchester
is on 2 and the streamer on 4, so the 4-Way is one of those; level 4 is the
default and `device fw level <n>` moves it.

### 15. sim.c, system call tracing

BOSS/IX reaches the kernel the ordinary 68k way, with a `TRAP` instruction, so
the question "what is the first user process actually asking for" is answered by
watching user mode instructions for opcode `4E4x`. `traptrace 1` turns it on.
Only user mode is examined, which is a small fraction of the stream, and the
program counter is translated through `mmu_peek_translate`, a copy of the MMU
arithmetic that touches neither the segment status bits nor the translation
counters, so tracing cannot disturb what is being traced. It prints the call,
the registers, five longs off the user stack, the value returned on the first
user instruction after the trap, and, for the write style call, the buffer
contents as text.

The calling convention it revealed: **`TRAP #2`, call number in `A0`,
arguments in `D0`, `D1` and `D2`.**

### 16. fourway.c, command termination status and the vector encoding

Two more corrections out of M8155A, both of which changed behaviour.

Section 3.2.1.4: when a command finishes the board writes **`0x81`** into the
status byte of the command block, or `0x83` if it did not understand the command
or hit a bus error during DMA. Writing zero, which is what the first cut did,
reads as "nothing has happened yet" and left the driver waiting.

Section 3.2.2.2: the vector handed back on the acknowledge is not the base
vector but **base + channel times four + condition**, where the conditions are
0 external/status change, 1 receive character available, 2 special receive
condition and 3 command executed.

With both fixed the driver went from configuring **one** port to configuring
**all eight**, four on each board, which is what a driver initialising the
hardware should do.

### 17. sim.c, msave

`msave [file]` writes all of RAM to a file, so the memory image can be searched
and decoded offline with ordinary tools. Searching a dump for the blocked
banner text is what located the console output queue, its 1024 byte buffer and
the tty structure that owns it, and reading the vector table out of the same
dump is what settled which interrupt level every device really uses.

### 18. scc.c, interrupts, the actual root cause of the silent console

The blocked write chain, followed to the end: the console device is `.crt`, its
tty structure sits at `0x3a846`, its output queue is 1024 bytes at `0x3f07a`,
and its character output routine at `0x27b78` targets the **CMB SCC port A**
through a hardware table at `0x36f1e` holding `0x600004` and `0x600000`. The
routine writes `WR1 = 0x17`, transmit, external and receive interrupts all
enabled, pushes one byte into the data register, sets a busy flag at `0x36f4a`
and sleeps until the transmit empty interrupt clears it. `scc.c` carried a
literal `/* TODO: interrupt handling */`, so that interrupt never came, the
flag never cleared, the queue filled, and the fifteenth write of the banner
blocked forever.

The vector table, read from the RAM dump, settles the level assignments and
corrects an OCR misreading of the service manual's interrupt table: 1 parallel,
2 I/O bus, 3 floppy, 4 I/O bus, **5 SCC**, 6 timer, 7 power fail. The kernel
installs a real handler at the level 5 autovector, leaves level 4's autovector
on the default, and the vectors `0x6a` through `0x89` are the 4-Way stubs, ten
bytes apart, confirming the base plus channel times four plus condition
encoding.

Implemented: a level 5 autovectored request line, transmit pending raised on a
data write when WR1 bit 1 is set, receive pending raised on a keystroke when a
WR1 receive mode is on, WR0 commands 5 and 7 clearing them, RR3 on channel A
reporting the pending bits and RR2 on channel B supplying the status modified
vector. The moment this went in, the entire banner appeared on the console and
the machine has been interactive ever since.

One earlier measurement needs its inference corrected here: "the kernel never
touches the SCC" was true when measured, but only because the tty
initialisation was stuck behind the then unfinished 4-Way completion protocol.
The conclusion drawn from it, that the console did not live on the SCC, was
wrong. The console lives on SCC port A at 19200 baud, terminal type `edt`,
exactly what `sysinfo` reports.

### 19. fourway.h, the base address, corrected by Armin Diehl

Armin tested the fork against his knowledge of the real machine and found the
boards mapped one slot too low: the first 4-Way lives at `0xd40000`, not
`0xd20000`. The kernel does probe `0xd20000` as well, which is what misled the
original measurement, but that slot belongs to something else. With the boards
at `0xd4`/`0xd6` the self test reports `fw [modules= 0,1]`, the FWAY diagnostic
finds boards 0 and 1, and the startup errors for ports 4 to 7 disappear.

### 20. sim.c and pit.c, time stood still whenever the machine was idle

Reported by Armin as "the real time clock does not yet work": `date` always
returned the boot time and `shutdown 0` printed its warning and hung at the
first line of the countdown.

Two causes, stacked:

* Device time, the 68230 pulse included, was advanced from the instruction
  hook, and a CPU that has executed `stop #$2000` executes no instructions, so
  the hook never ran while the kernel idled. Time only passed while the machine
  was busy, which is exactly backwards for a clock. The device tick now lives
  in the run loops, which keep spinning while the CPU is stopped, the way a
  crystal keeps oscillating on the real board.
* Enabling the timer now loads the counter from the preload register, per the
  68230 with zero detect control clear. Without that the kernel inherited the
  counter as the boot PROM left it, and the PROM's last self test ran in
  rollover mode, so the kernel's first tick was sixteen million pulses away.

With both in, `date` advances, and from multi user mode `shutdown 0` counts 15
down to 1, prints GOODBYE, kills its children and returns to single user.

### 21. Multi user mode works

With the 4-Way base right and the clock running: Ctrl-D at `ADMIN>`, answer
`multi`, and the system starts its update and errlog processes and paints the
MAI BASIC FOUR login banner on the configured terminals. ESC at the banner,
`admin` at `Account name:`, and you are logged in.

### 22. cs.c, command 8001, whose original guess was right

`trestore dev=cs` hung the machine. Three things combined: the streamer's
command `0x8001` was unimplemented, the code that would have handled it sat
commented out with the note "Assume Chain addr reset", and every kernel timeout
that would have rescued the situation was frozen along with the clock. The
trace shows the kernel sending `0x8001` immediately after handing over the IOPB
address, which is precisely arming the chain at its first block, so the
original commented out guess was correct and is now live. With the clock fix on
top, `trestore` reads the tape, identifies the saveset and returns to the
prompt.

### 23. The debugger now says when the CPU is stopped

Single stepping at the idle point shows the same PC forever, which looks like a
stepping bug and is not one: the PC points at the instruction after
`stop #$2000` and the CPU is in the stopped state, executing nothing until an
interrupt above the mask arrives. Both the step command and the Ctrl-X break-in
now print `CPU is stopped (a STOP instruction executed), waiting for an
interrupt` so the state is visible instead of puzzling. Stepping also advances
device time now, so a determined `s 200000` really would wake the machine.

## What is still missing

Multi user now works, so the frontier has moved:

* The parallel printer devices `/dev/lp` and `/dev/p1` do not open, error -5.
* The 4-Way data path carries the login banner and keyboard input for the
  configured terminals, but only the console has a real endpoint; wiring the
  other ports to pseudo terminals or sockets would make the extra logins
  reachable.
* Reads of `0x006c0000`, the NVRAM recall strobe, are unhandled. The MMU
  execute attribute is unimplemented.

`wd [modules= 0]` in the self test used to be listed here as a defect, on the
assumption that the field counted something that ought to have reached 1. It
does not: Armin Diehl points out that it lists the boards present, so
`[modules= 0]` is the correct output for one disk controller, and a machine
with two would print `[modules= 0,1]`, exactly as the two 4-Way boards do.

## Superseded, kept so nobody repeats it

The kernel used to die with `68010 illegal instruction trap, pc = 2BCC` in
pid 1, on a line F opcode sitting in what did not read as a coherent instruction
stream. Two readings looked plausible at the time, a syscall dispatch table being
indexed wrongly, or entry 37 legitimately holding a line F trap as an
unimplemented call marker. **Both were wrong.** It was memory corruption: the
cartridge streamer IOPB pointer had lost its upper half and the controller wrote
its completion status over kernel text. See change 11 above. The lesson is that
the disassembly of a corrupted word is not evidence about intent, and that the
question to ask first was who wrote that memory, not what the instruction means.

A related worry recorded here and also wrong: that the MMU descriptor layout in
`mmu.c` misread section 3.2.16.5, because every descriptor the kernel wrote had
base 0 and TYPE 0, absent. That trace had only ever caught the `clr.w` loop that
initialises the descriptors. Once the corruption was fixed the kernel got far
enough to program real ones, and they translate without a single fault.

## Building and running

```
make
./fetch-media.sh        # downloads the media and builds wd0.img
./eagleemu "msg all -" "bus -" "dev wd image wd0.img" g
```

To boot the diagnostic tape instead, which is the quickest way to prove the
machine is alive end to end:

```
./eagleemu "msg all -" "bus -" "dev cs dir cs/diag" "dev wd image wd0.img" g
```

It needs a real terminal. The emulator puts stdin into raw mode and echoes the
emulated serial console to **stderr**, not stdout, so redirecting stdin from
`/dev/null` gives a silent run that looks like a hang and is not one. Answer
`wd0` at the `Boot device:` prompt and press return at `System file:`.
Control-X breaks into the debugger, `quit` leaves.

`tools/drive2.py` and `tools/drive3.py` drive the emulator through a pty, answer
its prompts and, in the case of drive3, break in after a delay and run debugger
commands. They exist because of the terminal and stderr behaviour above.
