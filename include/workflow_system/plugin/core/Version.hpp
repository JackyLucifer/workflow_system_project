#ifndef PLUGIN_FRAMEWORK_VERSION_HPP
#define PLUGIN_FRAMEWORK_VERSION_HPP

#include <string>
#include <cstdint>
#include <sstream>
#include <stdexcept>

namespace WorkflowSystem { namespace Plugin {

/**
 * @brief 版本号类
 *
 * 支持语义化版本 (Semantic Versioning 2.0.0)
 * 格式: MAJOR.MINOR.PATCH[-PRERELEASE][+BUILD]
 */
class Version {
public:
    uint32_t major = 0;      // 主版本号（不兼容的API修改）
    uint32_t minor = 0;      // 次版本号（向后兼容的功能新增）
    uint32_t patch = 0;      // 补丁版本号（向后兼容的问题修正）
    std::string prerelease;  // 预发布版本标识（如 "alpha", "beta.1"）
    std::string build;        // 构建元数据（如 "githash.abc123"）

    Version() = default;

    Version(uint32_t maj, uint32_t min, uint32_t pat,
           const std::string& pre = "",
           const std::string& bld = "")
        : major(maj), minor(min), patch(pat)
        , prerelease(pre), build(bld) {}

    /**
     * @brief 从字符串解析版本号
     * @param versionStr 版本字符串 (如 "1.2.3", "2.0.0-beta.1+githash.abc")
     */
    static Version parse(const std::string& versionStr) {
        Version v;
        std::string::size_type pos = 0;

        // 解析主版本号
        auto dotPos = versionStr.find('.', pos);
        if (dotPos == std::string::npos) {
            throw std::invalid_argument("Invalid version format: " + versionStr);
        }
        v.major = std::stoul(versionStr.substr(pos, dotPos - pos));
        pos = dotPos + 1;

        // 解析次版本号
        dotPos = versionStr.find('.', pos);
        if (dotPos == std::string::npos) {
            throw std::invalid_argument("Invalid version format: " + versionStr);
        }
        v.minor = std::stoul(versionStr.substr(pos, dotPos - pos));
        pos = dotPos + 1;

        // 解析补丁版本号
        auto dashPos = versionStr.find('-', pos);
        auto plusPos = versionStr.find('+', pos);

        std::string patchStr;
        if (dashPos != std::string::npos && plusPos != std::string::npos) {
            patchStr = versionStr.substr(pos, std::min(dashPos, plusPos) - pos);
        } else if (dashPos != std::string::npos) {
            patchStr = versionStr.substr(pos, dashPos - pos);
        } else if (plusPos != std::string::npos) {
            patchStr = versionStr.substr(pos, plusPos - pos);
        } else {
            patchStr = versionStr.substr(pos);
        }
        v.patch = std::stoul(patchStr);

        // 解析预发布标识
        if (dashPos != std::string::npos) {
            pos = dashPos + 1;
            plusPos = versionStr.find('+', pos);
            if (plusPos != std::string::npos) {
                v.prerelease = versionStr.substr(pos, plusPos - pos);
            } else {
                v.prerelease = versionStr.substr(pos);
            }
        }

        // 解析构建元数据
        if (plusPos != std::string::npos) {
            v.build = versionStr.substr(plusPos + 1);
        }

        return v;
    }

    /**
     * @brief 转换为字符串
     */
    std::string toString() const {
        std::ostringstream oss;
        oss << major << "." << minor << "." << patch;
        if (!prerelease.empty()) {
            oss << "-" << prerelease;
        }
        if (!build.empty()) {
            oss << "+" << build;
        }
        return oss.str();
    }

    /**
     * @brief 比较版本号
     * @return -1 if this < other, 0 if equal, 1 if this > other
     */
    int compare(const Version& other) const {
        if (major != other.major) {
            return (major > other.major) ? 1 : -1;
        }
        if (minor != other.minor) {
            return (minor > other.minor) ? 1 : -1;
        }
        if (patch != other.patch) {
            return (patch > other.patch) ? 1 : -1;
        }

        // 预发布版本比较
        if (prerelease.empty() && !other.prerelease.empty()) {
            return 1;  // 正式版本 > 预发布版本
        }
        if (!prerelease.empty() && other.prerelease.empty()) {
            return -1; // 预发布版本 < 正式版本
        }
        if (prerelease != other.prerelease) {
            return (prerelease > other.prerelease) ? 1 : -1;
        }

        return 0;
    }

    // 比较运算符
    bool operator==(const Version& other) const { return compare(other) == 0; }
    bool operator!=(const Version& other) const { return compare(other) != 0; }
    bool operator<(const Version& other) const { return compare(other) < 0; }
    bool operator<=(const Version& other) const { return compare(other) <= 0; }
    bool operator>(const Version& other) const { return compare(other) > 0; }
    bool operator>=(const Version& other) const { return compare(other) >= 0; }

    /**
     * @brief 检查是否在指定版本范围内
     * @param minVersion 最低版本（包含）
     * @param maxVersion 最高版本（包含）
     */
    bool isInRange(const Version& minVersion, const Version& maxVersion) const {
        return *this >= minVersion && *this <= maxVersion;
    }
};

/**
 * @brief 版本约束
 */
class VersionConstraint {
public:
    Version minVersion;           // 最低版本
    Version maxVersion;           // 最高版本
    bool minInclusive = true;     // 是否包含最低版本
    bool maxInclusive = true;     // 是否包含最高版本

    VersionConstraint() = default;

    VersionConstraint(const Version& min, const Version& max,
                     bool minInc = true, bool maxInc = true)
        : minVersion(min), maxVersion(max)
        , minInclusive(minInc), maxInclusive(maxInc) {}

    /**
     * @brief 检查版本是否满足约束
     */
    bool satisfies(const Version& version) const {
        int minCmp = version.compare(minVersion);
        int maxCmp = version.compare(maxVersion);

        bool minOk = (minInclusive ? minCmp >= 0 : minCmp > 0);
        bool maxOk = (maxInclusive ? maxCmp <= 0 : maxCmp < 0);

        return minOk && maxOk;
    }

    /**
     * @brief 从字符串解析约束
     * 支持格式: ">=1.0.0,<2.0.0", "~1.2.3", "^1.2.3"
     */
    static VersionConstraint parse(const std::string& constraint) {
        // 简化实现，支持基本格式 ">=minVersion,<=maxVersion"
        VersionConstraint vc;
        // TODO: 实现完整的解析逻辑
        return vc;
    }

    std::string toString() const {
        std::ostringstream oss;
        oss << (minInclusive ? ">=" : ">") << minVersion.toString()
            << "," << (maxInclusive ? "<=" : "<") << maxVersion.toString();
        return oss.str();
    }
};

} // namespace Plugin
} // namespace WorkflowSystem

#endif // PLUGIN_FRAMEWORK_VERSION_HPP
