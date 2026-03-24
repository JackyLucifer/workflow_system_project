/**
 * @file version_manager.h
 * @brief 版本兼容性管理器 - semver 版本检查和兼容性验证
 */

#ifndef WORKFLOW_SYSTEM_VERSION_MANAGER_H
#define WORKFLOW_SYSTEM_VERSION_MANAGER_H

#include <string>
#include <vector>
#include <map>
#include <stdexcept>

namespace WorkflowSystem {
namespace Plugin {

/**
 * @brief 语义化版本号
 */
struct SemanticVersion {
    int major;
    int minor;
    int patch;
    std::string prerelease;
    std::string build;
    
    SemanticVersion() : major(0), minor(0), patch(0) {}
    
    SemanticVersion(int maj, int min, int pat, 
                   const std::string& pre = "", const std::string& bld = "")
        : major(maj), minor(min), patch(pat), prerelease(pre), build(bld) {}
    
    static SemanticVersion parse(const std::string& version) {
        SemanticVersion v;
        
        size_t pos = 0;
        size_t next = version.find('.', pos);
        if (next != std::string::npos) {
            v.major = std::stoi(version.substr(pos, next - pos));
            pos = next + 1;
            
            next = version.find('.', pos);
            if (next != std::string::npos) {
                v.minor = std::stoi(version.substr(pos, next - pos));
                pos = next + 1;
                
                next = version.find('-', pos);
                size_t plusPos = version.find('+', pos);
                size_t endNum = std::min(next, plusPos);
                if (endNum == std::string::npos) {
                    endNum = version.size();
                }
                v.patch = std::stoi(version.substr(pos, endNum - pos));
                
                if (next != std::string::npos && (plusPos == std::string::npos || next < plusPos)) {
                    size_t preEnd = (plusPos != std::string::npos) ? plusPos : version.size();
                    v.prerelease = version.substr(next + 1, preEnd - next - 1);
                }
                
                if (plusPos != std::string::npos) {
                    v.build = version.substr(plusPos + 1);
                }
            }
        }
        
        return v;
    }
    
    std::string toString() const {
        std::string result = std::to_string(major) + "." + 
                            std::to_string(minor) + "." + 
                            std::to_string(patch);
        if (!prerelease.empty()) {
            result += "-" + prerelease;
        }
        if (!build.empty()) {
            result += "+" + build;
        }
        return result;
    }
    
    bool operator<(const SemanticVersion& other) const {
        if (major != other.major) return major < other.major;
        if (minor != other.minor) return minor < other.minor;
        if (patch != other.patch) return patch < other.patch;
        return prerelease < other.prerelease;
    }
    
    bool operator>(const SemanticVersion& other) const { return other < *this; }
    bool operator<=(const SemanticVersion& other) const { return !(other < *this); }
    bool operator>=(const SemanticVersion& other) const { return !(*this < other); }
    bool operator==(const SemanticVersion& other) const {
        return major == other.major && minor == other.minor && 
               patch == other.patch && prerelease == other.prerelease;
    }
    bool operator!=(const SemanticVersion& other) const { return !(*this == other); }
};

/**
 * @brief 版本约束类型
 */
enum class VersionConstraintType {
    EXACT,          // =1.0.0
    GREATER,        // >1.0.0
    GREATER_EQUAL,  // >=1.0.0
    LESS,           // <1.0.0
    LESS_EQUAL,     // <=1.0.0
    CARET,          // ^1.0.0 (兼容)
    TILDE,          // ~1.0.0 (近似)
    RANGE,          // 1.0.0 - 2.0.0
    WILDCARD        // 1.x, 1.2.x
};

/**
 * @brief 版本约束
 */
struct VersionConstraint {
    VersionConstraintType type;
    SemanticVersion minVersion;
    SemanticVersion maxVersion;
    std::string rawExpression;
    
    VersionConstraint() : type(VersionConstraintType::EXACT) {}
    
    static VersionConstraint parse(const std::string& constraint) {
        VersionConstraint vc;
        vc.rawExpression = constraint;
        
        std::string expr = constraint;
        
        // 移除空格
        std::string trimmed;
        for (char c : expr) {
            if (c != ' ') trimmed += c;
        }
        expr = trimmed;
        
        if (expr.empty()) {
            vc.type = VersionConstraintType::GREATER_EQUAL;
            vc.minVersion = SemanticVersion(0, 0, 0);
            return vc;
        }
        
        // 范围: 1.0.0 - 2.0.0
        size_t rangePos = expr.find(" - ");
        if (rangePos != std::string::npos) {
            vc.type = VersionConstraintType::RANGE;
            vc.minVersion = SemanticVersion::parse(expr.substr(0, rangePos));
            vc.maxVersion = SemanticVersion::parse(expr.substr(rangePos + 3));
            return vc;
        }
        
        // 通配符: 1.x, 1.2.x
        if (expr.find('x') != std::string::npos || expr.find('*') != std::string::npos) {
            vc.type = VersionConstraintType::WILDCARD;
            
            std::string normalized = expr;
            for (char& c : normalized) {
                if (c == '*') c = 'x';
            }
            
            std::vector<int> parts;
            size_t pos = 0;
            while (pos < normalized.size()) {
                size_t next = normalized.find('.', pos);
                std::string part;
                if (next != std::string::npos) {
                    part = normalized.substr(pos, next - pos);
                    pos = next + 1;
                } else {
                    part = normalized.substr(pos);
                    pos = normalized.size();
                }
                
                if (part == "x") {
                    parts.push_back(-1);
                } else {
                    parts.push_back(std::stoi(part));
                }
            }
            
            if (parts.size() >= 1) {
                vc.minVersion.major = (parts[0] >= 0) ? parts[0] : 0;
            }
            if (parts.size() >= 2) {
                vc.minVersion.minor = (parts[1] >= 0) ? parts[1] : 0;
            }
            if (parts.size() >= 3) {
                vc.minVersion.patch = (parts[2] >= 0) ? parts[2] : 0;
            }
            
            return vc;
        }
        
        char firstChar = expr[0];
        size_t versionStart = 0;
        
        switch (firstChar) {
            case '=':
                vc.type = VersionConstraintType::EXACT;
                versionStart = 1;
                break;
            case '>':
                if (expr.size() > 1 && expr[1] == '=') {
                    vc.type = VersionConstraintType::GREATER_EQUAL;
                    versionStart = 2;
                } else {
                    vc.type = VersionConstraintType::GREATER;
                    versionStart = 1;
                }
                break;
            case '<':
                if (expr.size() > 1 && expr[1] == '=') {
                    vc.type = VersionConstraintType::LESS_EQUAL;
                    versionStart = 2;
                } else {
                    vc.type = VersionConstraintType::LESS;
                    versionStart = 1;
                }
                break;
            case '^':
                vc.type = VersionConstraintType::CARET;
                versionStart = 1;
                break;
            case '~':
                vc.type = VersionConstraintType::TILDE;
                versionStart = 1;
                break;
            default:
                vc.type = VersionConstraintType::EXACT;
                versionStart = 0;
                break;
        }
        
        vc.minVersion = SemanticVersion::parse(expr.substr(versionStart));
        
        return vc;
    }
    
    bool isSatisfiedBy(const SemanticVersion& version) const {
        switch (type) {
            case VersionConstraintType::EXACT:
                return version == minVersion;
            
            case VersionConstraintType::GREATER:
                return version > minVersion;
            
            case VersionConstraintType::GREATER_EQUAL:
                return version >= minVersion;
            
            case VersionConstraintType::LESS:
                return version < minVersion;
            
            case VersionConstraintType::LESS_EQUAL:
                return version <= minVersion;
            
            case VersionConstraintType::CARET: {
                if (version < minVersion) return false;
                if (minVersion.major == 0) {
                    if (minVersion.minor == 0) {
                        return version.major == 0 && version.minor == 0 && version.patch == minVersion.patch;
                    }
                    return version.major == 0 && version.minor == minVersion.minor;
                }
                return version.major == minVersion.major;
            }
            
            case VersionConstraintType::TILDE: {
                if (version < minVersion) return false;
                return version.major == minVersion.major && version.minor == minVersion.minor;
            }
            
            case VersionConstraintType::RANGE:
                return version >= minVersion && version <= maxVersion;
            
            case VersionConstraintType::WILDCARD: {
                std::string expr = rawExpression;
                for (char& c : expr) {
                    if (c == '*') c = 'x';
                }
                
                if (expr.find("x.x") != std::string::npos || expr == "x") {
                    return true;
                }
                
                size_t xPos = expr.find(".x");
                if (xPos != std::string::npos) {
                    if (xPos == 2) {
                        return version.major == minVersion.major;
                    }
                }
                
                return version >= minVersion;
            }
            
            default:
                return false;
        }
    }
};

/**
 * @brief 版本兼容性检查结果
 */
struct VersionCheckResult {
    bool compatible;
    std::string pluginName;
    SemanticVersion pluginVersion;
    std::string requiredVersion;
    std::string errorMessage;
    
    VersionCheckResult() : compatible(true) {}
    
    static VersionCheckResult ok() {
        return VersionCheckResult();
    }
    
    static VersionCheckResult error(const std::string& plugin, 
                                   const SemanticVersion& actual,
                                   const std::string& required,
                                   const std::string& msg) {
        VersionCheckResult r;
        r.compatible = false;
        r.pluginName = plugin;
        r.pluginVersion = actual;
        r.requiredVersion = required;
        r.errorMessage = msg;
        return r;
    }
};

/**
 * @brief 版本管理器
 */
class VersionManager {
public:
    static VersionManager& getInstance() {
        static VersionManager instance;
        return instance;
    }
    
    /**
     * @brief 设置宿主版本
     */
    void setHostVersion(const SemanticVersion& version) {
        hostVersion_ = version;
    }
    
    void setHostVersion(const std::string& version) {
        hostVersion_ = SemanticVersion::parse(version);
    }
    
    SemanticVersion getHostVersion() const {
        return hostVersion_;
    }
    
    /**
     * @brief 注册插件版本要求
     */
    void registerPluginRequirement(const std::string& pluginName,
                                   const std::string& minHostVersion,
                                   const std::string& maxHostVersion = "") {
        PluginRequirement req;
        req.pluginName = pluginName;
        req.minHostVersion = SemanticVersion::parse(minHostVersion);
        if (!maxHostVersion.empty()) {
            req.maxHostVersion = SemanticVersion::parse(maxHostVersion);
            req.hasMaxVersion = true;
        } else {
            req.hasMaxVersion = false;
        }
        
        requirements_[pluginName] = req;
    }
    
    /**
     * @brief 检查插件与宿主版本兼容性
     */
    VersionCheckResult checkHostCompatibility(const std::string& pluginName,
                                             const std::string& requiredHostVersion) const {
        VersionConstraint constraint = VersionConstraint::parse(requiredHostVersion);
        
        if (!constraint.isSatisfiedBy(hostVersion_)) {
            return VersionCheckResult::error(
                pluginName,
                hostVersion_,
                requiredHostVersion,
                "插件要求宿主版本 " + requiredHostVersion + 
                "，但当前宿主版本为 " + hostVersion_.toString()
            );
        }
        
        return VersionCheckResult::ok();
    }
    
    /**
     * @brief 检查插件版本兼容性
     */
    VersionCheckResult checkPluginVersion(const std::string& pluginName,
                                         const SemanticVersion& version,
                                         const std::string& constraint) const {
        VersionConstraint vc = VersionConstraint::parse(constraint);
        
        if (!vc.isSatisfiedBy(version)) {
            return VersionCheckResult::error(
                pluginName,
                version,
                constraint,
                "插件版本 " + version.toString() + 
                " 不满足约束 " + constraint
            );
        }
        
        return VersionCheckResult::ok();
    }
    
    /**
     * @brief 检查依赖版本兼容性
     */
    VersionCheckResult checkDependency(const std::string& pluginName,
                                       const std::string& depName,
                                       const SemanticVersion& depVersion,
                                       const std::string& requiredVersion) const {
        VersionConstraint constraint = VersionConstraint::parse(requiredVersion);
        
        if (!constraint.isSatisfiedBy(depVersion)) {
            return VersionCheckResult::error(
                pluginName + " -> " + depName,
                depVersion,
                requiredVersion,
                "依赖 " + depName + " 版本 " + depVersion.toString() + 
                " 不满足要求 " + requiredVersion
            );
        }
        
        return VersionCheckResult::ok();
    }
    
    /**
     * @brief 批量检查所有插件版本
     */
    std::vector<VersionCheckResult> checkAllPlugins(
        const std::map<std::string, SemanticVersion>& plugins) const {
        
        std::vector<VersionCheckResult> results;
        
        for (const auto& pair : plugins) {
            const std::string& name = pair.first;
            const SemanticVersion& version = pair.second;
            
            auto reqIt = requirements_.find(name);
            if (reqIt != requirements_.end()) {
                const PluginRequirement& req = reqIt->second;
                
                if (version < req.minHostVersion) {
                    results.push_back(VersionCheckResult::error(
                        name, version, req.minHostVersion.toString(),
                        "插件版本过低"
                    ));
                }
                
                if (req.hasMaxVersion && version > req.maxHostVersion) {
                    results.push_back(VersionCheckResult::error(
                        name, version, req.maxHostVersion.toString(),
                        "插件版本过高"
                    ));
                }
            }
        }
        
        return results;
    }
    
    /**
     * @brief 获取兼容的版本范围
     */
    std::string getCompatibleRange(const std::string& pluginName) const {
        auto it = requirements_.find(pluginName);
        if (it == requirements_.end()) {
            return ">=0.0.0";
        }
        
        const PluginRequirement& req = it->second;
        std::string range = ">=" + req.minHostVersion.toString();
        if (req.hasMaxVersion) {
            range += " && <=" + req.maxHostVersion.toString();
        }
        
        return range;
    }
    
    /**
     * @brief 清除所有注册的要求
     */
    void clear() {
        requirements_.clear();
    }

private:
    VersionManager() : hostVersion_(1, 0, 0) {}
    
    SemanticVersion hostVersion_;
    
    struct PluginRequirement {
        std::string pluginName;
        SemanticVersion minHostVersion;
        SemanticVersion maxHostVersion;
        bool hasMaxVersion;
        
        PluginRequirement() : hasMaxVersion(false) {}
    };
    
    std::map<std::string, PluginRequirement> requirements_;
};

/**
 * @brief 版本比较辅助函数
 */
inline bool versionSatisfies(const std::string& version, const std::string& constraint) {
    SemanticVersion v = SemanticVersion::parse(version);
    VersionConstraint c = VersionConstraint::parse(constraint);
    return c.isSatisfiedBy(v);
}

inline int compareVersions(const std::string& v1, const std::string& v2) {
    SemanticVersion sv1 = SemanticVersion::parse(v1);
    SemanticVersion sv2 = SemanticVersion::parse(v2);
    
    if (sv1 < sv2) return -1;
    if (sv1 > sv2) return 1;
    return 0;
}

} // namespace Plugin
} // namespace WorkflowSystem

#endif // WORKFLOW_SYSTEM_VERSION_MANAGER_H
