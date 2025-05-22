/**
 * @file config.cpp
 * @brief Implementation of the configuration utilities
 */

#include "protocol_parser/config.hpp"

// Include the full implementation of nlohmann::json
// Use a scoped include to avoid leaking the json class into the global namespace
#include <fstream>

namespace {
    // Include json.hpp in a nested anonymous namespace to isolate it
    #include <nlohmann/json.hpp>
}

namespace protocol_parser {

Parser::Config Config::loadParserConfig(const std::string& filepath) {
    nlohmann::json json = loadJsonFromFile(filepath);
    return parseParserConfig(json);
}

Validator::Config Config::loadValidatorConfig(const std::string& filepath) {
    nlohmann::json json = loadJsonFromFile(filepath);
    return parseValidatorConfig(json);
}

CommandBuilder::Config Config::loadCommandBuilderConfig(const std::string& filepath) {
    nlohmann::json json = loadJsonFromFile(filepath);
    return parseCommandBuilderConfig(json);
}

Parser::Config Config::parseParserConfig(const nlohmann::json& json) {
    Parser::Config config;
    
    if (json.contains("parser")) {
        const auto& parserJson = json["parser"];
        
        if (parserJson.contains("strictMode")) {
            config.strictMode = parserJson["strictMode"].get<bool>();
        }
        
        if (parserJson.contains("validateOnParse")) {
            config.validateOnParse = parserJson["validateOnParse"].get<bool>();
        }
        
        if (parserJson.contains("maxMessageSize")) {
            config.maxMessageSize = parserJson["maxMessageSize"].get<size_t>();
        }
        
        if (parserJson.contains("allowLegacyProtocol")) {
            config.allowLegacyProtocol = parserJson["allowLegacyProtocol"].get<bool>();
        }
        
        if (parserJson.contains("allocStrategy")) {
            const auto& strategyStr = parserJson["allocStrategy"].get<std::string>();
            if (strategyStr == "COPY") {
                config.allocStrategy = Parser::AllocStrategy::COPY;
            } else if (strategyStr == "REFERENCE") {
                config.allocStrategy = Parser::AllocStrategy::REFERENCE;
            } else if (strategyStr == "HYBRID") {
                config.allocStrategy = Parser::AllocStrategy::HYBRID;
            }
        }
        
        if (parserJson.contains("hybridThreshold")) {
            config.hybridThreshold = parserJson["hybridThreshold"].get<size_t>();
        }
    }
    
    // Load validator config if present
    if (json.contains("validator")) {
        config.validatorConfig = parseValidatorConfig(json);
    }
    
    return config;
}

Validator::Config Config::parseValidatorConfig(const nlohmann::json& json) {
    Validator::Config config;
    
    if (json.contains("validator")) {
        const auto& validatorJson = json["validator"];
        
        if (validatorJson.contains("strictMode")) {
            config.strictMode = validatorJson["strictMode"].get<bool>();
        }
        
        if (validatorJson.contains("validateChecksum")) {
            config.validateChecksum = validatorJson["validateChecksum"].get<bool>();
        }
        
        if (validatorJson.contains("validateRequired")) {
            config.validateRequired = validatorJson["validateRequired"].get<bool>();
        }
        
        if (validatorJson.contains("validateRanges")) {
            config.validateRanges = validatorJson["validateRanges"].get<bool>();
        }
        
        if (validatorJson.contains("allowUnknownFields")) {
            config.allowUnknownFields = validatorJson["allowUnknownFields"].get<bool>();
        }
        
        if (validatorJson.contains("minProtocolVersion")) {
            const auto& versionJson = validatorJson["minProtocolVersion"];
            config.minVersion.major = versionJson["major"].get<uint8_t>();
            config.minVersion.minor = versionJson["minor"].get<uint8_t>();
        }
        
        if (validatorJson.contains("maxProtocolVersion")) {
            const auto& versionJson = validatorJson["maxProtocolVersion"];
            config.maxVersion.major = versionJson["major"].get<uint8_t>();
            config.maxVersion.minor = versionJson["minor"].get<uint8_t>();
        }
    }
    
    return config;
}

CommandBuilder::Config Config::parseCommandBuilderConfig(const nlohmann::json& json) {
    CommandBuilder::Config config;
    
    if (json.contains("commandBuilder")) {
        const auto& builderJson = json["commandBuilder"];
        
        if (builderJson.contains("defaultProtocolVersion")) {
            const auto& versionJson = builderJson["defaultProtocolVersion"];
            config.version.major = versionJson["major"].get<uint8_t>();
            config.version.minor = versionJson["minor"].get<uint8_t>();
        }
        
        if (builderJson.contains("addChecksum")) {
            config.addChecksum = builderJson["addChecksum"].get<bool>();
        }
        
        if (builderJson.contains("validateOnBuild")) {
            config.validateOnBuild = builderJson["validateOnBuild"].get<bool>();
        }
    }
    
    return config;
}

nlohmann::json Config::loadJsonFromFile(const std::string& filepath) {
    std::ifstream file(filepath);
    if (!file.is_open()) {
        throw std::runtime_error("Failed to open configuration file: " + filepath);
    }
    
    try {
        nlohmann::json json;
        file >> json;
        return json;
    } catch (const nlohmann::json::exception& e) {
        throw std::runtime_error("Failed to parse configuration file: " + std::string(e.what()));
    }
}

} // namespace protocol_parser