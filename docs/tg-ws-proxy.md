# TG WS Proxy integration

TeleREWORK can optionally launch a local TG WS Proxy runtime on Windows.

## How it works

- The Windows build creates a command-line runtime from the pinned upstream source at Flowseal/tg-ws-proxy commit `2085861957070eeea07990a522b29f42e5230210`.
- On startup, when **Settings → Advanced → TG WS Proxy** is enabled, TeleREWORK starts `TgWsProxy.exe` bound only to `127.0.0.1:1443`.
- A fresh random MTProto secret is generated for that local listener. TeleREWORK configures its built-in MTProto client to use the local listener.
- When the setting is disabled, the stale local proxy entry is removed at the next start.
- The helper process is stopped when TeleREWORK exits.

The runtime is built from source in the Windows GitHub Actions workflow; the upstream GUI/tray executable is not bundled or launched.

## Limitations

- The integrated runtime is currently enabled on Windows builds only.
- If another proxy is already selected, TeleREWORK leaves it untouched and does not override it.
- Changing the setting requires restarting TeleREWORK.
- The integration does not guarantee that a WebSocket route is reachable on every network. It uses the upstream project's transport and fallback behavior; network filtering and upstream endpoint availability can still affect connectivity.
- The first build containing this integration must complete successfully before the runtime is present beside `Telegram.exe`.

## Upstream and license

- Project: https://github.com/Flowseal/tg-ws-proxy
- Pinned source: https://github.com/Flowseal/tg-ws-proxy/tree/2085861957070eeea07990a522b29f42e5230210
- License: MIT (see upstream `LICENSE`).

This component is separate from Telegram Desktop and retains its upstream copyright and license.
