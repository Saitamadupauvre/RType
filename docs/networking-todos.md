# Networking TODOs

## Tomorrow: Payload Codecs

Every payload follows the pattern already used by `InputPayload` in
`engine/src/network/Payloads.cpp`:

- the struct lives in `engine/src/network/Protocol.hpp` with
  `bool operator==(const T&) const = default;`;
- one `is_valid(const T&)` in the anonymous namespace of `Payloads.cpp`, used
  by both the encoder and the decoder;
- `encode_payload(const T&)` checks `is_valid`, writes with `ByteWriter`, and
  returns `std::move(writer).take()`;
- `decode_<message>(std::span<const std::uint8_t>)` reads with `ByteReader`,
  returns `std::nullopt` on any failed read or when `!reader.at_end()`, then
  checks `is_valid`;
- declarations go in `Payloads.hpp`, marked `[[nodiscard]] ENGINE_NETWORK_EXPORT`;
- tests go in `tests/engine/network/PayloadsTest.cpp`, one `TEST(<Struct>, ...)`
  group per message: exact bytes, round trip, truncated, trailing bytes, and
  every invalid value.

### 1. Fixed-size messages

- [x] `ConnectRequestPayload { std::uint32_t client_nonce; }`
  - `encode_payload(const ConnectRequestPayload&)`: `write_u32(client_nonce)`.
  - `decode_connect_request(payload)`: `read_u32()`, then `at_end()`. Exactly 4 bytes.
- [x] `HeartbeatPayload {}`
  - `encode_payload(const HeartbeatPayload&)`: returns an empty `Bytes`.
  - `decode_heartbeat(payload)`: accepts only `payload.empty()`.
- [x] `enum class DisconnectReason : std::uint8_t { normal = 0, timeout = 1, server_shutdown = 2, protocol_error = 3 };`
  and `DisconnectPayload { DisconnectReason reason; }`
  - `is_valid`: `reason <= DisconnectReason::protocol_error`.
  - `encode_payload(const DisconnectPayload&)`: `write_u8(static_cast<std::uint8_t>(reason))`.
  - `decode_disconnect(payload)`: `read_u8()`, `at_end()`, reject values above 3.

### 2. Event message

- [x] Structs in `Protocol.hpp`:
  - `SpawnEvent { std::uint32_t entity_id; std::uint16_t type_id; std::int16_t x; std::int16_t y; }`
  - `DestroyEvent { std::uint32_t entity_id; }`
  - `FireEvent { std::uint32_t source_entity_id; std::uint32_t projectile_entity_id; }`
  - `SoundEvent { std::uint32_t source_entity_id; std::uint16_t sound_id; }`
  - `using EventPayload = std::variant<SpawnEvent, DestroyEvent, FireEvent, SoundEvent>;`
  - `enum class EventType : std::uint8_t { spawn = 1, destroy = 2, fire = 3, sound = 4 };`
- [x] `encode_payload(const EventPayload&)`: `std::visit` writes the event type byte,
  then the fields in table order (`write_u32`, `write_u16`, `write_i16`).
- [x] `decode_event(payload)`: `read_u8()` for the type, look up its reader in the
  `event_readers` table, then `at_end()`. Sizes: spawn 11, destroy 5, fire 9, sound 7.
  Reject event type 0 and above 4.

### 3. Snapshot message

- [x] Structs in `Protocol.hpp`:
  - `EntityState { std::uint32_t entity_id; std::uint16_t type_id; std::int16_t x, y, vx, vy; }`
  - `SnapshotPayload { std::uint32_t server_tick; std::vector<EntityState> entities; }`
  - constants `snapshot_prefix_size = 6`, `entity_record_size = 14`,
    `max_snapshot_entities = (max_payload_size - snapshot_prefix_size) / entity_record_size` (84).
- [x] `is_valid`: `entities.size() <= max_snapshot_entities`.
- [x] `encode_payload(const SnapshotPayload&)`: `write_u32(server_tick)`,
  `write_u16(entity count)`, then each record.
- [x] `decode_snapshot(payload)`: `read_u32()`, `read_u16()` count, then require
  `reader.remaining() == count * entity_record_size` **before**
  `entities.reserve(count)`; reject `count > max_snapshot_entities`.
- [x] Test that a 13-byte packet claiming 65535 entities is rejected without allocating.

### 4. Connect accept message

- [x] Structs in `Protocol.hpp`:
  - `TypeEntry { std::uint16_t type_id; std::string name; }`
  - `ConnectAcceptPayload { std::uint32_t client_nonce; std::uint32_t player_entity_id; std::vector<TypeEntry> types; }`
  - constants `max_type_name_length = 63`, `min_type_entry_size = 3`.
- [x] `is_valid`: every name 1 to 63 bytes, no duplicate `type_id`, no duplicate
  name (two `std::unordered_set`), encoded size `<= max_payload_size`.
- [x] `encode_payload(const ConnectAcceptPayload&)`: `write_u32` nonce,
  `write_u32` player id, `write_u16` count, then per entry `write_u16(type_id)`,
  `write_u8(name.size())`, `write_bytes(name)`.
- [x] `decode_connect_accept(payload)`: read the three prefix fields, require
  `count * min_type_entry_size <= reader.remaining()` before `types.reserve(count)`,
  then per entry `read_u16()`, `read_u8()` length, `read_bytes(length)`;
  finish with `at_end()` and `is_valid`.
- [x] Tests: empty name, 64-byte name, duplicate id, duplicate name, name running
  past the end, count larger than the entries present, trailing bytes.

### 5. Typed message dispatch in `ProtocolHandler`

- [ ] In `ProtocolHandler.hpp`:
  - `using Message = std::variant<ConnectRequestPayload, ConnectAcceptPayload, InputPayload, SnapshotPayload, EventPayload, HeartbeatPayload, DisconnectPayload>;`
  - `struct ReceivedMessage { PacketHeader header; Message message; };`
  - `std::optional<Bytes> encode(const Message& message, std::uint32_t sequence) const;`
  - `std::optional<ReceivedMessage> decode(std::span<const std::uint8_t> datagram) const;`
- [ ] `encode`: `std::visit` calls `encode_payload`, then `serialize` with a header
  whose `type` comes from `message_type_of(const T&)` overloads in `ProtocolHandler.cpp`.
- [ ] `decode`: `deserialize`, then `switch (header.type)` to the matching
  `decode_<message>`, wrapped into `Message`.
- [ ] Tests in `tests/engine/network/ProtocolHandlerTest.cpp`: every message type
  round-trips through `encode` / `decode`, the header type matches the payload,
  and a valid header with a payload of the wrong message type is rejected.

## Protocol

- [x] Define the packet header: magic, protocol version, message type, sequence number, and payload length.
- [x] Define a fixed byte order for every multibyte field.
- [x] Implement explicit packet serialization without sending C++ object memory directly.
- [x] Implement packet decoding with size and bounds validation before every read.
- [x] Reject truncated packets, invalid magic values, unsupported versions, unknown message types, and mismatched payload lengths.
- [x] Define messages for connection, input, snapshots, entity events, heartbeat, and disconnection.
- [x] Document the protocol header and every message in `docs/protocol.md`.
- [x] Encode and decode the `input` payload.
- [x] Encode and decode the `connect_request` payload (4-byte client nonce).
- [x] Encode and decode the `connect_accept` payload, rejecting duplicate type IDs or names, empty names, names above 63 bytes, and truncated entries.
- [x] Encode and decode the `snapshot` payload, rejecting sizes other than `6 + entity_count * 14` and more than 84 entities.
- [ ] Validate snapshot and spawn `type_id` values against the connection type table.
- [x] Encode and decode the `event` payloads (`spawn`, `destroy`, `fire`, `sound`), rejecting unknown types and wrong fixed sizes.
- [x] Encode and decode the `disconnect` payload, rejecting undefined reasons.
- [x] Reject non-empty `heartbeat` payloads.

## UDP Transport

- [X] Define the `UdpTransport` public API using engine-owned endpoint and datagram types.
- [x] Implement RAII socket ownership and explicit close behavior.
- [x] Implement socket creation, bind, send, and receive.
- [x] Keep POSIX socket headers confined to private transport implementation files.
- [ ] Add the Windows socket implementation behind the same transport interface (postponed, see PROJECT.md §2).
- [ ] Translate platform socket failures into engine errors without exposing platform types.

## Server

- [ ] Start a dedicated network thread that receives UDP datagrams without blocking the game loop.
- [ ] Decode and validate packets on the network thread.
- [ ] Queue valid client inputs for the game thread.
- [ ] Apply queued inputs at the start of each fixed server tick.
- [ ] Send authoritative snapshots and game events to connected clients.
- [ ] Track client endpoints and heartbeat deadlines.
- [ ] Remove timed-out clients and notify the remaining clients.

## Client

- [ ] Send local input actions to the server over UDP.
- [ ] Receive and validate snapshots on a networking thread.
- [ ] Queue snapshots for the render/game thread.
- [ ] Apply authoritative snapshots without running authoritative gameplay logic.
- [ ] Handle server disconnection cleanly.

## Reliability And Performance

- [ ] Use sequence numbers to discard duplicated and stale snapshots.
- [ ] Identify reliable messages such as connection, spawn, destruction, and disconnection.
- [ ] Define acknowledgement and retransmission behavior for reliable UDP messages.
- [ ] Keep packets below the path MTU to avoid fragmentation.
- [ ] Lift the 84-entity snapshot limit (multi-packet snapshots or delta compression), and update `docs/protocol.md`.
- [ ] Measure packet rate, bandwidth use, latency, packet loss, and reordering.
- [ ] Evaluate client-side prediction and snapshot interpolation after the authoritative path works.

## Tests

- [x] Add protocol tests for valid headers and payloads.
- [x] Add protocol tests for truncated packets, invalid lengths, unknown types, and random bytes.
- [ ] Add deterministic transport tests with a fake transport instead of real network timing.
- [ ] Add server tests for queued input processing and heartbeat timeouts.
- [ ] Add client tests for stale, duplicated, and out-of-order snapshots.
- [ ] Add payload tests for every message: valid, truncated, oversized, out-of-range values, and random bytes.

## Documentation

- [x] Replace the Asio reference in `docs/PROJECT.md` with the socket-wrapper design.
- [ ] Document the server thread, game thread, queues, and ownership boundaries.
- [ ] Document how malformed packets and client crashes are handled.
- [ ] Add build and run examples for `r-type_server` and `r-type_client`.
