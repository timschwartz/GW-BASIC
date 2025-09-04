#include "Tokenizer.hpp"
#include <algorithm>
#include <cctype>
#include <sstream>
#include <stdexcept>
#include <iomanip>
#include <cmath>

Tokenizer::Tokenizer() : extendedMode(true), currentPosition(0), currentChar('\0') {
    initializeTables();
}

void Tokenizer::initializeTables() {
    // Initialize alphabet tables
    alphabetTables.resize(26);
    
    // Add statement tokens (single-byte, 0x80-0xC9)
    addStatement("END", TOKEN_END);
    addStatement("FOR", TOKEN_FOR);
    addStatement("NEXT", TOKEN_NEXT);
    addStatement("DATA", TOKEN_DATA);
    addStatement("INPUT", TOKEN_INPUT);
    addStatement("DIM", TOKEN_DIM);
    addStatement("READ", TOKEN_READ);
    addStatement("LET", TOKEN_LET);
    addStatement("GOTO", TOKEN_GOTO);
    addStatement("RUN", TOKEN_RUN);
    addStatement("IF", TOKEN_IF);
    addStatement("RESTORE", TOKEN_RESTORE);
    addStatement("GOSUB", TOKEN_GOSUB);
    addStatement("RETURN", TOKEN_RETURN);
    addStatement("REM", TOKEN_REM);
    addStatement("STOP", TOKEN_STOP);
    addStatement("PRINT", TOKEN_PRINT);
    addStatement("CLEAR", TOKEN_CLEAR);
    addStatement("LIST", TOKEN_LIST);
    addStatement("NEW", TOKEN_NEW);
    addStatement("ON", TOKEN_ON);
    addStatement("WAIT", TOKEN_WAIT);
    addStatement("DEF", TOKEN_DEF);
    addStatement("POKE", TOKEN_POKE);
    addStatement("CONT", TOKEN_CONT);
    addStatement("OUT", TOKEN_OUT);
    addStatement("LPRINT", TOKEN_LPRINT);
    addStatement("LLIST", TOKEN_LLIST);
    addStatement("WIDTH", TOKEN_WIDTH);
    addStatement("ELSE", TOKEN_ELSE);
    addStatement("TRON", TOKEN_TRON);
    addStatement("TROFF", TOKEN_TROFF);
    addStatement("SWAP", TOKEN_SWAP);
    addStatement("ERASE", TOKEN_ERASE);
    addStatement("EDIT", TOKEN_EDIT);
    addStatement("ERROR", TOKEN_ERROR);
    addStatement("RESUME", TOKEN_RESUME);
    addStatement("DELETE", TOKEN_DELETE);
    addStatement("AUTO", TOKEN_AUTO);
    addStatement("RENUM", TOKEN_RENUM);
    addStatement("DEFSTR", TOKEN_DEFSTR);
    addStatement("DEFINT", TOKEN_DEFINT);
    addStatement("DEFSNG", TOKEN_DEFSNG);
    addStatement("DEFDBL", TOKEN_DEFDBL);
    addStatement("LINE", TOKEN_LINE);
    addStatement("WHILE", TOKEN_WHILE);
    addStatement("WEND", TOKEN_WEND);
    addStatement("CALL", TOKEN_CALL);
    addStatement("WRITE", TOKEN_WRITE);
    addStatement("OPTION", TOKEN_OPTION);
    addStatement("RANDOMIZE", TOKEN_RANDOMIZE);
    addStatement("OPEN", TOKEN_OPEN);
    addStatement("CLOSE", TOKEN_CLOSE);
    addStatement("LOAD", TOKEN_LOAD);
    addStatement("MERGE", TOKEN_MERGE);
    addStatement("SAVE", TOKEN_SAVE);
    addStatement("COLOR", TOKEN_COLOR);
    addStatement("CLS", TOKEN_CLS);
    addStatement("MOTOR", TOKEN_MOTOR);
    addStatement("BSAVE", TOKEN_BSAVE);
    addStatement("BLOAD", TOKEN_BLOAD);
    addStatement("SOUND", TOKEN_SOUND);
    addStatement("BEEP", TOKEN_BEEP);
    addStatement("PSET", TOKEN_PSET);
    addStatement("PRESET", TOKEN_PRESET);
    addStatement("SCREEN", TOKEN_SCREEN);
    addStatement("KEY", TOKEN_KEY);
    addStatement("LOCATE", TOKEN_LOCATE);
    
    // Add keywords (continuing after statements)
    uint8_t keywordStart = TOKEN_LOCATE + 1;
    addKeyword("TO", keywordStart++);
    addKeyword("THEN", keywordStart++);
    addKeyword("TAB", keywordStart++);
    addKeyword("STEP", keywordStart++);
    addKeyword("USR", keywordStart++);
    addKeyword("FN", keywordStart++);
    addKeyword("SPC", keywordStart++);
    addKeyword("NOT", keywordStart++);
    addKeyword("ERL", keywordStart++);
    addKeyword("ERR", keywordStart++);
    addKeyword("STRING$", keywordStart++);
    addKeyword("USING", keywordStart++);
    addKeyword("INSTR", keywordStart++);
    addKeyword("VARPTR", keywordStart++);
    addKeyword("CSRLIN", keywordStart++);
    addKeyword("POINT", keywordStart++);
    addKeyword("OFF", keywordStart++);
    addKeyword("INKEY$", keywordStart++);
    
    // Add operators (with gaps for token alignment)
    uint8_t operatorStart = keywordStart + 7;
    addOperator('>', operatorStart++);  // GREATK
    addOperator('=', operatorStart++);  // EQULTK
    addOperator('<', operatorStart++);  // LESSTK
    addOperator('+', operatorStart++);  // PLUSTK
    addOperator('-', operatorStart++);  // MINUTK
    addOperator('*', operatorStart++);  // MULTK
    addOperator('/', operatorStart++);  // DIVTK
    addOperator('^', operatorStart++);  // EXPTK
    
    // Logical operators as keywords
    addKeyword("AND", operatorStart++);
    addKeyword("OR", operatorStart++);
    addKeyword("XOR", operatorStart++);
    addKeyword("EQV", operatorStart++);
    addKeyword("IMP", operatorStart++);
    addKeyword("MOD", operatorStart++);
    
    addOperator('\\', operatorStart++); // IDIVTK (integer division)
    addOperator('\'', operatorStart++); // SNGQTK (single quote -> REM)
    
    // Add standard functions (two-byte tokens with 0xFF prefix)
    addStandardFunction("LEFT$", 0x00);
    addStandardFunction("RIGHT$", 0x01);
    addStandardFunction("MID$", 0x02);
    addStandardFunction("SGN", 0x03);
    addStandardFunction("INT", 0x04);
    addStandardFunction("ABS", 0x05);
    addStandardFunction("SQR", 0x06);
    addStandardFunction("RND", 0x07);
    addStandardFunction("SIN", 0x08);
    addStandardFunction("LOG", 0x09);
    addStandardFunction("EXP", 0x0A);
    addStandardFunction("COS", 0x0B);
    addStandardFunction("TAN", 0x0C);
    addStandardFunction("ATN", 0x0D);
    addStandardFunction("FRE", 0x0E);
    addStandardFunction("INP", 0x0F);
    addStandardFunction("POS", 0x10);
    addStandardFunction("LEN", 0x11);
    addStandardFunction("STR$", 0x12);
    addStandardFunction("VAL", 0x13);
    addStandardFunction("ASC", 0x14);
    addStandardFunction("CHR$", 0x15);
    addStandardFunction("PEEK", 0x16);
    addStandardFunction("SPACE$", 0x17);
    addStandardFunction("OCT$", 0x18);
    addStandardFunction("HEX$", 0x19);
    addStandardFunction("LPOS", 0x1A);
    addStandardFunction("CINT", 0x1B);
    addStandardFunction("CSNG", 0x1C);
    addStandardFunction("CDBL", 0x1D);
    addStandardFunction("FIX", 0x1E);
    addStandardFunction("PEN", 0x1F);
    addStandardFunction("STICK", 0x20);
    addStandardFunction("STRIG", 0x21);
    addStandardFunction("EOF", 0x22);
    addStandardFunction("LOC", 0x23);
    addStandardFunction("LOF", 0x24);
    
    // Add extended statements (two-byte tokens with 0xFE prefix)
    if (extendedMode) {
        addExtendedStatement("FILES", 0x00);
        addExtendedStatement("FIELD", 0x01);
        addExtendedStatement("SYSTEM", 0x02);
        addExtendedStatement("NAME", 0x03);
        addExtendedStatement("LSET", 0x04);
        addExtendedStatement("RSET", 0x05);
        addExtendedStatement("KILL", 0x06);
        addExtendedStatement("PUT", 0x07);
        addExtendedStatement("GET", 0x08);
        addExtendedStatement("RESET", 0x09);
        addExtendedStatement("COMMON", 0x0A);
        addExtendedStatement("CHAIN", 0x0B);
        addExtendedStatement("DATE$", 0x0C);
        addExtendedStatement("TIME$", 0x0D);
        addExtendedStatement("PAINT", 0x0E);
        addExtendedStatement("COM", 0x0F);
        addExtendedStatement("CIRCLE", 0x10);
        addExtendedStatement("DRAW", 0x11);
        addExtendedStatement("PLAY", 0x12);
        addExtendedStatement("TIMER", 0x13);
        addExtendedStatement("ERDEV", 0x14);
        addExtendedStatement("IOCTL", 0x15);
        addExtendedStatement("CHDIR", 0x16);
        addExtendedStatement("MKDIR", 0x17);
        addExtendedStatement("RMDIR", 0x18);
        addExtendedStatement("SHELL", 0x19);
        addExtendedStatement("ENVIRON", 0x1A);
        addExtendedStatement("VIEW", 0x1B);
        addExtendedStatement("WINDOW", 0x1C);
        addExtendedStatement("PMAP", 0x1D);
        addExtendedStatement("PALETTE", 0x1E);
        addExtendedStatement("LCOPY", 0x1F);
        addExtendedStatement("CALLS", 0x20);
        
        // Add extended functions (two-byte tokens with 0xFD prefix)
        addExtendedFunction("CVI", 0x00);
        addExtendedFunction("CVS", 0x01);
        addExtendedFunction("CVD", 0x02);
        addExtendedFunction("MKI$", 0x03);
        addExtendedFunction("MKS$", 0x04);
        addExtendedFunction("MKD$", 0x05);
        addExtendedFunction("KTN", 0x06);
        addExtendedFunction("JIS", 0x07);
        addExtendedFunction("KPOS", 0x08);
        addExtendedFunction("KLEN", 0x09);
    }
}

void Tokenizer::addStatement(const std::string& name, uint8_t token) {
    ReservedWord word;
    word.name = name;
    word.token = token;
    word.type = TOKEN_STATEMENT;
    word.isFunction = false;
    word.prefix = 0;
    word.index = 0;
    
    reservedWords[name] = word;
    tokenNames[token] = name;
    
    // Add to alphabetic dispatch table
    if (!name.empty()) {
        char firstChar = std::toupper(name[0]);
        if (firstChar >= 'A' && firstChar <= 'Z') {
            alphabetTables[firstChar - 'A'].push_back(name);
        }
    }
}

void Tokenizer::addKeyword(const std::string& name, uint8_t token) {
    ReservedWord word;
    word.name = name;
    word.token = token;
    word.type = TOKEN_KEYWORD;
    word.isFunction = false;
    word.prefix = 0;
    word.index = 0;
    
    reservedWords[name] = word;
    tokenNames[token] = name;
    
    // Add to alphabetic dispatch table
    if (!name.empty()) {
        char firstChar = std::toupper(name[0]);
        if (firstChar >= 'A' && firstChar <= 'Z') {
            alphabetTables[firstChar - 'A'].push_back(name);
        }
    }
}

void Tokenizer::addOperator(char op, uint8_t token) {
    operatorTokens[op] = token;
    tokenNames[token] = std::string(1, op);
}

void Tokenizer::addStandardFunction(const std::string& name, uint8_t index) {
    ReservedWord word;
    word.name = name;
    word.token = 0; // Not used for two-byte tokens
    word.type = TOKEN_FUNCTION_STD;
    word.isFunction = true;
    word.prefix = STANDARD_FUNCTION_PREFIX;
    word.index = index;
    
    reservedWords[name] = word;
    
    // Add to alphabetic dispatch table
    if (!name.empty()) {
        char firstChar = std::toupper(name[0]);
        if (firstChar >= 'A' && firstChar <= 'Z') {
            alphabetTables[firstChar - 'A'].push_back(name);
        }
    }
}

void Tokenizer::addExtendedStatement(const std::string& name, uint8_t index) {
    ReservedWord word;
    word.name = name;
    word.token = 0; // Not used for two-byte tokens
    word.type = TOKEN_STATEMENT_EXT;
    word.isFunction = false;
    word.prefix = EXTENDED_STATEMENT_PREFIX;
    word.index = index;
    
    reservedWords[name] = word;
    
    // Add to alphabetic dispatch table
    if (!name.empty()) {
        char firstChar = std::toupper(name[0]);
        if (firstChar >= 'A' && firstChar <= 'Z') {
            alphabetTables[firstChar - 'A'].push_back(name);
        }
    }
}

void Tokenizer::addExtendedFunction(const std::string& name, uint8_t index) {
    ReservedWord word;
    word.name = name;
    word.token = 0; // Not used for two-byte tokens
    word.type = TOKEN_FUNCTION_EXT;
    word.isFunction = true;
    word.prefix = EXTENDED_FUNCTION_PREFIX;
    word.index = index;
    
    reservedWords[name] = word;
    
    // Add to alphabetic dispatch table
    if (!name.empty()) {
        char firstChar = std::toupper(name[0]);
        if (firstChar >= 'A' && firstChar <= 'Z') {
            alphabetTables[firstChar - 'A'].push_back(name);
        }
    }
}

std::vector<Tokenizer::Token> Tokenizer::tokenize(const std::string& source) {
    currentSource = source;
    currentPosition = 0;
    currentChar = source.empty() ? '\0' : source[0];
    
    std::vector<Token> tokens;
    
    while (!isAtEnd()) {
        Token token = scanToken();
        if (token.type != TOKEN_UNKNOWN) {
            tokens.push_back(token);
        }
    }
    
    // Add EOF token
    tokens.push_back(Token(TOKEN_EOF, "", currentPosition, 0));
    
    return tokens;
}

std::vector<uint8_t> Tokenizer::crunch(const std::string& source) {
    std::vector<Token> tokens = tokenize(source);
    std::vector<uint8_t> result;
    
    for (const auto& token : tokens) {
        if (token.type == TOKEN_EOF) {
            break;
        }
        
        // Add the tokenized bytes
        result.insert(result.end(), token.bytes.begin(), token.bytes.end());
    }
    
    // Add null terminator
    result.push_back(0x00);
    
    return result;
}

std::string Tokenizer::detokenize(const std::vector<uint8_t>& tokens) {
    std::ostringstream result;
    size_t i = 0;
    
    while (i < tokens.size() && tokens[i] != 0x00) {
        uint8_t token = tokens[i];
        
        // Check for special numeric/data tokens first
        if (token == 0x0D) {
            // Line number
            if (i + 2 < tokens.size()) {
                uint16_t lineNum = tokens[i + 1] | (tokens[i + 2] << 8);
                result << lineNum << " ";
                i += 3;
            } else {
                i++;
            }
        } else if (token == 0x11) {
            // Integer constant
            if (i + 2 < tokens.size()) {
                int16_t intVal = tokens[i + 1] | (tokens[i + 2] << 8);
                result << intVal;
                i += 3;
            } else {
                i++;
            }
        } else if (token == 0x1D) {
            // Float constant
            if (i + 4 < tokens.size()) {
                union { float f; uint32_t i; } floatUnion;
                floatUnion.i = tokens[i + 1] | (tokens[i + 2] << 8) | 
                              (tokens[i + 3] << 16) | (tokens[i + 4] << 24);
                result << floatUnion.f;
                i += 5;
            } else {
                i++;
            }
        } else if (token == 0x1F) {
            // Double constant
            if (i + 8 < tokens.size()) {
                union { double d; uint64_t i; } doubleUnion;
                doubleUnion.i = 0;
                for (int j = 0; j < 8; j++) {
                    doubleUnion.i |= static_cast<uint64_t>(tokens[i + 1 + j]) << (j * 8);
                }
                result << doubleUnion.d;
                i += 9;
            } else {
                i++;
            }
        } else if (token >= FIRST_STATEMENT_TOKEN) {
            // Single-byte token
            auto it = tokenNames.find(token);
            if (it != tokenNames.end()) {
                result << it->second;
                if (token != TOKEN_REM && !result.str().empty() && 
                    result.str().back() != ' ') {
                    result << " ";
                }
            } else {
                result << "[UNKNOWN:" << std::hex << (int)token << std::dec << "]";
            }
            i++;
        } else if (token == EXTENDED_STATEMENT_PREFIX || 
                   token == STANDARD_FUNCTION_PREFIX || 
                   token == EXTENDED_FUNCTION_PREFIX) {
            // Two-byte token
            if (i + 1 < tokens.size()) {
                uint8_t index = tokens[i + 1];
                bool found = false;
                // Find the corresponding reserved word
                for (const auto& pair : reservedWords) {
                    const ReservedWord& word = pair.second;
                    if (word.prefix == token && word.index == index) {
                        result << word.name;
                        if (!result.str().empty() && result.str().back() != ' ') {
                            result << " ";
                        }
                        found = true;
                        break;
                    }
                }
                if (!found) {
                    result << "[UNKNOWN:" << std::hex << (int)token << ":" 
                            << (int)index << std::dec << "]";
                }
                i += 2;
            } else {
                result << "[INCOMPLETE]";
                i++;
            }
        } else if (token == '"') {
            // String literal
            result << '"';
            i++;
            while (i < tokens.size() && tokens[i] != '"' && tokens[i] != 0x00) {
                result << static_cast<char>(tokens[i]);
                i++;
            }
            if (i < tokens.size() && tokens[i] == '"') {
                result << '"';
                i++;
            }
        } else {
            // Regular character
            result << static_cast<char>(token);
            i++;
        }
    }
    
    return result.str();
}

Tokenizer::Token Tokenizer::scanToken() {
    skipWhitespace();
    
    if (isAtEnd()) {
        return Token(TOKEN_EOF, "", currentPosition, 0);
    }
    
    size_t start = currentPosition;
    
    // Check for line numbers at start of line
    if (start == 0 || (start > 0 && currentSource[start - 1] == '\n')) {
        if (isDigit(currentChar)) {
            return scanLineNumber();
        }
    }
    
    // Check for numbers
    if (isDigit(currentChar)) {
        return scanNumber();
    }
    
    // Check for strings
    if (currentChar == '"') {
        return scanString();
    }
    
    // Check for comments
    if (currentChar == '\'') {
        return scanComment();
    }
    
    // Check for operators
    if (operatorTokens.find(currentChar) != operatorTokens.end()) {
        return scanOperator();
    }
    
    // Check for identifiers/reserved words
    if (isAlpha(currentChar)) {
        return scanIdentifier();
    }
    
    // Skip unknown characters
    advance();
    return Token(TOKEN_UNKNOWN, "", start, 1);
}

Tokenizer::Token Tokenizer::scanNumber() {
    size_t start = currentPosition;
    std::string numStr;
    bool hasDecimal = false;
    bool hasExponent = false;
    
    while (isDigit(currentChar) || currentChar == '.' || 
           currentChar == 'E' || currentChar == 'e' ||
           currentChar == '+' || currentChar == '-') {
        
        if (currentChar == '.') {
            if (hasDecimal || hasExponent) break;
            hasDecimal = true;
        } else if (currentChar == 'E' || currentChar == 'e') {
            if (hasExponent) break;
            hasExponent = true;
            numStr += currentChar;
            advance();
            if (currentChar == '+' || currentChar == '-') {
                numStr += currentChar;
                advance();
            }
            continue;
        }
        
        numStr += currentChar;
        advance();
    }
    
    Token token(TOKEN_NUMBER_INT, numStr, start, currentPosition - start);
    
    // Determine number type and encode
    if (hasDecimal || hasExponent) {
        // Check for double precision indicator (D suffix or # suffix)
        if (currentChar == 'D' || currentChar == '#') {
            token.type = TOKEN_NUMBER_DOUBLE;
            token.text += currentChar;
            token.length++;
            advance();
        } else {
            token.type = TOKEN_NUMBER_FLOAT;
        }
    }
    
    // Encode the number into bytes
    double value = std::stod(numStr);
    token.bytes = encodeNumber(value, token.type);
    
    return token;
}

Tokenizer::Token Tokenizer::scanString() {
    size_t start = currentPosition;
    std::string value;
    
    advance(); // Skip opening quote
    
    while (!isAtEnd() && currentChar != '"') {
        value += currentChar;
        advance();
    }
    
    if (currentChar == '"') {
        advance(); // Skip closing quote
    }
    
    Token token(TOKEN_STRING_LITERAL, value, start, currentPosition - start);
    
    // String literals are stored as-is with quote markers
    token.bytes.push_back('"');
    for (char c : value) {
        token.bytes.push_back(static_cast<uint8_t>(c));
    }
    token.bytes.push_back('"');
    
    return token;
}

Tokenizer::Token Tokenizer::scanIdentifier() {
    size_t start = currentPosition;
    std::string word;
    
    while (isAlphaNumeric(currentChar) || currentChar == '$' || currentChar == '%' || 
           currentChar == '!' || currentChar == '#' || currentChar == '&') {
        word += std::toupper(currentChar);
        advance();
    }
    
    // Check for multi-word tokens like "GO TO"
    std::string fullToken;
    if (matchMultiWordToken(word, fullToken)) {
        word = fullToken;
    }
    
    // Check if it's a reserved word
    Token token = matchReservedWord(word);
    if (token.type != TOKEN_UNKNOWN) {
        token.position = start;
        token.length = currentPosition - start;
        return token;
    }
    
    // It's an identifier
    token = Token(TOKEN_IDENTIFIER, word, start, currentPosition - start);
    
    // Identifiers are stored as-is
    for (char c : word) {
        token.bytes.push_back(static_cast<uint8_t>(c));
    }
    
    return token;
}

Tokenizer::Token Tokenizer::scanLineNumber() {
    size_t start = currentPosition;
    std::string numStr;
    
    while (isDigit(currentChar)) {
        numStr += currentChar;
        advance();
    }
    
    Token token(TOKEN_LINE_NUMBER, numStr, start, currentPosition - start);
    
    // Encode line number
    int lineNum = std::stoi(numStr);
    token.bytes = encodeLineNumber(lineNum);
    
    return token;
}

Tokenizer::Token Tokenizer::scanComment() {
    size_t start = currentPosition;
    std::string comment;
    
    // Single quote comment
    advance(); // Skip '
    
    while (!isAtEnd() && currentChar != '\n' && currentChar != '\r') {
        comment += currentChar;
        advance();
    }
    
    Token token(TOKEN_REM_COMMENT, comment, start, currentPosition - start);
    
    // Single quote is tokenized as REM
    auto it = operatorTokens.find('\'');
    if (it != operatorTokens.end()) {
        token.bytes.push_back(it->second);
    }
    
    // Add the comment text
    for (char c : comment) {
        token.bytes.push_back(static_cast<uint8_t>(c));
    }
    
    return token;
}

Tokenizer::Token Tokenizer::scanOperator() {
    size_t start = currentPosition;
    char op = currentChar;
    advance();
    
    Token token(TOKEN_OPERATOR, std::string(1, op), start, 1);
    
    auto it = operatorTokens.find(op);
    if (it != operatorTokens.end()) {
        token.bytes.push_back(it->second);
    }
    
    return token;
}

Tokenizer::Token Tokenizer::matchReservedWord(const std::string& word) {
    auto it = reservedWords.find(word);
    if (it == reservedWords.end()) {
        return Token(TOKEN_UNKNOWN);
    }
    
    const ReservedWord& resWord = it->second;
    Token token(resWord.type, word);
    
    if (resWord.prefix == 0) {
        // Single-byte token
        token.bytes.push_back(resWord.token);
    } else {
        // Two-byte token
        token.bytes.push_back(resWord.prefix);
        token.bytes.push_back(resWord.index);
    }
    
    return token;
}

bool Tokenizer::matchMultiWordToken(const std::string& word1, std::string& fullToken) {
    if (word1 == "GO") {
        skipWhitespace();
        if (currentPosition + 1 < currentSource.length() &&
            std::toupper(currentSource[currentPosition]) == 'T' &&
            std::toupper(currentSource[currentPosition + 1]) == 'O') {
            advance(); // T
            advance(); // O
            fullToken = "GOTO";
            return true;
        }
    }
    
    return false;
}

std::vector<uint8_t> Tokenizer::encodeNumber(double value, TokenType type) {
    std::vector<uint8_t> bytes;
    
    switch (type) {
        case TOKEN_NUMBER_INT: {
            // Integer constants (16-bit) - use correct GW-BASIC encoding
            int16_t intVal = static_cast<int16_t>(value);
            bytes.push_back(0x11); // Integer token marker (from GW-BASIC)
            bytes.push_back(intVal & 0xFF);
            bytes.push_back((intVal >> 8) & 0xFF);
            break;
        }
        case TOKEN_NUMBER_FLOAT: {
            // Single precision floating point (32-bit)
            float floatVal = static_cast<float>(value);
            union { float f; uint32_t i; } floatUnion;
            floatUnion.f = floatVal;
            uint32_t bits = floatUnion.i;
            bytes.push_back(0x1D); // Float token marker
            bytes.push_back(bits & 0xFF);
            bytes.push_back((bits >> 8) & 0xFF);
            bytes.push_back((bits >> 16) & 0xFF);
            bytes.push_back((bits >> 24) & 0xFF);
            break;
        }
        case TOKEN_NUMBER_DOUBLE: {
            // Double precision floating point (64-bit)
            union { double d; uint64_t i; } doubleUnion;
            doubleUnion.d = value;
            uint64_t bits = doubleUnion.i;
            bytes.push_back(0x1F); // Double token marker
            for (int i = 0; i < 8; i++) {
                bytes.push_back((bits >> (i * 8)) & 0xFF);
            }
            break;
        }
        default:
            break;
    }
    
    return bytes;
}

std::vector<uint8_t> Tokenizer::encodeLineNumber(int lineNum) {
    std::vector<uint8_t> bytes;
    bytes.push_back(0x0D); // Line number token marker (from GW-BASIC)
    bytes.push_back(lineNum & 0xFF);
    bytes.push_back((lineNum >> 8) & 0xFF);
    return bytes;
}

void Tokenizer::advance() {
    if (currentPosition < currentSource.length()) {
        currentPosition++;
        currentChar = (currentPosition < currentSource.length()) ? 
                     currentSource[currentPosition] : '\0';
    }
}

void Tokenizer::skipWhitespace() {
    while (!isAtEnd() && (currentChar == ' ' || currentChar == '\t' || 
                          currentChar == '\r' || currentChar == '\n')) {
        advance();
    }
}

char Tokenizer::peekChar(size_t offset) const {
    size_t pos = currentPosition + offset;
    return (pos < currentSource.length()) ? currentSource[pos] : '\0';
}

bool Tokenizer::isAtEnd() const {
    return currentPosition >= currentSource.length();
}

bool Tokenizer::isAlpha(char c) const {
    return std::isalpha(static_cast<unsigned char>(c));
}

bool Tokenizer::isDigit(char c) const {
    return std::isdigit(static_cast<unsigned char>(c));
}

bool Tokenizer::isAlphaNumeric(char c) const {
    return isAlpha(c) || isDigit(c);
}

std::string Tokenizer::toUpperCase(const std::string& str) const {
    std::string result = str;
    std::transform(result.begin(), result.end(), result.begin(), 
                   [](char c) { return std::toupper(c); });
    return result;
}

bool Tokenizer::startsWith(const std::string& str, const std::string& prefix) const {
    return str.length() >= prefix.length() &&
           str.substr(0, prefix.length()) == prefix;
}

bool Tokenizer::isReservedWord(const std::string& word) const {
    return reservedWords.find(toUpperCase(word)) != reservedWords.end();
}

uint8_t Tokenizer::getTokenValue(const std::string& word) const {
    auto it = reservedWords.find(toUpperCase(word));
    return (it != reservedWords.end()) ? it->second.token : 0;
}

std::string Tokenizer::getTokenName(uint8_t token) const {
    auto it = tokenNames.find(token);
    return (it != tokenNames.end()) ? it->second : "";
}

void Tokenizer::error(const std::string& message) {
    throw std::runtime_error("Tokenizer error at position " + 
                            std::to_string(currentPosition) + ": " + message);
}
