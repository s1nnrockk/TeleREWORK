# TeleREWORK Plugins & AI Studio

Status: first implementation committed to `fix/russia-connectivity`. The native manager, package validation, import/export, cloud AI generation and basic settings UI are present in source. GitHub Actions builds have not yet been confirmed successful.

## Product goal

A built-in Plugin Center where users can install, enable, disable, configure, import and export plugins. AI Studio turns a natural-language request into a validated plugin package in one generation request.

## Free AI strategy

The default provider is the hosted LLM7.io OpenAI-compatible endpoint `https://api.llm7.io/v1/chat/completions`, using its anonymous free-access route and the `gpt-oss:20b` model. No Ollama installation, local model download, account, or user API key is required. Anonymous access is rate-limited and capacity may change without notice; generation can fail when the upstream is busy or its free quota is exhausted. This is a third-party service, so plugin prompts are transmitted to that provider. The UI warns users not to include private chats, passwords, authentication data, or personal information.

Do not embed shared secret keys or paid credentials. Never claim that anonymous free capacity is unlimited or guaranteed. Keep the provider endpoint and model isolated in one function so they can be replaced if the service changes.

## Plugin package format

- Extension: `.tplugin`.
- A package is a UTF-8 JSON document with schema version, stable ID, name, version, description, author, permissions, and declarative actions.
- Export writes the exact validated package; import validates schema, ID, version, size, action count and permission declarations before installation.
- The first runtime generation deliberately supports declarative actions only. It must not execute arbitrary native code, shell commands, downloaded binaries, or unrestricted JavaScript.
- Unknown schema versions and unknown permissions are rejected, not silently accepted.
- Import never enables a plugin automatically; installation and enablement are separate user actions.

## Initial safe action model

Version 1 supports:
- `open_url`: open a validated HTTPS URL after a user click.
- `copy_text`: copy a bounded literal string after a user click.
- `insert_text_template`: expose a user-invoked text template; never send a message automatically.
- `theme_token`: schema-reserved allow-listed appearance token; execution is not enabled yet.
- `chat_action`: only explicitly implemented host actions, with a confirmation step for any action that changes chat state.

No plugin can read message history, contacts, authentication data, local files, or session keys by default. Future permissions must be narrow, visible, revocable and granted per plugin.

## AI generation flow

AI Studio is a plugin-only generator, not a general-purpose chat assistant. Out-of-scope prompts are rejected and do not produce conversational answers.

1. User describes the plugin in one prompt.
2. AI Studio sends only that prompt and the versioned schema to the free hosted AI endpoint.
3. The model returns one JSON package, not executable source code.
4. The app parses and validates the result locally; invalid output is rejected with actionable validation errors.
5. The user sees the manifest, requested permissions and preview.
6. User explicitly installs/enables it, or exports the `.tplugin` file.

AI output is untrusted input. It must never be installed or executed merely because the model returned it.

## Plugin Center UI

- Installed / Discover / AI Studio tabs.
- Plugin cards with icon, name, version, description, enabled state and permissions.
- Enable/disable, settings, uninstall, import and export actions.
- AI Studio prompt editor, model/provider status, generate, validation report, preview and export.
- Clear offline state when the cloud provider is unavailable or rate-limited.

## Storage and migration

Store user-installed packages and enabled state under the application data directory, separate from Telegram account data. Use atomic writes. Keep a backup before replacing an existing plugin version. Never sync plugin packages to Telegram chats automatically; sharing is an explicit export followed by the user's normal file-sharing flow.

## Required tests before release

- Valid and invalid package parsing.
- Duplicate IDs and version upgrades.
- Oversized package and excessive action count rejection.
- Unknown permission and schema rejection.
- Path traversal and archive extraction tests (if a future archive container is introduced).
- Atomic export/import and corrupt-file handling.
- AI endpoint timeout, malformed JSON, offline and model-not-found cases.
- Confirm that generated actions cannot execute arbitrary code or access private Telegram data.

## Current implementation boundary

Implemented in source:
- Native C++/Qt package validation, installation, storage and removal.
- `.tplugin` import/export with atomic writes and a 256 KB package limit.
- Enable/disable state stored separately from package contents.
- Anonymous cloud generation through LLM7.io (`gpt-oss:20b`); no user key or local model is required. Generated JSON is validated and installed disabled.
- Basic Advanced Settings UI for import, export, generation, plugin management, running supported actions and installing bundled catalog packages.
- User-invoked `open_url`, `copy_text` and `insert_text_template` actions. Text templates are copied to the clipboard for manual pasting.

Not yet implemented:
- A remote marketplace, automatic catalog updates, dedicated polished multi-tab Plugin Center window, plugin settings UI and rich action previews.
- Theme-token action execution and deeper Telegram host APIs/events.
- Sandboxed third-party native/JavaScript plugins. Arbitrary executable code is intentionally unsupported.
- Build verification. Check the GitHub Actions runs for the latest commit before distributing binaries.
