#include "application/ProjectPersistence.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QStandardPaths>

namespace safecrowd::application {
namespace {

constexpr auto kProjectFileName = "safecrowd-project.json";
constexpr auto kLayoutFileName = "layout.dxf";

QString projectFilePath(const QString& folderPath) {
    return QDir(folderPath).filePath(kProjectFileName);
}

QString recentProjectsPath() {
    const auto appData = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir().mkpath(appData);
    return QDir(appData).filePath("recent-projects.json");
}

ProjectMetadata fromJson(const QJsonObject& object) {
    return {
        .name = object.value("name").toString(),
        .folderPath = object.value("folderPath").toString(),
        .layoutPath = object.value("layoutPath").toString(),
        .savedAt = object.value("savedAt").toString(),
    };
}

QJsonObject toJson(const ProjectMetadata& metadata) {
    QJsonObject object;
    object["name"] = metadata.name;
    object["folderPath"] = metadata.folderPath;
    object["layoutPath"] = metadata.layoutPath;
    object["savedAt"] = metadata.savedAt;
    return object;
}

QJsonDocument readJsonDocument(const QString& path) {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        return {};
    }
    return QJsonDocument::fromJson(file.readAll());
}

bool writeJsonDocument(const QString& path, const QJsonDocument& document, QString* errorMessage) {
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        if (errorMessage != nullptr) {
            *errorMessage = file.errorString();
        }
        return false;
    }

    file.write(document.toJson(QJsonDocument::Indented));
    return true;
}

void upsertRecentProject(const ProjectMetadata& metadata) {
    QJsonArray projects;
    const auto recentPath = recentProjectsPath();
    const auto document = readJsonDocument(recentPath);
    if (document.isObject()) {
        projects = document.object().value("projects").toArray();
    }

    QJsonArray updated;
    updated.append(toJson(metadata));

    const auto normalizedFolder = QDir(metadata.folderPath).absolutePath();
    for (const auto& value : projects) {
        const auto existing = fromJson(value.toObject());
        if (QDir(existing.folderPath).absolutePath() == normalizedFolder) {
            continue;
        }
        updated.append(value);
    }

    QJsonObject root;
    root["projects"] = updated;
    QString ignoredError;
    writeJsonDocument(recentPath, QJsonDocument(root), &ignoredError);
}

bool copyLayoutIntoProject(ProjectMetadata& metadata, QString* errorMessage) {
    const auto sourcePath = QFileInfo(metadata.layoutPath).absoluteFilePath();
    const auto targetPath = QDir(metadata.folderPath).filePath(kLayoutFileName);

    if (QFileInfo(sourcePath).absoluteFilePath() == QFileInfo(targetPath).absoluteFilePath()) {
        metadata.layoutPath = targetPath;
        return true;
    }

    QFile::remove(targetPath);
    if (!QFile::copy(sourcePath, targetPath)) {
        if (errorMessage != nullptr) {
            *errorMessage = QString("Failed to copy layout file to %1.").arg(targetPath);
        }
        return false;
    }

    metadata.layoutPath = targetPath;
    return true;
}

}  // namespace

QList<ProjectMetadata> ProjectPersistence::loadRecentProjects() {
    QList<ProjectMetadata> projects;

    const auto document = readJsonDocument(recentProjectsPath());
    if (!document.isObject()) {
        return projects;
    }

    for (const auto& value : document.object().value("projects").toArray()) {
        const auto indexed = fromJson(value.toObject());
        const auto loaded = loadProject(indexed.folderPath);
        if (loaded.isValid()) {
            projects.push_back(loaded);
        }
    }

    return projects;
}

ProjectMetadata ProjectPersistence::loadProject(const QString& folderPath) {
    const auto document = readJsonDocument(projectFilePath(folderPath));
    if (!document.isObject()) {
        return {};
    }

    return fromJson(document.object());
}

bool ProjectPersistence::saveProject(ProjectMetadata metadata, QString* errorMessage) {
    if (!metadata.isValid()) {
        if (errorMessage != nullptr) {
            *errorMessage = "Project name, folder, and layout path are required.";
        }
        return false;
    }

    QDir folder(metadata.folderPath);
    if (!folder.exists() && !QDir().mkpath(metadata.folderPath)) {
        if (errorMessage != nullptr) {
            *errorMessage = QString("Failed to create project folder: %1").arg(metadata.folderPath);
        }
        return false;
    }

    if (!copyLayoutIntoProject(metadata, errorMessage)) {
        return false;
    }

    metadata.savedAt = QDateTime::currentDateTime().toString(Qt::ISODate);
    if (!writeJsonDocument(projectFilePath(metadata.folderPath), QJsonDocument(toJson(metadata)), errorMessage)) {
        return false;
    }

    upsertRecentProject(metadata);
    return true;
}

}  // namespace safecrowd::application
