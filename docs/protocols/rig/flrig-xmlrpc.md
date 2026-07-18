# flrig XML-RPC PTT evidence

## Accepted interoperability target

M2E targets flrig 2.0.11, tag `v2.0.11`, commit
`e2058cbd5bf6dc4e471d60a077a2ee65289a50a2`. Scanline SSTV uses the
interface only as factual protocol evidence. No flrig or flxmlrpc code is
copied or adapted.

The accepted operations are:

| Action | XML-RPC method | Parameters | Accepted readback |
| --- | --- | --- | --- |
| Key | `rig.set_ptt` | one `i4` value, `1` | separate `rig.get_ptt` returning `i4` value `1` |
| Unkey | `rig.set_ptt` | one `i4` value, `0` | separate `rig.get_ptt` returning `i4` value `0` |
| Query | `rig.get_ptt` | no parameters | one `i4` value, exactly `0` or `1` |

The source implementation waits for the transceiver state after
`rig.set_ptt`, but does not assign an XML-RPC result on the normal path.
flxmlrpc consequently serializes an empty string value. The help table describes
the method as `n:i`, meaning no useful result with one integer parameter. M2E
therefore accepts the set response only as acknowledgement that the RPC
completed. It always issues `rig.get_ptt` before reporting definite keyed or
unkeyed certainty.

## Sources

### flrig user documentation

- Title: *flrig_help 2.0.04, Configure XmlRpc Server*
- Project and author: flrig, W1HKJ software project, David Freese and contributors
- Version: documentation version 2.0.04
- Licence: documentation distributed with flrig; flrig source files state
  GPL-3.0-or-later
- URL: <https://www.w1hkj.org/flrig-help/xmlrpc_server.html>
- Inspected section: `flrig XmlRpc Command Structures`, entries
  `rig.get_ptt`, `rig.set_ptt`, and `rig.set_verify_ptt`
- Retrieval date: 2026-07-18
- Local pinned equivalent: `doxygen/user_src_doc/xmlrpc_server.txt` at flrig
  commit `e2058cbd5bf6dc4e471d60a077a2ee65289a50a2`
- Local file SHA-256:
  `8516151190021b4b7e248101a9e2ee13ec8c285d27452fc2ea309e3aa409b942`

The table identifies `rig.get_ptt` as integer return/no parameters and
`rig.set_ptt` as no useful return/one integer parameter, with `1` meaning on
and `0` meaning off. It marks `rig.set_ptt_fast` deprecated.

### flrig 2.0.11 source

- Title: *flrig 2.0.11 source tree*
- Project and author: flrig, David Freese/W1HKJ and contributors
- Tag: `v2.0.11`
- Commit: `e2058cbd5bf6dc4e471d60a077a2ee65289a50a2`
- Licence: `src/server/xml_server.cxx` states GPL-3.0-or-later;
  bundled flxmlrpc client files state LGPL-3.0-or-later
- Canonical repository:
  <https://sourceforge.net/p/fldigi/flrig/ci/v2.0.11/tree/>
- Retrieval date: 2026-07-18
- Inspected locations:
  - `src/server/xml_server.cxx`, `rig_get_ptt::execute`, lines 301-313
  - `src/server/xml_server.cxx`, `rig_set_ptt::execute`, lines 1675-1708
  - `src/server/xml_server.cxx`, method table symbols `rig.get_ptt` and
    `rig.set_ptt`
  - `src/xmlrpcpp/XmlRpcClient.cpp`, request constants,
    `XmlRpcClient::generateRequest`, and `XmlRpcClient::generateHeader`
  - `src/xmlrpcpp/XmlRpcServer.cpp`, `generateResponse`, `generateHeader`, and
    `generateFaultResponse`
  - `src/xmlrpcpp/XmlRpcValue.cpp`, `intToXml` and `stringToXml`
- SHA-256:
  - `src/server/xml_server.cxx`:
    `1c17ec2840a3f87270ae8456bdca74d6a33f82c8783736e9c7d4431314098c7b`
  - `src/xmlrpcpp/XmlRpcClient.cpp`:
    `547860f465f618ffd983a9ea9ae17b42dd04f389efe8695b73fb8f838761eb0d`

The source confirms that `rig.get_ptt` returns the integer `PTT` value and
that `rig.set_ptt` consumes `params[0]` as an integer. The bundled client uses
HTTP/1.1 POST, `Content-Type: text/xml`, `Content-length`, and `/RPC2` when no
path is supplied. The bundled server returns HTTP 200 with a bounded
Content-Length and XML-RPC `methodResponse`, or an XML-RPC fault structure.

## Project wire policy

`/RPC2` and flrig's documented default port are evidence only. M2E has no
enabled endpoint default: construction requires an explicit literal loopback
address, port, and path. Only `127.0.0.1` and `::1` are accepted.

The client sends one request per TCP connection and accepts only HTTP/1.1 200
responses framed by one valid Content-Length. It rejects chunking,
compression, redirects, upgrades, duplicate critical fields, malformed UTF-8,
DTD/entity/XInclude content, and unsupported XML-RPC values. A set response is
never sufficient confirmation. Lost key responses remain possibly keyed;
lost or malformed unkey responses remain indeterminate until a separate query
confirms state.

## Version comparison and disagreements

The 2.0.04 help page and 2.0.11 source agree on method names, parameter shape,
and PTT values. The help signature does not promise a set result, while the
2.0.11 implementation serializes an empty value because no result is assigned.
This is resolved conservatively by ignoring the set result value and requiring
`rig.get_ptt` readback. Deprecated `rig.set_verify_ptt` and
`rig.set_ptt_fast` are not used.

