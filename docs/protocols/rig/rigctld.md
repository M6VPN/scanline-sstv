# Hamlib rigctld PTT evidence

## Accepted interoperability target

M2F targets Hamlib 4.7.1, tag `4.7.1`, commit
`d042479a9f8095ba1a8e103a977c3614d7233cb2`. Scanline SSTV uses the source
only as factual protocol evidence. No Hamlib code is copied or adapted, and
the project does not link libhamlib or launch rigctld.

M2F uses only the newline-separated Extended Response Protocol:

| Action | Request | Accepted response |
| --- | --- | --- |
| Key | `+T 1\n` | `set_ptt: 1\nRPRT 0\n`, then a separate query |
| Unkey | `+T 0\n` | `set_ptt: 0\nRPRT 0\n`, then a separate query |
| Query | `+t\n` | `get_ptt:\nPTT: VALUE\nRPRT 0\n` |

The accepted readback values are `0` for receive/unkeyed, `1` for transmit,
`2` for transmit using microphone audio, and `3` for transmit using data
audio. Values `1`, `2`, and `3` are all keyed states. Scanline SSTV sends only
the generic `1` and `0` set values.

## Sources

### Hamlib 4.7.1 release and rigctld manual

- Title: *Hamlib 4.7.1* and *rigctld(1)*
- Project and authors: Hamlib, Frank Singleton, Stephane Fillod, Nate Bargmann,
  and the Hamlib Group
- Tag: `4.7.1`
- Commit: `d042479a9f8095ba1a8e103a977c3614d7233cb2`
- Licence: rigctld utility GPL-2.0-or-later; library headers
  LGPL-2.1-or-later
- Canonical repository: <https://github.com/Hamlib/Hamlib>
- Release: <https://github.com/Hamlib/Hamlib/releases/tag/4.7.1>
- Retrieval date: 2026-07-18
- Release archive SHA-256: `d197a08a3d5d936d7571ae573f745bbba619e88998742c8267e3fcb0fb3d5974`
- Inspected locations:
  - `doc/man1/rigctld.1`, command protocol, `set_ptt`, `get_ptt`, Default
    Protocol, Extended Response Protocol, and diagnostics sections
  - `tests/rigctld.c`, socket-server connection loop
  - `tests/rigctl_parse.c`, command table, extended-response prefix handling,
    response termination, `set_ptt`, and `get_ptt`
  - `include/hamlib/rig.h`, `ptt_t`, `rig_errcode_e`, and `NETRIGCTL_RET`
- File SHA-256:
  - `doc/man1/rigctld.1`:
    `71b530a379477ff91d417fdc4f5012b6c691a22cb1d6479d63a8760e01a93099`
  - `tests/rigctld.c`:
    `aac9c3832b281aa553f7638dfd55eaf3b48c919771f2fcee726855d1ff88fcf5`
  - `tests/rigctl_parse.c`:
    `bb405f72efb9604e9e6bb8c131f8986f77586223bda0f9c1d235b98f8ef93ef3`
  - `include/hamlib/rig.h`:
    `a2cfaedc3d92a641515ff58785db5bd1c30f62e562f63b8dadeee70202e000bd`

The manual states that each command is one line terminated by `\n`. Set
commands return `RPRT 0` on success or a negative Hamlib error code. Default
Protocol get commands return only their values on success. Extended Response
Protocol prepends the command with `+`, echoes the long command name and
arguments, labels returned values, and always terminates the block with
`RPRT CODE`.

The source command table binds `T` to `set_ptt` and `t` to `get_ptt`.
`set_ptt` accepts exactly the `ptt_t` values `0` through `3`; `get_ptt` emits
the numeric state. The extended parser produces the exact response records in
the table above.

## Project wire policy

M2F selects only the `+` Extended Response Protocol. Its explicit `RPRT`
terminator makes a complete response block detectable for both set and query
operations. Normal-protocol responses, alternative separators, CRLF output,
prompts, banners, unsolicited records, and arbitrary commands are rejected.

Each fixed command uses one bounded TCP connection. Set and readback use
separate connections to the same explicit endpoint, and both share the
original absolute `PttRequest` deadline. This is compatible with rigctld's
multi-command connection loop without depending on persistent connection
state.

No conventional rigctld port is enabled by default. M2F requires an explicit
port and accepts only literal `127.0.0.1` or `::1`. Hostnames, DNS,
non-loopback addresses, mapped addresses, and zone identifiers are rejected
before socket creation.

`RPRT 0` is an operation acknowledgement, not proof of physical PTT state.
Every set is followed by `get_ptt`. Keying is definitely confirmed only by
readback `1`, `2`, or `3`; unkeying is definitely confirmed only by readback
`0`. A lost response after a complete key command, a negative `RPRT`, an
unknown value, or a readback mismatch remains indeterminate and enters the
existing mandatory cleanup path.

## Version comparison and disagreements

The 4.7.1 manual recommends Extended Response Protocol for direct clients,
while Default Protocol remains the NET rigctl backend grammar. Earlier project
wording said to use extended responses where practical. M2F resolves that
choice by requiring extended responses exclusively.

Hamlib supports four keyed/unkeyed enum values, while many clients treat PTT
as boolean. M2F deliberately maps all three documented transmit values to
keyed and rejects every other integer. It does not assume that any arbitrary
nonzero value means keyed.
