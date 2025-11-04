#pragma once
#include <string>
#include <map>
#include <memory>
#include <vector>

/**
 * @brief Configuration management interface
 */
class IConfiguration {
public:
    virtual ~IConfiguration() = default;
    
    // Basic configuration access
    virtual std::string getString(const std::string& key, const std::string& defaultValue = "") const = 0;
    virtual int getInt(const std::string& key, int defaultValue = 0) const = 0;
    virtual float getFloat(const std::string& key, float defaultValue = 0.0f) const = 0;
    virtual bool getBool(const std::string& key, bool defaultValue = false) const = 0;
    
    // Path related functions
    virtual std::string getModelPath(const std::string& modelName) const = 0;
    // Optional list of model paths (for ensembles). Returns empty if not set.
    virtual std::vector<std::string> getModelPaths(const std::string& modelName) const = 0;
    virtual std::string getTemplatePath(const std::string& templateName) const = 0;
    virtual std::string getMaskPath(const std::string& maskName) const = 0;
    virtual std::string getTempDirPath() const = 0;
    
    // Parameter group access
    virtual std::map<std::string, std::string> getSection(const std::string& section) const = 0;
    
    // Configuration setting
    virtual void setString(const std::string& key, const std::string& value) = 0;
    
    // Configuration loading
    virtual bool loadFromFile(const std::string& configPath) = 0;
    virtual bool loadDefaults() = 0;
    
    // Debug functions
    virtual void printAllConfigurations() const = 0;
};

using ConfigurationPtr = std::shared_ptr<IConfiguration>;
