#pragma once

#include <string>

// Responsible for parsing the text input, configuring tasks and setups
// and managing errors and warnings.
class Parser {
    private:
        std::string filePath;  // Input file path (any name, any location)

    public:
        // Creates a parser with no file selected.
        Parser();

        // Creates a parser for the given input file.
        Parser(std::string filePath);

        ~Parser();
};
