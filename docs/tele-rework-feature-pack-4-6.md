# TeleREWORK feature pack: Effects Center, Quick Switcher, Favorites

Target branch: `fix/russia-connectivity`

This document defines the next three user-facing features. It is an implementation contract, not a claim that the UI is already shipped.

## 4. Effects Center

Add a dedicated **Effects Center** entry under Advanced settings. It should collect controls that are currently scattered across animation and power-saving settings.

Initial controls:
- Master switch: enable/disable decorative UI effects.
- Message effects: animated emoji/reaction and message-send effects.
- Interface motion: panel transitions and navigation animations.
- Background effects: particles, blur and other decorative background motion.
- Reduced-motion mode: disables nonessential motion without changing message content or network behavior.
- Preview area with a small live sample and a one-click reset to defaults.

Rules:
- Persist preferences locally through the existing application/session settings mechanisms.
- Do not introduce a second source of truth for an existing preference.
- Master-off and reduced-motion must take precedence over individual effect toggles.
- Respect platform accessibility settings where available.
- Keep a lightweight path for low-end hardware.

Acceptance checks:
- Every toggle has an observable effect and survives restart.
- Reset restores defaults.
- No effect setting changes message delivery, notifications, or protocol behavior.
- Search in Settings can find every control.

## 5. Quick Switcher

Add a quick chat switcher opened with `Ctrl+K` (and `Cmd+K` on macOS). The first implementation reuses the existing recent-chat switch overlay; searchable filtering is a follow-up implementation task.

Behavior:
- Opens a compact centered overlay without navigating away from the current chat.
- Current first slice: open the existing recent-chat switch overlay and cycle recent chats using its existing keyboard controls.
- Follow-up: add an inline search field for chat title, username and phone/contact name using already-loaded local dialog data.
- Show avatar, title, last-message preview and unread marker where available.
- Keyboard: Up/Down changes selection, Enter opens, Esc closes.
- Recent chats appear when the query is empty; matching results update as the user types.
- Selecting a result opens the chat through the existing session/window navigation path.
- Do not issue network requests for every keystroke; use local indexed data and existing search facilities.
- Do not intercept the shortcut while the user is typing in a text editor/input where the shortcut is already owned by that control.

Acceptance checks:
- Current first slice works with the existing keyboard-only recent-chat switch flow.
- Follow-up acceptance: searchable filtering, result preview and empty-state behavior.
- Correctly handles empty results, deleted chats and multiple accounts.
- Does not duplicate existing global search behavior or steal its shortcut unexpectedly.
- Selection returns focus to the chat composer only after navigation completes.

## 6. Favorites panel

Add a local **Favorites** panel for quick access to saved items. It is separate from Telegram's server-side pinned messages and must not silently pin messages in a chat.

Supported item types:
- Chat or channel.
- Message reference (peer + message id).
- Link.
- File/document reference already present in local message history.

Behavior:
- Add/remove from the item's context menu.
- Dedicated Favorites entry in the main navigation/sidebar.
- Groups: Chats, Messages, Links, Files.
- Optional user-defined title and note.
- Reorder items and remove them without deleting the original chat/message/file.
- If an item is no longer available, show it as unavailable and allow removal.
- Store references and metadata locally; never copy private message content to a remote service.
- Export/import through the existing `.tcfg` configuration pipeline only after explicit user action; imported entries are previewed before merge.

Acceptance checks:
- Favorites survive app restart and account switching.
- A favorite message opens the original chat and jumps to the message when it still exists.
- Removing a favorite never deletes the source item.
- Import conflicts are previewed and resolved without overwriting existing favorites silently.

## Integration order

1. Quick Switcher: smallest isolated interaction surface; reuse existing dialog/session navigation.
2. Effects Center: consolidate existing settings before adding new persisted flags.
3. Favorites: introduce a versioned local data model and migration tests before wiring context menus.

## Required verification

For each implementation commit:
- Build the affected target on the supported Windows configuration.
- Run focused unit tests and formatting/lint checks available in CI.
- Inspect the resulting GitHub Actions run; do not report a feature as complete based only on a successful commit.
- Keep all changes on `fix/russia-connectivity` until a reviewable feature branch/PR is prepared.
