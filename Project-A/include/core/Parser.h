#pragma once

#include <map>
#include <string>
#include <string_view>
#include <unordered_set>
#include <vector>

#include "core/Task.h"

// ---------------------------------------------------------------------------
// Constants
// ---------------------------------------------------------------------------

// Scheduling algorithms accepted in the first line of the file (uppercase).
inline const std::unordered_set<std::string_view> VALID_ALGORITHMS = {"RM", "EDF"};

// Constants to the system config
inline constexpr int MAX_CPUS = 64;
inline constexpr int MAX_QUANTUM = 1000000;

// ---------------------------------------------------------------------------
// Messages
// ---------------------------------------------------------------------------

enum class Severity { Error, Warning };

enum class MsgCode {
    // Errors: there is no sensible default, so the item cannot be used
    EmptyFile,
    NoValidTasks,
    TaskMissingField,
    TaskInvalidValue,
    TaskDuplicateId,

    // Warnings: a default value is used or the item is ignored
    SystemMissingValue,
    SystemInvalidValue,
    UnknownAlgorithm,
    InvalidColor,
    DuplicateColor,
    AperiodicIgnored,
    ExtraFields,
};

struct MessageInfo {
    Severity severity;
    std::string_view text;  // each "{}" is replaced by an argument, in order
};

inline const std::map<MsgCode, MessageInfo> MESSAGES = {
    {MsgCode::EmptyFile,
     {Severity::Error, "Configuration file is empty."}},
    {MsgCode::NoValidTasks,
     {Severity::Error, "No valid task found. At least one periodic task is required."}},
    {MsgCode::TaskMissingField,
     {Severity::Error, "Line {}: task cannot be added. Reason: field '{}' is missing."}},
    {MsgCode::TaskInvalidValue,
     {Severity::Error, "Line {}: task cannot be added. Reason: field '{}' has invalid value ({})."}},
    {MsgCode::TaskDuplicateId,
     {Severity::Error, "Line {}: task cannot be added. Reason: id '{}' is already used on line {}."}},
    {MsgCode::AperiodicIgnored,
     {Severity::Error, "Line {}: task cannot be added. Reason: task is aperiodic (period {})"}},

    {MsgCode::SystemMissingValue,
     {Severity::Warning, "Line {}: '{}' not provided. Using default '{}'."}},
    {MsgCode::SystemInvalidValue,
     {Severity::Warning, "Line {}: invalid value '{}' for '{}'. Using default '{}'."}},
    {MsgCode::UnknownAlgorithm,
     {Severity::Warning, "Line {}: unknown scheduling algorithm '{}'. Using default '{}'."}},
    {MsgCode::InvalidColor,
     {Severity::Warning, "Line {}: invalid color '{}' for task '{}'. Using default '{}'."}},
    {MsgCode::DuplicateColor,
     {Severity::Warning, "Line {}: color '{}' of task '{}' is already used by task '{}'."}},
    {MsgCode::ExtraFields,
     {Severity::Warning, "Line {}: extra fields were ignored."}},
};

// ---------------------------------------------------------------------------
// Result of the parser
// ---------------------------------------------------------------------------

// System configuration. The initial values are the defaults [A3.2].
struct Config {
    std::string algorithm = "RM";
    int quantum = 2;
    int cpuCount = 1;
    std::vector<Task> tasks;  // file fields filled in; runtime fields keep their defaults
};

struct ParseResult {
    Config config;
    std::vector<std::string> errors;    // if not empty, the simulation must not run
    std::vector<std::string> warnings;  // shown to the user, but the simulation can run
};

// ---------------------------------------------------------------------------
// Public function
// ---------------------------------------------------------------------------

// Reads the configuration text (not the file path) and returns the
// configuration found, plus all errors and warnings.
//
// Text format:
//   algorithm;quantum;cpu_count
//   id;color;arrival;duration;period;deadline;event_list
namespace parser {
    ParseResult parseConfig(const std::string& text);
}