/**
 * @file filesystem_resource_manager.h
 * @brief 业务流程管理系统 - 文件系统资源管理器实现
 *
 * 设计模式：
 * - 策略模式：实现 IResourceManager 接口
 *
 * 面向对象：
 * - 封装：管理文件系统资源
 * - 依赖注入：通过接口接收资源
 */

#ifndef WORKFLOW_SYSTEM_FILESYSTEM_RESOURCE_MANAGER_H
#define WORKFLOW_SYSTEM_FILESYSTEM_RESOURCE_MANAGER_H

#include <string>
#include <sys/stat.h>
#include <sys/types.h>
#include <dirent.h>
#include <unistd.h>
#include <fcntl.h>
#include <vector>
#include <algorithm>
#include <memory>
#include <mutex>
#include <unordered_map>
#include "interfaces/resource_manager.h"
#include "core/utils.h"
#include "core/logger.h"
#include "core/types.h"

namespace WorkflowSystem {

// C++11 兼容的文件系统工具类
namespace FsUtils {
    // 递归删除目录
    inline bool removeDirectory(const std::string& path) {
        DIR* dir = opendir(path.c_str());
        if (!dir) {
            return false;
        }

        struct dirent* entry;
        while ((entry = readdir(dir)) != nullptr) {
            std::string name(entry->d_name);
            if (name == "." || name == "..") {
                continue;
            }

            std::string fullPath = path + "/" + name;
            struct stat statbuf;
            if (stat(fullPath.c_str(), &statbuf) == 0) {
                if (S_ISDIR(statbuf.st_mode)) {
                    removeDirectory(fullPath);
                } else {
                    unlink(fullPath.c_str());
                }
            }
        }

        closedir(dir);
        rmdir(path.c_str());
        return true;
    }

    // 创建目录（递归）
    inline bool createDirectories(const std::string& path) {
        if (path.empty()) {
            return false;
        }

        size_t pos = 0;
        std::string currentPath;

        // 跳过开头的 /（绝对路径）
        if (path[0] == '/') {
            currentPath = "/";
            pos = 1;
        }

        while (pos < path.length()) {
            size_t nextPos = path.find('/', pos);
            if (nextPos == std::string::npos) {
                nextPos = path.length();
            }

            std::string segment = path.substr(pos, nextPos - pos);
            if (!segment.empty()) {
                currentPath += segment;
                if (pos != path.length()) {
                    currentPath += "/";
                }

                struct stat st;
                if (stat(currentPath.c_str(), &st) != 0) {
                    if (mkdir(currentPath.c_str(), 0755) != 0) {
                        return false;
                    }
                } else if (!S_ISDIR(st.st_mode)) {
                    return false;
                }
            }
            pos = nextPos + 1;
        }
        return true;
    }
}

/**
 * @brief 文件系统资源管理器
 *
 * 实现策略：使用 POSIX 系统调用管理文件系统资源
 */
class FileSystemResourceManager : public IResourceManager {
public:
    explicit FileSystemResourceManager(const std::string& basePath = "/tmp/workflow_resources")
        : basePath_(basePath) {
        ensureBasePath();
    }

    std::shared_ptr<Resource> createDirectory(const std::string& path) override {
        FsUtils::createDirectories(path);

        auto resource = std::make_shared<Resource>(
            IdGenerator::generate(),
            ResourceType::DIRECTORY,
            path
        );
        resource->setMetadata("created", "true");

        {
            std::lock_guard<std::mutex> lock(mutex_);
            resources_[resource->getId()] = resource;
        }

        LOG_INFO("[ResourceManager] Created directory: " + path);
        return resource;
    }

    std::shared_ptr<Resource> createDirectory() override {
        std::string dirName = "dir_" + TimeUtils::getCurrentTimestamp();
        std::string path = basePath_ + "/" + dirName;
        return createDirectory(path);
    }

    std::shared_ptr<Resource> getResource(const std::string& id) override {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = resources_.find(id);
        return (it != resources_.end()) ? it->second : nullptr;
    }

    bool cleanup(const std::string& id) override {
        auto resource = getResource(id);
        if (!resource) return false;;

        try {
            if (resource->getType() == ResourceType::DIRECTORY) {
                FsUtils::removeDirectory(resource->getPath());
                LOG_INFO("[ResourceManager] Cleaned directory: " + resource->getPath());
            }

            {
                std::lock_guard<std::mutex> lock(mutex_);
                resources_.erase(id);
            }
            return true;
        } catch (const std::exception& e) {
            LOG_ERROR("[ResourceManager] Error cleaning " + id + ": " + e.what());
            return false;
        }
    }

    size_t cleanupAll() override {
        size_t count = 0;

        // 收集所有资源ID（避免在锁内修改）
        std::vector<std::string> resourceIds;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            for (const auto& pair : resources_) {
                resourceIds.push_back(pair.first);
            }
        }

        // 清理资源
        for (const auto& id : resourceIds) {
            if (cleanup(id)) count++;
        }

        LOG_INFO("[ResourceManager] Cleaned " + std::to_string(count) + " resources");
        return count;
    }

    std::vector<std::shared_ptr<Resource>> getAllResources() const override {
        std::lock_guard<std::mutex> lock(mutex_);
        std::vector<std::shared_ptr<Resource>> result;
        for (const auto& pair : resources_) {
            result.push_back(pair.second);
        }
        return result;
    }

    // 获取基础路径（非接口方法）
    std::string getBasePath() const { return basePath_; }

private:
    void ensureBasePath() {
        FsUtils::createDirectories(basePath_);
    }

    std::string basePath_;
    std::unordered_map<std::string, std::shared_ptr<Resource>> resources_;
    mutable std::mutex mutex_;
};

} // namespace WorkflowSystem

#endif // WORKFLOW_SYSTEM_FILESYSTEM_RESOURCE_MANAGER_H
