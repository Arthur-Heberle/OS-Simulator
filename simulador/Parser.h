// =============================================================================
// Parser.h — Leitura do arquivo de configuração (texto -> Config) e o caminho
// inverso (Config -> texto).
//
// O quê: parser::parseConfig lê o formato do enunciado [A3.3]
//     algoritmo_escalonamento;quantum;qtde_cpus
//     id;cor;ingresso;duracao;periodo;prazo;lista_eventos
// e devolve a Config com os padrões aplicados [A3.2] mais TODOS os erros e
// avisos encontrados, cada um com linha, campo, valor e motivo [A3.3.6].
// parser::serializeConfig faz o caminho inverso (salvar a configuração).
//
// Por quê assim:
//   - Recebe o TEXTO, não o caminho (D29): quem abre o arquivo é a interface
//     (QFile aceita qualquer caminho, pendrive e acentos, A3.3.4) e os testes
//     passam strings, sem criar arquivos.
//   - Erro x aviso (D18): erro = não há padrão possível, nada é carregado;
//     aviso = usa um padrão ou ignora algo, e a simulação roda. Na dúvida,
//     aceitar com aviso: arquivo válido nunca pode falhar [A3.3.7].
//   - Severidade e texto de cada mensagem ficam numa tabela (MsgCode ->
//     MESSAGES): mudar uma regra, se o professor responder diferente, é
//     mexer numa linha.
//
// Requisitos: A3.2, A3.3, A3.3.1–A3.3.7, A4.1, A4.4, A5.
// Decisões: D4, D7, D8, D12, D17, D18, D29, D30; respostas Q10 e Q12.
// =============================================================================
#pragma once

#include <cstdint>
#include <map>
#include <string>
#include <string_view>
#include <vector>

#include "Model.h"

// ---------------------------------------------------------------------------
// Constants
// ---------------------------------------------------------------------------

// Os algoritmos aceitos vêm do registro de escalonadores (findScheduler /
// schedulerNames, Scheduler.h): incluir um algoritmo não exige mexer no parser [A4.2].

// Constants to the system config
inline constexpr int MAX_QUANTUM = 1000000;
inline constexpr Tick MAX_FIELD_VALUE = 1000000000;  // 10^9: limite dos números da tarefa (design.md §8)

// ---------------------------------------------------------------------------
// Messages
// ---------------------------------------------------------------------------

enum class Severity { Error, Warning };

enum class MsgCode {
    // Errors: there is no sensible default, so the item cannot be used
    // (o arquivo inteiro não é carregado; design.md §8, D18)
    EmptyFile,
    InternalError,
    TooFewLines,
    NoValidTasks,
    UnknownAlgorithm,
    SystemInvalidValue,
    TaskMissingField,
    TaskInvalidValue,
    TaskDuplicateId,
    NegativePeriod,
    InvalidColor,

    // Warnings: a default value is used or the item is ignored
    AperiodicIgnored,
    SystemMissingValue,
    AutoColor,
    DuplicateColor,
    DarkColor,
    LightColor,
    DefaultDeadline,
    DeadlineAfterPeriod,
    DurationAfterPeriod,
    EventsStored,
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
    {MsgCode::InternalError,
     {Severity::Error, "Não foi possível ler o arquivo (falha interna: {}). Nada foi carregado."}},
    {MsgCode::TooFewLines,
     {Severity::Error, "O arquivo precisa ter ao menos 2 linhas: a 1ª com 'algoritmo;quantum;qtde_cpus' e, a partir da 2ª, uma tarefa por linha."}},
    {MsgCode::NoValidTasks,
     {Severity::Error, "Nenhuma tarefa periódica para simular: todas as tarefas do arquivo foram ignoradas."}},
    {MsgCode::UnknownAlgorithm,
     {Severity::Error, "Linha {}: algoritmo de escalonamento desconhecido '{}'. Algoritmos disponíveis: {}."}},
    {MsgCode::SystemInvalidValue,
     {Severity::Error, "Linha {}: valor inválido '{}' para '{}' (esperado um número inteiro entre {} e {})."}},
    {MsgCode::TaskMissingField,
     {Severity::Error, "Linha {}: a tarefa não pode ser adicionada. Motivo: o campo '{}' está ausente."}},
    {MsgCode::TaskInvalidValue,
     {Severity::Error, "Linha {}: a tarefa não pode ser adicionada. Motivo: o campo '{}' tem o valor inválido '{}' ({})."}},
    {MsgCode::TaskDuplicateId,
     {Severity::Error, "Linha {}: a tarefa não pode ser adicionada. Motivo: o id '{}' já é usado na linha {} (maiúsculas e minúsculas contam como iguais)."}},
    {MsgCode::NegativePeriod,
     {Severity::Error, "Linha {}: a tarefa não pode ser adicionada. Motivo: o campo 'periodo' tem o valor '{}'; período negativo é erro no Projeto A (tarefas esporádicas só no Projeto B)."}},
    {MsgCode::InvalidColor,
     {Severity::Error, "Linha {}: a tarefa não pode ser adicionada. Motivo: a cor '{}' não está no formato RRGGBB hexadecimal (ex.: F0E0D0)."}},

    {MsgCode::AperiodicIgnored,
     {Severity::Warning, "Linha {}: a tarefa '{}' é aperiódica (período 0) e será ignorada no Projeto A."}},
    {MsgCode::SystemMissingValue,
     {Severity::Warning, "Linha {}: '{}' não informado. Usando o padrão '{}'."}},
    {MsgCode::AutoColor,
     {Severity::Warning, "Linha {}: a tarefa '{}' não tem cor. Usando '{}'."}},
    {MsgCode::DuplicateColor,
     {Severity::Warning, "Linha {}: a cor '{}' da tarefa '{}' já é usada na linha {}. Usando '{}' no lugar (cada tarefa precisa de uma cor diferente)."}},
    {MsgCode::DarkColor,
     {Severity::Warning, "Linha {}: a cor '{}' da tarefa '{}' é escura demais (o preto indica tarefa suspensa no gráfico). Usando '{}' no lugar."}},
    {MsgCode::LightColor,
     {Severity::Warning, "Linha {}: a cor '{}' da tarefa '{}' é clara demais (no gráfico, a ausência de cor indica tarefa pronta). Usando '{}' no lugar."}},
    {MsgCode::DefaultDeadline,
     {Severity::Warning, "Linha {}: prazo da tarefa '{}' não informado (ou 0). Usando o período ({})."}},
    {MsgCode::DeadlineAfterPeriod,
     {Severity::Warning, "Linha {}: o prazo ({}) da tarefa '{}' é maior que o período ({})."}},
    {MsgCode::DurationAfterPeriod,
     {Severity::Warning, "Linha {}: a duração ({}) da tarefa '{}' é maior que o período ({}): as ocorrências vão se acumular e perder prazos."}},
    {MsgCode::EventsStored,
     {Severity::Warning, "Linha {}: a lista de eventos da tarefa '{}' foi guardada, mas só é simulada no Projeto B."}},
    {MsgCode::ExtraFields,
     {Severity::Warning, "Linha {}: campos a mais foram ignorados."}},
};

// ---------------------------------------------------------------------------
// Result of the parser
// ---------------------------------------------------------------------------

// Config e ParseResult moram em Model.h (contrato de dados, design.md §5).

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

    // Gera o texto de um arquivo de configuração a partir de uma Config
    // (ida e volta: parseConfig(serializeConfig(c)) reproduz c; teste P14).
    // Usado por "Salvar configuração" na interface, depois de edições (A3.1).
    // A semente do sorteio não faz parte do formato e não é escrita.
    std::string serializeConfig(const Config& config);

    // Regras de cor do parser (D17), expostas para a edição manual (SetColor)
    // usar exatamente as mesmas. normalizeColor aceita "#ff0000" ou "FF0000",
    // devolve "FF0000" em 'out' e false se não for RRGGBB hexadecimal.
    bool normalizeColor(const std::string& raw, std::string& out);
    bool isTooDarkColor(const std::string& color);   // quase preta (preto = suspensa, A2.1)
    bool isTooLightColor(const std::string& color);  // quase branca (pareceria "pronta", A2.1)
}