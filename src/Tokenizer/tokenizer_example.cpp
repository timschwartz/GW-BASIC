#include "Tokenizer.hpp"
#include <iostream>
#include <fstream>
#include <string>

/**
 * Simple example program showing how to use the tokenizer
 * to process a BASIC program file.
 */

void processBasicFile(const std::string& filename) {
    std::ifstream file(filename);
    if (!file.is_open()) {
        std::cerr << "Error: Cannot open file " << filename << std::endl;
        return;
    }
    
    Tokenizer tokenizer;
    std::string line;
    int lineCount = 0;
    
    std::cout << "Processing BASIC file: " << filename << std::endl;
    std::cout << std::string(50, '=') << std::endl;
    
    while (std::getline(file, line)) {
        lineCount++;
        
        if (line.empty()) continue;
        
        try {
            // Tokenize the line
            auto tokens = tokenizer.tokenize(line);
            
            // Show the tokenization
            std::cout << "Line " << lineCount << ": " << line << std::endl;
            
            // Convert to bytes
            auto bytes = tokenizer.crunch(line);
            
            std::cout << "  Tokens: ";
            for (const auto& token : tokens) {
                if (token.type == Tokenizer::TOKEN_EOF) break;
                
                switch (token.type) {
                    case Tokenizer::TOKEN_STATEMENT:
                    case Tokenizer::TOKEN_STATEMENT_EXT:
                        std::cout << "[" << token.text << "] ";
                        break;
                    case Tokenizer::TOKEN_FUNCTION_STD:
                    case Tokenizer::TOKEN_FUNCTION_EXT:
                        std::cout << token.text << "() ";
                        break;
                    case Tokenizer::TOKEN_NUMBER_INT:
                    case Tokenizer::TOKEN_NUMBER_FLOAT:
                    case Tokenizer::TOKEN_NUMBER_DOUBLE:
                        std::cout << token.text << " ";
                        break;
                    case Tokenizer::TOKEN_STRING_LITERAL:
                        std::cout << "\"" << token.text << "\" ";
                        break;
                    case Tokenizer::TOKEN_IDENTIFIER:
                        std::cout << token.text << " ";
                        break;
                    default:
                        std::cout << token.text << " ";
                        break;
                }
            }
            std::cout << std::endl;
            
            // Show byte count
            std::cout << "  Crunched: " << bytes.size() << " bytes" << std::endl;
            
            // Verify round-trip
            std::string restored = tokenizer.detokenize(bytes);
            if (restored != line) {
                std::cout << "  Restored: " << restored << std::endl;
            }
            
        } catch (const std::exception& e) {
            std::cout << "  Error: " << e.what() << std::endl;
        }
        
        std::cout << std::endl;
    }
    
    std::cout << "Processed " << lineCount << " lines." << std::endl;
}

void interactiveMode() {
    Tokenizer tokenizer;
    std::string line;
    
    std::cout << "GW-BASIC Tokenizer - Interactive Mode" << std::endl;
    std::cout << "Enter BASIC code lines (empty line to exit):" << std::endl;
    std::cout << std::string(40, '-') << std::endl;
    
    while (true) {
        std::cout << "> ";
        if (!std::getline(std::cin, line) || line.empty()) {
            break;
        }
        
        try {
            auto tokens = tokenizer.tokenize(line);
            auto bytes = tokenizer.crunch(line);
            std::string restored = tokenizer.detokenize(bytes);
            
            std::cout << "  Tokenized: ";
            for (const auto& token : tokens) {
                if (token.type == Tokenizer::TOKEN_EOF) break;
                std::cout << "[" << token.text << "] ";
            }
            std::cout << std::endl;
            
            std::cout << "  Bytes (" << bytes.size() << "): ";
            for (size_t i = 0; i < bytes.size() && i < 20; i++) {
                printf("%02X ", bytes[i]);
            }
            if (bytes.size() > 20) {
                std::cout << "...";
            }
            std::cout << std::endl;
            
            std::cout << "  Restored: " << restored << std::endl;
            
        } catch (const std::exception& e) {
            std::cout << "  Error: " << e.what() << std::endl;
        }
        
        std::cout << std::endl;
    }
}

int main(int argc, char* argv[]) {
    if (argc > 1) {
        // Process file(s)
        for (int i = 1; i < argc; i++) {
            processBasicFile(argv[i]);
            if (i < argc - 1) {
                std::cout << std::endl << std::string(60, '=') << std::endl << std::endl;
            }
        }
    } else {
        // Interactive mode
        interactiveMode();
    }
    
    return 0;
}
