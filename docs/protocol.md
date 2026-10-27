# R-Type UDP Protocol

| | |
|---|---|
| Category | Informational |
| Protocol version | `3` |
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
| 4 | 2 | `version` | Unsigned 16-bit, big-endian | `3` |
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

The "Delivery" column states the intended guarantee. In version 3 no message is acknowledged: reliability comes from the protocol flow itself (a `connect_request` is retried until answered, heartbeats and timeouts detect a lost peer, the full world is resent every tick). Acknowledgement and retransmission of `event` and `disconnect` messages are planned for a later version.

## Connection Payloads

### Connect Request

The client sends this message from the UDP endpoint it will use for the game session. The nonce lets the client associate a later `connect_accept` packet with its request.

| Offset | Size | Field | Encoding |
|---|---:|---|---|
| 0 | 4 | `client_nonce` | Unsigned 32-bit, big-endian |

The payload is exactly four bytes.

### Connect Accept

The server sends this message to the endpoint that sent the matching request. It carries three tables that map the numeric IDs used by snapshots and events to names:

- the **type table** names the entity types of snapshots and `spawn` events;
- the **sound table** names the sounds of `sound` events;
- the **music table** names the musics of `music` events.

Sound and music names are file names (for R-Type, files under the client's `assets/sounds/` and `assets/music/` folders). Then comes the music the server is playing, so that a player joining mid-game hears it.

| Offset | Size | Field | Encoding |
|---|---:|---|---|
| 0 | 4 | `client_nonce` | Unsigned 32-bit, big-endian |
| 4 | 4 | `player_entity_id` | Unsigned 32-bit, big-endian: the player id of this client (1 to `max_players`) |
| 8 | 2 | `type_count` | Unsigned 16-bit, big-endian |
| 10 | Variable | `type_entries` | `type_count` entries |
| … | 2 | `sound_count` | Unsigned 16-bit, big-endian |
| … | Variable | `sound_entries` | `sound_count` entries |
| … | 2 | `music_count` | Unsigned 16-bit, big-endian |
| … | Variable | `music_entries` | `music_count` entries |
| … | 2 | `current_music` | Unsigned 16-bit, big-endian: a `music_entries` id, or `0xFFFF` for no music |

The payload is at least 16 bytes (every table empty). Type, sound and music entries share the following layout, with `type_id` holding the sound or music id:

| Offset | Size | Field | Encoding |
|---|---:|---|---|
| 0 | 2 | `type_id` | Unsigned 16-bit, big-endian |
| 2 | 1 | `name_length` | Unsigned 8-bit, from 1 to 63 |
| 3 | Variable | `name` | Opaque bytes, exactly `name_length` bytes |

Names are compared byte for byte and are not validated as UTF-8.

The receiver rejects, within one table, duplicate IDs or duplicate names; a name length of 0 or above 63; truncated entries or entries extending beyond the declared payload length; a music entry with id `0xFFFF`; and a `current_music` that is neither `0xFFFF` nor an id of the music table. The same name MAY appear in different tables.

A client MUST NOT use a sound or music name as a path without checking it: R-Type only loads names made of letters, digits, `_`, `-` and `.`, not starting with `.`, and ignores the others.

### Connection Flow

1. The client sends `connect_request` with a fresh `client_nonce`. Without an answer it resends the request after a retry interval, and gives up after a bounded number of attempts.
2. The server answers with one of:
   - `connect_accept` carrying the same nonce, the player ID, the type, sound and music tables and the current music;
   - `disconnect` with reason `server_full` when every player slot is taken. The number of slots is set by the game (`r-type_server --max-players`, 1 to 4, default 4). No session is created.
3. A repeated `connect_request` from an endpoint that already has a session is answered with `connect_accept` again, with the same player ID. It does not create a second session.
4. Once connected, the client sends `heartbeat` at a fixed interval and the server answers each one with `heartbeat`. Any valid packet from the client refreshes its session.
5. A session ends when the client sends `disconnect`, or when the server receives nothing from it for longer than the session timeout. On timeout the server sends `disconnect` with reason `timeout` to that client. In both cases every remaining client receives a `player_left` event and the player ID becomes free.
6. When the server stops, it sends `disconnect` with reason `server_shutdown` to every client and ends every session.
7. The client ends the connection when it receives `disconnect` from the server (any reason), or when it receives nothing from the server for longer than its timeout (5 seconds). The server sends a snapshot to every session each tick, so a running server is never silent. Once the connection has ended, the client ignores every packet from the server.

Packets from an endpoint without a session, other than `connect_request`, are ignored. A client with another protocol version never reaches this flow: its datagrams fail header validation and are dropped and logged.

## Input Payload

An input message has a three-byte payload. The server identifies the player from the endpoint and session established by the connection flow. The payload contains no client ID or player position.

| Offset | Size | Field | Encoding | Allowed values |
|---|---:|---|---|---|
| 0 | 1 | `action_flags` | Unsigned 8-bit bit set | Bit 0 is `fire`; bits 1-7 must be zero |
| 1 | 1 | `move_x` | Signed 8-bit | `-1`, `0`, or `1` |
| 2 | 1 | `move_y` | Signed 8-bit | `-1`, `0`, or `1` |

The server must reject input payloads that are not exactly three bytes, have reserved action bits set, or contain movement values outside `-1`, `0`, and `1`.

The client sends its current input every tick while connected; an input also counts as a heartbeat. The server keeps only the latest input of each session, starting from an idle input (no action, no movement). An input whose packet sequence is not newer than the last accepted input of that session (duplicated or out of order, with wrap-around) is ignored, but still refreshes the session. Inputs from an endpoint without a session are ignored.

## Snapshot Payload

A snapshot is an authoritative world state at one server tick. It has a six-byte prefix followed by fixed-size entity records.

| Offset | Size | Field | Encoding |
|---|---:|---|---|
| 0 | 4 | `server_tick` | Unsigned 32-bit, big-endian |
| 4 | 2 | `entity_count` | Unsigned 16-bit, big-endian |
| 6 | Variable | `entities` | `entity_count` records, 19 bytes each |

Each entity record has the following layout:

| Offset | Size | Field | Encoding |
|---|---:|---|---|
| 0 | 4 | `entity_id` | Unsigned 32-bit, big-endian |
| 4 | 2 | `type_id` | Unsigned 16-bit, big-endian |
| 6 | 2 | `x` | Signed 16-bit, big-endian virtual-world coordinate |
| 8 | 2 | `y` | Signed 16-bit, big-endian virtual-world coordinate |
| 10 | 2 | `vx` | Signed 16-bit, big-endian velocity in virtual units per second |
| 12 | 2 | `vy` | Signed 16-bit, big-endian velocity in virtual units per second |
| 14 | 1 | `owner` | Unsigned 8-bit player id of the controlling client, `0` for none |
| 15 | 1 | `r` | Unsigned 8-bit red of the entity color |
| 16 | 1 | `g` | Unsigned 8-bit green |
| 17 | 1 | `b` | Unsigned 8-bit blue |
| 18 | 1 | `a` | Unsigned 8-bit alpha |

A snapshot packet carries at most 62 entities, the largest count that fits in the maximum payload size. A larger world is sent as several snapshot packets with the same `server_tick`, each holding at most 62 entities. The format of each packet does not change.

The server replicates every entity that has the engine's `Replicated` component (its `type_id`) and a position; `vx` and `vy` are always zero for now. `owner` is `Replicated::owner`; a client compares it with the `player_entity_id` of its `connect_accept` to find its own entities. The color is the entity's sprite color, opaque white when it has no sprite. `entity_id` is the entity index. Coordinates and velocities are rounded to the nearest integer and clamped to the signed 16-bit range.

The client keeps the world of the newest `server_tick` it has received, comparing ticks with wrap-around like sequence numbers:

- a newer tick replaces the whole world with the packet's entities;
- another packet of the same tick is merged in, an entity already present being updated rather than duplicated;
- an older tick is ignored.

The receiver rejects a snapshot unless its payload size is exactly `6 + entity_count * 19` and every `type_id` exists in the connection type table.

## Event Payload

An event payload begins with a one-byte event type. Each event packet contains one event.

| Value | Name | Remaining payload |
|---:|---|---|
| 1 | `spawn` | `entity_id`, `type_id`, `x`, `y` |
| 2 | `destroy` | `entity_id` |
| 3 | `fire` | `source_entity_id`, `projectile_entity_id` |
| 4 | `sound` | `source_entity_id`, `sound_id` |
| 5 | `player_left` | `player_entity_id` |
| 6 | `music` | `music_id` |

The `spawn` event payload is 11 bytes.

| Offset | Size | Field | Encoding |
|---|---:|---|---|
| 0 | 1 | `event_type` | `1` |
| 1 | 4 | `entity_id` | Unsigned 32-bit, big-endian |
| 5 | 2 | `type_id` | Unsigned 16-bit, big-endian |
| 7 | 2 | `x` | Signed 16-bit, big-endian |
| 9 | 2 | `y` | Signed 16-bit, big-endian |

The `destroy` event payload is five bytes: one-byte event type `2` followed by an unsigned 32-bit big-endian `entity_id`.

The `fire` event payload is nine bytes: one-byte event type `3`, followed by unsigned 32-bit big-endian `source_entity_id` and `projectile_entity_id`.

The `sound` event payload is seven bytes: one-byte event type `4`, followed by unsigned 32-bit big-endian `source_entity_id` and unsigned 16-bit big-endian `sound_id`. `sound_id` is an id of the sound table. `source_entity_id` is the entity that played the sound, or `0xFFFFFFFF` when none did.

The `music` event payload is three bytes: one-byte event type `6` followed by the unsigned 16-bit big-endian `music_id` of the music to play in a loop from its start, replacing the current one. `0xFFFF` stops the music. The server remembers the last music sent and puts it in the `current_music` of every later `connect_accept`.

The `player_left` event payload is five bytes: one-byte event type `5` followed by the unsigned 32-bit big-endian `player_entity_id` of the player whose session ended.

The receiver rejects unknown event types, an event with an incorrect fixed payload size, and a spawn event with an unknown `type_id`. A `sound` or `music` event with an id missing from its table is ignored.

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
52 54 59 50  00 03  01  00 00 00 00  00 04  01 02 03 04
magic        ver    type sequence    size   client_nonce
```

`input` holding `fire` and moving left, sequence `42`:

```
52 54 59 50  00 03  03  00 00 00 2A  00 03  01  FF  00
                                            fire x=-1 y=0
```

`heartbeat`, sequence `7`:

```
52 54 59 50  00 03  06  00 00 00 07  00 00
```

`disconnect` with reason `normal`, sequence `8`:

```
52 54 59 50  00 03  07  00 00 00 08  00 01  00
```

`event` `music` playing music id `1`, sequence `9`:

```
52 54 59 50  00 03  05  00 00 00 09  00 03  06  00 01
                                            music music_id
```

## Writing a client

A minimal client:

1. Open a UDP socket and send `connect_request` with a random nonce to the
   server. Resend it every 500 ms until an answer arrives; give up after a few
   attempts.
2. On `connect_accept` with your nonce: remember your player id
   (`player_entity_id`), the type, sound and music tables, and start
   `current_music` unless it is `0xFFFF`. On `disconnect(server_full)`: stop.
3. Every tick (60 Hz), send an `input` with your current actions. Number every
   packet you send with an increasing sequence.
4. Send a `heartbeat` every second.
5. Keep the world of the newest `snapshot` tick, merging packets of the same
   tick. Draw each entity using the type name from the type table; entities
   whose `owner` equals your player id are yours.
6. On `event` `sound`, play the sound named by the sound table; on `event`
   `music`, loop the music named by the music table, or stop it on `0xFFFF`.
   On `event` `player_left`, show that the player left. On any `disconnect`,
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
| Cheating by sending positions | Clients only send inputs (one bit and two values in `{-1, 0, 1}`); the server computes every position |
| Replayed or reordered inputs | Inputs older than the last accepted sequence are ignored |
| Spoofed packets from another endpoint | Messages from an endpoint without a session are ignored. The protocol has no authentication or encryption: an attacker who can spoof a player's IP address and port can inject inputs for that player. This is accepted for a LAN game and listed as future work |
| Memory disclosure | C++ object memory is never sent; every field is written explicitly in big-endian order |

Only `connect_request` creates state on the server, and only up to
`max_players` sessions; each expires after 5 seconds of silence.
