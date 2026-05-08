/**
 * @file main.cpp
 * @brief Linux ARM HMI command line entry for direct-connected displays.
 */

#include "lvgl_lua_bindings.h"

#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <limits>
#include <string>

#include <unistd.h>

namespace {

struct Options {
    std::string scriptPath;
    std::string drmPath = "/dev/dri/card0";
    std::string pointerPath;
    std::string keypadPath;
    std::string logPath;
    int64_t connectorId = -1;
};

void printUsage(const char * programName)
{
    std::cout
        << "Usage: " << programName << " <script.lua> [options]\n\n"
        << "Options:\n"
        << "  --drm <path>          DRM card path. Default: /dev/dri/card0\n"
        << "  --connector <id>      DRM connector id. Default: -1 (auto)\n"
        << "  --touch <path>        evdev pointer/touch input path\n"
        << "  --pointer <path>      Alias of --touch\n"
        << "  --keypad <path>       evdev keypad/keyboard input path\n"
        << "  --log <path>          Redirect stdout/stderr to a log file\n"
        << "  -h, --help            Show this help\n"
        << "  -v, --version         Show version\n";
}

bool startsWith(const std::string & value, const char * prefix)
{
    const size_t prefixLen = std::strlen(prefix);
    return value.size() >= prefixLen && value.compare(0, prefixLen, prefix) == 0;
}

bool readOptionValue(int & index,
                     int argc,
                     char * argv[],
                     const std::string & current,
                     const char * name,
                     std::string & outValue)
{
    const std::string withEquals = std::string(name) + "=";
    if (startsWith(current, withEquals.c_str())) {
        outValue = current.substr(withEquals.size());
        return true;
    }

    if (current == name) {
        if (index + 1 >= argc) {
            std::cerr << "Missing value for " << name << "\n";
            return false;
        }
        outValue = argv[++index];
        return true;
    }

    return false;
}

bool parseInt64(const std::string & text, int64_t & outValue)
{
    errno = 0;
    char * end = nullptr;
    long long value = std::strtoll(text.c_str(), &end, 10);
    if (errno != 0 || end == text.c_str() || (end && *end != '\0')) {
        return false;
    }
    if (value < std::numeric_limits<int64_t>::min() ||
        value > std::numeric_limits<int64_t>::max()) {
        return false;
    }

    outValue = static_cast<int64_t>(value);
    return true;
}

bool parseArguments(int argc, char * argv[], Options & options)
{
    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];

        if (arg == "-h" || arg == "--help") {
            printUsage(argv[0]);
            std::exit(0);
        }
        if (arg == "-v" || arg == "--version") {
            std::cout << "hmi 1.0.0\n";
            std::exit(0);
        }

        std::string value;
        if (readOptionValue(i, argc, argv, arg, "--drm", value)) {
            options.drmPath = value;
            continue;
        }
        if (readOptionValue(i, argc, argv, arg, "--connector", value)) {
            if (!parseInt64(value, options.connectorId)) {
                std::cerr << "Invalid connector id: " << value << "\n";
                return false;
            }
            continue;
        }
        if (readOptionValue(i, argc, argv, arg, "--touch", value) ||
            readOptionValue(i, argc, argv, arg, "--pointer", value)) {
            options.pointerPath = value;
            continue;
        }
        if (readOptionValue(i, argc, argv, arg, "--keypad", value)) {
            options.keypadPath = value;
            continue;
        }
        if (readOptionValue(i, argc, argv, arg, "--log", value)) {
            options.logPath = value;
            continue;
        }

        if (!arg.empty() && arg[0] == '-') {
            std::cerr << "Unknown option: " << arg << "\n";
            return false;
        }

        if (!options.scriptPath.empty()) {
            std::cerr << "Only one Lua script path can be specified\n";
            return false;
        }
        options.scriptPath = arg;
    }

    if (options.scriptPath.empty()) {
        std::cerr << "Missing Lua script path\n";
        return false;
    }

    if (access(options.scriptPath.c_str(), R_OK) != 0) {
        std::cerr << "Lua script is not readable: " << options.scriptPath
                  << " (" << std::strerror(errno) << ")\n";
        return false;
    }

    return true;
}

bool setupLogFile(const std::string & logPath)
{
    if (logPath.empty()) return true;

    if (std::freopen(logPath.c_str(), "a", stdout) == nullptr) {
        std::cerr << "Failed to redirect stdout to log file: " << logPath << "\n";
        return false;
    }
    if (std::freopen(logPath.c_str(), "a", stderr) == nullptr) {
        std::fprintf(stdout, "Failed to redirect stderr to log file: %s\n", logPath.c_str());
        return false;
    }

    std::setvbuf(stdout, nullptr, _IOLBF, 0);
    std::setvbuf(stderr, nullptr, _IONBF, 0);
    return true;
}

} // namespace

int main(int argc, char * argv[])
{
    Options options;
    if (!parseArguments(argc, argv, options)) {
        printUsage(argv[0]);
        return 1;
    }

    if (!setupLogFile(options.logPath)) {
        return 2;
    }

    std::fprintf(stdout,
                 "[hmi] script=%s drm=%s connector=%lld pointer=%s keypad=%s\n",
                 options.scriptPath.c_str(),
                 options.drmPath.c_str(),
                 static_cast<long long>(options.connectorId),
                 options.pointerPath.empty() ? "(none)" : options.pointerPath.c_str(),
                 options.keypadPath.empty() ? "(none)" : options.keypadPath.c_str());
    std::fflush(stdout);

    return lvgl_hmi_run(options.scriptPath.c_str(),
                        options.drmPath.c_str(),
                        options.connectorId,
                        options.pointerPath.empty() ? nullptr : options.pointerPath.c_str(),
                        options.keypadPath.empty() ? nullptr : options.keypadPath.c_str());
}