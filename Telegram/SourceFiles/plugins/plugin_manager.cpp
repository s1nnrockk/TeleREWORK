/*
This file is part of Telegram Desktop,
the official desktop application of the Telegram messaging service.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#include "plugins/plugin_manager.h"

#include <QtCore/QDir>
#include <QtCore/QFile>
#include <QtCore/QJsonArray>
#include <QtCore/QJsonDocument>
#include <QtCore/QRegularExpression>
#include <QtCore/QSaveFile>
#include <QtCore/QStandardPaths>
#include <QtCore/QUrl>
#include <QtNetwork/QNetworkAccessManager>
#include <QtNetwork/QNetworkReply>
#include <QtNetwork/QNetworkRequest>

namespace Plugins {
namespace {

constexpr auto kMaxPackageBytes = 256 * 1024;
constexpr auto kMaxActions = 32;

[[nodiscard]] QString PackagePath(const QString &id) {
	return Directory() + '/' + id + ".tplugin";
}

[[nodiscard]] bool SafeId(const QString &id) {
	static const auto pattern = QRegularExpression(
		u"^[a-z0-9][a-z0-9._-]{2,95}$"_q);
	return pattern.match(id).hasMatch();
}

[[nodiscard]] bool AllowedPermission(const QString &permission) {
	static const auto allowed = QStringList{
		u"open_url"_q,
		u"clipboard.write"_q,
		u"theme.apply"_q,
	};
	return allowed.contains(permission);
}

[[nodiscard]] bool ValidAction(const QJsonObject &action) {
	const auto type = action.value(u"type"_q).toString();
	const auto id = action.value(u"id"_q).toString();
	const auto label = action.value(u"label"_q).toString();
	if (id.isEmpty() || id.size() > 64 || label.isEmpty() || label.size() > 120) {
		return false;
	}
	if (type == u"open_url"_q) {
		const auto url = QUrl(action.value(u"url"_q).toString());
		return url.isValid()
			&& url.scheme() == u"https"_q
			&& !url.host().isEmpty();
	} else if (type == u"copy_text"_q
		|| type == u"insert_text_template"_q) {
		const auto text = action.value(u"text"_q).toString();
		return !text.isEmpty() && text.size() <= 4000;
	} else if (type == u"theme_token"_q) {
		const auto token = action.value(u"token"_q).toString();
		return QStringList{
			u"glass"_q, u"dark"_q, u"light"_q, u"blue"_q, u"violet"_q
		}.contains(token);
	}
	return false;
}

[[nodiscard]] Result ValidateObject(const QJsonObject &root) {
	if (root.value(u"schema"_q).toString() != u"telerework.plugin"_q
		|| root.value(u"schema_version"_q).toInt() != 1) {
		return { false, u"Unsupported plugin format or schema version."_q };
	}
	const auto id = root.value(u"id"_q).toString();
	const auto name = root.value(u"name"_q).toString();
	const auto version = root.value(u"version"_q).toString();
	if (!SafeId(id) || name.isEmpty() || name.size() > 100
		|| version.isEmpty() || version.size() > 32) {
		return { false, u"Plugin metadata is incomplete or invalid."_q };
	}
	const auto permissions = root.value(u"permissions"_q).toArray();
	if (permissions.size() > 8) {
		return { false, u"Too many plugin permissions."_q };
	}
	for (const auto &value : permissions) {
		if (!value.isString() || !AllowedPermission(value.toString())) {
			return { false, u"Plugin requests an unknown permission."_q };
		}
	}
	const auto actions = root.value(u"actions"_q).toArray();
	if (actions.isEmpty() || actions.size() > kMaxActions) {
		return { false, u"Plugin must contain between 1 and 32 actions."_q };
	}
	for (const auto &value : actions) {
		if (!value.isObject() || !ValidAction(value.toObject())) {
			return { false, u"Plugin contains an unsupported or invalid action."_q };
		}
	}
	return { true, u"Plugin package is valid."_q, id };
}

} // namespace

QString Directory() {
	const auto root = QStandardPaths::writableLocation(
		QStandardPaths::AppDataLocation);
	return root + u"/TeleREWORK/plugins"_q;
}

int InstalledCount() {
	return InstalledIds().size();
}

QStringList InstalledIds() {
	auto result = QStringList();
	const auto directory = QDir(Directory());
	for (const auto &file : directory.entryList(
		{ u"*.tplugin"_q },
		QDir::Files,
		QDir::Name)) {
		result.push_back(file.chopped(QString(".tplugin").size()));
	}
	return result;
}

Result Validate(const QByteArray &bytes) {
	if (bytes.isEmpty() || bytes.size() > kMaxPackageBytes) {
		return { false, u"Plugin file is empty or exceeds the 256 KB limit."_q };
	}
	QJsonParseError error;
	const auto document = QJsonDocument::fromJson(bytes, &error);
	if (error.error != QJsonParseError::NoError || !document.isObject()) {
		return { false, u"Plugin file is not valid JSON."_q };
	}
	return ValidateObject(document.object());
}

Result Install(const QByteArray &bytes) {
	const auto checked = Validate(bytes);
	if (!checked.ok) {
		return checked;
	}
	if (!QDir().mkpath(Directory())) {
		return { false, u"Could not create the plugin directory."_q };
	}
	auto file = QSaveFile(PackagePath(checked.id));
	if (!file.open(QIODevice::WriteOnly)
		|| file.write(bytes) != bytes.size()
		|| !file.commit()) {
		return { false, u"Could not save the plugin package."_q };
	}
	return { true, u"Plugin installed. Review its permissions before enabling it."_q, checked.id };
}

Result Export(const QString &id, const QString &path) {
	if (!SafeId(id)) {
		return { false, u"Invalid plugin identifier."_q };
	}
	QFile source(PackagePath(id));
	if (!source.open(QIODevice::ReadOnly)) {
		return { false, u"Installed plugin was not found."_q };
	}
	const auto bytes = source.read(kMaxPackageBytes + 1);
	const auto checked = Validate(bytes);
	if (!checked.ok || checked.id != id) {
		return { false, u"Installed plugin failed validation."_q };
	}
	auto target = QSaveFile(path);
	if (!target.open(QIODevice::WriteOnly)
		|| target.write(bytes) != bytes.size()
		|| !target.commit()) {
		return { false, u"Could not export the plugin file."_q };
	}
	return { true, u"Plugin exported."_q, id };
}

Result SetEnabled(const QString &id, bool enabled) {
	if (!SafeId(id) || !QFile::exists(PackagePath(id))) {
		return { false, u"Installed plugin was not found."_q };
	}
	auto file = QSaveFile(Directory() + '/' + id + ".enabled");
	if (!file.open(QIODevice::WriteOnly)) {
		return { false, u"Could not update plugin state."_q };
	}
	const auto state = enabled ? QByteArray("1") : QByteArray("0");
	if (file.write(state) != state.size() || !file.commit()) {
		return { false, u"Could not update plugin state."_q };
	}
	return { true, enabled
		? u"Plugin enabled in the manager. Runtime actions are not yet exposed."_q
		: u"Plugin disabled."_q, id };
}

void GenerateWithLocalAI(const QString &prompt, Fn<void(Result)> done) {
	if (prompt.trimmed().isEmpty() || prompt.size() > 8000) {
		done({ false, u"Describe the plugin in 1–8000 characters."_q });
		return;
	}
	static auto manager = new QNetworkAccessManager(QCoreApplication::instance());
	auto request = QNetworkRequest(QUrl(u"http://127.0.0.1:11434/api/generate"_q));
	request.setHeader(QNetworkRequest::ContentTypeHeader, u"application/json"_q);
	const auto instructions = u"You generate TeleREWORK plugin JSON only. Return one JSON object matching schema telerework.plugin version 1. schema_version=1. Allowed actions only: open_url (https URL), copy_text (text), insert_text_template (text), theme_token (token glass/dark/light/blue/violet). permissions may only contain open_url, clipboard.write, theme.apply. Never include code, scripts, shell commands, network calls or file access. Every action needs id, type, label. Include id, name, version, description, author, permissions, actions. User request: "_q + prompt;
	const auto body = QJsonDocument(QJsonObject{
		{ u"model"_q, u"qwen2.5-coder:3b"_q },
		{ u"prompt"_q, instructions },
		{ u"stream"_q, false },
		{ u"format"_q, u"json"_q },
	}).toJson(QJsonDocument::Compact);
	auto reply = manager->post(request, body);
	QObject::connect(reply, &QNetworkReply::finished, [reply, done = std::move(done)]() mutable {
		const auto response = reply->readAll();
		const auto networkError = reply->error();
		const auto errorText = reply->errorString();
		reply->deleteLater();
		if (networkError != QNetworkReply::NoError) {
			done({ false, u"Local AI is unavailable. Install and start Ollama, then run: ollama pull qwen2.5-coder:3b. Details: "_q + errorText });
			return;
		}
		QJsonParseError parseError;
		const auto envelope = QJsonDocument::fromJson(response, &parseError);
		if (parseError.error != QJsonParseError::NoError || !envelope.isObject()) {
			done({ false, u"Local AI returned an invalid response."_q });
			return;
		}
		const auto generated = envelope.object().value(u"response"_q).toString().toUtf8();
		const auto installed = Install(generated);
		done(installed);
	});
}

} // namespace Plugins
