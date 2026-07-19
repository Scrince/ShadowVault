#include <sv/cli/output.hpp>

#include <iostream>
#include <sstream>

namespace sv::cli {

namespace {

std::string indent_json(const std::string& body) {
    if (body.empty()) {
        return "{}";
    }
    return body;
}

} 

Output::Output(const GlobalOptions& options) : options_(options) {}

void Output::info(const std::string& message) const {
    if (options_.quiet || message.empty()) {
        return;
    }
    if (options_.json) {
        return;
    }
    std::cout << message << '\n';
}

void Output::verbose(const std::string& message) const {
    if (!options_.verbose || message.empty()) {
        return;
    }
    if (options_.json) {
        return;
    }
    std::cerr << message << '\n';
}

int Output::error(ErrorCode code, const std::string& message) const {
    if (options_.json) {
        std::ostringstream json;
        json << "{\n";
        json << "  \"status\": \"error\",\n";
        json << "  \"code\": " << json_string(error_code_name(code)) << ",\n";
        json << "  \"exit_code\": " << exit_code_for(code) << ",\n";
        json << "  \"message\": " << json_string(message) << "\n";
        json << '}';
        std::cout << json.str() << '\n';
    } else {
        
        
        std::cout << "error: " << message << '\n';
    }
    return exit_code_for(code);
}

int Output::success(const std::string& message) const {
    if (!message.empty()) {
        info(message);
    }
    return 0;
}

int Output::emit_json(const std::string& json_body) const {
    std::cout << indent_json(json_body) << '\n';
    return 0;
}

std::string json_escape(const std::string& value) {
    std::string escaped;
    escaped.reserve(value.size());
    for (const char ch : value) {
        switch (ch) {
        case '\\':
            escaped += "\\\\";
            break;
        case '"':
            escaped += "\\\"";
            break;
        case '\n':
            escaped += "\\n";
            break;
        case '\r':
            escaped += "\\r";
            break;
        case '\t':
            escaped += "\\t";
            break;
        default:
            escaped += ch;
            break;
        }
    }
    return escaped;
}

std::string json_string(const std::string& value) {
    return '"' + json_escape(value) + '"';
}

std::string json_path(const std::filesystem::path& path) {
    return json_string(path.generic_string());
}

} 