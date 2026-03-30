/**
 * @file PluginManifest.cpp
 * @brief 插件清单解析器实现 - 简化版本
 */

#include "workflow_system/plugin/core/PluginManifest.hpp"
#include <fstream>
#include <sstream>
#include <algorithm>
#include <sys/stat.h>

namespace WorkflowSystem { namespace Plugin {

std::shared_ptr<PluginManifest> PluginManifestParser::parseFile(const std::string& filePath) {
    std::ifstream file(filePath);
    if (!file.is_open()) {
        return nullptr;
    }
    
    std::stringstream buffer;
    buffer << file.rdbuf();
    file.close();
    
    auto manifest = parseString(buffer.str());
    if (manifest) {
        manifest->setManifestPath(filePath);
        size_t pos = filePath.find_last_of("/\\");
        if (pos != std::string::npos) {
            manifest->setPluginDirectory(filePath.substr(0, pos));
        }
    }
    
    return manifest;
}

std::shared_ptr<PluginManifest> PluginManifestParser::parseString(const std::string& jsonContent) {
    auto manifest = std::shared_ptr<PluginManifest>(new PluginManifest());
    
    // 简单解析JSON字符串
    // 提取基本字段
    std::string id = extractJsonValue(jsonContent, "id");
    std::string name = extractJsonValue(jsonContent, "name");
    std::string version = extractJsonValue(jsonContent, "version");
    std::string desc = extractJsonValue(jsonContent, "description");
    std::string author = extractJsonValue(jsonContent, "author");
    std::string license = extractJsonValue(jsonContent, "license");
    std::string website = extractJsonValue(jsonContent, "website");
    std::string main = extractJsonValue(jsonContent, "main");
    
    manifest->setId(id);
    manifest->setName(name);
    manifest->setDescription(desc);
    manifest->setAuthor(author);
    manifest->setLicense(license);
    manifest->setWebsite(website);
    manifest->setMain(main);
    
    if (!version.empty()) {
        manifest->setVersion(Version::parse(version));
    }
    
    return manifest;
}

std::string PluginManifestParser::extractJsonValue(const std::string& json, const std::string& key) {
    std::string searchKey = "\"" + key + "\"";
    size_t keyPos = json.find(searchKey);
    if (keyPos == std::string::npos) {
        return "";
    }
    
    size_t colonPos = json.find(':', keyPos);
    if (colonPos == std::string::npos) {
        return "";
    }
    
    // 跳过空白
    size_t valueStart = colonPos + 1;
    while (valueStart < json.size() && 
           (json[valueStart] == ' ' || json[valueStart] == '\t' || 
           json[valueStart] == '\n' || json[valueStart] == '\r')) {
        valueStart++;
    }
    
    if (valueStart >= json.size()) {
        return "";
    }
    
    // 如果是字符串
    if (json[valueStart] == '"') {
        valueStart++;
        size_t valueEnd = json.find('"', valueStart);
        if (valueEnd == std::string::npos) {
            return "";
        }
        return json.substr(valueStart, valueEnd - valueStart);
    }
    
    return "";
}
std::string PluginManifestParser::toJson(const PluginManifest& manifest) {
    std::ostringstream oss;
    oss << "{\n";
    oss << "  \"id\": \"" << manifest.getId() << "\",\n";
    oss << "  \"name\": \"" << manifest.getName() << "\",\n";
    oss << "  \"version\": \"" << manifest.getVersion().toString() << "\",\n";
    if (!manifest.getDescription().empty()) {
        oss << "  \"description\": \"" << manifest.getDescription() << "\",\n";
    }
    if (!manifest.getAuthor().empty()) {
        oss << "  \"author\": \"" << manifest.getAuthor() << "\",\n";
    }
    oss << "  \"main\": \"" << manifest.getMain() << "\"\n";
    oss << "}\n";
    return oss.str();
}
bool PluginManifestParser::saveToFile(const PluginManifest& manifest, const std::string& filePath) {
    std::ofstream file(filePath);
    if (!file.is_open()) {
        return false;
    }
    file << toJson(manifest);
    return true;
}

std::vector<std::shared_ptr<PluginManifest>> PluginDiscoverer::discover(
    const std::string& directory, bool recursive) {
    std::vector<std::shared_ptr<PluginManifest>> manifests;
    
    // 查找plugin.json
    std::string manifestPath = directory + "/plugin.json";
    std::ifstream f(manifestPath);
    if (f.good()) {
        f.close();
        auto manifest = PluginManifestParser::parseFile(manifestPath);
        if (manifest && manifest->isValid()) {
            manifests.push_back(manifest);
        }
    }
    
    return manifests;
}

std::string PluginDiscoverer::findManifest(const std::string& directory) {
    return directory + "/plugin.json";
}
bool PluginDiscoverer::directoryExists(const std::string& path) {
    struct stat info;
    return stat(path.c_str(), &info) == 0 && (info.st_mode & S_IFDIR);
}

} // namespace Plugin
} // namespace WorkflowSystem
