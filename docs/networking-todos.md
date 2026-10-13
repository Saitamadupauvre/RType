# Networking TODOs

## Protocol

- [x] Define the packet header: magic, protocol version, message type, sequence number, and payload length.
- [x] Define a fixed byte order for every multibyte field.
- [x] Implement explicit packet serialization without sending C++ object memory directly.
- [x] Implement packet decoding with size and bounds validation before every read.
- [x] Reject truncated packets, invalid magic values, unsupported versions, unknown message types, and mismatched payload lengths.
- [x] Define messages for connection, input, snapshots, entity events, heartbeat, and disconnection.
- [x] Document the protocol header and every message in `docs/protocol.md`.
- [x] Encode and decode the `input` payload.
- [ ] Encode and decode the `connect_request` payload (4-byte client nonce).
- [ ] Encode and decode the `connect_accept` payload, rejecting duplicate type IDs or names, names above 63 bytes, and truncated entries.
- [ ] Encode and decode the `snapshot` payload, rejecting sizes other than `6 + entity_count * 14`.
- [ ] Validate snapshot and spawn `type_id` values against the connection type table.
- [ ] Encode and decode the `event` payloads (`spawn`, `destroy`, `fire`, `sound`), rejecting unknown types and wrong fixed sizes.
- [ ] Encode and decode the `disconnect` payload, rejecting undefined reasons.
- [ ] Reject non-empty `heartbeat` payloads.

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
