# Self-hosted relay deployment

This is a deployment template, not a hosted service. It requires an operator-owned
Linux VPS, a DNS hostname pointing to that VPS, and a working stock MTProxy endpoint.
No public relay or server credentials are included in this repository.

## Requirements

- Linux VPS with Docker Engine and the Docker Compose plugin
- Public DNS A/AAAA record for the relay hostname
- Inbound TCP 80 and 443 (UDP 443 is optional for HTTP/3)
- A reachable, fixed MTProxy host/port and its decoded secret
- A hostname you control; Caddy obtains and renews the HTTPS certificate

## Deploy

1. Copy this directory to the VPS and enter it.
2. Copy `.env.example` to `.env`.
3. Set `PUBLIC_HOST`, `ACME_EMAIL`, `MTPROXY_HOST`, `MTPROXY_PORT`,
   and `MTPROXY_SECRET_HEX`. The secret must be hex-encoded decoded MTProxy
   secret bytes; include the leading `dd` byte when applicable.
4. Ensure DNS points to this VPS and ports 80/443 are reachable.
5. Run:

   ```sh
   docker compose up -d --build
   docker compose ps
   docker compose logs --tail=100 caddy relay
   ```

6. Verify `https://YOUR_PUBLIC_HOST/` returns the ordinary-site page and
   `https://YOUR_PUBLIC_HOST/healthz` returns `{"status":"ok"}`.
7. In TeleREWORK, add a WEB proxy with the same hostname and the original MTProxy
   secret (not the hex representation unless the UI asks for hex).

## Operations

- Update: `git pull`, then `docker compose up -d --build`.
- Stop: `docker compose down`.
- Caddy certificate state is stored in named Docker volumes.
- Keep `.env` private and back it up securely. Never put relay secrets in GitHub
  Actions logs or client binaries.
- The relay currently keeps sessions in memory; restarting it disconnects active
  sessions. It is not production-proven and must pass the documented end-to-end test
  plan before being advertised as reliable.

## Important

This does not provision a VPS, register a domain, or create an MTProxy account.
Those require external provider accounts and credentials that are not available to
the repository workflow. Do not deploy an unauthenticated relay to a shared host.
