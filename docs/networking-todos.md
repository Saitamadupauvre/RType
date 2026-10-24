# Networking TODOs

## Monday: Network thread and message queues (#29)

Goal: a receive thread decodes and validates datagrams, a send thread encodes
and sends replies, and the game thread only exchanges decoded messages through
two queues (`docs/PROJECT.md` §7). No
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
- [x] `std::vector<T> wait_and_drain(std::stop_token)` so the send thread sleeps
  instead of polling: a `std::condition_variable_any ready_`, `push` calls
  `ready_.notify_one()` after unlocking, `wait_and_drain` uses
  `ready_.wait(lock, stop_token, [this] { return !items_.empty(); })` then swaps
  like `drain()`. Returns whatever is queued, so it is empty only when stop is
  requested with nothing queued.
  - [x] items already queued come back without waiting;
  - [x] a push from another thread wakes the waiting consumer (no sleep:
    the consumer `std::jthread` is joined after the push);
  - [x] `request_stop()` on an empty queue returns an empty vector;
  - [x] items queued before the stop still come back.

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

### 4. `NetworkIo` (`thread/NetworkIo.{hpp,cpp}`)

- [x] Owns the `UdpSocket`, a `ProtocolHandler`, the inbox
  `MessageQueue<Inbound>`, the outbox `MessageQueue<Outbound>`, the outgoing
  sequence counter and two `std::jthread`s, `receive_thread_` and
  `send_thread_`, declared last so they stop before the data they use.
- [x] `start(const Endpoint& bind_to, std::error_code& ec)`: create and bind
  the socket on the caller's thread (so bind errors reach the caller), then
  start both threads. A second `start` while running reports
  `std::errc::already_connected`.
- [x] Receive thread, `run_receive(stop_token)`: loop on `receive_one(buffer)`
  with a `max_datagram_size + 1` buffer:
  1. `receive_from` with `wait_forever` (no timeout: `close()` wakes it up);
  2. `SocketError::canceled`/`SocketError::closed` → return `false` (exit);
     other errors → log and return `true`;
  3. more than `max_datagram_size` bytes → log and drop (a 1200-byte buffer
     would silently truncate it, and a full-size valid packet with trailing
     bytes would be accepted);
  4. `handle_datagram` → push into the inbox. The inbox is bounded
     (`default_inbox_capacity`, 4096): when the game thread falls behind, extra
     messages are dropped and logged instead of growing memory without limit.
- [x] `MessageQueue` takes an optional capacity; `push` returns `false` when full.
- [x] Send thread, `run_send(stop_token)`: loop on
  `outbox_.wait_and_drain(stop_token)` → `send_all`, which runs
  `encode_outbound` with `next_sequence_++` and `send_to` for each message;
  log send errors and keep going. Only this thread touches `next_sequence_`.
- [x] Game-thread API: `std::vector<Inbound> poll()` drains the inbox;
  `void send(const Endpoint& to, Message message)` pushes into the outbox.
  Only the receive thread reads from the socket and only the send thread
  writes to it.
- [x] `stop()`, in this order:
  1. `send_thread_.request_stop()` and join: it sends what is left in the
     outbox (`drain()` once more after the wait returns) and exits;
  2. `close()` the socket: wakes the receive thread with `SocketError::canceled`;
  3. `receive_thread_.request_stop()` and join.
  Closing before the send thread has flushed loses the last messages with
  `SocketError::closed`. Idempotent; the destructor calls `stop()`.
- [x] `local_endpoint()` so tests can bind port 0 and find the real port.
- [x] Tests in `thread/NetworkIoTest.cpp` (loopback only, like `UdpSocketTest`):
  - [x] start on `127.0.0.1:0`, stop, destructor: no hang, no crash;
  - [x] binding a port already in use reports the error from `start`;
  - [x] a second `start` while running is refused;
  - [x] `start` after `stop` works again;
  - [x] a full-size datagram is accepted, one byte more is dropped and logged;
  - [x] a full inbox drops and logs extra messages;
  - [x] a datagram sent by a plain `UdpSocket` shows up in `poll()` (wait with
    a bounded loop on `poll()` plus a deadline, no fixed `sleep`);
  - [x] a garbage datagram never shows up in `poll()`;
  - [x] `send()` reaches a plain `UdpSocket` peer and decodes to the same `Message`;
  - [x] `stop()` with sends still queued flushes them before joining;
  - [x] `send()` reaches the peer while no datagram arrives (the send thread
    never waits on the receive side).

### 5. Wire it up and check

- [x] Add `DatagramHandler.cpp` and the `MessageQueue` / `DatagramHandler`
  tests to the CMake lists.
- [x] Add `NetworkIo.cpp` to `engine/CMakeLists.txt` and
  `NetworkIoTest.cpp` to `tests/CMakeLists.txt`.
- [x] `./fred.sh all` green.
- [x] `cmake --workflow --preset tsan` reports no data race (#29 acceptance criterion).
- [x] Describe the receive, game and send threads in `docs/PROJECT.md` §7.
- [x] Update the "Server" checklist below; the full thread/queue/failure
  documentation is #89.

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

## Sessions: connect, disconnect and timeouts (#19, #20)

Game-thread logic on decoded messages: it takes `Inbound`, returns `Outbound`,
reads time from an injected `IClock` and never touches a socket. New files go
in `engine/src/network/session/`, tests in `tests/engine/network/session/`.

### 1. Protocol additions

- [x] `Reject(reason)` is `disconnect` with the new reason
  `DisconnectReason::server_full = 4`, sent in answer to `connect_request`.
- [x] `PlayerLeft(player_id)` is the new event `PlayerLeftEvent
  { player_entity_id }`, `EventType::player_left = 5`, 5 bytes.
- [x] Encoder, `event_readers` table, validation and codec tests updated.
- [x] `docs/protocol.md`: new values and a "Connection Flow" section.
- [x] `Endpoint::operator==` (family, port, address), so sessions are keyed
  by endpoint without going through strings.

### 2. `ServerSessions` (`session/ServerSessions.{hpp,cpp}`)

- [x] `ServerSessionsConfig { max_players, timeout = 5s, types }`. The engine
  has no default player count: the game passes it, from `r-type_server
  --max-players` (1 to 4, default 4), like the port.
- [x] `handle(Inbound)`:
  - `connect_request` from a new endpoint: `connect_accept` with the lowest
    free player id and the type table, or `disconnect(server_full)` when full;
  - `connect_request` from a known endpoint: same `connect_accept` again
    (idempotent, no new session, no new event);
  - any message from a known endpoint refreshes `last_seen`;
  - `disconnect` from a known endpoint ends the session;
  - anything else from an unknown endpoint is ignored and logged at debug.
- [x] `drop_timed_out()`: sessions silent for longer than `timeout` get
  `disconnect(timeout)`; sessions that time out together are not told about
  each other.
- [x] Ending a session sends `player_left` to every remaining session and
  frees the player id.
- [x] `PlayerJoined` / `PlayerLeft` published on the `EventBus`
  (`engine/include/engine/network/Events.hpp`).

### 3. `ClientConnection` (`session/ClientConnection.{hpp,cpp}`)

- [x] `ClientConnectionConfig { retry_interval = 500ms, max_attempts = 5,
  heartbeat_interval = 1s }`; the nonce is injected.
- [x] `update()`: sends `connect_request` and retries every
  `retry_interval`, then `gave_up` after `max_attempts`; once connected,
  sends a `heartbeat` every `heartbeat_interval`.
- [x] `handle(Inbound)`: only from the server endpoint. `connect_accept` with
  the right nonce → `connected` (player id, types); `disconnect(server_full)`
  while connecting → `rejected`; any other `disconnect` → `disconnected`.
- [x] `disconnect()`: sends `disconnect(normal)` once.

## Inputs (#21)

- [x] `protocol/Sequence.hpp`: `is_newer_sequence(candidate, latest)`, true
  when `candidate` is ahead by less than half the 32-bit range (wrap-around).
- [x] `Session` gains `input` (latest `InputPayload`, idle at first) and
  `input_sequence` (packet sequence of that input).
- [x] `ServerSessions::handle`: an input from a session is stored only when its
  header sequence is newer than `input_sequence`; duplicated or out-of-order
  inputs are dropped (logged at debug) but still refresh `last_seen`. Inputs
  from unknown endpoints are ignored. Applying them to entities is the game
  loop's job (#42).
- [x] `ClientConnection::send_input(InputPayload)`: only while connected, and
  postpones the next heartbeat.
- [x] Malformed inputs (movement out of range, reserved action bits, wrong
  size) are dropped and logged by `handle_datagram` on the receive thread.
- [x] `docs/protocol.md`: input rules.

## Snapshots (#22)

- [x] Public `engine::network::Replicated { type_id }`
  (`engine/include/engine/network/Replicated.hpp`): marks the entities sent in
  snapshots. Registered by the game on the server registry.
- [x] `snapshot/SnapshotBuilder.{hpp,cpp}`:
  `build_snapshots(EntityManager&, server_tick)` queries `Replicated` +
  `Position` (+ `Velocity` when present) and returns snapshot packets of at
  most `max_snapshot_entities` (84), all with the same tick; an empty world
  still gives one empty snapshot. `to_wire_coordinate` rounds and clamps floats
  to `int16` (NaN becomes 0).
- [x] `ServerSessions::broadcast(Message)`: one `Outbound` per session; also
  used for `player_left`, and ready for events (#84).
- [x] `snapshot/SnapshotReceiver.{hpp,cpp}` (client): keeps the newest tick,
  merges packets of the same tick by `entity_id`, ignores older ticks (with
  wrap-around via `is_newer_sequence`).
- [x] `docs/protocol.md`: multi-packet snapshots, what is replicated, client
  rules.

## Server game loop, networking side (#42)

- [x] `server/ServerNetwork.{hpp,cpp}` (private, no sockets): one tick of
  networking. `receive(inbound)` runs every message through `ServerSessions`
  and then `drop_timed_out()`; `snapshot(registry)` advances the server tick and
  broadcasts every packet of `build_snapshots` (nothing when nobody is
  connected). Both return the `Outbound` to send. Tested end to end with fake
  messages and `ManualClock`.
- [x] Public `NetworkServer` (`engine/include/engine/network/Server.hpp`),
  pimpl over `NetworkIo` + `ServerNetwork`: `ServerOptions { port,
  max_players }`, `start(ec)` (binds `0.0.0.0:port`), `stop()`, `receive()`,
  `inputs()` (public `PlayerInput`), `send_snapshot(registry)`, `port()`,
  `player_count()`. Tested over loopback.
- [x] `game/server/ServerApp.{hpp,cpp}` (`r-type_server_app`): owns the event
  bus, registry (registers `Replicated`, `Position`, `Velocity`), system
  scheduler and `NetworkServer`. `tick(step)`: `receive()` → systems →
  `send_snapshot()`. `run(should_stop)` drives it with a 60 Hz
  `FixedStepLoop`, then stops the network.
- [x] `main.cpp` stays thin: CLI, `start` (exit `84` when the port cannot be
  bound), SIGINT/SIGTERM set a flag that stops `run`.
- [ ] Lua `tick` and applying `inputs()` to player entities: needs the tick
  phase order (#46) and the game scripts.
- [x] Client side: `NetworkClient` over `NetworkIo` + `ClientConnection` +
  `SnapshotReceiver` (see below).

## `NetworkClient` and server disconnection on the client (#85)

- [x] Public `NetworkClient` (`engine/include/engine/network/Client.hpp`),
  pimpl over `NetworkIo` + `ClientConnection` + `SnapshotReceiver`:
  `connect(ip, port, ec)` (binds a free local port), `update()`,
  `send_input(flags, x, y)`, `disconnect()`, `state()`, `player_id()`,
  `server_tick()`, `entities()` (public `EntitySnapshot`, world units).
- [x] `NetworkClient::types()`: the type table of `connect_accept`, so the
  game can name the `type_id` of each `EntitySnapshot`.
- [x] `ConnectionState` and `DisconnectReason` moved to public headers.
- [x] The client ends the connection on any `disconnect` from the server, or
  after 5 s without a packet from it (`ClientConnectionConfig::server_timeout`).
  Packets after the end are ignored.
- [x] When the connection ends, the network threads stop and `Disconnected
  { reason }` is published once on the client's event bus (`server_full` when
  rejected, `timeout` when the server never answered or went silent, `normal`
  after `disconnect()`).
- [x] `NetworkServer::stop()` sends `disconnect(server_shutdown)` to every
  player before closing.
- [ ] Use `NetworkClient` in `game/client/main.cpp` (#24), draw `entities()`
  (#23), send keyboard input (#17/#18).

## Game events and type table checks (#84, #82)

- [x] Public game events in `Events.hpp`: `EntitySpawned`, `EntityDestroyed`,
  `ShotFired`, `SoundPlayed`. On the server the game publishes them on the
  bus; `NetworkServer` queues each one for the sessions connected at that
  moment and sends the queue, in order, before the next snapshot. On the
  client `NetworkClient` publishes them (and `PlayerLeft`) on its bus.
- [x] `event/GameEvents.{hpp,cpp}`: public event ↔ `EventPayload`.
- [x] `ServerOptions::types` (public `ReplicatedType`) is sent in
  `connect_accept`; `start()` refuses an invalid table (`invalid_argument`).
- [x] `ClientConnection` keeps a `TypeTable`: snapshots and events before
  `connect_accept`, and snapshots or spawns with an unknown type id, are
  dropped and logged.
- [ ] Events are sent once: per-session queue and retransmission wait for
  acknowledgements (#86).
- [ ] The game declares its types (`ServerApp` passes none yet) and publishes
  the events, from Lua `spawn`/`destroy`/`play` once the server tick runs the
  scripts (#46).

## Network statistics (#87)

- [x] Sequences are per peer and assigned on the game thread
  (`Outbound::sequence`): each server session and the client count their
  own. `NetworkIo::send(Outbound)` no longer numbers packets.
- [x] `stats/TrafficMeter`: packets and bytes per second over 1 s windows of
  the engine clock, loss and reordering from sequences, latest round trip.
- [x] The server answers each client heartbeat; the client sends one every
  second even while sending inputs, and measures the round trip.
- [x] Public `NetworkStats` / `PlayerStats` (`Stats.hpp`):
  `NetworkServer::stats()`, `NetworkClient::stats()`, summary logged every
  10 s. Everything lives on the game thread: no lock.
- [ ] Round-trip time on the server side needs acknowledgements (#86).

### Next after #19/#20

- Wire `NetworkIo` + `ServerSessions` into `NetworkServer`, and `NetworkIo`
  + `ClientConnection` into `NetworkClient`, from the game loop (#42):
  each tick `poll()` → `handle` → `send`, then `drop_timed_out()` / `update()`.
- `player_entity_id` currently carries the session's player id (1 to 4): the
  game loop will spawn the player entity on `PlayerJoined`.
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

- [x] Start a dedicated network thread that receives UDP datagrams without blocking the game loop.
- [x] Decode and validate packets on the network thread.
- [x] Queue valid client inputs for the game thread.
- [ ] Apply queued inputs at the start of each fixed server tick.
- [ ] Send authoritative snapshots and game events to connected clients.
- [x] Track client endpoints and heartbeat deadlines.
- [x] Remove timed-out clients and notify the remaining clients.

## Client

- [ ] Send local input actions to the server over UDP.
- [ ] Receive and validate snapshots on a networking thread.
- [ ] Queue snapshots for the render/game thread.
- [ ] Apply authoritative snapshots without running authoritative gameplay logic.
- [ ] Handle server disconnection cleanly.

## Reliability And Performance

- [x] Use sequence numbers to discard duplicated and stale snapshots.
- [ ] Identify reliable messages such as connection, spawn, destruction, and disconnection.
- [ ] Define acknowledgement and retransmission behavior for reliable UDP messages.
- [ ] Keep packets below the path MTU to avoid fragmentation.
- [x] Lift the 84-entity snapshot limit (multi-packet snapshots or delta compression), and update `docs/protocol.md`.
- [ ] Measure packet rate, bandwidth use, latency, packet loss, and reordering.
- [ ] Evaluate client-side prediction and snapshot interpolation after the authoritative path works.

## Tests

- [x] Add protocol tests for valid headers and payloads.
- [x] Add protocol tests for truncated packets, invalid lengths, unknown types, and random bytes.
- [ ] Add deterministic transport tests with a fake transport instead of real network timing.
- [x] Add server tests for queued input processing and heartbeat timeouts.
- [x] Add client tests for stale, duplicated, and out-of-order snapshots.
- [ ] Add payload tests for every message: valid, truncated, oversized, out-of-range values, and random bytes.

## Documentation

- [x] Replace the Asio reference in `docs/PROJECT.md` with the socket-wrapper design.
- [ ] Document the server thread, game thread, queues, and ownership boundaries.
- [ ] Document how malformed packets and client crashes are handled.
- [x] Add build and run examples for `r-type_server` and `r-type_client` (README "Run").
