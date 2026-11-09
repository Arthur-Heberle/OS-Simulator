// =============================================================================
// Parser.cpp — Implementação do parser do arquivo de configuração.
//
// Etapas de parseConfig:
//   1. toUtf8 converte UTF-16 e ANSI; normalizeText remove o BOM e troca
//      CRLF/CR por LF;
//   2. cada linha é numerada como no arquivo, para as mensagens apontarem a
//      linha real; linhas em branco são ignoradas [A3.3.6];
//   3. a primeira linha útil é a do sistema (parseSystemLine) e as demais são
//      tarefas (parseTaskLine); cada linha é validada inteira, para o usuário
//      ver todos os erros de uma vez;
//   4. regras do arquivo inteiro (mínimo de 2 linhas, alguma tarefa simulável).
//
// Requisitos e decisões: cabeçalho de Parser.h.
// =============================================================================

#include "Parser.h"
#include "Scheduler.h"   // registro de algoritmos [A4.2]

#include <cctype>
#include <charconv>
#include <cstdint>
#include <cstdio>
#include <exception>
#include <sstream>

// Funções auxiliares, visíveis só neste arquivo.
namespace {

// ---------------------------------------------------------------------------
// Mensagens
// ---------------------------------------------------------------------------

// Troca cada "{}" do modelo pelo próximo argumento.
std::string formatMessage(std::string_view tmpl, const std::vector<std::string>& args) {
    std::string out;
    std::size_t next = 0;

    for (std::size_t i = 0; i < tmpl.size(); ++i) {
        bool isPlaceholder = tmpl[i] == '{' && i + 1 < tmpl.size() && tmpl[i + 1] == '}';

        if (isPlaceholder && next < args.size()) {
            out += args[next];
            ++next;
            ++i;  // pula o "}"
        } else {
            out += tmpl[i];
        }
    }
    return out;
}

// Monta o texto da mensagem e o guarda como erro ou aviso, conforme MESSAGES.
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
// Texto
// ---------------------------------------------------------------------------

// Remove espaços, tabulações e quebras de linha das duas pontas [A3.3.6].
std::string trim(const std::string& s) {
    const char* spaces = " \t\r\n";

    std::size_t first = s.find_first_not_of(spaces);
    if (first == std::string::npos) {
        return "";
    }
    std::size_t last = s.find_last_not_of(spaces);
    return s.substr(first, last - first + 1);
}

// Cópia do texto em maiúsculas, para comparar sem diferenciar caixa [A3.3.2].
std::string toUpper(std::string s) {
    for (char& c : s) {
        c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
    }
    return s;
}

// Acrescenta o código Unicode 'cp' a 'out' em UTF-8.
void appendUtf8(std::string& out, std::uint32_t cp) {
    if (cp < 0x80) {
        out += static_cast<char>(cp);
    } else if (cp < 0x800) {
        out += static_cast<char>(0xC0 | (cp >> 6));
        out += static_cast<char>(0x80 | (cp & 0x3F));
    } else if (cp < 0x10000) {
        out += static_cast<char>(0xE0 | (cp >> 12));
        out += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
        out += static_cast<char>(0x80 | (cp & 0x3F));
    } else {
        out += static_cast<char>(0xF0 | (cp >> 18));
        out += static_cast<char>(0x80 | ((cp >> 12) & 0x3F));
        out += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
        out += static_cast<char>(0x80 | (cp & 0x3F));
    }
}

// true se 'text' é UTF-8 bem formado (sequências de 2 a 4 bytes completas).
bool isValidUtf8(const std::string& text) {
    for (std::size_t i = 0; i < text.size();) {
        const auto c = static_cast<unsigned char>(text[i]);
        const std::size_t length = c < 0x80 ? 1 : (c >> 5) == 0x6 ? 2 : (c >> 4) == 0xE ? 3 : (c >> 3) == 0x1E ? 4 : 0;
        if (length == 0 || i + length > text.size()) return false;
        for (std::size_t k = 1; k < length; ++k) {
            if ((static_cast<unsigned char>(text[i + k]) & 0xC0) != 0x80) return false;
        }
        i += length;
    }
    return true;
}

// Converte o conteúdo para UTF-8, a codificação usada no resto do programa.
// O Bloco de Notas grava "Unicode" como UTF-16 com BOM e "ANSI" como
// Windows-1252; os dois são texto simples e precisam carregar [A3.3, A3.3.7].
// 'encoding' recebe o nome da codificação convertida (vazio se já era UTF-8).
// Texto que não é UTF-8 válido é lido como Latin-1, igual ao Windows-1252 nas
// letras acentuadas.
std::string toUtf8(const std::string& raw, std::string& encoding) {
    encoding.clear();
    const bool littleEndian = raw.size() >= 2 && raw[0] == '\xFF' && raw[1] == '\xFE';
    const bool bigEndian = raw.size() >= 2 && raw[0] == '\xFE' && raw[1] == '\xFF';
    std::string out;
    if (littleEndian || bigEndian) {
        encoding = "UTF-16";
        auto unit = [&raw, bigEndian](std::size_t i) {
            const auto a = static_cast<unsigned char>(raw[i]);
            const auto b = static_cast<unsigned char>(raw[i + 1]);
            return static_cast<std::uint32_t>(bigEndian ? (a << 8) | b : (b << 8) | a);
        };
        for (std::size_t i = 2; i + 1 < raw.size(); i += 2) {
            std::uint32_t cp = unit(i);
            if (cp >= 0xD800 && cp <= 0xDFFF) {  // par substituto (caractere fora do plano básico)
                const std::uint32_t low = i + 3 < raw.size() ? unit(i + 2) : 0;
                if (cp <= 0xDBFF && low >= 0xDC00 && low <= 0xDFFF) {
                    cp = 0x10000 + ((cp - 0xD800) << 10) + (low - 0xDC00);
                    i += 2;
                } else {
                    cp = 0xFFFD;  // metade de par isolada: caractere de substituição
                }
            }
            appendUtf8(out, cp);
        }
        return out;
    }
    if (isValidUtf8(raw)) return raw;
    encoding = "ANSI (Windows-1252)";
    for (char c : raw) appendUtf8(out, static_cast<unsigned char>(c));
    return out;
}

// Prepara o texto para a leitura linha a linha (D18): remove o BOM UTF-8, que
// o Bloco de Notas pode gravar e que transformaria "EDF" em um algoritmo
// desconhecido, e troca CRLF e CR isolado por LF, para std::getline separar
// as linhas em qualquer caso [A3.3.7].
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

// Divide a linha por ';', já aparando cada campo. Campos vazios são mantidos,
// porque a posição define o significado (";2;10" = algoritmo vazio, quantum 2,
// 10 CPUs).
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

    // Um ';' no fim da linha não cria um campo a mais [A3.3.3].
    if (fields.size() > 1 && fields.back().empty()) {
        fields.pop_back();
    }
    return fields;
}

// Campo da posição pedida, ou "" se a linha for mais curta: campo ausente e
// campo vazio recebem o mesmo tratamento.
std::string fieldAt(const std::vector<std::string>& fields, std::size_t index) {
    if (index < fields.size()) {
        return fields[index];
    }
    return "";
}

// Converte o texto inteiro em número; false se sobrar algum caractere ("12abc"
// é recusado, ao contrário de std::stoi). Não lança exceção. Usado com int
// (linha do sistema) e Tick (campos da tarefa).
template <typename Int>
bool parseInt(const std::string& text, Int& out) {
    if (text.empty()) {
        return false;
    }
    const char* begin = text.data();
    const char* end = text.data() + text.size();
    // from_chars não aceita '+', mas "+5" é um inteiro válido [A3.3.7]. O
    // algarismo depois do sinal mantém "+-5" inválido.
    if (*begin == '+' && text.size() > 1 && std::isdigit(static_cast<unsigned char>(begin[1]))) {
        ++begin;
    }

    auto [stop, error] = std::from_chars(begin, end, out);
    return error == std::errc() && stop == end;
}

// ---------------------------------------------------------------------------
// Linha do sistema: algoritmo;quantum;qtde_cpus
// ---------------------------------------------------------------------------

// Lê um campo inteiro da linha do sistema. Ausente: mantém o padrão já em
// 'target' e avisa [A3.2]. Presente mas inválido: erro, porque trocar em
// silêncio por um padrão esconderia o engano do usuário (design.md §8).
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

// Nomes do registro separados por vírgula ("EDF, RM"), para a mensagem de erro.
std::string joinNames(const std::vector<std::string>& names) {
    std::string out;
    for (const std::string& name : names) {
        if (!out.empty()) out += ", ";
        out += name;
    }
    return out;
}

// Linha do sistema. Campo vazio ou ausente recebe o padrão [A3.2]; valor
// inválido vira erro com linha, campo e valor [A3.3.6].
void parseSystemLine(const std::vector<std::string>& fields, int lineNumber, ParseResult& result) {
    std::string line = std::to_string(lineNumber);
    Config& config = result.config;

    // Campo 0: algoritmo, comparado em maiúsculas ("rm" = "RM") [A3.3.2].
    std::string rawAlgorithm = fieldAt(fields, 0);
    std::string algorithm = toUpper(rawAlgorithm);

    if (algorithm.empty()) {
        addMessage(result, MsgCode::SystemMissingValue, {line, "algoritmo_escalonamento", config.algorithm});
    } else if (findScheduler(algorithm) == nullptr) {
        // Erro, não aviso: simular RM quando o usuário pediu outro algoritmo
        // (ex.: "EFD") produziria um Gantt errado sem que ele percebesse.
        addMessage(result, MsgCode::UnknownAlgorithm, {line, rawAlgorithm, joinNames(schedulerNames())});
    } else {
        config.algorithm = algorithm;
    }

    // Campo 1: quantum. 0 é aceito (sem fatia de tempo): RM e EDF não o usam
    // (Q4), e recusar 0 barraria um arquivo válido [A3.3.7] (D13).
    readSystemInt(fields, 1, "quantum", 0, MAX_QUANTUM, config.quantum, lineNumber, result);

    // Campo 2: número de CPUs, de 1 a MAX_CPUS [RG2] (D12).
    readSystemInt(fields, 2, "qtde_cpus", 1, MAX_CPUS, config.cpuCount, lineNumber, result);

    if (fields.size() > 3) {
        addMessage(result, MsgCode::ExtraFields, {line});
    }
}

// ---------------------------------------------------------------------------
// Linhas de tarefa: id;cor;ingresso;duracao;periodo;prazo;lista_eventos
// ---------------------------------------------------------------------------

// true se o texto tem exatamente 6 dígitos hexadecimais (ex.: "F0E0D0").
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

// Cor quase preta (os três canais abaixo de 0x40), que se confundiria com o
// preto pontilhado da tarefa suspensa [A2.1] (D17). Testar os três canais, e
// não a luminância, preserva cores fortes como o azul 0000FF. Recebe uma cor
// já validada.
bool isTooDark(const std::string& color) {
    unsigned value = static_cast<unsigned>(std::stoul(color, nullptr, 16));
    unsigned r = (value >> 16) & 0xFF, g = (value >> 8) & 0xFF, b = value & 0xFF;
    return r < 0x40 && g < 0x40 && b < 0x40;
}

// Cor quase branca (os três canais acima de 0xE0): no fundo branco do Gantt,
// a barra pareceria "pronta" (ausência de cor, A2.1) (D17).
bool isTooLight(const std::string& color) {
    unsigned value = static_cast<unsigned>(std::stoul(color, nullptr, 16));
    unsigned r = (value >> 16) & 0xFF, g = (value >> 8) & 0xFF, b = value & 0xFF;
    return r > 0xE0 && g > 0xE0 && b > 0xE0;
}

// k-ésima cor candidata da paleta automática: primeiro cores fixas bem
// distintas; depois uma sequência que percorre as 2^24 cores sem repetir
// (multiplicar por um número ímpar é uma permutação módulo 2^24).
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

// Próxima cor da paleta que nenhuma tarefa usa e que não é quase preta nem
// quase branca, para cada tarefa ter cor própria no Gantt [A2.1]. 'cursor'
// guarda onde a busca parou, para a seguinte continuar dali: com 100 000
// tarefas sem cor a busca total continua linear.
std::string nextFreeColor(const std::map<std::string, int>& usedColors, std::size_t& cursor) {
    while (true) {
        std::string color = candidateColor(cursor++);
        if (usedColors.count(color) == 0 && !isTooDark(color) && !isTooLight(color)) {
            return color;
        }
    }
}

// Lê um campo inteiro da tarefa. Devolve false, com erro, se estiver ausente,
// não for inteiro ou sair de [minValue, MAX_FIELD_VALUE]. Com 'optional', campo
// vazio não é erro e 'target' fica como está.
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

// Lê o período. Valor negativo é erro do arquivo no Projeto A (A3.3, Q10), com
// mensagem própria; no Projeto B indicará tarefa esporádica (B1.3).
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

// lista_eventos: tudo depois do 6º ';' (D30). O formato dos eventos só é
// definido no Projeto B e pode conter ';', então os campos restantes são
// juntados de volta [A3.3.5].
std::string eventsText(const std::vector<std::string>& fields) {
    std::string out;
    for (std::size_t i = 6; i < fields.size(); ++i) {
        if (i > 6) out += ';';
        out += fields[i];
    }
    return out;
}

// Lê uma linha de tarefa e, sem erros, a acrescenta à Config. usedIds e
// usedColors guardam a linha onde cada id e cada cor apareceram primeiro.
void parseTaskLine(const std::vector<std::string>& fields, int lineNumber, ParseResult& result,
                   std::map<std::string, int>& usedIds, std::map<std::string, int>& usedColors,
                   std::size_t& colorCursor) {
    std::string line = std::to_string(lineNumber);
    Task task;       // só entra na Config se a linha não tiver erro
    bool ok = true;  // a validação continua após um erro, para mostrar todos

    // Campo 0: id, obrigatório e único sem diferenciar maiúsculas [A3.3.2]
    // (D4). O mapa usa a forma em maiúsculas; a tarefa guarda o id como foi
    // escrito.
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

    // Campos 2 a 5. As mensagens usam os nomes do formato do arquivo [A3.3].
    ok = readTaskInt(fields, 2, "ingresso", 0, task.arrival,  lineNumber, result) && ok;
    ok = readTaskInt(fields, 3, "duracao",  1, task.duration, lineNumber, result) && ok;
    ok = readPeriod(fields, task.period, lineNumber, result) && ok;  // negativo é erro [A3.3]
    // Prazo opcional: vazio ou 0 vale o período [A3.2] (D8); negativo é erro.
    ok = readTaskInt(fields, 5, "prazo", 0, task.deadline, lineNumber, result, true) && ok;

    // Campo 1: cor, em maiúsculas ("ff0000" = "FF0000") [A3.3.2]. Vazia usa a
    // paleta automática; fora de RRGGBB é erro (D17).
    std::string rawColor = fieldAt(fields, 1);
    std::string color = toUpper(rawColor);
    if (!color.empty() && color[0] == '#') {
        color.erase(0, 1);  // "#FF0000" é uma forma comum de escrever a cor (D17)
    }
    if (!color.empty() && !isValidHexColor(color)) {
        addMessage(result, MsgCode::InvalidColor, {line, rawColor});
        ok = false;
    }

    if (!ok) {
        return;  // com erro, a tarefa não é adicionada
    }

    // Campo 6 em diante: lista de eventos, opcional, guardada como texto (D30).
    task.rawEvents = eventsText(fields);
    if (!task.rawEvents.empty()) {
        addMessage(result, MsgCode::EventsStored, {line, task.id});
    }

    if (task.period == 0) {
        // Aperiódica: carregada e marcada como ignorada, sem simular no A (A4.4,
        // D7). Continua no TCB para aparecer na tabela.
        task.kind = TaskKind::Aperiodic;
        task.ignored = true;
        task.ignoredReason = "aperiódica (período 0): só é simulada no Projeto B";
        addMessage(result, MsgCode::AperiodicIgnored, {line, task.id});  // [A4.4]
    } else {
        // Ids BSxx/PSxx/DSxx/SSxx com período > 0 são periódicas comuns no A (Q12).
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

    // Cada tarefa precisa de cor própria, e o preto indica "suspensa" [A2.1]:
    // cor ausente, repetida, quase preta ou quase branca é trocada pela próxima
    // cor livre, com aviso (D17).
    if (color.empty()) {
        color = nextFreeColor(usedColors, colorCursor);
        addMessage(result, MsgCode::AutoColor, {line, task.id, color});
    } else if (usedColors.count(color) > 0) {
        // PERGUNTA EM ABERTO Q-A7: cor repetida no arquivo é erro ou aviso?
        // Padrão adotado: aviso, e a tarefa recebe outra cor livre (D17).
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
// Funções públicas
// ---------------------------------------------------------------------------

// Leitura do arquivo inteiro; parser::parseConfig só a protege de exceções.
static ParseResult parseText(const std::string& text) {
    ParseResult result;
    std::string encoding;
    std::stringstream stream(normalizeText(toUtf8(text, encoding)));
    if (!encoding.empty()) addMessage(result, MsgCode::ConvertedEncoding, {encoding});
    std::string line;
    int lineNumber = 0;
    bool systemLineRead = false;
    int taskLines = 0;               // linhas de tarefa (com ou sem erro)
    std::map<std::string, int> usedIds;
    std::map<std::string, int> usedColors;
    std::size_t colorCursor = 0;     // posição na sequência de cores automáticas

    while (std::getline(stream, line)) {
        ++lineNumber;  // conta todas as linhas: as mensagens apontam a linha do arquivo

        line = trim(line);
        if (line.empty()) {
            continue;  // linha em branco não é erro [A3.3.6]
        }

        std::vector<std::string> fields = splitFields(line);

        // A primeira linha útil é a do sistema; as demais são tarefas.
        if (!systemLineRead) {
            parseSystemLine(fields, lineNumber, result);
            systemLineRead = true;
        } else {
            parseTaskLine(fields, lineNumber, result, usedIds, usedColors, colorCursor);
            ++taskLines;
        }
    }

    // Tarefas simuladas (as aperiódicas ficam de fora no A).
    int simulated = 0;
    for (const Task& task : result.config.tasks) {
        if (!task.ignored) ++simulated;
    }

    if (!systemLineRead) {
        addMessage(result, MsgCode::EmptyFile);
    } else if (taskLines == 0) {
        addMessage(result, MsgCode::TooFewLines);       // [A3.3.1]
    } else if (simulated == 0 && result.errors.empty()) {
        // O arquivo obedece ao formato, então carrega [A3.3.7]; as aperiódicas
        // são ignoradas e isso é avisado [A4.4]. A simulação termina no instante 0.
        addMessage(result, MsgCode::NoValidTasks);
    }

    return result;
}

// Ponto de entrada do parser.
ParseResult parser::parseConfig(const std::string& text) {
    // A leitura não lança exceção em uso normal, mas um arquivo de gigabytes
    // pode esgotar a memória. Nesse caso o usuário recebe um erro claro e nada
    // é carregado, sem a exceção chegar à interface [A3.3.6, A5].
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
    // Formato do enunciado [A3.3], uma tarefa por linha. Os padrões já
    // aplicados (prazo = período, cor automática) são escritos por extenso,
    // então ler o texto de volta reproduz a Config (teste P14). A semente não é
    // escrita: o formato não tem esse campo e ela volta ao padrão ao recarregar.
    // std::to_string não depende de locale.
    std::string out = config.algorithm + ";" + std::to_string(config.quantum) + ";" +
                      std::to_string(config.cpuCount) + "\n";
    for (const Task& t : config.tasks) {
        out += t.id + ";" + t.color + ";" + std::to_string(t.arrival) + ";" +
               std::to_string(t.duration) + ";" + std::to_string(t.period) + ";" +
               std::to_string(t.deadline);
        if (!t.rawEvents.empty()) {
            out += ";" + t.rawEvents;  // lista_eventos como foi lida, com os ';' internos (D30)
        }
        out += "\n";
    }
    return out;
}

bool parser::normalizeColor(const std::string& raw, std::string& out) {
    std::string color = toUpper(trim(raw));
    if (!color.empty() && color[0] == '#') {
        color.erase(0, 1);
    }
    if (!isValidHexColor(color)) {
        return false;
    }
    out = color;
    return true;
}

bool parser::isTooDarkColor(const std::string& color) {
    return isTooDark(color);
}

bool parser::isTooLightColor(const std::string& color) {
    return isTooLight(color);
}
