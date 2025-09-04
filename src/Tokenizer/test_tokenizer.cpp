#define CATCH_CONFIG_MAIN
#include <catch2/catch_all.hpp>
#include "Tokenizer.hpp"
#include <sstream>

TEST_CASE("Tokenizer Basic Functionality", "[tokenizer]") {
    Tokenizer tokenizer;
    
    SECTION("Simple PRINT statement") {
        std::string source = "10 PRINT \"Hello, World!\"";
        auto tokens = tokenizer.tokenize(source);
        
        REQUIRE(tokens.size() >= 3);
        REQUIRE(tokens[0].type == Tokenizer::TOKEN_LINE_NUMBER);
        REQUIRE(tokens[0].text == "10");
        
        bool foundPrint = false;
        bool foundString = false;
        for (const auto& token : tokens) {
            if (token.type == Tokenizer::TOKEN_STATEMENT && token.text == "PRINT") {
                foundPrint = true;
            }
            if (token.type == Tokenizer::TOKEN_STRING_LITERAL && token.text == "Hello, World!") {
                foundString = true;
            }
        }
        REQUIRE(foundPrint);
        REQUIRE(foundString);
    }
    
    SECTION("FOR loop statement") {
        std::string source = "20 FOR I = 1 TO 10";
        auto tokens = tokenizer.tokenize(source);
        
        REQUIRE(tokens.size() >= 6);
        
        bool foundFor = false;
        bool foundTo = false;
        bool foundIdentifier = false;
        for (const auto& token : tokens) {
            if (token.type == Tokenizer::TOKEN_STATEMENT && token.text == "FOR") {
                foundFor = true;
            }
            if (token.type == Tokenizer::TOKEN_KEYWORD && token.text == "TO") {
                foundTo = true;
            }
            if (token.type == Tokenizer::TOKEN_IDENTIFIER && token.text == "I") {
                foundIdentifier = true;
            }
        }
        REQUIRE(foundFor);
        REQUIRE(foundTo);
        REQUIRE(foundIdentifier);
    }
    
    SECTION("NEXT statement") {
        std::string source = "30 NEXT I";
        auto tokens = tokenizer.tokenize(source);
        
        bool foundNext = false;
        for (const auto& token : tokens) {
            if (token.type == Tokenizer::TOKEN_STATEMENT && token.text == "NEXT") {
                foundNext = true;
                break;
            }
        }
        REQUIRE(foundNext);
    }
    
    SECTION("IF-THEN-GOTO statement") {
        std::string source = "40 IF X > 5 THEN GOTO 100";
        auto tokens = tokenizer.tokenize(source);
        
        bool foundIf = false;
        bool foundThen = false;
        bool foundGoto = false;
        for (const auto& token : tokens) {
            if (token.type == Tokenizer::TOKEN_STATEMENT && token.text == "IF") {
                foundIf = true;
            }
            if (token.type == Tokenizer::TOKEN_KEYWORD && token.text == "THEN") {
                foundThen = true;
            }
            if (token.type == Tokenizer::TOKEN_STATEMENT && token.text == "GOTO") {
                foundGoto = true;
            }
        }
        REQUIRE(foundIf);
        REQUIRE(foundThen);
        REQUIRE(foundGoto);
    }
    
    SECTION("Mathematical expression with function") {
        std::string source = "50 LET A = SQR(B * B + C * C)";
        auto tokens = tokenizer.tokenize(source);
        
        bool foundLet = false;
        bool foundSqr = false;
        for (const auto& token : tokens) {
            if (token.type == Tokenizer::TOKEN_STATEMENT && token.text == "LET") {
                foundLet = true;
            }
            if (token.type == Tokenizer::TOKEN_FUNCTION_STD && token.text == "SQR") {
                foundSqr = true;
            }
        }
        REQUIRE(foundLet);
        REQUIRE(foundSqr);
    }
    
    SECTION("INPUT statement with prompt") {
        std::string source = "60 INPUT \"Enter a number: \"; N";
        auto tokens = tokenizer.tokenize(source);
        
        bool foundInput = false;
        bool foundString = false;
        for (const auto& token : tokens) {
            if (token.type == Tokenizer::TOKEN_STATEMENT && token.text == "INPUT") {
                foundInput = true;
            }
            if (token.type == Tokenizer::TOKEN_STRING_LITERAL) {
                foundString = true;
            }
        }
        REQUIRE(foundInput);
        REQUIRE(foundString);
    }
    
    SECTION("Comment (apostrophe)") {
        std::string source = "70 ' This is a comment";
        auto tokens = tokenizer.tokenize(source);
        
        bool foundComment = false;
        for (const auto& token : tokens) {
            if (token.type == Tokenizer::TOKEN_REM_COMMENT) {
                foundComment = true;
                break;
            }
        }
        REQUIRE(foundComment);
    }
    
    SECTION("Extended statement") {
        std::string source = "80 FILES";
        auto tokens = tokenizer.tokenize(source);
        
        bool foundFiles = false;
        for (const auto& token : tokens) {
            if ((token.type == Tokenizer::TOKEN_STATEMENT || 
                 token.type == Tokenizer::TOKEN_STATEMENT_EXT) && 
                token.text == "FILES") {
                foundFiles = true;
                break;
            }
        }
        REQUIRE(foundFiles);
    }
    
    SECTION("Graphics statement") {
        std::string source = "90 CIRCLE (100, 100), 50";
        auto tokens = tokenizer.tokenize(source);
        
        bool foundCircle = false;
        for (const auto& token : tokens) {
            if ((token.type == Tokenizer::TOKEN_STATEMENT || 
                 token.type == Tokenizer::TOKEN_STATEMENT_EXT) && 
                token.text == "CIRCLE") {
                foundCircle = true;
                break;
            }
        }
        REQUIRE(foundCircle);
    }
    
    SECTION("END statement") {
        std::string source = "100 END";
        auto tokens = tokenizer.tokenize(source);
        
        bool foundEnd = false;
        for (const auto& token : tokens) {
            if (token.type == Tokenizer::TOKEN_STATEMENT && token.text == "END") {
                foundEnd = true;
                break;
            }
        }
        REQUIRE(foundEnd);
    }
}

TEST_CASE("Tokenizer Crunch and Detokenize", "[tokenizer]") {
    Tokenizer tokenizer;
    
    SECTION("Round trip conversion") {
        std::vector<std::string> testCases = {
            "10 PRINT \"Hello\"",
            "20 FOR I = 1 TO 10",
            "30 END"
        };
        
        for (const auto& source : testCases) {
            INFO("Testing source: " << source);
            
            // Crunch to bytes
            auto crunched = tokenizer.crunch(source);
            REQUIRE_FALSE(crunched.empty());
            
            // Detokenize back to text
            std::string detokenized = tokenizer.detokenize(crunched);
            REQUIRE_FALSE(detokenized.empty());
            
            // The detokenized version should contain the same semantic content
            // (exact string match may not be possible due to formatting)
            REQUIRE((detokenized.find("PRINT") != std::string::npos || source.find("PRINT") == std::string::npos));
            REQUIRE((detokenized.find("FOR") != std::string::npos || source.find("FOR") == std::string::npos));
            REQUIRE((detokenized.find("END") != std::string::npos || source.find("END") == std::string::npos));
        }
    }
}

TEST_CASE("Tokenizer Numbers", "[tokenizer]") {
    Tokenizer tokenizer;
    
    SECTION("Integer numbers") {
        std::string source = "10 LET X = 42";
        auto tokens = tokenizer.tokenize(source);
        
        bool foundInteger = false;
        for (const auto& token : tokens) {
            if (token.type == Tokenizer::TOKEN_NUMBER_INT && token.text == "42") {
                foundInteger = true;
                break;
            }
        }
        REQUIRE(foundInteger);
    }
    
    SECTION("Floating point numbers") {
        std::string source = "10 LET X = 3.14";
        auto tokens = tokenizer.tokenize(source);
        
        bool foundFloat = false;
        for (const auto& token : tokens) {
            if ((token.type == Tokenizer::TOKEN_NUMBER_FLOAT || 
                 token.type == Tokenizer::TOKEN_NUMBER_DOUBLE) && 
                token.text == "3.14") {
                foundFloat = true;
                break;
            }
        }
        REQUIRE(foundFloat);
    }
}

TEST_CASE("Tokenizer Reserved Words", "[tokenizer]") {
    Tokenizer tokenizer;
    
    SECTION("Check reserved word detection") {
        REQUIRE(tokenizer.isReservedWord("PRINT"));
        REQUIRE(tokenizer.isReservedWord("FOR"));
        REQUIRE(tokenizer.isReservedWord("NEXT"));
        REQUIRE(tokenizer.isReservedWord("IF"));
        REQUIRE(tokenizer.isReservedWord("THEN"));
        REQUIRE(tokenizer.isReservedWord("GOTO"));
        REQUIRE(tokenizer.isReservedWord("END"));
        
        REQUIRE_FALSE(tokenizer.isReservedWord("NOTARESERVEDWORD"));
        REQUIRE_FALSE(tokenizer.isReservedWord("X"));
        REQUIRE_FALSE(tokenizer.isReservedWord("VARIABLE"));
    }
    
    SECTION("Get token values") {
        // These should return non-zero token values for reserved words
        REQUIRE(tokenizer.getTokenValue("PRINT") != 0);
        REQUIRE(tokenizer.getTokenValue("FOR") != 0);
        REQUIRE(tokenizer.getTokenValue("END") != 0);
        
        // Non-reserved words should return 0
        REQUIRE(tokenizer.getTokenValue("NOTARESERVEDWORD") == 0);
    }
}

TEST_CASE("Tokenizer Error Handling", "[tokenizer]") {
    Tokenizer tokenizer;
    
    SECTION("Empty string") {
        std::string source = "";
        REQUIRE_NOTHROW(tokenizer.tokenize(source));
        auto tokens = tokenizer.tokenize(source);
        // Should at least have EOF token
        REQUIRE_FALSE(tokens.empty());
    }
    
    SECTION("Whitespace only") {
        std::string source = "   \t  \n  ";
        REQUIRE_NOTHROW(tokenizer.tokenize(source));
    }
    
    SECTION("Unclosed string") {
        std::string source = "10 PRINT \"Unclosed string";
        // This may or may not throw depending on implementation
        // Just ensure it doesn't crash
        REQUIRE_NOTHROW(tokenizer.tokenize(source));
    }
}
