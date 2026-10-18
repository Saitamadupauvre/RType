# Networking TODOs

## Monday: Network thread and message queues (#29)

Goal: the server receives, decodes and validates datagrams on its own thread,
and the game thread only exchanges decoded messages through two queues. No
session logic yet (that is #19/#20). New files go in `engine/src/network/thread/`,
tests in `tests/engine/network/thread/`.

### 0. Housekeeping

- [x] Update the #29 issue body: it still says `asio::io_context`,
  `asio::post` and `parse_packet`. We use our own `UdpSocket` and
  `ProtocolHandler::decode`, and the outbox replaces `asio::post`.
- [ ] Close #13, #14, #41, #77–#81, #82 and #83 by hand once
  `networking/base` is merged into `dev` (`Closes` only fires on the default
  branch).
- [x] Delete the untracked `pr-stack/` and `protocol_codecs.md`.

### 1. `MessageQueue<T>` (`thread/MessageQueue.hpp`, header-only template)

- [x] `void push(T value)`: locks a `std::mutex`, `push_back` into a `std::vector<T>`.
- [x] `std::vector<T> drain()`: locks, swaps the buffer with an empty vector,
  returns it, so the game thread holds the lock only for the swap.
- [x] No `static` members (AGENTS.md: no global state in headers).
- [x] Tests in `thread/MessageQueueTest.cpp`:
  - [x] drain on an empty queue returns an empty vector;
  - [x] items come out in push order for a single producer;
  - [x] 4 `std::jthread` producers push 10 000 distinct values each, join,
    then one `drain()`: every value exactly once (sort and compare), no sleeps;
  - [x] a consumer draining while producers push loses nothing (sum of all
    drains equals everything pushed).

### 2. Queue item types (`thread/NetworkMessages.hpp`)

- [x] `struct Inbound { Endpoint from; ReceivedMessage message; };`
- [x] `struct Outbound { Endpoint to; Message message; };`

### 3. Datagram handling without threads (`thread/DatagramHandler.{hpp,cpp}`)

Keep the per-datagram logic out of the thread so it is tested deterministically.

- [x] `std::optional<Inbound> handle_datagram(const ProtocolHandler&, std::span<const std::uint8_t>, const Endpoint& from)`:
  `decode`, on failure `engine::core::log::warn` with `from.to_string()` and
  the datagram size, return `std::nullopt`.
- [x] `std::optional<Bytes> encode_outbound(const ProtocolHandler&, const Outbound&, std::uint32_t sequence)`:
  `encode`, log and drop on failure.
- [x] Tests: valid datagram becomes an `Inbound` with the right endpoint;
  truncated, bad magic, wrong version, random bytes are dropped and logged
  (check with `MemorySink`); invalid outbound payload (e.g. `InputPayload{.move_x = 2}`) is dropped.

### 4. `NetworkThread` (`thread/NetworkThread.{hpp,cpp}`)

- [ ] Owns the `UdpSocket`, a `ProtocolHandler`, the inbox
  `MessageQueue<Inbound>`, the outbox `MessageQueue<Outbound>`, the outgoing
  sequence counter and a `std::jthread` (declared last so it stops first).
- [ ] `start(const Endpoint& bind_to, std::error_code& ec)`: create and bind
  the socket on the caller's thread (so bind errors reach the caller), then
  start the thread.
- [ ] Loop while `!stop_token.stop_requested()`:
  1. drain the outbox, `encode_outbound`, `send_to` each one;
  2. `receive_from` into a `max_datagram_size` buffer with a short timeout
     (e.g. 10 ms, a named constant) so the outbox is flushed regularly;
  3. `SocketError::timed_out` → continue; `SocketError::canceled`/`SocketError::closed` →
     exit; other errors → log and continue;
  4. `handle_datagram` → push into the inbox.
- [ ] Game-thread API: `std::vector<Inbound> poll()` drains the inbox;
  `void send(const Endpoint& to, Message message)` pushes into the outbox.
  Only the network thread touches the socket.
- [ ] `stop()`: `request_stop()`, flush the remaining outbox, `close()` the
  socket (unblocks `receive_from`), join. Destructor calls `stop()`.
- [ ] `local_endpoint()` so tests can bind port 0 and find the real port.
- [ ] Tests in `thread/NetworkThreadTest.cpp` (loopback only, like `UdpSocketTest`):
  - [ ] start on `127.0.0.1:0`, stop, destructor: no hang, no crash;
  - [ ] binding a port already in use reports the error from `start`;
  - [ ] a datagram sent by a plain `UdpSocket` shows up in `poll()` (wait with
    a bounded loop on `poll()` plus a deadline, no fixed `sleep`);
  - [ ] a garbage datagram never shows up in `poll()`;
  - [ ] `send()` reaches a plain `UdpSocket` peer and decodes to the same `Message`;
  - [ ] `stop()` with sends still queued flushes them before joining.

### 5. Wire it up and check

- [x] Add `DatagramHandler.cpp` and the `MessageQueue` / `DatagramHandler`
  tests to the CMake lists.
- [ ] Add `NetworkThread.cpp` to `engine/CMakeLists.txt` and
  `NetworkThreadTest.cpp` to `tests/CMakeLists.txt`.
- [ ] `./fred.sh all` green.
- [ ] `cmake --workflow --preset tsan` reports no data race (#29 acceptance criterion).
- [ ] Update the "Server" checklist below and document the thread/queue
  ownership in `docs/PROJECT.md` §7 (#89).

### Done alongside #29

- [x] #82 `TypeTable` (`protocol/TypeTable.{hpp,cpp}`): `assign(connect_accept)`,
  then `admits(Message)` drops and logs snapshots and spawn events with an
  unknown `type_id`, and any snapshot or event received before `connect_accept`.
- [x] #83 `SocketError` (`socket/SocketError.{hpp,cpp}`): engine-owned error
  codes in their own `std::error_category`, mapped from `errno` in
  `socket_error_from_errno`; unmapped values become `unknown` and are logged.
  `UdpSocket` reports only these codes.
- [x] #90 README "Run" section: arguments, `--help`, exit code `84`, local
  example with one server and two clients.

### Next after #29

- #19/#20 sessions: handshake and session table on the game thread, fed by
  `NetworkThread::poll()`, tested with fake `Inbound` messages and `ManualClock`.
  Decide first how to add `Reject(reason)` and `PlayerLeft(player_id)` to the
  protocol: neither exists yet.
- Hook `TypeTable` into the client: `assign` on `connect_accept`, then drop
  every `Inbound` that `admits` refuses.
- #90 stays open until a client really connects (#19, #42); recheck the
  README commands then.

## Payload codecs (done)

Every payload follows the pattern already used by `InputPayload`. Paths are
relative to `engine/src/network/`:

- `protocol/Protocol.hpp` holds the wire constants, `MessageType` and
  `PacketHeader`;
- the struct lives in `protocol/Messages.hpp` with
  `bool operator==(const T&) const = default;`, and is added to `Message`;
- one `is_valid(const T&)` in `protocol/Validation.cpp`, used by both the
  encoder and the decoder;
- `encode_payload(const T&)` in `encoder/PayloadEncoder.cpp` checks `is_valid`,
  writes with `bytes/ByteWriter.hpp`, and returns `std::move(writer).take()`;
- `decode_<message>(std::span<const std::uint8_t>)` in
  `decoder/PayloadDecoder.cpp` reads with `bytes/ByteReader.hpp`, returns
  `std::nullopt` on any failed read or when `!reader.at_end()`, then checks
  `is_valid`; event variants get a reader in the `event_readers` table of
  `decoder/EventReader.cpp`;
- declarations go in `encoder/PayloadEncoder.hpp` and
  `decoder/PayloadDecoder.hpp`, marked `[[nodiscard]] ENGINE_NETWORK_EXPORT`;
- tests mirror the folders under `tests/engine/network/`, one
  `TEST(<Struct>, ...)` group per message: exact bytes and encoder rejections in
  `encoder/PayloadEncoderTest.cpp`; round trip, truncated, trailing bytes and
  decoder rejections in `decoder/PayloadDecoderTest.cpp`; shared sample
  builders in `PayloadSamples.hpp`.

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

- [x] Structs in `protocol/Messages.hpp`:
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

- [x] Structs in `protocol/Messages.hpp`:
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

- [x] Structs in `protocol/Messages.hpp`:
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

- [x] In `protocol/Messages.hpp`:
  - `using Message = std::variant<ConnectRequestPayload, ConnectAcceptPayload, InputPayload, SnapshotPayload, EventPayload, HeartbeatPayload, DisconnectPayload>;`
- [x] In `protocol/ProtocolHandler.hpp`:
  - `struct ReceivedMessage { PacketHeader header; Message message; };`
  - `std::optional<Bytes> encode(const Message& message, std::uint32_t sequence) const;`
  - `std::optional<ReceivedMessage> decode(std::span<const std::uint8_t> datagram) const;`
- [x] `encode`: `std::visit` calls `encode_payload`, then `serialize` with a header
  whose `type` comes from `message_type_of(const T&)` overloads in `ProtocolHandler.cpp`.
- [x] `decode`: `deserialize`, then look up the matching `decode_<message>` in the
  `message_decoders` table, wrapped into `Message`.
- [x] Tests in `tests/engine/network/protocol/ProtocolHandlerTest.cpp`: every message type
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
- [x] Validate snapshot and spawn `type_id` values against the connection type table.
- [x] Encode and decode the `event` payloads (`spawn`, `destroy`, `fire`, `sound`), rejecting unknown types and wrong fixed sizes.
- [x] Encode and decode the `disconnect` payload, rejecting undefined reasons.
- [x] Reject non-empty `heartbeat` payloads.

## UDP Transport

- [X] Define the `UdpTransport` public API using engine-owned endpoint and datagram types.
- [x] Implement RAII socket ownership and explicit close behavior.
- [x] Implement socket creation, bind, send, and receive.
- [x] Keep POSIX socket headers confined to private transport implementation files.
- [ ] Add the Windows socket implementation behind the same transport interface (postponed, see PROJECT.md §2).
- [x] Translate platform socket failures into engine errors without exposing platform types.

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
- [x] Add build and run examples for `r-type_server` and `r-type_client` (README "Run").
