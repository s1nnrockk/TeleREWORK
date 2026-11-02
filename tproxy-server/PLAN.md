# Hosted WEB proxy relay

This directory is the server-side counterpart to the Telegram Desktop WEB proxy.
The relay is deliberately **not** a general-purpose proxy: it accepts opaque,
already-MTProxy-protected bytes and can dial only one configured MTProxy endpoint.

## Current status

The client protocol is documented in `../docs/web-proxy-plan.md`. The server must
implement the v1 bridge and frame contract there before the client can be considered
usable. No production hostname, TLS certificate, MTProxy endpoint, or deployment
credentials are committed here. Do not advertise a working VPN-free connection
until a deployment has passed the end-to-end matrix in
`../docs/web-proxy-test-plan.md`.

## Required implementation

- HTTPS-only deployment behind a reverse proxy; preserve a normal-site response for
  invalid or absent bridge capabilities.
- Derive and constant-time compare the bridge capability using the exact v1/v2
  HMAC context and normative vectors in the client plan.
- Bootstrap with a fresh nonce and exact-origin `MessageChannel`; never expose the
  MTProxy secret to browser JavaScript.
- `POST /api/v1/session`, `POST /api/v1/up`, `POST /api/v1/down`, and
  `GET /api/v1/ws` under the optional base path. A session bearer is random,
  short-lived, scoped to the bridge capability, and stored hashed.
- Parse the shared 8-byte frame header, reject malformed/oversized frames, and
  implement HELLO/WELCOME, OPEN/DATA/CLOSE/WINDOW/PING/PONG/BYE, implicit 4 MiB
  stream windows, per-session limits, and bounded queues.
- OPEN creates a TCP connection only to the configured MTProxy host and port.
  No client-supplied hostname, port, DNS name, or arbitrary destination is accepted.
- Never log secrets, bearer tokens, or frame payloads. Apply rate limits, connection
  caps, idle timeouts, and cleanup on disconnect.
- Use ordered, deduplicated polling or WebSocket delivery with explicit cursor
  semantics; do not silently drop DATA.

## Deployment configuration

Required environment variables:

- `LISTEN_ADDR` (default `127.0.0.1:8080`; keep behind TLS reverse proxy)
- `PUBLIC_BASE_PATH` (empty or canonical ASCII slug)
- `MTProxy_HOST` (one fixed backend hostname)
- `MTProxy_PORT` (one fixed backend port)
- `BRIDGE_SECRET` (deployment-only high-entropy secret for session-token derivation)
- `SESSION_TTL` (short bounded lifetime)

The reverse proxy must provide HTTPS, HSTS, request-size limits, and a CSP that
allows the numeric loopback iframe origin used by the explicitly requested browser
fallback. Do not use `X-Frame-Options: DENY` or `SAMEORIGIN` on the bridge page.

## Verification gate

Before deployment, test all normative HMAC vectors and frame-parser cases, then run
the full client test plan on Windows, macOS, and Linux. Verify that OPEN cannot
reach any destination except the configured MTProxy, invalid capabilities receive
ordinary-site content, and all queues/session counts remain bounded under load.
