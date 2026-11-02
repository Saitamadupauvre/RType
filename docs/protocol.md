# R-Type UDP Protocol

| | |
|---|---|
| Category | Informational |
| Protocol version | `7` |
| Transport | UDP |
| Implementation | `engine/src/network/` |

## Abstract

This document specifies the binary protocol spoken between an R-Type server
and its clients. A client connects, sends its inputs, and receives the
authoritative world state (snapshots) and game events. The protocol is
generic: entity types, sounds and musics are names exchanged at connection
time, so the same protocol serves any game built on the engine.

Anyone should be able to write a new client for the server from this
document alone. A summary for client writers is in
[Writing a client](#writing-a-client).

## Conventions

The key words **MUST**, **MUST NOT**, **SHOULD**, **SHOULD NOT** and **MAY**
are to be interpreted as described in
[RFC 2119](https://www.rfc-editor.org/rfc/rfc2119).

- "Server" is the authoritative `r-type_server`; "client" is any program
  connecting to it.
- Byte offsets start at 0. Sizes are in bytes.
- All integers are unsigned unless marked signed. Signed integers use two's
  complement.
- World coordinates are in virtual units: the world is 1920 × 1080, origin at
  the top-left, Y pointing down.

## Transport

- Transport: UDP.
- Every UDP datagram contains exactly one packet.
- Maximum datagram size: 1200 bytes.
- Maximum payload size: 1187 bytes.
- Multibyte integers use big-endian byte order.
- Fields are encoded individually. C++ object memory is never transmitted directly.

## Packet Format

Every datagram consists of a 13-byte header followed by `payload_size` payload bytes.

| Offset | Size | Field | Encoding | Value |
|---|---:|---|---|---|
| 0 | 4 | `magic` | Unsigned 32-bit, big-endian | `0x52545950` (`RTYP`) |
| 4 | 2 | `version` | Unsigned 16-bit, big-endian | `7` |
| 6 | 1 | `type` | Unsigned 8-bit | Message type |
| 7 | 4 | `sequence` | Unsigned 32-bit, big-endian | Sender packet sequence |
| 11 | 2 | `payload_size` | Unsigned 16-bit, big-endian | Number of bytes after the header |

Receivers must reject a datagram when its size is smaller than 13 bytes, its magic or version is invalid, its message type is unknown, its payload size exceeds 1187 bytes, or its total size is not exactly `13 + payload_size`.

## Message Types

| Value | Name | Direction | Delivery |
|---:|---|---|---|
| 0 | `invalid` | None | Rejected |
| 1 | `connect_request` | Client to server | Reliable |
| 2 | `connect_accept` | Server to client | Reliable |
| 3 | `input` | Client to server | Unreliable |
| 4 | `snapshot` | Server to client | Unreliable |
| 5 | `event` | Server to client | Reliable |
| 6 | `heartbeat` | Both directions | Unreliable |
| 7 | `disconnect` | Both directions | Reliable |

The "Delivery" column states the intended guarantee. In version 7 no message is acknowledged: reliability comes from the protocol flow itself (a `connect_request` is retried until answered, heartbeats and timeouts detect a lost peer, the full world is resent every tick). Acknowledgement and retransmission of `event` and `disconnect` messages are planned for a later version.

## Connection Payloads

### Connect Request

The client sends this message from the UDP endpoint it will use for the game session. The nonce lets the client associate a later `connect_accept` packet with its request.

| Offset | Size | Field | Encoding |
|---|---:|---|---|
| 0 | 4 | `client_nonce` | Unsigned 32-bit, big-endian |

The payload is exactly four bytes.

### Connect Accept

The server sends this message to the endpoint that sent the matching request. It carries one **string table** that maps the numeric IDs used by snapshots and events to names. Each entry belongs to a category, and each category has its own ID space:

| Value | Category | Names |
|---:|---|---|
| 0 | `type` | entity types of snapshots |
| 1 | `sound` | sounds of `sound` events |
| 2 | `music` | musics of `music` events |
| 3 | `event` | names of `named` events |
| 4 | `shader` | shaders (reserved for a later version) |
| 5 | `field` | replicated component fields, named `Component.field` (e.g. `Position.pos`) |
| 6 | `event_key` | argument keys of `named` events, shared by every event |

Sound and music names are file names (for R-Type, files under the client's `assets/sounds/` and `assets/music/` folders). Then comes the music the server is playing, so that a player joining mid-game hears it.

| Offset | Size | Field | Encoding |
|---|---:|---|---|
| 0 | 4 | `client_nonce` | Unsigned 32-bit, big-endian |
| 4 | 4 | `player_entity_id` | Unsigned 32-bit, big-endian: the player id of this client (1 to `max_players`) |
| 8 | 2 | `string_count` | Unsigned 16-bit, big-endian |
| 10 | Variable | `string_entries` | `string_count` entries |
| … | 2 | `current_music` | Unsigned 16-bit, big-endian: the id of a `music` entry, or `0xFFFF` for no music |

The payload is at least 12 bytes (empty table). Each entry has the following layout:

| Offset | Size | Field | Encoding |
|---|---:|---|---|
| 0 | 1 | `category` | Unsigned 8-bit, a value of the category table above |
| 1 | 2 | `id` | Unsigned 16-bit, big-endian, from 0 to `0xFFFE` |
| 3 | 1 | `name_length` | Unsigned 8-bit, from 1 to 63 |
| 4 | Variable | `name` | Opaque bytes, exactly `name_length` bytes |

Names are compared byte for byte and are not validated as UTF-8. The whole table must fit in one packet: a server whose names do not fit refuses to start.

The receiver rejects an unknown category; within one category, duplicate IDs or duplicate names; the id `0xFFFF`; a name length of 0 or above 63; truncated entries or entries extending beyond the declared payload length; and a `current_music` that is neither `0xFFFF` nor the id of a `music` entry. The same ID or name MAY appear in different categories.

The server fills the `field` category itself with every field its field registry flags `replicated`. A client resolves each `field` name in its own field registry. When one is unknown, it ends the connection: it sends `disconnect` with reason `protocol_error` and reports the connection as rejected.

A client MUST NOT use a sound or music name as a path without checking it: R-Type only loads names made of letters, digits, `_`, `-` and `.`, not starting with `.`, and ignores the others.

### Connection Flow

1. The client sends `connect_request` with a fresh `client_nonce`. Without an answer it resends the request after a retry interval, and gives up after a bounded number of attempts.
2. The server answers with one of:
   - `connect_accept` carrying the same nonce, the player ID, the string table and the current music;
   - `disconnect` with reason `server_full` when every player slot is taken. The number of slots is set by the game (`r-type_server --max-players`, 1 to 4, default 4). No session is created.
3. A repeated `connect_request` from an endpoint that already has a session is answered with `connect_accept` again, with the same player ID. It does not create a second session.
4. Once connected, the client sends `heartbeat` at a fixed interval and the server answers each one with `heartbeat`. Any valid packet from the client refreshes its session.
5. A session ends when the client sends `disconnect`, or when the server receives nothing from it for longer than the session timeout. On timeout the server sends `disconnect` with reason `timeout` to that client. In both cases every remaining client receives a `player_left` event and the player ID becomes free.
6. When the server stops, it sends `disconnect` with reason `server_shutdown` to every client and ends every session.
7. The client ends the connection when it receives `disconnect` from the server (any reason), or when it receives nothing from the server for longer than its timeout (5 seconds). The server sends a snapshot to every session each tick, so a running server is never silent. Once the connection has ended, the client ignores every packet from the server.

Packets from an endpoint without a session, other than `connect_request`, are ignored. A client with another protocol version never reaches this flow: its datagrams fail header validation and are dropped and logged.

## Input Payload

An input message carries the raw keyboard and mouse state of the player: every key and mouse button held, and an optional pointer position. The server identifies the player from the endpoint and session established by the connection flow. The payload contains no client ID or player position. It is 18 bytes long, or 22 with a pointer:

| Offset | Size | Field | Encoding | Allowed values |
|---|---:|---|---|---|
| 0 | 16 | `keys` | 128-bit set: key `k` is bit `k % 8` (lowest first) of byte `k / 8` | Bits of keys 1 to 105 only (`A` = 1 … `GraveAccent` = 105); bit 0 (`Unknown`) and bits 106-127 must be zero |
| 16 | 1 | `mouse_buttons` | Bit set | Bit 0 `Left`, bit 1 `Right`, bit 2 `Middle`; bits 3-7 must be zero |
| 17 | 1 | `flags` | Bit set | Bit 0: a pointer follows; bits 1-7 must be zero |
| 18 | 4 | `pointer` | Two signed 16-bit big-endian ints `x`, `y` | Present only with bit 0 of `flags`; virtual units |

Key values are those of the engine's `Key` enumeration (`engine/core/Keys.hpp`), physical keys independent of the graphics backend; adding a key there changes the wire format and needs a new protocol version. The engine gives keys no meaning: the server's scripts decide what each one does.

The receiver rejects input payloads that are truncated or have trailing bytes, hold bit 0 or a bit above the last key, hold an unknown mouse button, or set unknown `flags` bits.

The client sends its current input every tick while connected; an input also counts as a heartbeat. The server keeps only the latest input of each session, starting from an idle input (nothing held, no pointer). An input whose packet sequence is not newer than the last accepted input of that session (duplicated or out of order, with wrap-around) is ignored, but still refreshes the session. Inputs from an endpoint without a session are ignored.

## Snapshot Payload

A snapshot is an authoritative world state at one server tick. It has a six-byte prefix followed by variable-size entity records.

| Offset | Size | Field | Encoding |
|---|---:|---|---|
| 0 | 4 | `server_tick` | Unsigned 32-bit, big-endian |
| 4 | 2 | `entity_count` | Unsigned 16-bit, big-endian |
| 6 | Variable | `entities` | `entity_count` records |

Each entity record has a seven-byte header followed by its fields:

| Offset | Size | Field | Encoding |
|---|---:|---|---|
| 0 | 4 | `entity_id` | Unsigned 32-bit, big-endian |
| 4 | 2 | `type_id` | Unsigned 16-bit, big-endian |
| 6 | 1 | `field_count` | Unsigned 8-bit |
| 7 | Variable | `fields` | `field_count` fields |

Each field has the following layout:

| Offset | Size | Field | Encoding |
|---|---:|---|---|
| 0 | 2 | `field_id` | Unsigned 16-bit, big-endian: the id of a `field` entry of the connection string table |
| 2 | 1 | `kind` | Unsigned 8-bit, a value of the kind table below |
| 3 | Variable | `value` | Encoded according to `kind` |

| Kind | Name | Value encoding |
|---:|---|---|
| 0 | `float` | IEEE 754 single precision, big-endian (4 bytes); never NaN or infinite |
| 1 | `int` | Signed 32-bit, big-endian (4 bytes) |
| 2 | `bool` | Unsigned 8-bit, `0` or `1` |
| 3 | `string` | Unsigned 8-bit length, then that many opaque bytes |
| 4 | `vec2` | Two signed 16-bit, big-endian: `x` then `y` (4 bytes) |
| 5 | `color` | Four unsigned 8-bit: `r`, `g`, `b`, `a` |
| 6 | `asset` | Same as `string`: a path relative to the assets folder |

Lists (kind 7 in the engine field registry) are not replicated: a server with a replicated list field refuses to start.

The server replicates every entity that has the engine's `Replicated` component. Its record carries `Replicated::type_id` and, in registration order, every field flagged `replicated` that the entity owns; a field of a component the entity lacks is left out, and the client keeps its default value (opaque white for a missing `Sprite.color`). A string longer than 255 bytes or a non-finite float is left out and logged. `entity_id` is the entity index. Vec2 components (positions, velocities) are rounded to the nearest integer and clamped to the signed 16-bit range; ints are clamped to the signed 32-bit range. With the R-Type fields, an entity sends `Position.pos`, `Sprite.color`, `PhysicsBody.velocity` (when it has a body) and `Replicated.owner`; a client compares `owner` with the `player_entity_id` of its `connect_accept` to find its own entities.

A snapshot packet carries as many entities as fit in the maximum payload size: their records take at most 1181 bytes. A larger world is sent as several snapshot packets with the same `server_tick`. The format of each packet does not change. A server whose replicated fields, all present with the longest strings, would not fit one entity in a packet refuses to start, as does one with more than 255 replicated fields or a field name longer than 63 bytes.

The client keeps the world of the newest `server_tick` it has received, comparing ticks with wrap-around like sequence numbers:

- a newer tick replaces the whole world with the packet's entities;
- another packet of the same tick is merged in, an entity already present being updated rather than duplicated;
- an older tick is ignored.

The receiver rejects the whole snapshot when a record is truncated or extends beyond the payload, when bytes remain after the last record, when the records take more than 1181 bytes, on an unknown `kind`, a `bool` other than `0` or `1`, a NaN or infinite `float`, the same `field_id` twice in one record, a `type_id` that is not the id of a `type` entry, a `field_id` that is not the id of a `field` entry, or a `kind` other than the kind of that field in the client's registry.

## Event Payload

An event payload begins with a one-byte event type. Each event packet contains one event.

| Value | Name | Remaining payload |
|---:|---|---|
| 4 | `sound` | `source_entity_id`, `sound_id` |
| 5 | `player_left` | `player_entity_id` |
| 6 | `music` | `music_id` |
| 7 | `named` | `name_id`, `argument_count`, arguments |

Values 1 to 3 (`spawn`, `destroy` and `fire` before version 6) are no longer used and are rejected.

The `sound` event payload is seven bytes: one-byte event type `4`, followed by unsigned 32-bit big-endian `source_entity_id` and unsigned 16-bit big-endian `sound_id`. `sound_id` is the id of a `sound` entry of the string table. `source_entity_id` is the entity that played the sound, or `0xFFFFFFFF` when none did.

The `music` event payload is three bytes: one-byte event type `6` followed by the unsigned 16-bit big-endian `music_id` of the music to play in a loop from its start, replacing the current one. `0xFFFF` stops the music. The server remembers the last music sent and puts it in the `current_music` of every later `connect_accept`.

The `player_left` event payload is five bytes: one-byte event type `5` followed by the unsigned 32-bit big-endian `player_entity_id` of the player whose session ended.

The `named` event carries a game-defined event and its arguments. Its payload is at least four bytes:

| Offset | Size | Field | Encoding |
|---|---:|---|---|
| 0 | 1 | `event_type` | `7` |
| 1 | 2 | `name_id` | Unsigned 16-bit, big-endian: id of an `event` entry |
| 3 | 1 | `argument_count` | Unsigned 8-bit, at most 16 |
| 4 | | arguments | `argument_count` times `key_id`, `kind`, value |

Each argument is an unsigned 16-bit big-endian `key_id` (id of an `event_key` entry), a one-byte `kind` and a value encoded exactly like a snapshot field value of that kind. `list` is not allowed, and `string` and `asset` values are at most 63 bytes long, so a `named` event always fits in one packet. The server checks no schema: which arguments an event carries is up to the game.

The receiver rejects unknown event types, an event with an incorrect fixed payload size, and a `named` event that is truncated, has trailing bytes, more than 16 arguments, the same `key_id` twice, an unknown or `list` `kind`, a `bool` other than `0` or `1`, a NaN or infinite `float`, or a text longer than 63 bytes. A `sound` or `music` event with an id missing from its category is ignored; a `named` event whose `name_id` or one `key_id` is missing from its category is dropped and logged.

The server sends the events of a tick to every connected session after the tick, in the order they happened, before that tick's snapshot. An event goes only to the sessions that existed when it happened: a client that connects later never receives earlier events. A client drops every snapshot and event that arrives before `connect_accept`. Until acknowledgements exist (a later protocol version), events are sent once and are not retransmitted.

## Heartbeat Payload

A heartbeat payload is empty. Both peers may send it periodically to prove that their UDP endpoint remains reachable. A peer that does not receive any valid packet or heartbeat before the configured timeout is considered disconnected. The client sends a heartbeat every second while connected, inputs or not. The server answers each one with a heartbeat, and the client uses the delay as its round-trip time. The server never sends heartbeats of its own: its snapshots already reach every client each tick.

## Disconnect Payload

The disconnect payload contains one byte.

| Value | Name |
|---:|---|
| 0 | `normal` |
| 1 | `timeout` |
| 2 | `server_shutdown` |
| 3 | `protocol_error` |
| 4 | `server_full` |

The server sends `server_full` in answer to a `connect_request` when no player slot is free.

Receivers reject a disconnect payload unless it is exactly one byte and contains a defined reason.

## Sequence Numbers

Each sender keeps one packet sequence per peer and increments it for every datagram it sends to that peer: the server numbers each session separately, starting at `0`. Receivers use sequence numbers to detect duplicated and stale packets, and to measure loss (gaps) and reordering (a packet older than the newest one). Sequence numbers wrap from `0xFFFFFFFF` to `0`.

## Receiver Rules

- Reject a datagram before allocating memory when its header, payload size, or payload layout is invalid.
- Do not process a packet from an endpoint before its connection request has been accepted.
- Drop duplicated or stale unreliable packets.
- Treat malformed datagrams as untrusted input: drop and log them without stopping the client or server.

## Examples

All bytes are hexadecimal.

`connect_request` with nonce `0x01020304`, sequence `0`:

```
52 54 59 50  00 07  01  00 00 00 00  00 04  01 02 03 04
magic        ver    type sequence    size   client_nonce
```

`input` holding `Space` (key 58) and `Left` (key 56), no mouse button, pointer at (640, 360), sequence `42`:

```
52 54 59 50  00 07  03  00 00 00 2A  00 16
00 00 00 00  00 00 00 05  00 00 00 00  00 00 00 00   keys: byte 7 = keys 56, 58
00  01  02 80  01 68                                 mouse, flags, x=640, y=360
```

`heartbeat`, sequence `7`:

```
52 54 59 50  00 07  06  00 00 00 07  00 00
```

`disconnect` with reason `normal`, sequence `8`:

```
52 54 59 50  00 07  07  00 00 00 08  00 01  00
```

`event` `music` playing music id `1`, sequence `9`:

```
52 54 59 50  00 07  05  00 00 00 09  00 03  06  00 01
                                            music music_id
```

`event` `named` with name id `2` and one `vec2` argument of key id `1` at `(10, -4)`, sequence `10`:

```
52 54 59 50  00 07  05  00 00 00 0A  00 0A  07  00 02  01  00 01  04  00 0A FF FC
                                            named name  n   key   vec2 x     y
```

## Writing a client

A minimal client:

1. Open a UDP socket and send `connect_request` with a random nonce to the
   server. Resend it every 500 ms until an answer arrives; give up after a few
   attempts.
2. On `connect_accept` with your nonce: remember your player id
   (`player_entity_id`), the string table, and start
   `current_music` unless it is `0xFFFF`. On `disconnect(server_full)`: stop.
3. Every tick (60 Hz), send an `input` with your current actions. Number every
   packet you send with an increasing sequence.
4. Send a `heartbeat` every second.
5. Keep the world of the newest `snapshot` tick, merging packets of the same
   tick. Draw each entity using its `type` name from the string table; entities
   whose `owner` equals your player id are yours.
6. On `event` `sound`, play the `sound` entry of that id; on `event`
   `music`, loop the `music` entry of that id, or stop it on `0xFFFF`.
   On `event` `named`, look up its `event` name and `event_key` keys to
   react to it (e.g. show an explosion). On `event` `player_left`, show that
   the player left. On any `disconnect`,
   or after 5 seconds without a packet, consider the connection over.
7. Before quitting, send `disconnect(normal)`.

## Security Considerations

The server and the clients treat every datagram as hostile.

| Threat | Mitigation |
|---|---|
| Malformed or truncated packets crashing a peer | Every read is bounds-checked; the header, the exact payload size and every field range are validated before use; invalid datagrams are dropped and logged |
| Oversized datagrams | Datagrams above 1200 bytes are dropped, never truncated or reassembled |
| Memory exhaustion through announced counts | Counts (`entity_count`, `type_count`, `sound_count`, `music_count`) are checked against the bytes actually present **before** allocating |
| Path traversal through sound or music names | Names are only used after checking they are plain file names (no `/`, `\`, leading `.`); others are ignored |
| Flooding the server | The receive queue is bounded (4096 messages); extra messages are dropped. Sessions are limited to `max_players` |
| Cheating by sending positions | Clients only send inputs (keys and mouse buttons held, and a pointer); the server computes every position |
| Replayed or reordered inputs | Inputs older than the last accepted sequence are ignored |
| Spoofed packets from another endpoint | Messages from an endpoint without a session are ignored. The protocol has no authentication or encryption: an attacker who can spoof a player's IP address and port can inject inputs for that player. This is accepted for a LAN game and listed as future work |
| Memory disclosure | C++ object memory is never sent; every field is written explicitly in big-endian order |

Only `connect_request` creates state on the server, and only up to
`max_players` sessions; each expires after 5 seconds of silence.
