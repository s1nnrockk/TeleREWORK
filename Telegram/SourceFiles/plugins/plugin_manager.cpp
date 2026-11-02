/*
This file is part of Telegram Desktop,
the official desktop application of the Telegram messaging service.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#include "plugins/plugin_manager.h"

#include <QtCore/QCoreApplication>
#include <QtCore/QDir>
#include <QtCore/QFile>
#include <QtCore/QJsonArray>
#include <QtCore/QJsonDocument>
#include <QtCore/QRegularExpression>
#include <QtCore/QSaveFile>
#include <QtCore/QStandardPaths>
#include <QtCore/QUrl>
#include <QtGui/QClipboard>
#include <QtGui/QDesktopServices>
#include <QtGui/QGuiApplication>
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

[[nodiscard]] bool ValidAction(const QJsonObject &action, const QStringList &permissions) {
	const auto type = action.value(u"type"_q).toString();
	const auto id = action.value(u"id"_q).toString();
	const auto label = action.value(u"label"_q).toString();
	if (id.isEmpty() || id.size() > 64 || label.isEmpty() || label.size() > 120) {
		return false;
	}
	if (type == u"open_url"_q) {
		const auto url = QUrl(action.value(u"url"_q).toString());
		return permissions.contains(u"open_url"_q)
			&& url.isValid()
			&& url.scheme() == u"https"_q
			&& !url.host().isEmpty();
	} else if (type == u"copy_text"_q
		|| type == u"insert_text_template"_q) {
		const auto text = action.value(u"text"_q).toString();
		return !text.isEmpty()
			&& text.size() <= 4000
			&& ((type != u"copy_text"_q
				&& type != u"insert_text_template"_q)
				|| permissions.contains(u"clipboard.write"_q));
	} else if (type == u"theme_token"_q) {
		const auto token = action.value(u"token"_q).toString();
		return permissions.contains(u"theme.apply"_q)
			&& QStringList{
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
		if (!value.isObject() || !ValidAction(value.toObject(), [&] {
			auto result = QStringList();
			for (const auto &permission : permissions) {
				result.push_back(permission.toString());
			}
			return result;
		}())) {
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


QStringList EnabledIds() {
	auto result = QStringList();
	for (const auto &id : InstalledIds()) {
		if (IsEnabled(id)) {
			result.push_back(id);
		}
	}
	return result;
}

QStringList CatalogNames() {
	return { u"Quick Greeting Pack"_q, u"Meeting Follow-up"_q, u"Status Message Pack"_q };
}

Result InstallCatalog(const QString &name) {
	const auto package = [&]() -> QByteArray {
		if (name == u"Quick Greeting Pack"_q) return QByteArray(R"PLUGIN({"schema":"telerework.plugin","schema_version":1,"id":"org.telerework.catalog.greetings","name":"Quick Greeting Pack","version":"1.0.0","description":"Ready-to-copy friendly greetings.","author":"TeleREWORK Catalog","permissions":["clipboard.write"],"actions":[{"id":"hello","type":"copy_text","label":"Copy friendly hello","text":"Привет! Рад(а) тебя видеть 😊"},{"id":"thanks","type":"copy_text","label":"Copy thank you","text":"Спасибо большое! Очень ценю это."},{"id":"later","type":"copy_text","label":"Copy talk later","text":"Давай продолжим общение чуть позже!"}]})PLUGIN");
		if (name == u"Meeting Follow-up"_q) return QByteArray(R"PLUGIN({"schema":"telerework.plugin","schema_version":1,"id":"org.telerework.catalog.meeting-followup","name":"Meeting Follow-up","version":"1.0.0","description":"Copy a concise meeting follow-up template.","author":"TeleREWORK Catalog","permissions":["clipboard.write"],"actions":[{"id":"followup","type":"insert_text_template","label":"Copy follow-up template","text":"Спасибо за встречу!\n\nКраткие итоги:\n• \n\nСледующие шаги:\n• \n\nЕсли что-то упустил(а), пожалуйста, дополните."}]})PLUGIN");
		if (name == u"Status Message Pack"_q) return QByteArray(R"PLUGIN({"schema":"telerework.plugin","schema_version":1,"id":"org.telerework.catalog.status-messages","name":"Status Message Pack","version":"1.0.0","description":"Copy common status messages.","author":"TeleREWORK Catalog","permissions":["clipboard.write"],"actions":[{"id":"busy","type":"copy_text","label":"Copy busy status","text":"Сейчас занят(а), отвечу немного позже."},{"id":"focus","type":"copy_text","label":"Copy focus status","text":"Сейчас сосредоточен(а) на задаче. Напишу, когда освобожусь."},{"id":"offline","type":"copy_text","label":"Copy offline status","text":"Я пока не у компьютера. Вернусь и отвечу."}]})PLUGIN");
		return QByteArray();
	}();
	if (package.isEmpty()) return { false, u"Catalog item not found."_q };
	return Install(package);
}

QStringList ActionLabels(const QString &id) {
	auto result = QStringList();
	if (!SafeId(id) || !IsEnabled(id)) {
		return result;
	}
	QFile file(PackagePath(id));
	if (!file.open(QIODevice::ReadOnly)) {
		return result;
	}
	const auto document = QJsonDocument::fromJson(file.read(kMaxPackageBytes + 1));
	if (!document.isObject() || !ValidateObject(document.object()).ok) {
		return result;
	}
	for (const auto &value : document.object().value(u"actions"_q).toArray()) {
		const auto action = value.toObject();
		const auto type = action.value(u"type"_q).toString();
		if (type == u"open_url"_q
			|| type == u"copy_text"_q
			|| type == u"insert_text_template"_q) {
			result.push_back(action.value(u"label"_q).toString());
		}
	}
	return result;
}

Result ExecuteAction(const QString &id, const QString &label) {
	if (!SafeId(id) || !IsEnabled(id)) {
		return { false, u"Enable this plugin before running its actions."_q };
	}
	QFile file(PackagePath(id));
	if (!file.open(QIODevice::ReadOnly)) {
		return { false, u"Plugin package could not be read."_q };
	}
	const auto document = QJsonDocument::fromJson(file.read(kMaxPackageBytes + 1));
	if (!document.isObject()) {
		return { false, u"Plugin package is invalid."_q };
	}
	const auto root = document.object();
	if (!ValidateObject(root).ok) {
		return { false, u"Plugin package failed validation."_q };
	}
	for (const auto &value : root.value(u"actions"_q).toArray()) {
		const auto action = value.toObject();
		if (action.value(u"label"_q).toString() != label) {
			continue;
		}
		const auto type = action.value(u"type"_q).toString();
		if (type == u"open_url"_q) {
			const auto url = QUrl(action.value(u"url"_q).toString());
			if (!url.isValid() || url.scheme() != u"https"_q || url.host().isEmpty()) {
				return { false, u"Only valid HTTPS links can be opened."_q };
			}
			return QDesktopServices::openUrl(url)
				? Result{ true, u"Link opened."_q, id }
				: Result{ false, u"Could not open this link."_q, id };
		} else if (type == u"copy_text"_q
			|| type == u"insert_text_template"_q) {
			QGuiApplication::clipboard()->setText(
				action.value(u"text"_q).toString());
			return { true, u"Text copied. Paste it where you need it."_q, id };
		}
	}
	return { false, u"Action not found or not supported."_q, id };
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

bool IsEnabled(const QString &id) {
	if (!SafeId(id) || !QFile::exists(PackagePath(id))) {
		return false;
	}
	QFile state(Directory() + '/' + id + ".enabled");
	if (!state.open(QIODevice::ReadOnly)) {
		return false;
	}
	return state.readAll() == QByteArray("1");
}

Result Remove(const QString &id) {
	if (!SafeId(id)) {
		return { false, u"Invalid plugin identifier."_q };
	}
	if (!QFile::remove(PackagePath(id))) {
		return { false, u"Could not remove the plugin package."_q };
	}
	QFile::remove(Directory() + '/' + id + ".enabled");
	return { true, u"Plugin removed."_q, id };
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
		? u"Plugin enabled. You can now run its supported actions from Plugin Center."_q
		: u"Plugin disabled."_q, id };
}

void GenerateWithAI(const QString &prompt, std::function<void(Result)> done) {
	if (prompt.trimmed().isEmpty() || prompt.size() > 8000) {
		done({ false, u"Describe the plugin in 1–8000 characters."_q });
		return;
	}
	static auto manager = new QNetworkAccessManager(QCoreApplication::instance());
	auto request = QNetworkRequest(QUrl(u"https://api.llm7.io/v1/chat/completions"_q));
	request.setHeader(QNetworkRequest::ContentTypeHeader, u"application/json"_q);
	request.setRawHeader("User-Agent", "TeleREWORK/PluginStudio");
	const auto system = u"You are a strict plugin-manifest generator for TeleREWORK AI Studio, not a general chatbot. Your ONLY task is to create a plugin package from a request specifically about a TeleREWORK plugin. Do not answer questions, chat, explain unrelated topics, write essays, or provide general assistance. If the user request is not clearly asking to create or modify a TeleREWORK plugin, return exactly PLUGIN_SCOPE_REJECTED and nothing else. For in-scope requests, return ONLY one JSON object matching schema telerework.plugin, schema_version 1. Required fields: id, name, version, description, author, permissions, actions. Allowed actions: open_url with HTTPS url; copy_text with text; insert_text_template with text; theme_token with token glass/dark/light/blue/violet. open_url requires permission open_url. copy_text and insert_text_template require clipboard.write. theme_token requires theme.apply. Permissions may only be open_url, clipboard.write, theme.apply. Each action needs id, type, label. Never include scripts, source code, shell commands, network calls, file access, credentials, or private Telegram data. Keep the manifest small and valid."_q;
	const auto messages = QJsonArray{
		QJsonObject{ { u"role"_q, u"system"_q }, { u"content"_q, system } },
		QJsonObject{ { u"role"_q, u"user"_q }, { u"content"_q, prompt } },
	};
	const auto body = QJsonDocument(QJsonObject{
		{ u"model"_q, u"gpt-oss:20b"_q },
		{ u"messages"_q, messages },
		{ u"temperature"_q, 0.1 },
		{ u"max_tokens"_q, 2200 },
	}).toJson(QJsonDocument::Compact);
	auto reply = manager->post(request, body);
	QObject::connect(reply, &QNetworkReply::finished, [reply, done = std::move(done)]() mutable {
		const auto response = reply->readAll();
		const auto networkError = reply->error();
		const auto errorText = reply->errorString();
		const auto status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
		reply->deleteLater();
		if (networkError != QNetworkReply::NoError || status < 200 || status >= 300) {
			done({ false, u"Free cloud AI is temporarily unavailable or rate-limited. Try again later. Network: "_q + errorText + u" (HTTP "_q + QString::number(status) + u")."_q });
			return;
		}
		QJsonParseError parseError;
		const auto envelope = QJsonDocument::fromJson(response, &parseError);
		if (parseError.error != QJsonParseError::NoError || !envelope.isObject()) {
			done({ false, u"Cloud AI returned an invalid response."_q });
			return;
		}
		const auto choices = envelope.object().value(u"choices"_q).toArray();
		if (choices.isEmpty() || !choices.front().isObject()) {
			done({ false, u"Cloud AI returned no generated plugin. The free service may be busy."_q });
			return;
		}
		const auto generatedText = choices.front().toObject()
			.value(u"message"_q).toObject()
			.value(u"content"_q).toString().trimmed();
		if (generatedText == u"PLUGIN_SCOPE_REJECTED"_q) {
			done({ false, u"AI Studio only creates TeleREWORK plugins. Describe a plugin you want to build."_q });
			return;
		}
		const auto generated = generatedText.toUtf8();
		const auto checked = Validate(generated);
		if (!checked.ok) {
			done({ false, u"AI output failed plugin validation: "_q + checked.message });
			return;
		}
		auto installed = Install(generated);
		if (installed.ok) {
			installed.message = u"Plugin generated by free cloud AI and installed disabled. Review its permissions before enabling."_q;
		}
		done(installed);
	});
}

} // namespace Plugins
