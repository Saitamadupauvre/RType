# R-Type UDP Protocol

## Status

Protocol version `1` defines every message payload in this document.

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
| 4 | 2 | `version` | Unsigned 16-bit, big-endian | `1` |
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

Reliable messages use acknowledgement and retransmission rules that will be added in a later protocol version. Until then, implementations must not silently treat reliable messages as delivered.

## Connection Payloads

### Connect Request

The client sends this message from the UDP endpoint it will use for the game session. The nonce lets the client associate a later `connect_accept` packet with its request.

| Offset | Size | Field | Encoding |
|---|---:|---|---|
| 0 | 4 | `client_nonce` | Unsigned 32-bit, big-endian |

The payload is exactly four bytes.

### Connect Accept

The server sends this message to the endpoint that sent the matching request. The entity-type table maps the numeric IDs used by snapshots and events to game entity names.

| Offset | Size | Field | Encoding |
|---|---:|---|---|
| 0 | 4 | `client_nonce` | Unsigned 32-bit, big-endian |
| 4 | 4 | `player_entity_id` | Unsigned 32-bit, big-endian |
| 8 | 2 | `type_count` | Unsigned 16-bit, big-endian |
| 10 | Variable | `type_entries` | `type_count` entries |

Each type entry has the following layout:

| Offset | Size | Field | Encoding |
|---|---:|---|---|
| 0 | 2 | `type_id` | Unsigned 16-bit, big-endian |
| 2 | 1 | `name_length` | Unsigned 8-bit, maximum 63 |
| 3 | Variable | `name` | UTF-8 bytes, exactly `name_length` bytes |

The receiver rejects duplicate type IDs, duplicate names, a name length above 63, truncated entries, or entries extending beyond the declared payload length.

## Input Payload

An input message has a three-byte payload. The server identifies the player from the endpoint and session established by the connection flow. The payload contains no client ID or player position.

| Offset | Size | Field | Encoding | Allowed values |
|---|---:|---|---|---|
| 0 | 1 | `action_flags` | Unsigned 8-bit bit set | Bit 0 is `fire`; bits 1-7 must be zero |
| 1 | 1 | `move_x` | Signed 8-bit | `-1`, `0`, or `1` |
| 2 | 1 | `move_y` | Signed 8-bit | `-1`, `0`, or `1` |

The server must reject input payloads that are not exactly three bytes, have reserved action bits set, or contain movement values outside `-1`, `0`, and `1`.

## Snapshot Payload

A snapshot is an authoritative world state at one server tick. It has a six-byte prefix followed by fixed-size entity records.

| Offset | Size | Field | Encoding |
|---|---:|---|---|
| 0 | 4 | `server_tick` | Unsigned 32-bit, big-endian |
| 4 | 2 | `entity_count` | Unsigned 16-bit, big-endian |
| 6 | Variable | `entities` | `entity_count` records, 14 bytes each |

Each entity record has the following layout:

| Offset | Size | Field | Encoding |
|---|---:|---|---|
| 0 | 4 | `entity_id` | Unsigned 32-bit, big-endian |
| 4 | 2 | `type_id` | Unsigned 16-bit, big-endian |
| 6 | 2 | `x` | Signed 16-bit, big-endian virtual-world coordinate |
| 8 | 2 | `y` | Signed 16-bit, big-endian virtual-world coordinate |
| 10 | 2 | `vx` | Signed 16-bit, big-endian velocity in virtual units per second |
| 12 | 2 | `vy` | Signed 16-bit, big-endian velocity in virtual units per second |

The receiver rejects a snapshot unless its payload size is exactly `6 + entity_count * 14` and every `type_id` exists in the connection type table.

## Event Payload

An event payload begins with a one-byte event type. Each event packet contains one event.

| Value | Name | Remaining payload |
|---:|---|---|
| 1 | `spawn` | `entity_id`, `type_id`, `x`, `y` |
| 2 | `destroy` | `entity_id` |
| 3 | `fire` | `source_entity_id`, `projectile_entity_id` |
| 4 | `sound` | `source_entity_id`, `sound_id` |

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

The `sound` event payload is seven bytes: one-byte event type `4`, followed by unsigned 32-bit big-endian `source_entity_id` and unsigned 16-bit big-endian `sound_id`.

The receiver rejects unknown event types, an event with an incorrect fixed payload size, and a spawn event with an unknown `type_id`.

## Heartbeat Payload

A heartbeat payload is empty. Both peers send it periodically to prove that their UDP endpoint remains reachable. A peer that does not receive any valid packet or heartbeat before the configured timeout is considered disconnected.

## Disconnect Payload

The disconnect payload contains one byte.

| Value | Name |
|---:|---|
| 0 | `normal` |
| 1 | `timeout` |
| 2 | `server_shutdown` |
| 3 | `protocol_error` |

Receivers reject a disconnect payload unless it is exactly one byte and contains a defined reason.

## Sequence Numbers

Each sender increments its packet sequence for every datagram it sends. Receivers use sequence numbers to detect duplicated and stale packets. Sequence numbers wrap from `0xFFFFFFFF` to `0`.

## Receiver Rules

- Reject a datagram before allocating memory when its header, payload size, or payload layout is invalid.
- Do not process a packet from an endpoint before its connection request has been accepted.
- Drop duplicated or stale unreliable packets.
- Treat malformed datagrams as untrusted input: drop and log them without stopping the client or server.
