#include "MetaFile.h"
#include "LibraryManager.h"
#include <random>
#include <iomanip>
#include <iostream>
#include <algorithm>
#include <windows.h>
#include "Log.h"

UID MetaFile::GenerateUID() {
    std::random_device rd;
    std::mt19937_64 gen(rd());
    std::uniform_int_distribution<UID> dis;
    return dis(gen);
}

AssetType MetaFile::GetAssetType(const std::string& extension) {
    std::string ext = extension;
    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);

    if (ext == ".fbx") return AssetType::MODEL_FBX;
    if (ext == ".png") return AssetType::TEXTURE_PNG;
    if (ext == ".jpg" || ext == ".jpeg") return AssetType::TEXTURE_JPG;
    if (ext == ".dds") return AssetType::TEXTURE_DDS;
    if (ext == ".tga") return AssetType::TEXTURE_TGA;
    if (ext == ".h") return AssetType::SCRIPT_H;
    if (ext == ".cpp") return AssetType::SCRIPT_CPP;

    return AssetType::UNKNOWN;
}

bool MetaFile::Save(const std::string& metaFilePath) const {
    std::ofstream file(metaFilePath);
    if (!file.is_open()) {
        std::cerr << "[MetaFile] ERROR: Cannot create .meta file: " << metaFilePath << std::endl;
        return false;
    }

    if (IsScript()) {
        file << "fileFormatVersion: 2\n";
        file << "guid: " << std::hex << std::setw(16) << std::setfill('0') << uid << std::dec << "\n";
    }
    else {
        std::string relativeOriginalPath = MakeRelativeToProject(originalPath);

        file << "uid: " << uid << "\n";
        file << "type: " << static_cast<int>(type) << "\n";
        file << "lastModified: " << lastModified << "\n";

        file << "importScale: " << importSettings.importScale << "\n";
        file << "generateNormals: " << (importSettings.generateNormals ? "1" : "0") << "\n";
        file << "flipUVs: " << (importSettings.flipUVs ? "1" : "0") << "\n";
        file << "optimizeMeshes: " << (importSettings.optimizeMeshes ? "1" : "0") << "\n";
        file << "upAxis: " << importSettings.upAxis << "\n";
        file << "frontAxis: " << importSettings.frontAxis << "\n";

        file << "generateMipmaps: " << (importSettings.generateMipmaps ? "1" : "0") << "\n";
        file << "filterMode: " << importSettings.filterMode << "\n";
        file << "flipHorizontal: " << (importSettings.flipHorizontal ? "1" : "0") << "\n";
        file << "maxTextureSize: " << importSettings.maxTextureSize << "\n";
    }

    file.close();
    return true;
}

MetaFile MetaFile::Load(const std::string& metaFilePath) {
    MetaFile meta;

    std::ifstream file(metaFilePath);
    if (!file.is_open()) {
        return meta;
    }

    std::string line;
    bool isScriptFormat = false;

    while (std::getline(file, line)) {
        size_t colonPos = line.find(':');
        if (colonPos == std::string::npos) continue;

        std::string key = line.substr(0, colonPos);
        std::string value = line.substr(colonPos + 2);

        if (key == "fileFormatVersion") {
            isScriptFormat = true;
            continue;
        }
        else if (key == "guid") {
            if (isScriptFormat) {
                meta.uid = std::stoull(value, nullptr, 16);
            }
            else {
                meta.uid = std::stoull(value);
            }
        }
        else if (key == "uid") {
            meta.uid = std::stoull(value);
        }
        else if (key == "type") {
            meta.type = static_cast<AssetType>(std::stoi(value));
        }
        else if (key == "originalPath") {
            meta.originalPath = MakeAbsoluteFromProject(value);
        }
        else if (key == "lastModified") {
            meta.lastModified = std::stoll(value);
        }
        else if (key == "importScale") {
            meta.importSettings.importScale = std::stof(value);
        }
        else if (key == "generateNormals") {
            meta.importSettings.generateNormals = (value == "1");
        }
        else if (key == "flipUVs") {
            meta.importSettings.flipUVs = (value == "1");
        }
        else if (key == "optimizeMeshes") {
            meta.importSettings.optimizeMeshes = (value == "1");
        }
        else if (key == "upAxis") {
            meta.importSettings.upAxis = std::stoi(value);
        }
        else if (key == "frontAxis") {
            meta.importSettings.frontAxis = std::stoi(value);
        }
        else if (key == "generateMipmaps") {
            meta.importSettings.generateMipmaps = (value == "1");
        }
        else if (key == "filterMode") {
            meta.importSettings.filterMode = std::stoi(value);
        }
        else if (key == "flipHorizontal") {
            meta.importSettings.flipHorizontal = (value == "1");
        }
        else if (key == "maxTextureSize") {
            meta.importSettings.maxTextureSize = std::stoi(value);
        }
    }

    file.close();

    return meta;
}

bool MetaFile::NeedsReimport(const std::string& assetPath) const {
    if (IsScript()) {
        return false;
    }

    if (!std::filesystem::exists(assetPath)) {
        return false;
    }

    long long currentTimestamp = std::filesystem::last_write_time(assetPath)
        .time_since_epoch().count();

    const long long tolerance = 20000000000;

    long long diff = std::abs(currentTimestamp - lastModified);

    return diff > tolerance;
}

std::string MetaFile::MakeRelativeToProject(const std::string& absolutePath) {
    if (absolutePath.empty()) {
        return "";
    }

    try {
        std::filesystem::path absPath(absolutePath);
        std::filesystem::path projectRoot = LibraryManager::GetAssetsRoot();
        projectRoot = projectRoot.parent_path();

        if (absPath.is_relative()) {
            return absolutePath;
        }

        std::filesystem::path relativePath = std::filesystem::relative(absPath, projectRoot);

        std::string result = relativePath.string();
        std::replace(result.begin(), result.end(), '\\', '/');

        return result;
    }
    catch (...) {
        return absolutePath;
    }
}

std::string MetaFile::MakeAbsoluteFromProject(const std::string& relativePath) {
    if (relativePath.empty()) {
        return "";
    }

    try {
        std::filesystem::path relPath(relativePath);

        if (relPath.is_absolute()) {
            return relativePath;
        }

        std::filesystem::path projectRoot = LibraryManager::GetAssetsRoot();
        projectRoot = projectRoot.parent_path();

        std::filesystem::path absolutePath = projectRoot / relPath;

        return absolutePath.string();
    }
    catch (...) {
        return relativePath;
    }
}

void MetaFileManager::Initialize() {
    CleanOrphanedMetaFiles();
    ScanAssets();
}

void MetaFileManager::ScanAssets() {
    std::string assetsPath = LibraryManager::GetAssetsRoot();

    if (!std::filesystem::exists(assetsPath)) {
        LOG_DEBUG("[MetaFileManager] Assets folder not found: %s", assetsPath.c_str());
        return;
    }

    int metasCreated = 0;
    int metasExisting = 0;

    for (const auto& entry : std::filesystem::recursive_directory_iterator(assetsPath)) {
        if (!entry.is_regular_file()) continue;

        std::string assetPath = entry.path().string();
        std::string extension = entry.path().extension().string();

        if (extension == ".meta") continue;

        AssetType type = MetaFile::GetAssetType(extension);
        if (type == AssetType::UNKNOWN) continue;

        std::string metaPath = GetMetaPath(assetPath);

        if (!std::filesystem::exists(metaPath)) {
            MetaFile meta;
            meta.uid = MetaFile::GenerateUID();
            meta.type = type;
            meta.originalPath = assetPath;

            if (type != AssetType::SCRIPT_H && type != AssetType::SCRIPT_CPP) {
                meta.lastModified = GetFileTimestamp(assetPath);
            }

            if (meta.Save(metaPath)) {
                metasCreated++;
                LOG_DEBUG("[MetaFileManager] Created .meta for: %s",
                    entry.path().filename().string().c_str());
            }
        }
        else {
            metasExisting++;
        }
    }

    LOG_CONSOLE("[MetaFileManager] Scan complete: %d created, %d existing",
        metasCreated, metasExisting);
}

void MetaFileManager::CleanOrphanedMetaFiles() {
    std::string assetsPath = LibraryManager::GetAssetsRoot();

    if (!std::filesystem::exists(assetsPath)) {
        LOG_DEBUG("[MetaFileManager] Assets folder not found: %s", assetsPath.c_str());
        return;
    }

    int metasDeleted = 0;

    for (const auto& entry : std::filesystem::recursive_directory_iterator(assetsPath)) {
        if (!entry.is_regular_file()) continue;

        std::string filePath = entry.path().string();
        std::string extension = entry.path().extension().string();

        if (extension != ".meta") continue;

        std::string assetPath = filePath.substr(0, filePath.length() - 5);

        if (!std::filesystem::exists(assetPath)) {
            try {
                std::filesystem::remove(filePath);
                LOG_CONSOLE("[MetaFileManager] Deleted orphaned .meta: %s", filePath.c_str());
                metasDeleted++;
            }
            catch (const std::exception& e) {
                LOG_CONSOLE("[MetaFileManager] ERROR deleting .meta: %s - %s", filePath.c_str(), e.what());
            }
        }
    }

    if (metasDeleted > 0) {
        LOG_CONSOLE("[MetaFileManager] Cleanup complete: %d orphaned .meta files deleted", metasDeleted);
    }
}

void MetaFileManager::CheckForChanges() {
    std::string assetsPath = LibraryManager::GetAssetsRoot();

    if (!std::filesystem::exists(assetsPath)) {
        return;
    }

    int metasCreated = 0;
    int metasDeleted = 0;

    try {
        for (const auto& entry : std::filesystem::recursive_directory_iterator(assetsPath)) {
            if (!entry.is_regular_file()) continue;

            std::string filePath = entry.path().string();
            std::string extension = entry.path().extension().string();

            if (extension != ".meta") continue;

            std::string assetPath = filePath.substr(0, filePath.length() - 5);

            if (!std::filesystem::exists(assetPath)) {
                try {
                    std::filesystem::remove(filePath);
                    LOG_CONSOLE("[MetaFileManager] Deleted orphaned .meta: %s", filePath.c_str());
                    metasDeleted++;
                }
                catch (const std::exception& e) {
                    LOG_CONSOLE("[MetaFileManager] ERROR deleting .meta: %s - %s", filePath.c_str(), e.what());
                }
            }
        }

        for (const auto& entry : std::filesystem::recursive_directory_iterator(assetsPath)) {
            if (!entry.is_regular_file()) continue;

            std::string assetPath = entry.path().string();
            std::string extension = entry.path().extension().string();

            if (extension == ".meta") continue;

            AssetType type = MetaFile::GetAssetType(extension);
            if (type == AssetType::UNKNOWN) continue;

            std::string metaPath = GetMetaPath(assetPath);

            if (!std::filesystem::exists(metaPath)) {
                MetaFile meta;
                meta.uid = MetaFile::GenerateUID();
                meta.type = type;
                meta.originalPath = assetPath;

                if (type != AssetType::SCRIPT_H && type != AssetType::SCRIPT_CPP) {
                    meta.lastModified = GetFileTimestamp(assetPath);
                }

                if (meta.Save(metaPath)) {
                    LOG_CONSOLE("[MetaFileManager] Created .meta for new asset: %s", assetPath.c_str());
                    metasCreated++;
                }
            }
        }

        if (metasCreated > 0 || metasDeleted > 0) {
            LOG_CONSOLE("[MetaFileManager] Changes detected: %d created, %d deleted", metasCreated, metasDeleted);
        }
    }
    catch (const std::exception& e) {
        LOG_CONSOLE("[MetaFileManager] ERROR during change detection: %s", e.what());
    }
}

bool MetaFileManager::UpdateMetaIfModified(const std::string& assetPath) {
    std::string metaPath = GetMetaPath(assetPath);

    if (!std::filesystem::exists(metaPath)) {
        LOG_CONSOLE("[MetaFileManager] No .meta found, creating for: %s", assetPath.c_str());
        GetOrCreateMeta(assetPath);
        return true;
    }

    MetaFile meta = MetaFile::Load(metaPath);

    std::filesystem::path path(assetPath);
    std::string extension = path.extension().string();
    AssetType type = MetaFile::GetAssetType(extension);

    if (type == AssetType::SCRIPT_H || type == AssetType::SCRIPT_CPP) {
        return false;
    }

    long long currentTimestamp = GetFileTimestamp(assetPath);

    if (meta.lastModified != currentTimestamp) {
        LOG_CONSOLE("[MetaFileManager] File modified, updating .meta: %s", assetPath.c_str());

        meta.lastModified = currentTimestamp;

        if (meta.Save(metaPath)) {
            LOG_CONSOLE("[MetaFileManager] .meta updated successfully");
            return true;
        }
        else {
            LOG_CONSOLE("[MetaFileManager] ERROR: Failed to save .meta");
            return false;
        }
    }

    return false;
}

MetaFile MetaFileManager::GetOrCreateMeta(const std::string& assetPath) {
    std::string metaPath = GetMetaPath(assetPath);

    if (std::filesystem::exists(metaPath)) {
        MetaFile meta = MetaFile::Load(metaPath);

        if (meta.uid == 0) {
            meta.uid = MetaFile::GenerateUID();
            meta.Save(metaPath);
        }

        return meta;
    }

    MetaFile meta;
    meta.uid = MetaFile::GenerateUID();
    meta.type = MetaFile::GetAssetType(std::filesystem::path(assetPath).extension().string());
    meta.originalPath = assetPath;

    if (meta.type != AssetType::SCRIPT_H && meta.type != AssetType::SCRIPT_CPP) {
        meta.lastModified = GetFileTimestamp(assetPath);
    }

    meta.Save(metaPath);

    return meta;
}

bool MetaFileManager::NeedsReimport(const std::string& assetPath) {
    std::string metaPath = GetMetaPath(assetPath);

    if (!std::filesystem::exists(metaPath)) {
        return true;
    }

    MetaFile meta = MetaFile::Load(metaPath);
    return meta.NeedsReimport(assetPath);
}

void MetaFileManager::RegenerateLibrary() {
    ScanAssets();
}

MetaFile MetaFileManager::LoadMeta(const std::string& assetPath) {
    std::string metaPath = GetMetaPath(assetPath);

    if (std::filesystem::exists(metaPath)) {
        return MetaFile::Load(metaPath);
    }

    return MetaFile();
}

UID MetaFileManager::GetUIDFromAsset(const std::string& assetPath) {
    MetaFile meta = LoadMeta(assetPath);

    if (meta.uid == 0 && std::filesystem::exists(assetPath)) {
        meta.uid = MetaFile::GenerateUID();
        meta.type = MetaFile::GetAssetType(std::filesystem::path(assetPath).extension().string());
        std::string metaPath = GetMetaPath(assetPath);
        meta.Save(metaPath);
    }

    return meta.uid;
}

std::string MetaFileManager::GetAssetFromUID(UID uid) {
    if (uid == 0) return "";

    std::string assetsPath = LibraryManager::GetAssetsRoot();

    if (!std::filesystem::exists(assetsPath)) {
        return "";
    }

    for (const auto& entry : std::filesystem::recursive_directory_iterator(assetsPath)) {
        if (!entry.is_regular_file()) continue;

        std::string path = entry.path().string();
        std::string extension = entry.path().extension().string();

        if (extension != ".meta") continue;

        MetaFile meta = MetaFile::Load(path);
        if (meta.uid == uid) {
            return meta.originalPath;
        }
    }

    return "";
}

long long MetaFileManager::GetFileTimestamp(const std::string& filePath) {
    if (!std::filesystem::exists(filePath)) {
        return 0;
    }

    return std::filesystem::last_write_time(filePath).time_since_epoch().count();
}

std::string MetaFileManager::GetMetaPath(const std::string& assetPath) {
    return assetPath + ".meta";
}