// =============================================================================
// Parser.cpp — Implementação do parser do arquivo de configuração (Parser.h).
//
// Fluxo de parseConfig:
//   1. normalizeText: tira o BOM e troca CRLF/CR por LF;
//   2. lê linha a linha contando TODAS as linhas (as mensagens apontam a linha
//      real do arquivo); linhas em branco são ignoradas [A3.3.6];
//   3. a 1ª linha útil é a do sistema (parseSystemLine); as demais são tarefas
//      (parseTaskLine), cada uma validada por inteiro para mostrar todos os
//      erros de uma vez;
//   4. regras do arquivo como um todo (mínimo de 2 linhas, alguma tarefa
//      simulável).
// serializeConfig escreve a Config de volta no mesmo formato.
//
// Requisitos e decisões: ver o cabeçalho de Parser.h.
// =============================================================================

#include "Parser.h"
#include "Scheduler.h"   // registro de algoritmos [A4.2]

#include <cctype>
#include <charconv>
#include <cstdint>
#include <cstdio>
#include <exception>
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
        result.errors.push_back("[ERRO] " + text);
    } else {
        result.warnings.push_back("[AVISO] " + text);
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

// Prepara o texto do arquivo para ser lido linha a linha (design.md §8, D18):
// - remove o BOM UTF-8 do início (o Bloco de Notas do Windows pode gravá-lo;
//   sem isso "EDF" viraria "\xEF\xBB\xBFEDF", um algoritmo desconhecido);
// - troca CRLF (Windows) e CR isolado (Mac antigo) por LF, para std::getline
//   separar as linhas certo em qualquer caso.
// Arquivo que obedece ao formato nunca pode falhar ao carregar [A3.3.7].
std::string normalizeText(const std::string& text) {
    std::size_t start = 0;
    if (text.compare(0, 3, "\xEF\xBB\xBF") == 0) {
        start = 3;
    }
    std::string out;
    out.reserve(text.size());
    for (std::size_t i = start; i < text.size(); ++i) {
        if (text[i] == '\r') {
            out += '\n';
            if (i + 1 < text.size() && text[i + 1] == '\n') {
                ++i;  // CRLF vira um único LF
            }
        } else {
            out += text[i];
        }
    }
    return out;
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
    // "+5" também é um inteiro válido (from_chars não aceita o '+'): na dúvida,
    // aceitar [A3.3.7]. Só se vier um algarismo depois, para "+-5" continuar inválido.
    if (*begin == '+' && text.size() > 1 && std::isdigit(static_cast<unsigned char>(begin[1]))) {
        ++begin;
    }

    auto [stop, error] = std::from_chars(begin, end, out);
    return error == std::errc() && stop == end;
}

// ---------------------------------------------------------------------------
// First line: algorithm;quantum;cpu_count
// ---------------------------------------------------------------------------

// Reads an integer system field. If it is missing, the default value already
// stored in "target" is kept and a warning is added [A3.2]. If it is present
// but invalid, it is an ERROR (design.md §8): o usuário escreveu algo errado,
// e trocar em silêncio por um padrão esconderia o engano.
void readSystemInt(const std::vector<std::string>& fields, std::size_t index,
                   const std::string& name, int minValue, int maxValue,
                   int& target, int lineNumber, ParseResult& result) {
    std::string line = std::to_string(lineNumber);
    std::string raw = fieldAt(fields, index);
    int value = 0;

    if (raw.empty()) {
        addMessage(result, MsgCode::SystemMissingValue, {line, name, std::to_string(target)});
    } else if (!parseInt(raw, value) || value < minValue || value > maxValue) {
        addMessage(result, MsgCode::SystemInvalidValue,
                   {line, raw, name, std::to_string(minValue), std::to_string(maxValue)});
    } else {
        target = value;
    }
}

// Lista "EDF, RM" com os nomes do registro, para a mensagem de erro.
std::string joinNames(const std::vector<std::string>& names) {
    std::string out;
    for (const std::string& name : names) {
        if (!out.empty()) out += ", ";
        out += name;
    }
    return out;
}

void parseSystemLine(const std::vector<std::string>& fields, int lineNumber, ParseResult& result) {
    std::string line = std::to_string(lineNumber);
    Config& config = result.config;

    // Field 0: algorithm (compared in uppercase, so "rm" == "RM")
    std::string rawAlgorithm = fieldAt(fields, 0);
    std::string algorithm = toUpper(rawAlgorithm);

    if (algorithm.empty()) {
        addMessage(result, MsgCode::SystemMissingValue, {line, "algoritmo_escalonamento", config.algorithm});
    } else if (findScheduler(algorithm) == nullptr) {
        // Erro, não aviso: rodar RM quando o usuário pediu outro algoritmo
        // (ex.: erro de digitação "EFD") daria um Gantt errado sem ele perceber.
        addMessage(result, MsgCode::UnknownAlgorithm, {line, rawAlgorithm, joinNames(schedulerNames())});
    } else {
        config.algorithm = algorithm;
    }

    // Field 1: quantum (at least 1)
    readSystemInt(fields, 1, "quantum", 1, MAX_QUANTUM, config.quantum, lineNumber, result);

    // Field 2: number of CPUs (between 1 and MAX_CPUS) [RG2]
    readSystemInt(fields, 2, "qtde_cpus", 1, MAX_CPUS, config.cpuCount, lineNumber, result);

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

// Cor quase preta (os três canais abaixo de 0x40): confundiria com o preto
// pontilhado que o Gantt usa para tarefa suspensa [A2.1] (D17). Olhar os três
// canais, e não a luminância, mantém cores fortes como o azul 0000FF.
// Recebe cor já validada (6 dígitos hexadecimais).
bool isTooDark(const std::string& color) {
    unsigned value = static_cast<unsigned>(std::stoul(color, nullptr, 16));
    unsigned r = (value >> 16) & 0xFF, g = (value >> 8) & 0xFF, b = value & 0xFF;
    return r < 0x40 && g < 0x40 && b < 0x40;
}

// Cor quase branca (os três canais acima de 0xE0): sobre o fundo branco do
// Gantt, uma barra assim pareceria "pronta" (ausência de cor, A2.1). A paleta
// automática nunca a escolhe e, se vier do arquivo, é trocada com aviso (D17).
bool isTooLight(const std::string& color) {
    unsigned value = static_cast<unsigned>(std::stoul(color, nullptr, 16));
    unsigned r = (value >> 16) & 0xFF, g = (value >> 8) & 0xFF, b = value & 0xFF;
    return r > 0xE0 && g > 0xE0 && b > 0xE0;
}

// k-ésima cor candidata: primeiro uma paleta fixa de cores bem distintas;
// depois uma sequência que percorre as 2^24 cores sem repetir (multiplicar
// por um número ímpar é uma permutação módulo 2^24).
std::string candidateColor(std::size_t k) {
    static const std::vector<std::string> PALETTE = {
        "E6194B", "3CB44B", "FFE119", "4363D8", "F58231", "911EB4",
        "46F0F0", "F032E6", "BCF60C", "FABEBE", "008080", "E6BEFF",
    };
    if (k < PALETTE.size()) {
        return PALETTE[k];
    }
    uint32_t value = (static_cast<uint32_t>(k - PALETTE.size()) * 0x9E3779B1u) & 0xFFFFFFu;
    char buffer[7];
    std::snprintf(buffer, sizeof(buffer), "%06X", static_cast<unsigned>(value));
    return buffer;
}

// Returns a color that no task is using yet, so every task keeps a
// different color in the Gantt chart [A2.1].
// 'cursor' guarda até onde a sequência já foi percorrida neste arquivo: a
// próxima busca continua dali, e 100 000 tarefas sem cor não ficam O(n²)
// (a versão anterior recomeçava do início a cada tarefa e podia estourar o int).
std::string nextFreeColor(const std::map<std::string, int>& usedColors, std::size_t& cursor) {
    while (true) {
        std::string color = candidateColor(cursor++);
        if (usedColors.count(color) == 0 && !isTooDark(color) && !isTooLight(color)) {
            return color;
        }
    }
}

// Reads an integer task field. Returns false (and adds an error) if it is
// missing, not a number, or outside [minValue, MAX_FIELD_VALUE].
// 'optional': campo vazio não é erro (devolve true e deixa target como está).
bool readTaskInt(const std::vector<std::string>& fields, std::size_t index,
                 const std::string& name, Tick minValue,
                 Tick& target, int lineNumber, ParseResult& result,
                 bool optional = false) {
    std::string line = std::to_string(lineNumber);
    std::string raw = fieldAt(fields, index);
    Tick value = 0;

    if (raw.empty()) {
        if (optional) {
            return true;
        }
        addMessage(result, MsgCode::TaskMissingField, {line, name});
        return false;
    }
    if (!parseInt(raw, value) || value < minValue || value > MAX_FIELD_VALUE) {
        std::string reason = "deve ser um número inteiro entre " + std::to_string(minValue) +
                             " e " + std::to_string(MAX_FIELD_VALUE);
        addMessage(result, MsgCode::TaskInvalidValue, {line, name, raw, reason});
        return false;
    }
    target = value;
    return true;
}

// Período: além das regras gerais, o valor negativo tem mensagem própria.
// No Projeto A ele é ERRO do arquivo (A3.3; resposta do professor Q10); no
// Projeto B passa a indicar tarefa esporádica (B1.3): trocar só aqui.
bool readPeriod(const std::vector<std::string>& fields, Tick& target,
                int lineNumber, ParseResult& result) {
    std::string raw = fieldAt(fields, 4);
    Tick value = 0;
    if (parseInt(raw, value) && value < 0) {
        addMessage(result, MsgCode::NegativePeriod, {std::to_string(lineNumber), raw});
        return false;
    }
    return readTaskInt(fields, 4, "periodo", 0, target, lineNumber, result);
}

// lista_eventos = tudo depois do 6º ';' (D30). Os eventos só serão definidos
// no Projeto B e podem conter ';', então os campos que sobraram são juntados
// de volta em vez de descartados [A3.3.5].
std::string eventsText(const std::vector<std::string>& fields) {
    std::string out;
    for (std::size_t i = 6; i < fields.size(); ++i) {
        if (i > 6) out += ';';
        out += fields[i];
    }
    return out;
}

// usedIds and usedColors map each id/color to the line where it first appeared.
void parseTaskLine(const std::vector<std::string>& fields, int lineNumber, ParseResult& result,
                   std::map<std::string, int>& usedIds, std::map<std::string, int>& usedColors,
                   std::size_t& colorCursor) {
    std::string line = std::to_string(lineNumber);
    Task task;       // built locally, added only at the end
    bool ok = true;  // becomes false on any error, but we keep checking the
                     // other fields so the user sees all errors at once

    // Field 0: id (required and unique)
    // Unicidade sem diferenciar maiúsculas [A3.3.2] (D4): "a1" e "A1" são o
    // mesmo id. O mapa usa a forma em maiúsculas; a tarefa guarda o id como
    // foi digitado, para mostrar ao usuário.
    task.id = fieldAt(fields, 0);
    std::string idKey = toUpper(task.id);
    if (task.id.empty()) {
        addMessage(result, MsgCode::TaskMissingField, {line, "id"});
        ok = false;
    } else if (usedIds.count(idKey) > 0) {
        addMessage(result, MsgCode::TaskDuplicateId,
                   {line, task.id, std::to_string(usedIds.at(idKey))});
        ok = false;
    } else {
        usedIds[idKey] = lineNumber;
    }

    // Fields 2 to 5: integers
    // Nomes dos campos como no formato do arquivo [A3.3], para o usuário achar o erro.
    ok = readTaskInt(fields, 2, "ingresso", 0, task.arrival,  lineNumber, result) && ok;
    ok = readTaskInt(fields, 3, "duracao",  1, task.duration, lineNumber, result) && ok;
    ok = readPeriod(fields, task.period, lineNumber, result) && ok;  // < 0 is an error [A3.3]
    // Prazo é opcional: vazio ou 0 => período (padrão, A3.2, D8). Negativo é erro.
    ok = readTaskInt(fields, 5, "prazo", 0, task.deadline, lineNumber, result, true) && ok;

    // Field 1: color. Uppercase so "ff0000" and "FF0000" are the same [A3.3.2].
    // Vazia => paleta automática; fora do formato RRGGBB => erro (D17).
    std::string rawColor = fieldAt(fields, 1);
    std::string color = toUpper(rawColor);
    if (!color.empty() && color[0] == '#') {
        color.erase(0, 1);  // "#FF0000" é a forma comum de escrever cor: tolerada (D17)
    }
    if (!color.empty() && !isValidHexColor(color)) {
        addMessage(result, MsgCode::InvalidColor, {line, rawColor});
        ok = false;
    }

    if (!ok) {
        return;  // at least one error: task is not added
    }

    // Field 6+: event list (optional, kept as raw text for Project B, D30)
    task.rawEvents = eventsText(fields);
    if (!task.rawEvents.empty()) {
        addMessage(result, MsgCode::EventsStored, {line, task.id});
    }

    if (task.period == 0) {
        // Aperiódica: carregada e marcada, mas não simulada no A (A4.4, D7). Fica
        // no TCB para aparecer na tabela e para o Projeto B só desligar a flag.
        task.kind = TaskKind::Aperiodic;
        task.ignored = true;
        task.ignoredReason = "aperiódica (período 0): só é simulada no Projeto B";
        addMessage(result, MsgCode::AperiodicIgnored, {line, task.id});  // [A4.4]
    } else {
        // IDs BSxx/PSxx/DSxx/SSxx com período > 0 são periódicas comuns no A
        // (resposta do professor Q12): nenhum tratamento especial aqui.
        if (task.deadline == 0) {
            task.deadline = task.period;
            addMessage(result, MsgCode::DefaultDeadline, {line, task.id, std::to_string(task.period)});
        } else if (task.deadline > task.period) {
            addMessage(result, MsgCode::DeadlineAfterPeriod,
                       {line, std::to_string(task.deadline), task.id, std::to_string(task.period)});
        }
        if (task.duration > task.period) {
            addMessage(result, MsgCode::DurationAfterPeriod,
                       {line, std::to_string(task.duration), task.id, std::to_string(task.period)});
        }
    }

    // Cor final: cada tarefa precisa de uma cor diferente, e o preto é
    // reservado para "suspensa" [A2.1]. Nesses casos a tarefa recebe a
    // próxima cor livre e o usuário é avisado (ideia do Arthur, D17).
    if (color.empty()) {
        color = nextFreeColor(usedColors, colorCursor);
        addMessage(result, MsgCode::AutoColor, {line, task.id, color});
    } else if (usedColors.count(color) > 0) {
        std::string firstLine = std::to_string(usedColors.at(color));
        std::string newColor = nextFreeColor(usedColors, colorCursor);
        addMessage(result, MsgCode::DuplicateColor, {line, color, task.id, firstLine, newColor});
        color = newColor;
    } else if (isTooDark(color)) {
        std::string newColor = nextFreeColor(usedColors, colorCursor);
        addMessage(result, MsgCode::DarkColor, {line, color, task.id, newColor});
        color = newColor;
    } else if (isTooLight(color)) {
        std::string newColor = nextFreeColor(usedColors, colorCursor);
        addMessage(result, MsgCode::LightColor, {line, color, task.id, newColor});
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

// Leitura propriamente dita; parser::parseConfig (abaixo) só a protege.
static ParseResult parseText(const std::string& text) {
    ParseResult result;
    std::stringstream stream(normalizeText(text));
    std::string line;
    int lineNumber = 0;
    bool systemLineRead = false;
    int taskLines = 0;               // linhas de tarefa (com ou sem erro)
    std::map<std::string, int> usedIds;
    std::map<std::string, int> usedColors;
    std::size_t colorCursor = 0;     // posição na sequência de cores automáticas

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
            parseTaskLine(fields, lineNumber, result, usedIds, usedColors, colorCursor);
            ++taskLines;
        }
    }

    // Tarefas que de fato serão simuladas (as aperiódicas ficam de fora no A).
    int simulated = 0;
    for (const Task& task : result.config.tasks) {
        if (!task.ignored) ++simulated;
    }

    if (!systemLineRead) {
        addMessage(result, MsgCode::EmptyFile);
    } else if (taskLines == 0) {
        addMessage(result, MsgCode::TooFewLines);       // [A3.3.1]
    } else if (simulated == 0 && result.errors.empty()) {
        // Com outros erros, eles já explicam o problema; aqui o arquivo está
        // certo, mas não sobra nenhuma tarefa periódica para simular.
        addMessage(result, MsgCode::NoValidTasks);
    }

    return result;
}

ParseResult parser::parseConfig(const std::string& text) {
    // O parser nunca deixa uma exceção chegar à interface (design.md §8): o
    // código acima não lança nada em uso normal, mas falta de memória num
    // arquivo absurdo (ex.: gigabytes de texto) ainda poderia. Nesse caso o
    // usuário recebe um erro claro, e nada é carregado [A3.3.6, A5].
    try {
        return parseText(text);
    } catch (const std::exception& e) {
        ParseResult result;
        addMessage(result, MsgCode::InternalError, {e.what()});
        return result;
    } catch (...) {
        ParseResult result;
        addMessage(result, MsgCode::InternalError, {"erro desconhecido"});
        return result;
    }
}

std::string parser::serializeConfig(const Config& config) {
    // Mesmo formato que parseConfig lê [A3.3], uma linha por tarefa, sem ';'
    // final. Números com std::to_string (não dependem de locale). Ida e volta
    // (teste P14): parseConfig(serializeConfig(c)) devolve os mesmos campos de c,
    // porque os padrões já aplicados (prazo = período, cor automática) são
    // escritos por extenso.
    // A semente do sorteio (Config::seed) NÃO é escrita: o formato do enunciado
    // não tem esse campo, e um 4º campo na linha do sistema faria o arquivo
    // fugir do formato. Ao recarregar, ela volta ao padrão.
    std::string out = config.algorithm + ";" + std::to_string(config.quantum) + ";" +
                      std::to_string(config.cpuCount) + "\n";
    for (const Task& t : config.tasks) {
        out += t.id + ";" + t.color + ";" + std::to_string(t.arrival) + ";" +
               std::to_string(t.duration) + ";" + std::to_string(t.period) + ";" +
               std::to_string(t.deadline);
        if (!t.rawEvents.empty()) {
            out += ";" + t.rawEvents;  // lista_eventos crua, com os ';' internos (D30)
        }
        out += "\n";
    }
    return out;
}
