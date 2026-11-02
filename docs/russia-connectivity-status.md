# Russia connectivity work: deployment status

This repository contains a Telegram Desktop client-side WEB proxy transport. It does
not contain a deployed relay, and a client change alone cannot make Telegram reachable
when the direct Telegram endpoints are blocked by the network.

## Current branch

Work is isolated on `fix/russia-connectivity`.

## Safe implementation sequence

1. Keep the stock MTProto protocol and credentials handling in the client.
2. Deploy a relay on an operator-controlled host with valid HTTPS. The relay must only
   dial a configured local MTProxy backend; it must not accept arbitrary destinations.
3. Implement the versioned frame protocol in `docs/web-proxy-plan.md`, including
   session authentication, replay protection, stream multiplexing, flow control,
   limits, and orderly shutdown.
4. Configure the client with the relay hostname and MTProxy secret through the existing
   WEB proxy settings. Never embed server credentials or a shared relay secret in the
   public client binary.
5. Run the release gate in `docs/web-proxy-test-plan.md` on Russian networks with
   VPN disabled. Record sanitized results only.

## Deployment blockers

- No relay hostname or server is configured in this repository.
- No server deployment credentials are available to this workflow.
- The client-side test plan requires a real compatible relay before end-to-end testing.

## Important limitation

A self-hosted relay can provide an alternate route only if the relay's host and its
upstream MTProxy are reachable from the user's network. It cannot guarantee access
from every Russian ISP or network, and it is not a substitute for testing against
actual current blocking conditions.
