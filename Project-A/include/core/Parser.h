#pragma once

#include <cstdint>
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
    AperiodicIgnored,
    SystemMissingValue,
    SystemInvalidValue,
    UnknownAlgorithm,
    InvalidColor,
    DuplicateColor,
    ExtraFields,
};

struct MessageInfo {
    Severity severity;
    std::string_view text;  // each "{}" is replaced by an argument, in order
};

// Textos mostrados ao usuário: em português, sempre com o motivo [A3.3.6, A5].
inline const std::map<MsgCode, MessageInfo> MESSAGES = {
    {MsgCode::EmptyFile,
     {Severity::Error, "O arquivo de configuração está vazio."}},
    {MsgCode::NoValidTasks,
     {Severity::Error, "Nenhuma tarefa válida encontrada. É necessária ao menos uma tarefa periódica."}},
    {MsgCode::TaskMissingField,
     {Severity::Error, "Linha {}: a tarefa não pode ser adicionada. Motivo: o campo '{}' está ausente."}},
    {MsgCode::TaskInvalidValue,
     {Severity::Error, "Linha {}: a tarefa não pode ser adicionada. Motivo: o campo '{}' tem o valor inválido '{}' ({})."}},
    {MsgCode::TaskDuplicateId,
     {Severity::Error, "Linha {}: a tarefa não pode ser adicionada. Motivo: o id '{}' já é usado na linha {}."}},

    {MsgCode::AperiodicIgnored,
     {Severity::Warning, "Linha {}: a tarefa '{}' é aperiódica (período 0) e será ignorada no Projeto A."}},
    {MsgCode::SystemMissingValue,
     {Severity::Warning, "Linha {}: '{}' não informado. Usando o padrão '{}'."}},
    {MsgCode::SystemInvalidValue,
     {Severity::Warning, "Linha {}: valor inválido '{}' para '{}'. Usando o padrão '{}'."}},
    {MsgCode::UnknownAlgorithm,
     {Severity::Warning, "Linha {}: algoritmo de escalonamento desconhecido '{}'. Usando o padrão '{}'."}},
    {MsgCode::InvalidColor,
     {Severity::Warning, "Linha {}: cor inválida '{}' na tarefa '{}'. Usando '{}'."}},
    {MsgCode::DuplicateColor,
     {Severity::Warning, "Linha {}: a cor '{}' da tarefa '{}' já é usada na linha {}. Usando '{}' no lugar."}},
    {MsgCode::ExtraFields,
     {Severity::Warning, "Linha {}: campos a mais foram ignorados."}},
};

// ---------------------------------------------------------------------------
// Result of the parser
// ---------------------------------------------------------------------------

// System configuration. The initial values are the defaults [A3.2].
struct Config {
    std::string algorithm = "RM";
    int quantum = 2;
    int cpuCount = 1;
    uint64_t seed = 1;        // semente do sorteio (D14); alterável na interface
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