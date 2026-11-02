/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#pragma once

#include <QtCore/QString>
#include <QtCore/QByteArray>
#include <QtCore/QJsonObject>
#include <QtCore/QStringList>

#include <functional>

namespace Plugins {

struct Result {
	bool ok = false;
	QString message;
	QString id;
};

[[nodiscard]] QString Directory();
[[nodiscard]] int InstalledCount();
[[nodiscard]] Result Validate(const QByteArray &bytes);
[[nodiscard]] Result Install(const QByteArray &bytes);
[[nodiscard]] Result Export(const QString &id, const QString &path);
[[nodiscard]] Result SetEnabled(const QString &id, bool enabled);
[[nodiscard]] bool IsEnabled(const QString &id);
[[nodiscard]] Result Remove(const QString &id);
[[nodiscard]] QStringList InstalledIds();

// Uses a local Ollama-compatible endpoint. No API key or paid provider is used.
void GenerateWithLocalAI(
	const QString &prompt,
	Fn<void(Result)> done);

} // namespace Plugins
