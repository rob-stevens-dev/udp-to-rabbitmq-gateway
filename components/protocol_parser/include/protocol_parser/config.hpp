/**
 * @file config.hpp
 * @brief Configuration utilities for the protocol parser
 * 
 * This file provides utilities for loading configuration from JSON files.
 */

#pragma once

#include "protocol_parser/parser.hpp"
#include "protocol_parser/validator.hpp"
#include "protocol_parser/builder.hpp"
#include "protocol_parser/types.hpp"

#include <string>
#include <fstream>

// Forward declare nlohmann::json to avoid any ambiguity issues
namespace nlohmann {
    class json;
}

namespace protocol_parser {

/**
 * @brief Configuration utilities
 */
class Config {
public:
    /**
     * @brief Load parser configuration from JSON file
     * @param filepath Path to JSON configuration file
     * @return Parser configuration
     * @throws std::runtime_error if file cannot be opened or parsed
     */
    static Parser::Config loadParserConfig(const std::string& filepath);
    
    /**
     * @brief Load validator configuration from JSON file
     * @param filepath Path to JSON configuration file
     * @return Validator configuration
     * @throws std::runtime_error if file cannot be opened or parsed
     */
    static Validator::Config loadValidatorConfig(const std::string& filepath);
    
    /**
     * @brief Load command builder configuration from JSON file
     * @param filepath Path to JSON configuration file
     * @return Command builder configuration
     * @throws std::runtime_error if file cannot be opened or parsed
     */
    static CommandBuilder::Config loadCommandBuilderConfig(const std::string& filepath);
    
    /**
     * @brief Parse parser configuration from JSON
     * @param json JSON object
     * @return Parser configuration
     */
    static Parser::Config parseParserConfig(const nlohmann::json& json);
    
    /**
     * @brief Parse validator configuration from JSON
     * @param json JSON object
     * @return Validator configuration
     */
    static Validator::Config parseValidatorConfig(const nlohmann::json& json);
    
    /**
     * @brief Parse command builder configuration from JSON
     * @param json JSON object
     * @return Command builder configuration
     */
    static CommandBuilder::Config parseCommandBuilderConfig(const nlohmann::json& json);

private:
    /**
     * @brief Load JSON from file
     * @param filepath Path to JSON file
     * @return JSON object
     * @throws std::runtime_error if file cannot be opened or parsed
     */
    static nlohmann::json loadJsonFromFile(const std::string& filepath);
};

} // namespace protocol_parser