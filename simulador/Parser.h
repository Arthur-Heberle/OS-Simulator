// =============================================================================
// Parser.h — Leitura do arquivo de configuração (texto -> Config) e escrita
// no mesmo formato (Config -> texto).
//
// Formato [A3.3]:
//     algoritmo_escalonamento;quantum;qtde_cpus
//     id;cor;ingresso;duracao;periodo;prazo;lista_eventos
// parseConfig devolve a Config com os padrões aplicados [A3.2] e todos os
// erros e avisos encontrados, cada um com linha, campo, valor e motivo
// [A3.3.6].
//
// O parser recebe o texto, não o caminho (D29): a interface abre o arquivo com
// QFile, que aceita qualquer caminho [A3.3.4], e os testes passam strings.
// Erro (nada é carregado) só quando não existe padrão possível; nos demais
// casos o item recebe um padrão ou é ignorado, com aviso (D18), para que um
// arquivo válido nunca falhe [A3.3.7]. Severidade e texto de cada mensagem
// ficam na tabela MESSAGES.
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
// Limites
// ---------------------------------------------------------------------------
// Os algoritmos aceitos vêm do registro de escalonadores (Scheduler.h), então
// um algoritmo novo não exige mudança no parser [A4.2].

inline constexpr int MAX_QUANTUM = 1000000;
// Maior número aceito nos campos da tarefa (10^9): mantém os instantes
// calculados (ingresso + 10 períodos) longe do limite de Tick (design.md §8).
inline constexpr Tick MAX_FIELD_VALUE = 1000000000;

// ---------------------------------------------------------------------------
// Mensagens
// ---------------------------------------------------------------------------

enum class Severity { Error, Warning };

enum class MsgCode {
    // Erros: não há padrão possível; o arquivo inteiro não é carregado (D18)
    EmptyFile,
    InternalError,
    TooFewLines,
    UnknownAlgorithm,
    SystemInvalidValue,
    TaskMissingField,
    TaskInvalidValue,
    TaskDuplicateId,
    NegativePeriod,
    InvalidColor,

    // Avisos: o item recebe um valor padrão ou é ignorado, e o arquivo carrega
    AperiodicIgnored,
    NoValidTasks,
    ConvertedEncoding,
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
    std::string_view text;  // cada "{}" é trocado pelo próximo argumento
};

// Textos mostrados ao usuário, sempre com o motivo [A3.3.6, A5].
inline const std::map<MsgCode, MessageInfo> MESSAGES = {
    {MsgCode::EmptyFile,
     {Severity::Error, "O arquivo de configuração está vazio."}},
    {MsgCode::InternalError,
     {Severity::Error, "Não foi possível ler o arquivo (falha interna: {}). Nada foi carregado."}},
    {MsgCode::TooFewLines,
     {Severity::Error, "O arquivo precisa ter ao menos 2 linhas: a 1ª com 'algoritmo;quantum;qtde_cpus' e, a partir da 2ª, uma tarefa por linha."}},
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
    {MsgCode::NoValidTasks,
     {Severity::Warning, "Nenhuma tarefa periódica para simular: todas as tarefas do arquivo são aperiódicas e foram ignoradas. A simulação termina no instante 0."}},
    {MsgCode::ConvertedEncoding,
     {Severity::Warning, "O arquivo está em {} e foi convertido para UTF-8."}},
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
// Funções públicas (Config e ParseResult estão em Model.h)
// ---------------------------------------------------------------------------

namespace parser {
    // Lê o conteúdo do arquivo de configuração. Nunca lança exceção: falhas
    // internas viram a mensagem InternalError.
    ParseResult parseConfig(const std::string& text);

    // Texto de um arquivo de configuração a partir de uma Config, usado em
    // "Salvar configuração" [A3.1]. parseConfig(serializeConfig(c)) reproduz c
    // (teste P14). A semente do sorteio não faz parte do formato.
    std::string serializeConfig(const Config& config);

    // Regras de cor (D17), compartilhadas com a edição manual (SetColor).
    // normalizeColor aceita "#ff0000" ou "FF0000", grava "FF0000" em 'out' e
    // devolve false se o texto não for RRGGBB hexadecimal.
    bool normalizeColor(const std::string& raw, std::string& out);
    bool isTooDarkColor(const std::string& color);   // quase preta (preto = suspensa, A2.1)
    bool isTooLightColor(const std::string& color);  // quase branca (pareceria "pronta", A2.1)
}