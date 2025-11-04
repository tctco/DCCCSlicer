#pragma once
#include "../interfaces/IConfiguration.h"
#include <unordered_map>
#include <vector>
#include <toml++/toml.hpp>

/**
 * @brief 基于内存的配置实现
 */
class Configuration : public IConfiguration {
public:
    Configuration();
    virtual ~Configuration() = default;
    
    // IConfiguration接口实现
    std::string getString(const std::string& key, const std::string& defaultValue = "") const override;
    int getInt(const std::string& key, int defaultValue = 0) const override;
    float getFloat(const std::string& key, float defaultValue = 0.0f) const override;
    bool getBool(const std::string& key, bool defaultValue = false) const override;
    
    std::string getModelPath(const std::string& modelName) const override;
    std::vector<std::string> getModelPaths(const std::string& modelName) const override;
    std::string getTemplatePath(const std::string& templateName) const override;
    std::string getMaskPath(const std::string& maskName) const override;
    
    // 临时目录路径处理
    std::string getTempDirPath() const override;
    
    // 配置文件搜索
    static std::string findConfigFile(const std::string& configFileName);
    
    std::map<std::string, std::string> getSection(const std::string& section) const override;
    
    void setString(const std::string& key, const std::string& value) override;
    
    bool loadFromFile(const std::string& configPath) override;
    bool loadDefaults() override;
    
    // Debug functions
    void printAllConfigurations() const override;
    
private:
    std::unordered_map<std::string, std::string> configMap;
    std::unordered_map<std::string, std::vector<std::string>> listMap;
    std::string executableDir;
    
    void initializeDefaults();
    std::string getExecutablePath() const;
    void flattenTomlTable(const toml::table& table, const std::string& prefix = "");
    void collectArraysFromToml(const toml::table& table, const std::string& prefix = "");
};
