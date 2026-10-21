#include "core/Parser.h"

#include <utility>

Parser::Parser() : filePath("") {}

Parser::Parser(std::string filePath) : filePath(std::move(filePath)) {}

Parser::~Parser() {}
