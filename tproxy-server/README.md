# Telegram Desktop Web Proxy relay (experimental)

This is the hosted relay component for the WEB proxy transport. It is **not a public relay** and is not ready to connect until you deploy it behind HTTPS and configure a reachable stock MTProxy backend.

## Requirements

- Node.js 20+
- A publicly reachable HTTPS hostname with a valid TLS certificate
- One fixed, reachable stock MTProxy endpoint (hostname and port)
- Reverse proxy routing the configured base path to this Node service
- A WEB proxy entry in the patched Telegram Desktop using the same hostname, optional base-path slug, and MTProxy secret

## Configure

Set these environment variables:

- `LISTEN_ADDR` — bind address, default `127.0.0.1:8080`
- `PUBLIC_HOST` — canonical public relay hostname used for capability derivation (required in practice; must match the hostname entered in Telegram)
- `PUBLIC_BASE_PATH` — optional single slug, without slashes
- `MTPROXY_HOST` — fixed stock MTProxy hostname
- `MTPROXY_PORT` — MTProxy port, default `443`
- `MTPROXY_SECRET_HEX` — decoded MTProxy secret bytes as lowercase/uppercase hex, including leading `dd` byte for random-padding secrets
- `SESSION_TTL` — session lifetime in seconds, clamped to 60–3600; default 900

The relay does not accept arbitrary destinations. Every OPEN connects only to `MTPROXY_HOST:MTPROXY_PORT`.

## Run locally

```sh
npm test
MTPROXY_HOST=your-mtproxy.example MTPROXY_SECRET_HEX=00112233445566778899aabbccddeeff PUBLIC_HOST=relay.example npm start
```

Local HTTP is for development only. Production must terminate TLS at a reverse proxy and route the entire root or configured base-path prefix to this service. The configured path must be the same one entered in the client.

## Deploy with Docker Compose

See [DEPLOY.md](./DEPLOY.md) for a VPS deployment using Docker Compose and Caddy automatic HTTPS. The template keeps the relay private on the Docker network and publishes only ports 80/443.

## Status

Capability vectors are covered by automated tests. Deployment, browser bridge compatibility, relay flow control, and end-to-end Telegram connectivity still require an actual HTTPS host and MTProxy endpoint; passing unit tests alone does not establish service availability.
