#include <SDL3/SDL.h>
#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <memory>
#include <sstream>
#include <iomanip>
#include <cctype>
#include <algorithm>
#include <unistd.h> // for isatty

// GW-BASIC components
#include "Tokenizer/Tokenizer.hpp"
#include "ProgramStore/ProgramStore.hpp"
#include "InterpreterLoop/InterpreterLoop.hpp"
#include "InterpreterLoop/BasicDispatcher.hpp"
#include "BitmapFont.hpp"

// SDL3-based GW-BASIC Shell
class GWBasicShell {
private:
    // SDL components
    SDL_Window* window;
    SDL_Renderer* renderer;
    SDL_Texture* fontTexture;
    
    // Screen dimensions (emulating 80x25 text mode)
    static constexpr int SCREEN_WIDTH = 720;
    static constexpr int SCREEN_HEIGHT = 400;
    static constexpr int CHAR_W = BitmapFont::FONT_WIDTH;
    static constexpr int CHAR_H = BitmapFont::FONT_HEIGHT;
    static constexpr int COLS = 80;
    static constexpr int ROWS = 25;
    
    // Colors (CGA-style)
    struct Color {
        uint8_t r, g, b, a;
    };
    static constexpr Color BLACK = {0, 0, 0, 255};
    static constexpr Color WHITE = {255, 255, 255, 255};
    static constexpr Color GREEN = {0, 255, 0, 255};
    static constexpr Color CYAN = {0, 255, 255, 255};
    static constexpr Color BLUE = {0, 0, 255, 255};
    
    // Text buffer (screen memory)
    struct ScreenChar {
        char ch;
        Color fg;
        Color bg;
    };
    ScreenChar screen[ROWS][COLS];
    
    // Cursor position
    int cursorX, cursorY;
    bool cursorVisible;
    uint32_t lastCursorBlink;
    
    // Input handling
    std::string inputLine;
    std::vector<std::string> history;
    size_t historyIndex;
    bool insertMode;
    
    // GW-BASIC components
    std::shared_ptr<Tokenizer> tokenizer;
    std::shared_ptr<ProgramStore> programStore;
    std::shared_ptr<InterpreterLoop> interpreter;
    std::unique_ptr<BasicDispatcher> dispatcher;
    
    // State
    bool running;
    bool programMode;  // true = accepting program lines, false = immediate mode
    bool waitingForInput;  // true when INPUT statement is waiting for user input
    std::string inputPrompt;  // prompt to show when waiting for input
    std::string pendingInput; // input being typed by user during INPUT
    
public:
    GWBasicShell() : window(nullptr), renderer(nullptr), fontTexture(nullptr),
                     cursorX(0), cursorY(0), cursorVisible(true), lastCursorBlink(0),
                     historyIndex(0), insertMode(true), running(true), programMode(false),
                     waitingForInput(false) {
        
        // Initialize screen buffer
        clearScreen();
        
        // Initialize GW-BASIC components
        tokenizer = std::make_shared<Tokenizer>();
        programStore = std::make_shared<ProgramStore>();
        interpreter = std::make_shared<InterpreterLoop>(programStore, tokenizer);
        interpreter->setTrace(true);
        interpreter->setTraceCallback([](uint16_t ln, const std::vector<uint8_t>& b){
            std::cerr << "TRACE " << ln << ": ";
            for (auto x : b) fprintf(stderr, "%02X ", x);
            std::cerr << "\n";
        });        // Create dispatcher with print and input callbacks
        dispatcher = std::make_unique<BasicDispatcher>(tokenizer, programStore,
            [this](const std::string& text) { print(text); },
            [this](const std::string& prompt) -> std::string { return getInput(prompt); });
        
        // Connect event trap system between interpreter and dispatcher
        interpreter->setEventTrapSystem(dispatcher->getEventTrapSystem());
        
        // Set up interpreter with our dispatcher
        interpreter->setStatementHandler([this](const std::vector<uint8_t>& tokens, uint16_t currentLine) -> uint16_t {
            try {
                auto result = (*dispatcher)(tokens, currentLine);
                if (result == 0xFFFF) {
                    // END/STOP encountered
                    print("Break\n");
                    return 0xFFFF; // propagate halt sentinel
                }
                return result;
            } catch (const std::exception& e) {
                errorPrint(e.what());
                return 0;
            }
        });
    }
    
    ~GWBasicShell() {
        cleanup();
    }
    
    // Load a BASIC program from file
    bool loadFile(const std::string& filename) {
        std::ifstream file(filename);
        if (!file.is_open()) {
            print("?File not found: ");
            print(filename);
            print("\n");
            return false;
        }

        // Clear current program
        programStore->clear();

        // Read file line by line
        std::string line;
        int linesLoaded = 0;
        while (std::getline(file, line)) {
            if (line.empty()) continue;
            
            // Parse line number if present
            std::istringstream iss(line);
            std::string token;
            iss >> token;
            
            uint16_t lineNum = 0;
            if (std::isdigit(token[0])) {
                try {
                    lineNum = static_cast<uint16_t>(std::stoul(token));
                    // Get rest of line
                    std::string restOfLine;
                    std::getline(iss, restOfLine);
                    if (!restOfLine.empty() && restOfLine[0] == ' ') {
                        restOfLine = restOfLine.substr(1); // remove leading space
                    }
                    
                    // Tokenize and store the line
                    if (!restOfLine.empty()) {
                        auto tokens = tokenizer->crunch(restOfLine);
                        programStore->insertLine(lineNum, tokens);
                        linesLoaded++;
                    }
                } catch (const std::exception& e) {
                    errorPrint("Syntax error in line " + std::to_string(lineNum) + ": " + e.what());
                }
            } else {
                // Line without line number - treat as immediate command
                print("?Line number required: ");
                print(line);
                print("\n");
            }
        }
        
        file.close();
        
        if (linesLoaded > 0) {
            programMode = true;
            print("Loaded ");
            print(std::to_string(linesLoaded));
            print(" lines from ");
            print(filename);
            print("\n");
            return true;
        } else {
            print("?No valid program lines found in ");
            print(filename);
            print("\n");
            return false;
        }
    }
    
    bool initialize() {
        if (!SDL_Init(SDL_INIT_VIDEO)) {
            std::cerr << "SDL initialization failed: " << SDL_GetError() << std::endl;
            return false;
        }
        
        window = SDL_CreateWindow("GW-BASIC",
                                  SCREEN_WIDTH, SCREEN_HEIGHT,
                                  SDL_WINDOW_RESIZABLE);
        if (!window) {
            std::cerr << "Window creation failed: " << SDL_GetError() << std::endl;
            return false;
        }
        
        renderer = SDL_CreateRenderer(window, nullptr);
        if (!renderer) {
            std::cerr << "Renderer creation failed: " << SDL_GetError() << std::endl;
            return false;
        }
        
        // Create a simple bitmap font texture (8x16 monospace)
        if (!createFontTexture()) {
            return false;
        }
        
        // Display startup message
        printStartupMessage();
        showPrompt();
        
        return true;
    }
    
    void run() {
        SDL_Event event;
        
        while (running) {
            // Handle events
            while (SDL_PollEvent(&event)) {
                handleEvent(event);
            }
            
            // Update cursor blink
            uint32_t now = SDL_GetTicks();
            if (now - lastCursorBlink > 500) {
                cursorVisible = !cursorVisible;
                lastCursorBlink = now;
            }
            
            // Render
            render();
            
            // Cap framerate
            SDL_Delay(16); // ~60 FPS
        }
    }
    
    // Get input from the GUI (for INPUT statements)
    std::string getInput(const std::string& prompt) {
        // Show the prompt
        print(prompt);
        
        // Set up for input mode
        waitingForInput = true;
        inputPrompt = prompt;
        pendingInput.clear();
        
        // Run the event loop until we get input
        SDL_Event event;
        while (waitingForInput && running) {
            while (SDL_PollEvent(&event)) {
                handleEvent(event);
            }
            
            // Update cursor blink
            uint32_t now = SDL_GetTicks();
            if (now - lastCursorBlink > 500) {
                cursorVisible = !cursorVisible;
                lastCursorBlink = now;
            }
            
            // Render
            render();
            
            // Cap framerate
            SDL_Delay(16); // ~60 FPS
        }
        
        return pendingInput;
    }
    
private:
    void cleanup() {
        if (fontTexture) {
            SDL_DestroyTexture(fontTexture);
            fontTexture = nullptr;
        }
        if (renderer) {
            SDL_DestroyRenderer(renderer);
            renderer = nullptr;
        }
        if (window) {
            SDL_DestroyWindow(window);
            window = nullptr;
        }
        SDL_Quit();
    }
    
    bool createFontTexture() {
        // Create a simple 8x16 bitmap font
        // For now, we'll use SDL's built-in font rendering capability
        // In a full implementation, you'd load a proper bitmap font
        
        // Create a texture for character rendering
        fontTexture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA8888,
                                        SDL_TEXTUREACCESS_TARGET,
                                        256 * CHAR_W, CHAR_H);
        if (!fontTexture) {
            std::cerr << "Font texture creation failed: " << SDL_GetError() << std::endl;
            return false;
        }
        
        SDL_SetTextureBlendMode(fontTexture, SDL_BLENDMODE_BLEND);
        return true;
    }
    
    void clearScreen() {
        for (int y = 0; y < ROWS; y++) {
            for (int x = 0; x < COLS; x++) {
                screen[y][x] = {' ', WHITE, BLACK};
            }
        }
        cursorX = 0;
        cursorY = 0;
    }
    
    void printStartupMessage() {
        print("GW-BASIC Interpreter v0.1\n");
        print("Copyright (C) 2025\n");
        print("32768 Bytes free\n");
        print("\n");
    }
    
    void print(const std::string& text) {
        for (char ch : text) {
            printChar(ch);
        }
    }
    
    void errorPrint(const std::string& message) {
        std::string errorMsg = "Error: " + message;
        
        // Output to console stderr
        std::cerr << errorMsg << std::endl;
        
        // Output to SDL
        print(errorMsg + "\n");
    }
    
    void printChar(char ch) {
        if (ch == '\n') {
            cursorX = 0;
            cursorY++;
            if (cursorY >= ROWS) {
                scrollUp();
                cursorY = ROWS - 1;
            }
        } else if (ch == '\r') {
            cursorX = 0;
        } else if (ch == '\b') {
            if (cursorX > 0) {
                cursorX--;
                screen[cursorY][cursorX] = {' ', WHITE, BLACK};
            }
        } else if (ch >= 32 && ch <= 126) {
            screen[cursorY][cursorX] = {ch, WHITE, BLACK};
            cursorX++;
            if (cursorX >= COLS) {
                cursorX = 0;
                cursorY++;
                if (cursorY >= ROWS) {
                    scrollUp();
                    cursorY = ROWS - 1;
                }
            }
        }
    }
    
    void scrollUp() {
        for (int y = 0; y < ROWS - 1; y++) {
            for (int x = 0; x < COLS; x++) {
                screen[y][x] = screen[y + 1][x];
            }
        }
        // Clear bottom line
        for (int x = 0; x < COLS; x++) {
            screen[ROWS - 1][x] = {' ', WHITE, BLACK};
        }
    }
    
    void showPrompt() {
        if (programMode) {
            // In program mode, no prompt
        } else {
            print("Ok\n");
        }
    }
    
    void handleEvent(const SDL_Event& event) {
        switch (event.type) {
        case SDL_EVENT_QUIT:
            running = false;
            break;
            
        case SDL_EVENT_KEY_DOWN:
            handleKeyDown(event.key);
            break;
            
        case SDL_EVENT_WINDOW_RESIZED:
            // Handle window resize if needed
            break;
        }
    }
    
    void handleKeyDown(const SDL_KeyboardEvent& key) {
        SDL_Keycode keycode = key.key;
        bool shift = (key.mod & SDL_KMOD_SHIFT) != 0;
        bool ctrl = (key.mod & SDL_KMOD_CTRL) != 0;
        
        // Inject key event for trap handling
        if (interpreter) {
            interpreter->injectKeyEvent(static_cast<uint8_t>(key.scancode), true);
        }
        
        if (ctrl) {
            handleControlKey(keycode);
            return;
        }
        
        switch (keycode) {
        case SDLK_RETURN:
            handleEnter();
            break;
            
        case SDLK_BACKSPACE:
            handleBackspace();
            break;
            
        case SDLK_DELETE:
            handleDelete();
            break;
            
        case SDLK_LEFT:
            // Handle cursor movement if needed
            break;
            
        case SDLK_RIGHT:
            // Handle cursor movement if needed
            break;
            
        case SDLK_UP:
            handleHistoryUp();
            break;
            
        case SDLK_DOWN:
            handleHistoryDown();
            break;
            
        case SDLK_INSERT:
            insertMode = !insertMode;
            break;
            
        default:
            // Handle character input
            if (keycode >= 32 && keycode <= 126) {
                char ch = static_cast<char>(keycode);
                if (shift) {
                    ch = getShiftedChar(ch);
                }
                handleCharInput(ch);
            }
            break;
        }
    }
    
    char getShiftedChar(char ch) {
        // Handle shifted characters
        if (ch >= 'a' && ch <= 'z') {
            return ch - 'a' + 'A';
        }
        
        switch (ch) {
        case '1': return '!';
        case '2': return '@';
        case '3': return '#';
        case '4': return '$';
        case '5': return '%';
        case '6': return '^';
        case '7': return '&';
        case '8': return '*';
        case '9': return '(';
        case '0': return ')';
        case '-': return '_';
        case '=': return '+';
        case '[': return '{';
        case ']': return '}';
        case '\\': return '|';
        case ';': return ':';
        case '\'': return '"';
        case ',': return '<';
        case '.': return '>';
        case '/': return '?';
        case '`': return '~';
        default: return ch;
        }
    }
    
    void handleControlKey(SDL_Keycode keycode) {
        switch (keycode) {
        case SDLK_C:
            // Ctrl+C - Break
            print("^C\n");
            interpreter->stop();
            inputLine.clear();
            showPrompt();
            break;
            
        case SDLK_L:
            // Ctrl+L - Clear screen
            clearScreen();
            showPrompt();
            break;
            
        case SDLK_D:
            // Ctrl+D - Exit
            running = false;
            break;
        }
    }
    
    void handleCharInput(char ch) {
        if (waitingForInput) {
            // During INPUT statement
            pendingInput += ch;
            printChar(ch);
        } else {
            // Regular command input
            inputLine += ch;
            printChar(ch);
        }
    }
    
    void handleBackspace() {
        if (waitingForInput) {
            // During INPUT statement
            if (!pendingInput.empty()) {
                pendingInput.pop_back();
                printChar('\b');
            }
        } else {
            // Regular command input
            if (!inputLine.empty()) {
                inputLine.pop_back();
                printChar('\b');
            }
        }
    }
    
    void handleDelete() {
        // For now, same as backspace
        handleBackspace();
    }
    
    void handleEnter() {
        print("\n");
        
        if (waitingForInput) {
            // During INPUT statement - finish input
            waitingForInput = false;
            return;
        }
        
        // Regular command input
        if (inputLine.empty()) {
            showPrompt();
            return;
        }
        
        // Add to history
        history.push_back(inputLine);
        historyIndex = history.size();
        
        // Process the input line
        processInput(inputLine);
        
        inputLine.clear();
    }
    
    void handleHistoryUp() {
        if (historyIndex > 0) {
            historyIndex--;
            // Clear current input
            for (size_t i = 0; i < inputLine.length(); i++) {
                printChar('\b');
            }
            inputLine = history[historyIndex];
            print(inputLine);
        }
    }
    
    void handleHistoryDown() {
        if (historyIndex < history.size() - 1) {
            historyIndex++;
            // Clear current input
            for (size_t i = 0; i < inputLine.length(); i++) {
                printChar('\b');
            }
            inputLine = history[historyIndex];
            print(inputLine);
        } else if (historyIndex == history.size() - 1) {
            historyIndex++;
            // Clear current input
            for (size_t i = 0; i < inputLine.length(); i++) {
                printChar('\b');
            }
            inputLine.clear();
        }
    }
    
    void processInput(const std::string& input) {
        std::string trimmed = trim(input);
        
        if (trimmed.empty()) {
            showPrompt();
            return;
        }
        
        // Check if this is a line number (program line)
        if (std::isdigit(trimmed[0])) {
            handleProgramLine(trimmed);
        } else {
            // Convert to uppercase for command processing
            std::string upperInput = toUpper(trimmed);
            
            // Handle immediate commands
            if (handleImmediateCommand(upperInput)) {
                return;
            }
            
            // Execute as immediate BASIC statement
            try {
                auto tokens = tokenizer->crunch(trimmed);
                auto result = (*dispatcher)(tokens);
                if (result == 0xFFFF) {
                    // END/STOP in immediate mode
                }
            } catch (const std::exception& e) {
                errorPrint(e.what());
            }
        }
        
        showPrompt();
    }
    
    void handleProgramLine(const std::string& input) {
        // Parse line number
        size_t pos = 0;
        int lineNumber = 0;
        
        while (pos < input.length() && std::isdigit(input[pos])) {
            lineNumber = lineNumber * 10 + (input[pos] - '0');
            pos++;
        }
        
        if (lineNumber < 1 || lineNumber > 65529) {
            print("?Illegal function call\n");
            return;
        }
        
        // Skip whitespace after line number
        while (pos < input.length() && std::isspace(input[pos])) {
            pos++;
        }
        
        std::string statement = input.substr(pos);
        
        if (statement.empty()) {
            // Delete line
            programStore->deleteLine(static_cast<uint16_t>(lineNumber));
        } else {
            // Add/replace line
            try {
                auto tokens = tokenizer->crunch(statement);
                programStore->insertLine(static_cast<uint16_t>(lineNumber), tokens);
            } catch (const std::exception& e) {
                errorPrint("Syntax error");
            }
        }
        
        programMode = !programStore->isEmpty();
    }
    
    bool handleImmediateCommand(const std::string& cmd) {
        if (cmd == "LIST") {
            listProgram();
            return true;
        } else if (cmd == "RUN") {
            runProgram();
            return true;
        } else if (cmd == "NEW") {
            newProgram();
            return true;
        } else if (cmd == "CLEAR") {
            clearProgram();
            return true;
        } else if (cmd.substr(0, 4) == "LIST") {
            handleListCommand(cmd);
            return true;
        } else if (cmd == "SYSTEM" || cmd == "QUIT" || cmd == "EXIT") {
            running = false;
            return true;
        }
        
        return false;
    }
    
    void listProgram() {
        if (programStore->isEmpty()) {
            print("Ok\n");
            return;
        }
        
        for (auto it = programStore->begin(); it != programStore->end(); ++it) {
            print(std::to_string(it->lineNumber));
            print(" ");
            try {
                std::string detokenized = tokenizer->detokenize(it->tokens);
                print(detokenized);
            } catch (const std::exception&) {
                print("?Syntax error");
            }
            print("\n");
        }
    }
    
    void handleListCommand(const std::string& cmd) {
        // For now, just list everything
        // TODO: Parse line ranges
        (void)cmd; // Suppress unused parameter warning
        listProgram();
    }
    
    void runProgram() {
        if (programStore->isEmpty()) {
            print("?Illegal function call\n");
            return;
        }
        
        try {
            interpreter->run();
        } catch (const std::exception& e) {
            errorPrint("Runtime error: " + std::string(e.what()));
        }
    }
    
    void newProgram() {
        programStore->clear();
        programMode = false;
        print("Ok\n");
    }
    
    void clearProgram() {
        programStore->clear();
        dispatcher->environment().vars.clear();
        programMode = false;
        print("Ok\n");
    }
    
    std::string trim(const std::string& str) {
        size_t start = str.find_first_not_of(" \t\r\n");
        if (start == std::string::npos) return "";
        size_t end = str.find_last_not_of(" \t\r\n");
        return str.substr(start, end - start + 1);
    }
    
    std::string toUpper(const std::string& str) {
        std::string result = str;
        for (char& ch : result) {
            ch = static_cast<char>(std::toupper(ch));
        }
        return result;
    }
    
    void render() {
        // Clear screen
        SDL_SetRenderDrawColor(renderer, BLACK.r, BLACK.g, BLACK.b, BLACK.a);
        SDL_RenderClear(renderer);
        
        // Render text
        for (int y = 0; y < ROWS; y++) {
            for (int x = 0; x < COLS; x++) {
                renderChar(x, y, screen[y][x]);
            }
        }
        
        // Render cursor
        if (cursorVisible) {
            SDL_SetRenderDrawColor(renderer, WHITE.r, WHITE.g, WHITE.b, WHITE.a);
            SDL_FRect cursorRect = {
                static_cast<float>(cursorX * CHAR_W),
                static_cast<float>(cursorY * CHAR_H + CHAR_H - 2),
                static_cast<float>(CHAR_W),
                2.0f
            };
            SDL_RenderFillRect(renderer, &cursorRect);
        }
        
        SDL_RenderPresent(renderer);
    }
    
    void renderChar(int x, int y, const ScreenChar& screenChar) {
        if (screenChar.ch == ' ') {
            // Still render background for spaces
            if (screenChar.bg.r != 0 || screenChar.bg.g != 0 || screenChar.bg.b != 0) {
                SDL_SetRenderDrawColor(renderer, screenChar.bg.r, screenChar.bg.g, screenChar.bg.b, screenChar.bg.a);
                SDL_FRect bgRect = {
                    static_cast<float>(x * CHAR_W),
                    static_cast<float>(y * CHAR_H),
                    static_cast<float>(CHAR_W),
                    static_cast<float>(CHAR_H)
                };
                SDL_RenderFillRect(renderer, &bgRect);
            }
            return;
        }
        
        // Background
        if (screenChar.bg.r != 0 || screenChar.bg.g != 0 || screenChar.bg.b != 0) {
            SDL_SetRenderDrawColor(renderer, screenChar.bg.r, screenChar.bg.g, screenChar.bg.b, screenChar.bg.a);
            SDL_FRect bgRect = {
                static_cast<float>(x * CHAR_W),
                static_cast<float>(y * CHAR_H),
                static_cast<float>(CHAR_W),
                static_cast<float>(CHAR_H)
            };
            SDL_RenderFillRect(renderer, &bgRect);
        }
        
        // Render character using bitmap font
        const uint8_t* charData = BitmapFont::getCharData(screenChar.ch);
        SDL_SetRenderDrawColor(renderer, screenChar.fg.r, screenChar.fg.g, screenChar.fg.b, screenChar.fg.a);
        
        int baseX = x * CHAR_W;
        int baseY = y * CHAR_H;
        
        for (int row = 0; row < CHAR_H; row++) {
            uint8_t rowData = charData[row];
            for (int col = 0; col < CHAR_W; col++) {
                if (rowData & (0x80 >> col)) {
                    // Draw pixel
                    SDL_FRect pixelRect = {
                        static_cast<float>(baseX + col),
                        static_cast<float>(baseY + row),
                        1.0f, 1.0f
                    };
                    SDL_RenderFillRect(renderer, &pixelRect);
                }
            }
        }
    }
};

// Helper function to output errors to both console stderr and SDL (if available)
void outputError(const std::string& message, std::function<void(const std::string&)> printCallback = nullptr) {
    std::string errorMsg = "Error: " + message;
    
    // Always output to console stderr
    std::cerr << errorMsg << std::endl;
    
    // Also output to SDL if callback is available
    if (printCallback) {
        printCallback(errorMsg);
    }
}

// Console-only mode for piped input
int runConsoleMode(int argc, char* argv[]) {
    // Initialize GW-BASIC components
    auto tokenizer = std::make_shared<Tokenizer>();
    auto programStore = std::make_shared<ProgramStore>();
    auto dispatcher = std::make_unique<BasicDispatcher>(tokenizer, programStore, 
        [](const std::string& text) { std::cout << text; },  // print callback
        [](const std::string& prompt) -> std::string {       // input callback
            std::cout << prompt;
            std::string input;
            std::getline(std::cin, input);
            return input;
        });
    
    // Create InterpreterLoop for proper program execution
    auto interpreter = std::make_unique<InterpreterLoop>(programStore, tokenizer);
    
    // Connect event trap system between interpreter and dispatcher
    interpreter->setEventTrapSystem(dispatcher->getEventTrapSystem());
    
    interpreter->setStatementHandler([&dispatcher](const std::vector<uint8_t>& tokens, uint16_t currentLine) -> uint16_t {
        try {
            auto result = (*dispatcher)(tokens, currentLine);
            if (result == 0xFFFF) {
                // END/STOP encountered
                return 0xFFFF; // propagate halt sentinel
            }
            return result;
        } catch (const std::exception& e) {
            std::cerr << "Error: " << e.what() << std::endl;
            return 0;
        }
    });
    
    std::cout << "GW-BASIC Interpreter v0.1\n";
    std::cout << "Copyright (C) 2025\n";
    std::cout << "32768 Bytes free\n";
    std::cout << "\n";
    
    // If filename argument provided, load it first
    if (argc > 1) {
        std::string filename = argv[1];
        std::ifstream file(filename);
        if (file.is_open()) {
            std::string line;
            int linesLoaded = 0;
            while (std::getline(file, line)) {
                if (line.empty()) continue;
                
                // Parse line number if present
                std::istringstream iss(line);
                std::string token;
                iss >> token;
                
                if (std::isdigit(token[0])) {
                    try {
                        uint16_t lineNum = static_cast<uint16_t>(std::stoul(token));
                        std::string restOfLine;
                        std::getline(iss, restOfLine);
                        if (!restOfLine.empty() && restOfLine[0] == ' ') {
                            restOfLine = restOfLine.substr(1);
                        }
                        
                        if (!restOfLine.empty()) {
                            auto tokens = tokenizer->crunch(restOfLine);
                            programStore->insertLine(lineNum, tokens);
                            linesLoaded++;
                        }
                    } catch (const std::exception& e) {
                        std::cout << "?Syntax error in line: " << e.what() << "\n";
                    }
                }
            }
            file.close();
            std::cout << "Loaded " << linesLoaded << " lines from " << filename << "\n";
        }
    }
    
    // Process stdin line by line
    std::string inputLine;
    while (std::getline(std::cin, inputLine)) {
        if (inputLine.empty()) continue;
        
        // Trim whitespace
        size_t start = inputLine.find_first_not_of(" \t\r\n");
        if (start == std::string::npos) continue;
        size_t end = inputLine.find_last_not_of(" \t\r\n");
        inputLine = inputLine.substr(start, end - start + 1);
        
        // Check if this is a program line (starts with number)
        if (std::isdigit(inputLine[0])) {
            // Parse line number
            size_t pos = 0;
            int lineNumber = 0;
            while (pos < inputLine.length() && std::isdigit(inputLine[pos])) {
                lineNumber = lineNumber * 10 + (inputLine[pos] - '0');
                pos++;
            }
            
            // Skip whitespace
            while (pos < inputLine.length() && std::isspace(inputLine[pos])) {
                pos++;
            }
            
            std::string statement = inputLine.substr(pos);
            if (statement.empty()) {
                programStore->deleteLine(static_cast<uint16_t>(lineNumber));
            } else {
                try {
                    auto tokens = tokenizer->crunch(statement);
                    programStore->insertLine(static_cast<uint16_t>(lineNumber), tokens);
                } catch (const std::exception& e) {
                    outputError("Syntax error");
                }
            }
        } else {
            // Immediate command
            std::string upperInput = inputLine;
            std::transform(upperInput.begin(), upperInput.end(), upperInput.begin(), ::toupper);
            
            // Handle SYSTEM (exit) before tokenization to avoid spurious syntax errors
            if (upperInput == "SYSTEM") {
                // Emulate GW-BASIC SYSTEM: exit interpreter
                break; // leave input loop, program will return
            }

            if (upperInput == "LIST") {
                if (programStore->isEmpty()) {
                    std::cout << "Ok\n";
                } else {
                    for (auto it = programStore->begin(); it != programStore->end(); ++it) {
                        std::cout << it->lineNumber << " ";
                        try {
                            std::string detokenized = tokenizer->detokenize(it->tokens);
                            std::cout << detokenized << "\n";
                        } catch (const std::exception&) {
                            std::cout << "?Syntax error\n";
                        }
                    }
                }
            } else if (upperInput == "RUN") {
                if (programStore->isEmpty()) {
                    std::cout << "?Illegal function call\n";
                } else {
                    // Use InterpreterLoop for proper execution
                    try {
                        interpreter->run();
                    } catch (const std::exception& e) {
                        outputError("Runtime error: " + std::string(e.what()));
                    }
                }
            } else if (upperInput == "NEW" || upperInput == "CLEAR") {
                programStore->clear();
                std::cout << "Ok\n";
            } else {
                // Execute as immediate statement
                try {
                    auto tokens = tokenizer->crunch(inputLine);
                    (*dispatcher)(tokens);
                } catch (const std::exception& e) {
                    outputError(e.what());
                }
            }
        }
    }
    
    return 0;
}

int main(int argc, char* argv[]) {
    // Show usage if help is requested
    if (argc > 1 && (std::string(argv[1]) == "--help" || std::string(argv[1]) == "-h")) {
        std::cout << "GW-BASIC Interpreter v0.1\n";
        std::cout << "Usage: " << argv[0] << " [filename.bas]\n";
        std::cout << "\n";
        std::cout << "If a filename is provided, it will be loaded automatically at startup.\n";
        std::cout << "The file should contain BASIC program lines with line numbers.\n";
        std::cout << "\n";
        std::cout << "When input is piped, the interpreter runs in console mode.\n";
        std::cout << "Otherwise, it runs in GUI mode with SDL3.\n";
        std::cout << "\n";
        std::cout << "Examples:\n";
        std::cout << "  " << argv[0] << " program.bas          # GUI mode\n";
        std::cout << "  echo 'PRINT \"Hi\"' | " << argv[0] << "    # Console mode\n";
        std::cout << "\n";
        std::cout << "Interactive commands:\n";
        std::cout << "  LIST      - List the current program\n";
        std::cout << "  RUN       - Run the current program\n";
        std::cout << "  NEW       - Clear the current program\n";
        std::cout << "  LOAD \"filename\" - Load a program from file\n";
        std::cout << "  SAVE \"filename\" - Save the current program to file\n";
        std::cout << "  SYSTEM    - Exit the interpreter\n";
        return 0;
    }
    
    // Check if stdin is a terminal (interactive) or piped
    bool isInputPiped = !isatty(STDIN_FILENO);
    
    if (isInputPiped) {
        // Run in console mode for piped input
        return runConsoleMode(argc, argv);
    }
    
    // Run in GUI mode for interactive use
    GWBasicShell shell;
    
    if (!shell.initialize()) {
        std::cerr << "Failed to initialize GW-BASIC shell" << std::endl;
        return 1;
    }
    
    // Check for command line filename argument
    if (argc > 1) {
        std::string filename = argv[1];
        
        // Check if file exists and has a reasonable extension
        std::ifstream testFile(filename);
        if (testFile.good()) {
            testFile.close();
            shell.loadFile(filename);
        } else {
            std::cerr << "Error: Cannot open file '" << filename << "'" << std::endl;
            // Continue anyway - user might want to create the file
            shell.loadFile(filename); // This will show the error message in the UI
        }
    }
    
    shell.run();
    
    return 0;
}
