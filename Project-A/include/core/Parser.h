#pragma once

#include <string>
#include <vector>

#include "core/Task.h"

struct Config {
    std::string algorithm = "rm";   // valores padrão [A3.2]
    int quantum = 2;
    int cpuCount = 1;
    std::vector<Task> tasks;        // campos do arquivo preenchidos; os de execução com o padrão
};

struct ParseResult {
    Config config;
    std::vector<std::string> errors;    // ex.: "Linha 4, campo cor: 'GG0000' não é hexadecimal"
    std::vector<std::string> warnings;  // ex.: "Linha 6: tarefa aperiódica ignorada no Projeto A"
};

// Responsible for parsing the text input, configuring tasks and setups
// and managing errors and warnings.