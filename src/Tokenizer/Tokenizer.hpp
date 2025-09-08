#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <cstdint>

/**
 * GW-BASIC Tokenizer (Cruncher) Implementation
 * 
 * Converts BASIC source code into tokenized form compatible with the original
 * GW-BASIC interpreter. Supports single-byte tokens, two-byte prefixed tokens
 * (0xFE, 0xFF, 0xFD), and special character operators.
 */
class Tokenizer {
public:
    // Token type enumeration
    enum TokenType {
        TOKEN_UNKNOWN = 0,
        TOKEN_STATEMENT,        // Single-byte statement tokens (0x80-0xC9)
        TOKEN_KEYWORD,          // Single-byte keywords (THEN, TO, etc.)
        TOKEN_OPERATOR,         // Single-byte operators (+, -, *, etc.)
        TOKEN_FUNCTION_STD,     // Two-byte standard functions (0xFF prefix)
        TOKEN_STATEMENT_EXT,    // Two-byte extended statements (0xFE prefix)
        TOKEN_FUNCTION_EXT,     // Two-byte extended functions (0xFD prefix)
        TOKEN_NUMBER_INT,       // Integer constant
        TOKEN_NUMBER_FLOAT,     // Floating point constant
        TOKEN_NUMBER_DOUBLE,    // Double precision constant
        TOKEN_STRING_LITERAL,   // Quoted string literal
        TOKEN_LINE_NUMBER,      // Line number
        TOKEN_IDENTIFIER,       // Variable or user-defined function name
        TOKEN_REM_COMMENT,      // REM statement or ' comment
        TOKEN_EOF               // End of input
    };

    // Token structure
    struct Token {
        TokenType type;
        std::string text;       // Original text
        std::vector<uint8_t> bytes; // Tokenized bytes
        size_t position;        // Position in source
        size_t length;          // Length in source
        
        Token(TokenType t = TOKEN_UNKNOWN, const std::string& txt = "", 
              size_t pos = 0, size_t len = 0) 
            : type(t), text(txt), position(pos), length(len) {}
    };

    // Constructor
    Tokenizer();
    
    // Main tokenization interface
    std::vector<Token> tokenize(const std::string& source);
    std::vector<uint8_t> crunch(const std::string& source);
    
    // Utility functions
    std::string detokenize(const std::vector<uint8_t>& tokens);
    bool isReservedWord(const std::string& word) const;
    uint8_t getTokenValue(const std::string& word) const;
    std::string getTokenName(uint8_t token) const;
    
    // Configuration
    void setExtendedMode(bool enabled) { extendedMode = enabled; }
    bool getExtendedMode() const { return extendedMode; }

private:
    // Internal state
    bool extendedMode;
    std::string currentSource;
    size_t currentPosition;
    char currentChar;
    
    // Token tables
    struct ReservedWord {
        std::string name;
        uint8_t token;
        TokenType type;
        bool isFunction;
        uint8_t prefix;  // 0 for single-byte, 0xFE/0xFF/0xFD for two-byte
        uint8_t index;   // Index for two-byte tokens
    };
    
    std::unordered_map<std::string, ReservedWord> reservedWords;
    std::unordered_map<char, uint8_t> operatorTokens;
    std::unordered_map<uint8_t, std::string> tokenNames;
    
    // Alphabetic dispatch tables (A-Z)
    std::vector<std::vector<std::string>> alphabetTables;
    
    // Initialization
    void initializeTables();
    void addStatement(const std::string& name, uint8_t token);
    void addKeyword(const std::string& name, uint8_t token);
    void addOperator(char op, uint8_t token);
    void addStandardFunction(const std::string& name, uint8_t index);
    void addExtendedStatement(const std::string& name, uint8_t index);
    void addExtendedFunction(const std::string& name, uint8_t index);
    
    // Lexical analysis
    void advance();
    void skipWhitespace();
    void handleLineEnd();
    std::string preprocessLineContinuation(const std::string& source);
    char peekChar(size_t offset = 1) const;
    bool isAtEnd() const;
    bool isAlpha(char c) const;
    bool isDigit(char c) const;
    bool isAlphaNumeric(char c) const;
    
    // Token recognition
    Token scanToken();
    Token scanNumber();
    Token scanHexNumber();
    Token scanOctalNumber();
    Token scanString();
    Token scanIdentifier();
    Token scanLineNumber();
    Token scanComment();
    Token scanOperator();
    
    // Reserved word matching
    Token matchReservedWord(const std::string& word);
    bool matchMultiWordToken(const std::string& word1, std::string& fullToken);
    
    // Number parsing
    Token parseIntegerConstant(const std::string& numStr);
    Token parseFloatingConstant(const std::string& numStr);
    Token parseDoubleConstant(const std::string& numStr);
    
    // Utility
    std::string toUpperCase(const std::string& str) const;
    bool startsWith(const std::string& str, const std::string& prefix) const;
    std::vector<uint8_t> encodeNumber(double value, TokenType type);
    std::vector<uint8_t> encodeLineNumber(int lineNum);
    
    // Error handling
    void error(const std::string& message);
    
    // Constants from IBMRES.ASM
    static constexpr uint8_t FIRST_STATEMENT_TOKEN = 0x80;  // END token
    static constexpr uint8_t EXTENDED_STATEMENT_PREFIX = 0xFE;
    static constexpr uint8_t STANDARD_FUNCTION_PREFIX = 0xFF;
    static constexpr uint8_t EXTENDED_FUNCTION_PREFIX = 0xFD;
    
    // Statement tokens (0x80-0xC9)
    static constexpr uint8_t TOKEN_END = 0x80;
    static constexpr uint8_t TOKEN_FOR = 0x81;
    static constexpr uint8_t TOKEN_NEXT = 0x82;
    static constexpr uint8_t TOKEN_DATA = 0x83;
    static constexpr uint8_t TOKEN_INPUT = 0x84;
    static constexpr uint8_t TOKEN_DIM = 0x85;
    static constexpr uint8_t TOKEN_READ = 0x86;
    static constexpr uint8_t TOKEN_LET = 0x87;
    static constexpr uint8_t TOKEN_GOTO = 0x88;
    static constexpr uint8_t TOKEN_RUN = 0x89;
    static constexpr uint8_t TOKEN_IF = 0x8A;
    static constexpr uint8_t TOKEN_RESTORE = 0x8B;
    static constexpr uint8_t TOKEN_GOSUB = 0x8C;
    static constexpr uint8_t TOKEN_RETURN = 0x8D;
    static constexpr uint8_t TOKEN_REM = 0x8E;
    static constexpr uint8_t TOKEN_STOP = 0x8F;
    static constexpr uint8_t TOKEN_PRINT = 0x90;
    static constexpr uint8_t TOKEN_CLEAR = 0x91;
    static constexpr uint8_t TOKEN_LIST = 0x92;
    static constexpr uint8_t TOKEN_NEW = 0x93;
    static constexpr uint8_t TOKEN_ON = 0x94;
    static constexpr uint8_t TOKEN_WAIT = 0x95;
    static constexpr uint8_t TOKEN_DEF = 0x96;
    static constexpr uint8_t TOKEN_POKE = 0x97;
    static constexpr uint8_t TOKEN_CONT = 0x98;
    static constexpr uint8_t TOKEN_OUT = 0x9B;
    static constexpr uint8_t TOKEN_LPRINT = 0x9C;
    static constexpr uint8_t TOKEN_LLIST = 0x9D;
    static constexpr uint8_t TOKEN_WIDTH = 0x9F;
    static constexpr uint8_t TOKEN_ELSE = 0xA0;
    static constexpr uint8_t TOKEN_TRON = 0xA1;
    static constexpr uint8_t TOKEN_TROFF = 0xA2;
    static constexpr uint8_t TOKEN_SWAP = 0xA3;
    static constexpr uint8_t TOKEN_ERASE = 0xA4;
    static constexpr uint8_t TOKEN_EDIT = 0xA5;
    static constexpr uint8_t TOKEN_ERROR = 0xA6;
    static constexpr uint8_t TOKEN_RESUME = 0xA7;
    static constexpr uint8_t TOKEN_DELETE = 0xA8;
    static constexpr uint8_t TOKEN_AUTO = 0xA9;
    static constexpr uint8_t TOKEN_RENUM = 0xAA;
    static constexpr uint8_t TOKEN_DEFSTR = 0xAB;
    static constexpr uint8_t TOKEN_DEFINT = 0xAC;
    static constexpr uint8_t TOKEN_DEFSNG = 0xAD;
    static constexpr uint8_t TOKEN_DEFDBL = 0xAE;
    static constexpr uint8_t TOKEN_LINE = 0xAF;
    static constexpr uint8_t TOKEN_WHILE = 0xB0;
    static constexpr uint8_t TOKEN_WEND = 0xB1;
    static constexpr uint8_t TOKEN_CALL = 0xB2;
    static constexpr uint8_t TOKEN_WRITE = 0xB6;
    static constexpr uint8_t TOKEN_OPTION = 0xB7;
    static constexpr uint8_t TOKEN_RANDOMIZE = 0xB8;
    static constexpr uint8_t TOKEN_OPEN = 0xB9;
    static constexpr uint8_t TOKEN_CLOSE = 0xBA;
    static constexpr uint8_t TOKEN_LOAD = 0xBB;
    static constexpr uint8_t TOKEN_MERGE = 0xBC;
    static constexpr uint8_t TOKEN_SAVE = 0xBD;
    static constexpr uint8_t TOKEN_COLOR = 0xBE;
    static constexpr uint8_t TOKEN_CLS = 0xBF;
    static constexpr uint8_t TOKEN_MOTOR = 0xC0;
    static constexpr uint8_t TOKEN_BSAVE = 0xC1;
    static constexpr uint8_t TOKEN_BLOAD = 0xC2;
    static constexpr uint8_t TOKEN_SOUND = 0xC3;
    static constexpr uint8_t TOKEN_BEEP = 0xC4;
    static constexpr uint8_t TOKEN_PSET = 0xC5;
    static constexpr uint8_t TOKEN_PRESET = 0xC6;
    static constexpr uint8_t TOKEN_SCREEN = 0xC7;
    static constexpr uint8_t TOKEN_KEY = 0xC8;
    static constexpr uint8_t TOKEN_LOCATE = 0xC9;
};

