#include "application/ProjectPersistence.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <vector>

#include <QDateTime>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QStandardPaths>
#include <QStorageInfo>

#include "application/LayoutReviewCodec.h"
#include "application/ProjectMetadataCodec.h"
#include "application/WorkspaceStateCodec.h"
#include "domain/ImportValidationService.h"

namespace safecrowd::application {
namespace {

constexpr auto kProjectFileName = "safecrowd-project.json";
constexpr auto kLayoutFileName = "layout.dxf";
constexpr auto kReviewFileName = "layout-review.json";
constexpr auto kImportArtifactsFileName = "import-artifacts.json";
constexpr auto kWorkspaceFileName = "workspace-state.json";

bool isProjectManagedEntry(const QString& fileName) {
    return fileName.compare(kProjectFileName, Qt::CaseInsensitive) == 0
        || fileName.compare(kLayoutFileName, Qt::CaseInsensitive) == 0
        || fileName.compare(kReviewFileName, Qt::CaseInsensitive) == 0
        || fileName.compare(kImportArtifactsFileName, Qt::CaseInsensitive) == 0
        || fileName.compare(kWorkspaceFileName, Qt::CaseInsensitive) == 0;
}

QString normalizedFolderKey(const QString& folderPath) {
    const auto canonical = QFileInfo(folderPath).canonicalFilePath();
    const auto base = canonical.isEmpty() ? QDir(folderPath).absolutePath() : canonical;
#if defined(Q_OS_WIN) || defined(Q_OS_MAC)
    return base.toLower();
#else
    return base;
#endif
}

bool sameFolderPath(const QString& lhs, const QString& rhs) {
    return normalizedFolderKey(lhs) == normalizedFolderKey(rhs);
}

QString projectFilePath(const QString& folderPath) {
    return QDir(folderPath).filePath(kProjectFileName);
}

QString reviewFilePath(const QString& folderPath) {
    return QDir(folderPath).filePath(kReviewFileName);
}

QString importArtifactsFilePath(const QString& folderPath) {
    return QDir(folderPath).filePath(kImportArtifactsFileName);
}

QString workspaceFilePath(const QString& folderPath) {
    return QDir(folderPath).filePath(kWorkspaceFileName);
}

QString recentProjectsPath() {
    const auto appData = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir().mkpath(appData);
    return QDir(appData).filePath("recent-projects.json");
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
    updated.append(projectMetadataToJson(metadata));

    for (const auto& value : projects) {
        const auto existing = projectMetadataFromJson(value.toObject());
        if (sameFolderPath(existing.folderPath, metadata.folderPath)) {
            continue;
        }
        updated.append(value);
    }

    QJsonObject root;
    root["projects"] = updated;
    QString ignoredError;
    writeJsonDocument(recentPath, QJsonDocument(root), &ignoredError);
}

void removeRecentProject(const QString& folderPath) {
    const auto recentPath = recentProjectsPath();
    const auto document = readJsonDocument(recentPath);
    if (!document.isObject()) {
        return;
    }

    QJsonArray updated;
    for (const auto& value : document.object().value("projects").toArray()) {
        const auto existing = projectMetadataFromJson(value.toObject());
        if (sameFolderPath(existing.folderPath, folderPath)) {
            continue;
        }
        updated.append(value);
    }

    QJsonObject root;
    root["projects"] = updated;
    QString writeError;
    if (!writeJsonDocument(recentPath, QJsonDocument(root), &writeError)) {
        qWarning() << "Failed to update recent projects list:" << writeError;
    }
}

bool canDeleteProjectFolder(const QString& folderPath, QString* errorMessage) {
    const auto setError = [errorMessage](const QString& message) {
        if (errorMessage != nullptr) {
            *errorMessage = message;
        }
    };

    QDir folder(folderPath);
    if (!folder.exists()) {
        setError(QString("Project folder does not exist: %1").arg(folderPath));
        return false;
    }

    const QFileInfo folderInfo(folder.absolutePath());
    if (folderInfo.isSymLink() || folderInfo.isJunction()) {
        setError(QString("Refusing to delete a symbolic link or junction: %1").arg(folderPath));
        return false;
    }

    if (QDir(folderInfo.absoluteFilePath()).isRoot()) {
        setError(QString("Refusing to delete a drive root: %1").arg(folderPath));
        return false;
    }

    const QStorageInfo storage(folderInfo.absoluteFilePath());
    if (storage.isValid() && !storage.rootPath().isEmpty()
        && QFileInfo(storage.rootPath()).absoluteFilePath() == folderInfo.absoluteFilePath()) {
        setError(QString("Refusing to delete a volume root: %1").arg(folderPath));
        return false;
    }

    const auto entries = folder.entryInfoList(
        QDir::AllEntries | QDir::Hidden | QDir::System | QDir::NoDotAndDotDot,
        QDir::Name);
    for (const auto& entry : entries) {
        if (entry.isSymLink() || entry.isJunction()) {
            setError(QString(
                "Refusing to delete project folder because it contains a link not created by SafeCrowd: %1")
                .arg(entry.fileName()));
            return false;
        }
        if (!entry.isFile() || !isProjectManagedEntry(entry.fileName())) {
            setError(QString(
                "Refusing to delete project folder because it contains a file or folder not created by SafeCrowd: %1")
                .arg(entry.fileName()));
            return false;
        }
    }

    return true;
}

// Verifies the *location* of a folder being used to save a SafeCrowd project.
// Rejects symlinks/junctions and drive/volume roots. Used by the save path;
// the delete path keeps its own checks for now.
bool validateProjectFolderLocation(const QString& folderPath, QString* errorMessage) {
    const auto setError = [errorMessage](const QString& message) {
        if (errorMessage != nullptr) {
            *errorMessage = message;
        }
    };

    if (folderPath.isEmpty()) {
        setError("Project folder path is empty.");
        return false;
    }

    const QFileInfo folderInfo(QDir(folderPath).absolutePath());
    if (folderInfo.isSymLink() || folderInfo.isJunction()) {
        setError(QString("Refusing to save into a symbolic link or junction: %1").arg(folderPath));
        return false;
    }

    if (QDir(folderInfo.absoluteFilePath()).isRoot()) {
        setError(QString("Refusing to save into a drive root: %1").arg(folderPath));
        return false;
    }

    const QStorageInfo storage(folderInfo.absoluteFilePath());
    if (storage.isValid() && !storage.rootPath().isEmpty()
        && QFileInfo(storage.rootPath()).absoluteFilePath() == folderInfo.absoluteFilePath()) {
        setError(QString("Refusing to save into a volume root: %1").arg(folderPath));
        return false;
    }

    return true;
}

// Verifies that `folderPath` is safe to receive a SafeCrowd project save.
// The folder may not yet exist (it will be created); if it exists, it must
// be either empty or contain only SafeCrowd-managed files.
bool canSaveIntoProjectFolder(const QString& folderPath, QString* errorMessage) {
    const auto setError = [errorMessage](const QString& message) {
        if (errorMessage != nullptr) {
            *errorMessage = message;
        }
    };

    if (!validateProjectFolderLocation(folderPath, errorMessage)) {
        return false;
    }

    QDir folder(folderPath);
    if (!folder.exists()) {
        return true;
    }

    const auto entries = folder.entryInfoList(
        QDir::AllEntries | QDir::Hidden | QDir::System | QDir::NoDotAndDotDot,
        QDir::Name);
    for (const auto& entry : entries) {
        if (entry.isSymLink() || entry.isJunction()) {
            setError(QString(
                "Refusing to save into a folder that contains a link not created by SafeCrowd: %1")
                .arg(entry.fileName()));
            return false;
        }
        if (!entry.isFile() || !isProjectManagedEntry(entry.fileName())) {
            setError(QString(
                "Refusing to save into a folder that contains a file or folder not created by SafeCrowd: %1\n\n"
                "Please choose an empty folder, or a folder that already contains a SafeCrowd project.")
                .arg(entry.fileName()));
            return false;
        }
    }

    return true;
}

bool copyLayoutIntoProject(ProjectMetadata& metadata, QString* errorMessage) {
    if (metadata.isBlankLayoutProject()) {
        return true;
    }

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


bool isLiveValidationIssue(safecrowd::domain::ImportIssueCode code) {
    using safecrowd::domain::ImportIssueCode;

    switch (code) {
    case ImportIssueCode::MissingExit:
    case ImportIssueCode::MissingRoom:
    case ImportIssueCode::DisconnectedWalkableArea:
    case ImportIssueCode::WidthBelowMinimum:
    case ImportIssueCode::ConnectionSpanMisaligned:
    case ImportIssueCode::InvalidFloorReference:
    case ImportIssueCode::ObstructedConnection:
        return true;
    default:
        return false;
    }
}

void updateLiveValidationIssues(safecrowd::domain::ImportResult* importResult) {
    if (importResult == nullptr) {
        return;
    }

    std::vector<safecrowd::domain::ImportIssue> issues;
    issues.reserve(importResult->issues.size());
    for (const auto& issue : importResult->issues) {
        if (!isLiveValidationIssue(issue.code)) {
            issues.push_back(issue);
        }
    }

    if (importResult->layout.has_value()) {
        safecrowd::domain::ImportValidationService validator;
        auto validatedIssues = validator.validate(*importResult->layout);
        issues.insert(issues.end(), validatedIssues.begin(), validatedIssues.end());
    }

    importResult->issues = std::move(issues);
}

}  // namespace

QList<ProjectMetadata> ProjectPersistence::loadRecentProjects() {
    QList<ProjectMetadata> projects;
    projects.push_back(makeBuiltInDemoProject());
    projects.push_back(makeBuiltInDemo2FProject());
    projects.push_back(makeBuiltInEvacuationScenarioDemoProject());
    projects.push_back(makeBuiltInTwoFloorEvacuationDemoProject());

    const auto document = readJsonDocument(recentProjectsPath());
    if (!document.isObject()) {
        return projects;
    }

    for (const auto& value : document.object().value("projects").toArray()) {
        const auto indexed = projectMetadataFromJson(value.toObject());
        if (indexed.isBuiltInDemo()) {
            continue;
        }
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

    return projectMetadataFromJson(document.object());
}

bool ProjectPersistence::deleteProject(const ProjectMetadata& metadata, QString* errorMessage) {
    const auto setError = [errorMessage](const QString& message) {
        if (errorMessage != nullptr) {
            *errorMessage = message;
        }
    };

    if (metadata.isBuiltInDemo()) {
        setError("Built-in demo projects cannot be deleted.");
        return false;
    }

    if (metadata.folderPath.isEmpty()) {
        setError("Project folder is missing.");
        return false;
    }

    const auto projectFile = projectFilePath(metadata.folderPath);
    if (!QFileInfo::exists(projectFile)) {
        removeRecentProject(metadata.folderPath);
        return true;
    }

    const auto loaded = loadProject(metadata.folderPath);
    if (!loaded.isValid()) {
        setError("The selected folder does not contain a valid SafeCrowd project.");
        return false;
    }

    if (!canDeleteProjectFolder(metadata.folderPath, errorMessage)) {
        return false;
    }

    const auto absoluteFolder = QFileInfo(metadata.folderPath).absoluteFilePath();
    QString trashPath;
    if (QFile::moveToTrash(absoluteFolder, &trashPath)) {
        qInfo().noquote() << "Moved project folder to trash:" << absoluteFolder
                          << "->" << (trashPath.isEmpty() ? QStringLiteral("(unknown)") : trashPath);
        removeRecentProject(metadata.folderPath);
        return true;
    }

    qWarning().noquote() << "moveToTrash failed for" << absoluteFolder
                         << "- aborting deletion to keep the operation reversible";
    setError(QString(
        "Failed to move the project folder to the recycle bin:\n%1\n\n"
        "The project was not deleted. Please check folder permissions and try again, "
        "or remove the folder manually.")
        .arg(metadata.folderPath));
    return false;
}

bool ProjectPersistence::loadProjectReview(const ProjectMetadata& metadata, safecrowd::domain::ImportResult* importResult) {
    if (metadata.isBuiltInDemo() || importResult == nullptr) {
        return false;
    }

    const auto document = readJsonDocument(reviewFilePath(metadata.folderPath));
    if (!document.isObject()) {
        return false;
    }

    const auto root = document.object();
    if (!root.contains("layout") || !root.value("layout").isObject()) {
        return false;
    }

    importResult->layout = layoutFromJson(root.value("layout").toObject());
    importResult->reviewStatus = static_cast<safecrowd::domain::ImportReviewStatus>(root.value("reviewStatus").toInt());

    const auto artifactsDocument = readJsonDocument(importArtifactsFilePath(metadata.folderPath));
    if (artifactsDocument.isObject()) {
        importArtifactsFromJson(artifactsDocument.object(), importResult);
    }

    updateLiveValidationIssues(importResult);

    if (safecrowd::domain::hasBlockingImportIssue(importResult->issues)
        && importResult->reviewStatus == safecrowd::domain::ImportReviewStatus::Approved) {
        importResult->reviewStatus = safecrowd::domain::ImportReviewStatus::Pending;
    }

    return true;
}

bool ProjectPersistence::loadProjectWorkspace(const ProjectMetadata& metadata, ProjectWorkspaceState* state) {
    if (metadata.isBuiltInDemo() || state == nullptr) {
        return false;
    }

    const auto document = readJsonDocument(workspaceFilePath(metadata.folderPath));
    if (!document.isObject()) {
        return false;
    }

    *state = workspaceStateFromJson(document.object());
    return true;
}

bool ProjectPersistence::saveProject(ProjectMetadata metadata, QString* errorMessage) {
    if (metadata.isBuiltInDemo()) {
        if (errorMessage != nullptr) {
            *errorMessage = "Built-in demo projects do not need to be saved.";
        }
        return false;
    }

    if (!metadata.isValid()) {
        if (errorMessage != nullptr) {
            *errorMessage = "Project name and folder are required.";
        }
        return false;
    }

    if (!canSaveIntoProjectFolder(metadata.folderPath, errorMessage)) {
        return false;
    }

    QDir folder(metadata.folderPath);
    if (!folder.exists() && !QDir().mkpath(metadata.folderPath)) {
        if (errorMessage != nullptr) {
            *errorMessage = QString("Failed to create project folder: %1").arg(metadata.folderPath);
        }
        return false;
    }

    if (!metadata.isBlankLayoutProject() && !copyLayoutIntoProject(metadata, errorMessage)) {
        return false;
    }

    metadata.savedAt = QDateTime::currentDateTime().toString(Qt::ISODate);
    if (!writeJsonDocument(projectFilePath(metadata.folderPath), QJsonDocument(projectMetadataToJson(metadata)), errorMessage)) {
        return false;
    }

    upsertRecentProject(metadata);
    return true;
}

bool ProjectPersistence::saveProjectReview(
    const ProjectMetadata& metadata,
    const safecrowd::domain::ImportResult& importResult,
    QString* errorMessage) {
    if (metadata.isBuiltInDemo()) {
        if (errorMessage != nullptr) {
            *errorMessage = "Built-in demo projects do not need to be saved.";
        }
        return false;
    }

    if (!importResult.layout.has_value()) {
        if (errorMessage != nullptr) {
            *errorMessage = "No editable layout is loaded for this project.";
        }
        return false;
    }

    QJsonObject root;
    root["reviewStatus"] = static_cast<int>(importResult.reviewStatus);
    root["layout"] = layoutToJson(*importResult.layout);
    if (!writeJsonDocument(reviewFilePath(metadata.folderPath), QJsonDocument(root), errorMessage)) {
        return false;
    }

    return writeJsonDocument(
        importArtifactsFilePath(metadata.folderPath),
        QJsonDocument(importArtifactsToJson(importResult.artifacts, importResult.traceRefs)),
        errorMessage);
}

bool ProjectPersistence::saveProjectWorkspace(
    const ProjectMetadata& metadata,
    const ProjectWorkspaceState& state,
    QString* errorMessage) {
    if (metadata.isBuiltInDemo()) {
        if (errorMessage != nullptr) {
            *errorMessage = "Built-in demo projects do not need to be saved.";
        }
        return false;
    }

    return writeJsonDocument(workspaceFilePath(metadata.folderPath), QJsonDocument(workspaceStateToJson(state)), errorMessage);
}

}  // namespace safecrowd::application
