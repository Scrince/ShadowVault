#pragma once

#include <sv/result.hpp>

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace sv::cli {

struct GlobalOptions {
    bool help{false};
    bool version{false};
    bool json{false};
    bool quiet{false};
    bool verbose{false};
    bool portable{false};
    bool no_host_traces{false};
    std::optional<std::filesystem::path> config_path;
    std::optional<std::filesystem::path> vault_path;
};

class Output {
public:
    explicit Output(const GlobalOptions& options);

    void info(const std::string& message) const;
    void verbose(const std::string& message) const;
    int error(ErrorCode code, const std::string& message) const;
    int success(const std::string& message = {}) const;
    int emit_json(const std::string& json_body) const;

    [[nodiscard]] bool json_enabled() const { return options_.json; }
    [[nodiscard]] const GlobalOptions& options() const { return options_; }

private:
    GlobalOptions options_;
};

std::string json_escape(const std::string& value);
std::string json_string(const std::string& value);
std::string json_path(const std::filesystem::path& path);

} // namespace sv::cli