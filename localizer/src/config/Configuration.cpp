#include "Configuration.h"
#include "../utils/common.h"
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <toml++/toml.hpp>

Configuration::Configuration() {
  executableDir = getExecutablePath();
  loadDefaults();
}

std::string Configuration::getExecutablePath() const {
  return Common::getExecutablePath();
}

void Configuration::initializeDefaults() {
  // 模型路径
  configMap["models.rigid"] = "models/registration/rigid.onnx";
  configMap["models.affine_voxelmorph"] =
      "models/registration/affine_voxelmorph.onnx";
  configMap["models.abeta_decoupler"] = "models/decouple/abeta.onnx";
  configMap["models.tau_decoupler"] = "models/decouple/tau.onnx";

  // 模板路径
  configMap["templates.adni_pet_core"] = "nii/ADNI_empty.nii";
  configMap["templates.padded"] = "nii/paddedTemplate.nii";

  // 掩膜路径
  configMap["masks.cerebral_gray"] = "nii/voi_CerebGry_2mm.nii";
  configMap["masks.centiloid_voi"] = "nii/voi_ctx_2mm.nii";
  configMap["masks.whole_cerebral"] = "nii/voi_WhlCbl_2mm.nii";
  configMap["masks.centaur_voi"] = "nii/CenTauR.nii";
  configMap["masks.centaur_ref"] = "nii/voi_CerebGry_tau_2mm.nii";

  // 处理参数
  configMap["processing.max_iter"] = "5";
  configMap["processing.ac_diff_threshold"] = "2.0";
  configMap["processing.crop_mni.start_x"] = "8";
  configMap["processing.crop_mni.start_y"] = "16";
  configMap["processing.crop_mni.start_z"] = "8";
  configMap["processing.crop_mni.size_x"] = "79";
  configMap["processing.crop_mni.size_y"] = "95";
  configMap["processing.crop_mni.size_z"] = "79";

  // Centiloid参数
  configMap["centiloid.tracers.pib.slope"] = "93.7";
  configMap["centiloid.tracers.pib.intercept"] = "-94.6";
  configMap["centiloid.tracers.fbp.slope"] = "175.4";
  configMap["centiloid.tracers.fbp.intercept"] = "-182.3";
  configMap["centiloid.tracers.fbb.slope"] = "153.4";
  configMap["centiloid.tracers.fbb.intercept"] = "-154.9";
  configMap["centiloid.tracers.fmm.slope"] = "121.4";
  configMap["centiloid.tracers.fmm.intercept"] = "-121.2";
  configMap["centiloid.tracers.nav.slope"] = "85.2";
  configMap["centiloid.tracers.nav.intercept"] = "-87.6";
}

std::string Configuration::getString(const std::string &key,
                                     const std::string &defaultValue) const {
  auto it = configMap.find(key);
  return (it != configMap.end()) ? it->second : defaultValue;
}

int Configuration::getInt(const std::string &key, int defaultValue) const {
  try {
    return std::stoi(getString(key, std::to_string(defaultValue)));
  } catch (...) {
    return defaultValue;
  }
}

float Configuration::getFloat(const std::string &key,
                              float defaultValue) const {
  try {
    return std::stof(getString(key, std::to_string(defaultValue)));
  } catch (...) {
    return defaultValue;
  }
}

bool Configuration::getBool(const std::string &key, bool defaultValue) const {
  std::string value = getString(key, defaultValue ? "true" : "false");
  std::transform(value.begin(), value.end(), value.begin(), ::tolower);
  return value == "true" || value == "1" || value == "yes";
}

std::string Configuration::getModelPath(const std::string &modelName) const {
  return executableDir + "/" + getString("models." + modelName);
}

std::vector<std::string>
Configuration::getModelPaths(const std::string &modelName) const {
  std::vector<std::string> result;
  auto it = listMap.find("models." + modelName);
  if (it == listMap.end()) return result;
  result.reserve(it->second.size());
  for (const auto &rel : it->second) {
    // 统一视为相对可执行目录的路径
    result.emplace_back(executableDir + "/" + rel);
  }
  return result;
}

std::string
Configuration::getTemplatePath(const std::string &templateName) const {
  return executableDir + "/" + getString("templates." + templateName);
}

std::string Configuration::getMaskPath(const std::string &maskName) const {
  return executableDir + "/" + getString("masks." + maskName);
}

std::map<std::string, std::string>
Configuration::getSection(const std::string &section) const {
  std::map<std::string, std::string> result;
  std::string prefix = section + ".";

  for (const auto &[key, value] : configMap) {
    if (key.substr(0, prefix.length()) == prefix) {
      std::string subKey = key.substr(prefix.length());
      result[subKey] = value;
    }
  }

  return result;
}

void Configuration::setString(const std::string &key,
                              const std::string &value) {
  configMap[key] = value;
}

bool Configuration::loadFromFile(const std::string &configPath) {
  try {
    toml::table config = toml::parse_file(configPath);
    configMap.clear(); // 清空现有配置
    listMap.clear();   // 清空数组配置
    flattenTomlTable(config); // 递归解析TOML表并扁平化
    collectArraysFromToml(config); // 收集数组（如模型集成列表）
    return true;
  } catch (const toml::parse_error &err) {
    std::cerr << "Error parsing TOML config file " << configPath << ": "
              << err.description() << std::endl;
    return false;
  } catch (const std::exception &e) {
    std::cerr << "Error loading config file " << configPath << ": " << e.what()
              << std::endl;
    return false;
  }
}

bool Configuration::loadDefaults() {
  initializeDefaults();
  return true;
}

std::string Configuration::getTempDirPath() const {
  std::string tempDir = getString("temp_dir", "./tmp");

  // 只有当使用默认的./tmp路径时才自动创建目录
  if (tempDir == "./tmp") {
    std::string fullTempPath = executableDir + "/" + tempDir;
    if (!std::filesystem::exists(fullTempPath)) {
      std::filesystem::create_directories(fullTempPath);
    }
    return fullTempPath;
  }

  // 如果不是默认路径，直接返回配置的路径（可能是绝对路径或其他相对路径）
  return tempDir;
}

std::string Configuration::findConfigFile(const std::string &configFileName) {
  // 先检查当前工作目录
  if (std::filesystem::exists(configFileName)) {
    return configFileName;
  }

  // 获取可执行文件目录
  std::string executableDir = Common::getExecutablePath();

  // 检查可执行文件目录下的configs文件夹
  std::string configsPath = executableDir + "/assets/configs/" + configFileName;
  if (std::filesystem::exists(configsPath)) {
    return configsPath;
  }

  // 检查可执行文件目录下的文件
  std::string execDirPath = executableDir + "/" + configFileName;
  if (std::filesystem::exists(execDirPath)) {
    return execDirPath;
  }

  // 如果都找不到，返回原始文件名（让调用者处理错误）
  return configFileName;
}

void Configuration::printAllConfigurations() const {
  std::cout << "\n=== Configuration Settings ===" << std::endl;
  std::cout << "Executable Directory: " << executableDir << std::endl;
  std::cout << "\n--- Model Paths ---" << std::endl;
  std::cout << "rigid: " << getModelPath("rigid") << std::endl;
  std::cout << "affine_voxelmorph: " << getModelPath("affine_voxelmorph")
            << std::endl;
  std::cout << "abeta_decoupler: " << getModelPath("abeta_decoupler")
            << std::endl;
  std::cout << "tau_decoupler: " << getModelPath("tau_decoupler") << std::endl;

  std::cout << "\n--- Template Paths ---" << std::endl;
  std::cout << "adni_pet_core: " << getTemplatePath("adni_pet_core")
            << std::endl;
  std::cout << "padded: " << getTemplatePath("padded") << std::endl;

  std::cout << "\n--- Mask Paths ---" << std::endl;
  std::cout << "cerebral_gray: " << getMaskPath("cerebral_gray") << std::endl;
  std::cout << "centiloid_voi: " << getMaskPath("centiloid_voi") << std::endl;
  std::cout << "whole_cerebral: " << getMaskPath("whole_cerebral") << std::endl;
  std::cout << "centaur_voi: " << getMaskPath("centaur_voi") << std::endl;
  std::cout << "centaur_ref: " << getMaskPath("centaur_ref") << std::endl;

  std::cout << "\n--- Processing Parameters ---" << std::endl;
  std::cout << "max_iter: " << getInt("processing.max_iter") << std::endl;
  std::cout << "ac_diff_threshold: " << getFloat("processing.ac_diff_threshold")
            << std::endl;
  std::cout << "temp_dir: " << getTempDirPath() << std::endl;

  std::cout << "\n--- Centiloid Parameters ---" << std::endl;
  auto centiloidSection = getSection("centiloid.tracers");
  for (const auto &[key, value] : centiloidSection) {
    std::cout << key << ": " << value << std::endl;
  }
  std::cout << "=========================" << std::endl;
}

void Configuration::flattenTomlTable(const toml::table &table, const std::string &prefix) {
  for (auto &[key, value] : table) {
    std::string fullKey = prefix.empty() ? std::string(key) : prefix + "." + std::string(key);
    
    // 处理不同类型的TOML值
    if (value.is_table()) {
      // 递归处理子表
      flattenTomlTable(*value.as_table(), fullKey);
    } else if (value.is_array()) {
      // 数组将由独立的收集函数处理
    } else if (value.is_string()) {
      configMap[fullKey] = value.as_string()->get();
    } else if (value.is_integer()) {
      configMap[fullKey] = std::to_string(value.as_integer()->get());
    } else if (value.is_floating_point()) {
      configMap[fullKey] = std::to_string(value.as_floating_point()->get());
    } else if (value.is_boolean()) {
      configMap[fullKey] = value.as_boolean()->get() ? "true" : "false";
    }
    // 可以添加对数组等其他类型的处理
  }
}

void Configuration::collectArraysFromToml(const toml::table &table, const std::string &prefix) {
  for (auto &[key, value] : table) {
    std::string fullKey = prefix.empty() ? std::string(key) : prefix + "." + std::string(key);
    if (value.is_table()) {
      collectArraysFromToml(*value.as_table(), fullKey);
      continue;
    }
    if (!value.is_array()) continue;

    const auto &arr = *value.as_array();
    bool allStrings = true;
    std::vector<std::string> items;
    items.reserve(arr.size());
    for (const auto &elem : arr) {
      if (elem.is_string()) {
        items.push_back(elem.as_string()->get());
      } else {
        allStrings = false;
        break;
      }
    }
    if (allStrings) {
      listMap[fullKey] = std::move(items);
    }
  }
}
