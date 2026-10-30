#include "core/Parser.h"

#include <cctype>
#include <charconv>
#include <sstream>

// Everything inside this unnamed namespace is private to this file.
namespace {

// ---------------------------------------------------------------------------
// Messages
// ---------------------------------------------------------------------------

// Replaces each "{}" in the template with the next argument.
std::string formatMessage(std::string_view tmpl, const std::vector<std::string>& args) {
    std::string out;
    std::size_t next = 0;

    for (std::size_t i = 0; i < tmpl.size(); ++i) {
        bool isPlaceholder = tmpl[i] == '{' && i + 1 < tmpl.size() && tmpl[i + 1] == '}';

        if (isPlaceholder && next < args.size()) {
            out += args[next];
            ++next;
            ++i;  // skip the "}"
        } else {
            out += tmpl[i];
        }
    }
    return out;
}

// Builds the message text and stores it as an error or a warning.
void addMessage(ParseResult& result, MsgCode code, const std::vector<std::string>& args = {}) {
    const MessageInfo& info = MESSAGES.at(code);
    std::string text = formatMessage(info.text, args);

    if (info.severity == Severity::Error) {
        result.errors.push_back("[ERROR] " + text);
    } else {
        result.warnings.push_back("[WARNING] " + text);
    }
}

// ---------------------------------------------------------------------------
// Text helpers
// ---------------------------------------------------------------------------

// Removes spaces, tabs and line breaks from both ends.
std::string trim(const std::string& s) {
    const char* spaces = " \t\r\n";

    std::size_t first = s.find_first_not_of(spaces);
    if (first == std::string::npos) {
        return "";
    }
    std::size_t last = s.find_last_not_of(spaces);
    return s.substr(first, last - first + 1);
}

// Returns a copy of the text in uppercase [A3.3.2].
std::string toUpper(std::string s) {
    for (char& c : s) {
        c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
    }
    return s;
}

// Splits a line by ';'. Empty fields are KEPT, because the position of a
// field defines its meaning (";2;10" means: no algorithm, quantum 2, 10 CPUs).
std::vector<std::string> splitFields(const std::string& line) {
    std::vector<std::string> fields;
    std::string current;

    for (char c : line) {
        if (c == ';') {
            fields.push_back(trim(current));
            current.clear();
        } else {
            current += c;
        }
    }
    fields.push_back(trim(current));

    // A ';' at the end of the line does not create a new field [A3.3.3].
    if (fields.size() > 1 && fields.back().empty()) {
        fields.pop_back();
    }
    return fields;
}

// Returns the field at the given position, or "" if the line is too short.
// This way, a missing field and an empty field are handled the same way.
std::string fieldAt(const std::vector<std::string>& fields, std::size_t index) {
    if (index < fields.size()) {
        return fields[index];
    }
    return "";
}

// Converts text to int. Returns false if the WHOLE text is not an integer
// (so "12abc" is rejected, unlike std::stoi). Never throws.
// Template: serve para int (linha do sistema) e Tick (campos de tempo da tarefa).
template <typename Int>
bool parseInt(const std::string& text, Int& out) {
    if (text.empty()) {
        return false;
    }
    const char* begin = text.data();
    const char* end = text.data() + text.size();

    auto [stop, error] = std::from_chars(begin, end, out);
    return error == std::errc() && stop == end;
}

// ---------------------------------------------------------------------------
// First line: algorithm;quantum;cpu_count
// ---------------------------------------------------------------------------

// Reads an integer system field. If it is missing or invalid, the default
// value already stored in "target" is kept and a warning is added.
void readSystemInt(const std::vector<std::string>& fields, std::size_t index,
                   const std::string& name, int minValue, int maxValue,
                   int& target, int lineNumber, ParseResult& result) {
    std::string line = std::to_string(lineNumber);
    std::string raw = fieldAt(fields, index);
    int value = 0;

    if (raw.empty()) {
        addMessage(result, MsgCode::SystemMissingValue, {line, name, std::to_string(target)});
    } else if (!parseInt(raw, value) || value < minValue || value > maxValue) {
        addMessage(result, MsgCode::SystemInvalidValue, {line, raw, name, std::to_string(target)});
    } else {
        target = value;
    }
}

void parseSystemLine(const std::vector<std::string>& fields, int lineNumber, ParseResult& result) {
    std::string line = std::to_string(lineNumber);
    Config& config = result.config;

    // Field 0: algorithm (compared in uppercase, so "rm" == "RM")
    std::string rawAlgorithm = fieldAt(fields, 0);
    std::string algorithm = toUpper(rawAlgorithm);

    if (algorithm.empty()) {
        addMessage(result, MsgCode::SystemMissingValue, {line, "algorithm", config.algorithm});
    } else if (VALID_ALGORITHMS.count(algorithm) == 0) {
        addMessage(result, MsgCode::UnknownAlgorithm, {line, rawAlgorithm, config.algorithm});
    } else {
        config.algorithm = algorithm;
    }

    // Field 1: quantum (at least 1)
    readSystemInt(fields, 1, "quantum", 1, MAX_QUANTUM, config.quantum, lineNumber, result);

    // Field 2: number of CPUs (between 1 and MAX_CPUS) [RG2]
    readSystemInt(fields, 2, "cpu count", 1, MAX_CPUS, config.cpuCount, lineNumber, result);

    if (fields.size() > 3) {
        addMessage(result, MsgCode::ExtraFields, {line});
    }
}

// ---------------------------------------------------------------------------
// Other lines: id;color;arrival;duration;period;deadline;event_list
// ---------------------------------------------------------------------------
// Checks if the text has exactly 6 hexadecimal digits (e.g. "F0E0D0").
bool isValidHexColor(const std::string& color) {
    if (color.size() != 6) {
        return false;
    }
    for (char c : color) {
        if (!std::isxdigit(static_cast<unsigned char>(c))) {  // 0-9, a-f, A-F
            return false;
        }
    }
    return true;
}

// Returns a color that no task is using yet, so every task keeps a
// different color in the Gantt chart [A2.1].
std::string nextFreeColor(const std::map<std::string, int>& usedColors) {
    static const std::vector<std::string> PALETTE = {
        "E6194B", "3CB44B", "FFE119", "4363D8", "F58231", "911EB4",
        "46F0F0", "F032E6", "BCF60C", "FABEBE", "008080", "E6BEFF",
    };
    for (const std::string& color : PALETTE) {
        if (usedColors.count(color) == 0) {
            return color;
        }
    }
    // Palette exhausted: walk through other colors until a free one is found.
    for (int value = 0x101010; ; value += 0x0F0F37) {
        char buffer[7];
        std::snprintf(buffer, sizeof(buffer), "%06X", value & 0xFFFFFF);
        if (usedColors.count(buffer) == 0) {
            return buffer;
        }
    }
}

// Reads an integer task field. Returns false (and adds an error) if it is
// missing, not a number, or smaller than minValue.
bool readTaskInt(const std::vector<std::string>& fields, std::size_t index,
                 const std::string& name, Tick minValue,
                 Tick& target, int lineNumber, ParseResult& result) {
    std::string line = std::to_string(lineNumber);
    std::string raw = fieldAt(fields, index);
    Tick value = 0;

    if (raw.empty()) {
        addMessage(result, MsgCode::TaskMissingField, {line, name});
        return false;
    }
    if (!parseInt(raw, value) || value < minValue) {
        std::string reason = "must be an integer >= " + std::to_string(minValue);
        addMessage(result, MsgCode::TaskInvalidValue, {line, name, raw, reason});
        return false;
    }
    target = value;
    return true;
}

// usedIds and usedColors map each id/color to the line where it first appeared.
void parseTaskLine(const std::vector<std::string>& fields, int lineNumber, ParseResult& result,
                   std::map<std::string, int>& usedIds, std::map<std::string, int>& usedColors) {
    std::string line = std::to_string(lineNumber);
    Task task;       // built locally, added only at the end
    bool ok = true;  // becomes false on any error, but we keep checking the
                     // other fields so the user sees all errors at once

    // Field 0: id (required and unique)
    task.id = fieldAt(fields, 0);
    if (task.id.empty()) {
        addMessage(result, MsgCode::TaskMissingField, {line, "id"});
        ok = false;
    } else if (usedIds.count(task.id) > 0) {
        addMessage(result, MsgCode::TaskDuplicateId,
                   {line, task.id, std::to_string(usedIds.at(task.id))});
        ok = false;
    } else {
        usedIds[task.id] = lineNumber;
    }

    // Fields 2 to 5: integers
    ok = readTaskInt(fields, 2, "arrival",  0, task.arrival,  lineNumber, result) && ok;
    ok = readTaskInt(fields, 3, "duration", 1, task.duration, lineNumber, result) && ok;
    ok = readTaskInt(fields, 4, "period",   0, task.period,   lineNumber, result) && ok;  // < 0 is an error [A3.3]
    ok = readTaskInt(fields, 5, "deadline", 1, task.deadline, lineNumber, result) && ok;

    // Field 6: event list (optional, kept as raw text for Project B)
    // task.rawEvents = fieldAt(fields, 6);

    if (!ok) {
        return;  // at least one error: task is not added
    }
    if (task.period == 0) {
        addMessage(result, MsgCode::AperiodicIgnored, {line, task.id});  // [A4.4]
        return;
    }

    // Field 1: color. Checked last, so only tasks that are really added
    // reserve a color. Uppercase so "ff0000" and "FF0000" are the same [A3.3.2].
    std::string rawColor = fieldAt(fields, 1);
    std::string color = toUpper(rawColor);

    if (!isValidHexColor(color)) {
        color = nextFreeColor(usedColors);
        addMessage(result, MsgCode::InvalidColor, {line, rawColor, task.id, color});
    } else if (usedColors.count(color) > 0) {
        std::string firstLine = std::to_string(usedColors.at(color));
        std::string newColor = nextFreeColor(usedColors);
        addMessage(result, MsgCode::DuplicateColor, {line, color, task.id, firstLine, newColor});
        color = newColor;
    }

    task.color = color;
    usedColors[color] = lineNumber;
    result.config.tasks.push_back(task);
}
}  // namespace

// ---------------------------------------------------------------------------
// Public function
// ---------------------------------------------------------------------------

ParseResult parser::parseConfig(const std::string& text) {
    ParseResult result;
    std::stringstream stream(text);
    std::string line;
    int lineNumber = 0;
    bool systemLineRead = false;
    std::map<std::string, int> usedIds;
    std::map<std::string, int> usedColors;

    while (std::getline(stream, line)) {
        ++lineNumber;  // counts every line, so messages match the file

        line = trim(line);
        if (line.empty()) {
            continue;  // blank lines are not errors [A3.3.6]
        }

        std::vector<std::string> fields = splitFields(line);

        // The first non-empty line is the system line; the others are tasks.
        if (!systemLineRead) {
            parseSystemLine(fields, lineNumber, result);
            systemLineRead = true;
        } else {
            parseTaskLine(fields, lineNumber, result, usedIds, usedColors);
        }
    }

    if (!systemLineRead) {
        addMessage(result, MsgCode::EmptyFile);
    } else if (result.config.tasks.empty()) {
        addMessage(result, MsgCode::NoValidTasks);
    }

    return result;
}