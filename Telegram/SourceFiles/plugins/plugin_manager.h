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
[[nodiscard]] QStringList EnabledIds();
[[nodiscard]] QStringList CatalogNames();
[[nodiscard]] Result InstallCatalog(const QString &name);
[[nodiscard]] QStringList ActionLabels(const QString &id);
[[nodiscard]] Result ExecuteAction(const QString &id, const QString &label);

// Uses a hosted anonymous free-tier endpoint. No local model or user API key is required.
void GenerateWithAI(
	const QString &prompt,
	std::function<void(Result)> done);

} // namespace Plugins
