#include <SDL3/SDL.h>
#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <memory>
#include <array>
#include <queue>
#include <sstream>
#include <iomanip>
#include <cctype>
#include <algorithm>
#include <cmath>
#include <unistd.h> // for isatty
#include <optional>

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
    
    // Screen dimensions (emulating 80x25 text mode by default)
    int screenWidth = 720;
    int screenHeight = 400;
    static constexpr int CHAR_W = BitmapFont::FONT_WIDTH;
    static constexpr int CHAR_H = BitmapFont::FONT_HEIGHT;
    int cols = 80;
    int rows = 25;
    
    // Window size tracking for resizing
    int windowWidth = 720;
    int windowHeight = 400;
    
    // Current screen mode
    int currentScreenMode = 0;
    bool graphicsMode = false;
    
    // Colors (CGA-style)
    struct Color {
        uint8_t r, g, b, a;
    };
    static constexpr Color BLACK = {0, 0, 0, 255};
    static constexpr Color WHITE = {255, 255, 255, 255};
    static constexpr Color GREEN = {0, 255, 0, 255};
    static constexpr Color CYAN = {0, 255, 255, 255};
    static constexpr Color BLUE = {0, 0, 255, 255};
    
    // Standard 16-color CGA/EGA/VGA palette
    static constexpr Color PALETTE[16] = {
        {0,   0,   0,   255}, // 0  Black
        {0,   0,   170, 255}, // 1  Blue
        {0,   170, 0,   255}, // 2  Green
        {0,   170, 170, 255}, // 3  Cyan
        {170, 0,   0,   255}, // 4  Red
        {170, 0,   170, 255}, // 5  Magenta
        {170, 85,  0,   255}, // 6  Brown
        {170, 170, 170, 255}, // 7  Light Gray
        {85,  85,  85,  255}, // 8  Dark Gray
        {85,  85,  255, 255}, // 9  Light Blue
        {85,  255, 85,  255}, // 10 Light Green
        {85,  255, 255, 255}, // 11 Light Cyan
        {255, 85,  85,  255}, // 12 Light Red
        {255, 85,  255, 255}, // 13 Light Magenta
        {255, 255, 85,  255}, // 14 Yellow
        {255, 255, 255, 255}  // 15 White
    };
    
    // Current text colors
    int currentForeground = 7;  // Default light gray
    int currentBackground = 0;  // Default black
    
    // Text buffer (screen memory)
    struct ScreenChar {
        char ch;
        Color fg;
        Color bg;
    };
    static constexpr int MAX_ROWS = 50;
    static constexpr int MAX_COLS = 132;
    ScreenChar screen[MAX_ROWS][MAX_COLS];
    
    // Graphics pixel buffer for graphics modes
    struct PixelData {
        uint8_t r, g, b, a;
    };
    std::vector<PixelData> pixelBuffer;
    
    // Simple graphics buffer for drawing operations (8-bit color indices)
    std::vector<uint8_t> graphicsBuffer;
    
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
    
    // Function key soft key storage (like STRTAB in original GW-BASIC)
    static constexpr int NUM_FUNCTION_KEYS = 10;  // F1-F10
    static constexpr int SOFT_KEY_LENGTH = 15;    // Max chars per soft key
    std::array<std::string, NUM_FUNCTION_KEYS> softKeys;
    bool functionKeysEnabled = true;  // Whether to display function keys
    
    // INKEY$ keyboard buffer
    std::queue<char> keyBuffer;
    
    // Audio system for PLAY command
    SDL_AudioDeviceID audioDevice;
    int audioSampleRate;
    
    // Helper function to get effective text rows (excluding function key line)
    int getTextRows() const {
        return functionKeysEnabled ? rows - 1 : rows;
    }
    
    // Aspect ratio helper functions
    double getTargetAspectRatio() const {
        return static_cast<double>(screenWidth) / static_cast<double>(screenHeight);
    }
    
    // Calculate new window dimensions that maintain aspect ratio
    void calculateAspectRatioConstrainedSize(int requestedWidth, int requestedHeight, 
                                           int& newWidth, int& newHeight) const {
        double targetRatio = getTargetAspectRatio();
        double requestedRatio = static_cast<double>(requestedWidth) / static_cast<double>(requestedHeight);
        
        if (requestedRatio > targetRatio) {
            // Requested window is too wide, constrain by height
            newHeight = requestedHeight;
            newWidth = static_cast<int>(requestedHeight * targetRatio);
        } else {
            // Requested window is too tall, constrain by width
            newWidth = requestedWidth;
            newHeight = static_cast<int>(requestedWidth / targetRatio);
        }
    }
    
    // Get rendering scale factors based on current window vs screen dimensions
    void getScaleFactors(float& scaleX, float& scaleY) const {
        scaleX = static_cast<float>(windowWidth) / static_cast<float>(screenWidth);
        scaleY = static_cast<float>(windowHeight) / static_cast<float>(screenHeight);
    }
    
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
                     waitingForInput(false), audioDevice(0), audioSampleRate(44100) {
        
        // Initialize screen buffer
        clearScreen();
        
        // Initialize default function key soft keys (based on original GW-BASIC)
        initializeSoftKeys();
        
        // Initialize GW-BASIC components
        tokenizer = std::make_shared<Tokenizer>();
        programStore = std::make_shared<ProgramStore>();
        interpreter = std::make_shared<InterpreterLoop>(programStore, tokenizer);
        interpreter->setTrace(false);
        interpreter->setTraceCallback([](uint16_t ln, const std::vector<uint8_t>& b){
            std::cerr << "TRACE " << ln << ": ";
            for (auto x : b) fprintf(stderr, "%02X ", x);
            std::cerr << "\n";
        });        // Create dispatcher with print, input, screen mode, and color callbacks
        dispatcher = std::make_unique<BasicDispatcher>(tokenizer, programStore,
            [this](const std::string& text) { print(text); },
            [this](const std::string& prompt) -> std::string { return getInput(prompt); },
            [this](int mode) -> bool { return changeScreenMode(mode); },
            [this](int foreground, int background) -> bool { return changeColor(foreground, background); },
            [this]() -> uint8_t* { return getGraphicsBuffer(); },
            [this](int columns) -> bool { return changeWidth(columns); },
            // LOCATE callback
            [this](int row, int col, int cursor, int start, int stop) -> bool {
                (void)start; (void)stop; // Not used yet
                // Accept -1 as "no change"; BASIC LOCATE rows/cols are 1-based
                // Clamp to screen bounds when specified
                if (row != -1) {
                    if (row <= 0) return false; // invalid
                    int maxRow = getTextRows();
                    cursorY = std::max(0, std::min(maxRow - 1, row - 1));
                }
                if (col != -1) {
                    if (col <= 0) return false; // invalid
                    cursorX = std::max(0, std::min(cols - 1, col - 1));
                }
                if (cursor != -1) {
                    // In GW-BASIC, cursor parameter controls cursor visibility/blink
                    // 0 = off, 1 = on; some dialects support shape via start/stop
                    cursorVisible = (cursor != 0);
                }
                return true;
            },
            // CLS callback
            [this]() -> bool {
                clearScreen();
                return true;
            },
            // INKEY$ callback
            [this]() -> std::string {
                return checkKeyPressed();
            },
            // Sound callback for PLAY command
            [this](double frequency, int durationMs) {
                playSound(frequency, durationMs);
            });
        
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
        if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO)) {
            std::cerr << "SDL initialization failed: " << SDL_GetError() << std::endl;
            return false;
        }
        
        // Initialize window tracking with current screen dimensions
        windowWidth = screenWidth;
        windowHeight = screenHeight;
        
        window = SDL_CreateWindow("GW-BASIC",
                                  screenWidth, screenHeight,
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
        
        // Initialize audio system
        if (!initializeAudio()) {
            std::cerr << "Warning: Audio initialization failed. PLAY command will be silent." << std::endl;
        }
        
        return true;
    }
    
    // Public method to change screen mode (called by SCREEN statement)
    bool changeScreenMode(int mode) {
        return setScreenMode(mode);
    }
    
    // Public method to change colors (called by COLOR statement)
    bool changeColor(int foreground, int background) {
        bool changed = false;
        
        // Update foreground color if specified
        if (foreground >= 0 && foreground <= 15) {
            currentForeground = foreground;
            changed = true;
        }
        
        // Update background color if specified  
        if (background >= 0 && background <= 7) {
            currentBackground = background;
            changed = true;
        }
        
        // Apply the color change to subsequent text output
        // The color change will take effect for new text printed
        
        return changed;
    }

    // Public method to change text width (called by WIDTH statement)
    bool changeWidth(int columns) {
        if (!(columns == 40 || columns == 80 || columns == 132)) {
            return false;
        }
        // Update columns; adjust window size only in text mode
        cols = columns;
        if (!graphicsMode) {
            screenWidth = cols * CHAR_W;
            screenHeight = rows * CHAR_H;
            if (window) {
                SDL_SetWindowSize(window, screenWidth, screenHeight);
            }
        }
        clearScreen();
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
        if (audioDevice) {
            SDL_CloseAudioDevice(audioDevice);
            audioDevice = 0;
        }
        SDL_Quit();
    }
    
    bool initializeAudio() {
        // For now, we'll just use text output for PLAY commands
        // TODO: Implement proper SDL3 audio when the API is clarified
        audioSampleRate = 44100;
        audioDevice = 1; // Fake device ID to indicate "initialized"
        return true;
    }
    
    void playSound(double frequency, int durationMs) {
        // For now, just print what would be played until we have proper SDL3 audio
        // TODO: Implement proper SDL3 audio playback
        if (frequency > 0) {
            std::cout << "♪ " << frequency << "Hz for " << durationMs << "ms" << std::endl;
        } else {
            std::cout << "♫ Pause for " << durationMs << "ms" << std::endl;
        }
        
        // Simulate the delay
        SDL_Delay(durationMs / 10); // Shortened delay for testing
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
        int textRows = getTextRows();
        for (int y = 0; y < textRows && y < MAX_ROWS; y++) {
            for (int x = 0; x < cols && x < MAX_COLS; x++) {
                screen[y][x] = {' ', PALETTE[currentForeground], PALETTE[currentBackground]};
            }
        }
        cursorX = 0;
        cursorY = 0;
        
        // Clear pixel buffer for graphics modes
        if (graphicsMode && !pixelBuffer.empty()) {
            std::fill(pixelBuffer.begin(), pixelBuffer.end(), PixelData{0, 0, 0, 255});
        }
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
            if (cursorY >= getTextRows()) {
                scrollUp();
                cursorY = getTextRows() - 1;
            }
        } else if (ch == '\r') {
            cursorX = 0;
        } else if (ch == '\b') {
            if (cursorX > 0) {
                cursorX--;
                if (cursorY >= 0 && cursorY < MAX_ROWS && cursorX >= 0 && cursorX < MAX_COLS) {
                    screen[cursorY][cursorX] = {' ', PALETTE[currentForeground], PALETTE[currentBackground]};
                }
            }
        } else if (ch >= 32 && ch <= 126) {
            if (cursorY >= 0 && cursorY < MAX_ROWS && cursorX >= 0 && cursorX < MAX_COLS) {
                screen[cursorY][cursorX] = {ch, PALETTE[currentForeground], PALETTE[currentBackground]};
            }
            cursorX++;
            if (cursorX >= cols) {
                cursorX = 0;
                cursorY++;
                if (cursorY >= getTextRows()) {
                    scrollUp();
                    cursorY = getTextRows() - 1;
                }
            }
        }
    }
    
    void scrollUp() {
        int textRows = getTextRows();
        for (int y = 0; y < textRows - 1 && y < MAX_ROWS - 1; y++) {
            for (int x = 0; x < cols && x < MAX_COLS; x++) {
                screen[y][x] = screen[y + 1][x];
            }
        }
        // Clear bottom text line (but don't touch function key line)
        for (int x = 0; x < cols && x < MAX_COLS; x++) {
            screen[textRows - 1][x] = {' ', PALETTE[currentForeground], PALETTE[currentBackground]};
        }
    }
    
    void showPrompt() {
        if (programMode) {
            // In program mode, no prompt
        } else {
            print("Ok\n");
        }
    }
    
    // INKEY$ implementation - check for a key press without blocking
    std::string checkKeyPressed() {
        // Return any buffered key first
        if (!this->keyBuffer.empty()) {
            char key = this->keyBuffer.front();
            this->keyBuffer.pop();
            return std::string(1, key);
        }

        // Non-blocking: drain the SDL event queue, but don't starve QUIT
        // Capture at most one character to return and buffer any extras
        bool haveChar = false;
        char firstChar = 0;

        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_EVENT_KEY_DOWN) {
                SDL_Keycode keycode = event.key.key;

                auto pushChar = [&](char ch){
                    if (!haveChar) { haveChar = true; firstChar = ch; }
                    else { this->keyBuffer.push(ch); }
                };

                // Convert common keys to single characters for INKEY$
                if (keycode >= SDLK_SPACE && keycode <= SDLK_Z) {
                    char key = static_cast<char>(keycode);
                    // Handle shift for letters
                    if (keycode >= SDLK_A && keycode <= SDLK_Z) {
                        if (!(event.key.mod & SDL_KMOD_SHIFT)) {
                            key = static_cast<char>(std::tolower(key));
                        }
                    }
                    pushChar(key);
                    continue; // continue draining events so QUIT is honored
                }

                // Handle special keys
                switch (keycode) {
                    case SDLK_RETURN: pushChar('\r'); break;
                    case SDLK_ESCAPE: pushChar('\x1B'); break;
                    case SDLK_BACKSPACE: pushChar('\x08'); break;
                    case SDLK_TAB: pushChar('\t'); break;
                    case SDLK_SPACE: pushChar(' '); break;
                    default:
                        // Ignore other keys for INKEY$
                        break;
                }
                // Keep draining to handle non-key events (e.g., QUIT)
            } else {
                // Route non-key events (like window close) to the normal handler
                this->handleEvent(event);
            }
        }

        if (haveChar) return std::string(1, firstChar);
        return ""; // No key pressed
    }
    
    void handleEvent(const SDL_Event& event) {
        switch (event.type) {
        case SDL_EVENT_QUIT:
            // Stop both the GUI loop and any running BASIC program
            this->running = false;
            if (this->interpreter) {
                this->interpreter->stop();
            }
            break;
            
        case SDL_EVENT_KEY_DOWN:
            handleKeyDown(event.key);
            break;
            
        case SDL_EVENT_WINDOW_RESIZED:
            handleWindowResize(event.window.data1, event.window.data2);
            break;
        }
    }
    
    void handleWindowResize(int newWidth, int newHeight) {
        // Maintain aspect ratio when window is resized
        int constrainedWidth, constrainedHeight;
        calculateAspectRatioConstrainedSize(newWidth, newHeight, constrainedWidth, constrainedHeight);
        
        // Update our window tracking variables
        windowWidth = constrainedWidth;
        windowHeight = constrainedHeight;
        
        // If the constrained size differs from requested, resize the window
        if (constrainedWidth != newWidth || constrainedHeight != newHeight) {
            SDL_SetWindowSize(window, constrainedWidth, constrainedHeight);
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
            
        // Function keys F1-F10
        case SDLK_F1:
            handleFunctionKey(1);
            break;
        case SDLK_F2:
            handleFunctionKey(2);
            break;
        case SDLK_F3:
            handleFunctionKey(3);
            break;
        case SDLK_F4:
            handleFunctionKey(4);
            break;
        case SDLK_F5:
            handleFunctionKey(5);
            break;
        case SDLK_F6:
            handleFunctionKey(6);
            break;
        case SDLK_F7:
            handleFunctionKey(7);
            break;
        case SDLK_F8:
            handleFunctionKey(8);
            break;
        case SDLK_F9:
            handleFunctionKey(9);
            break;
        case SDLK_F10:
            handleFunctionKey(10);
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
        // Update function key display
        displayFunctionKeys();
        
        // Clear screen
        SDL_SetRenderDrawColor(renderer, BLACK.r, BLACK.g, BLACK.b, BLACK.a);
        SDL_RenderClear(renderer);
        
        // Render text or graphics
        if (graphicsMode) {
            renderGraphics();
            renderTextOverlay();  // Re-enable text overlay for graphics modes
        } else {
            renderTextMode();
        }
        
        SDL_RenderPresent(renderer);
    }
    
    void renderTextMode() {
        float scaleX, scaleY;
        getScaleFactors(scaleX, scaleY);
        
        for (int y = 0; y < rows; y++) {
            for (int x = 0; x < cols; x++) {
                renderCharScaled(x, y, screen[y][x], scaleX, scaleY);
            }
        }
        
        // Render cursor
        if (cursorVisible) {
            SDL_SetRenderDrawColor(renderer, WHITE.r, WHITE.g, WHITE.b, WHITE.a);
            SDL_FRect cursorRect = {
                static_cast<float>(cursorX * CHAR_W * scaleX),
                static_cast<float>((cursorY * CHAR_H + CHAR_H - 2) * scaleY),
                static_cast<float>(CHAR_W * scaleX),
                2.0f * scaleY
            };
            SDL_RenderFillRect(renderer, &cursorRect);
        }
    }
    
    void renderGraphics() {
        // Convert graphics buffer (8-bit color indices) to pixel buffer (RGBA)
        if (!graphicsBuffer.empty() && !pixelBuffer.empty() && 
            graphicsBuffer.size() == pixelBuffer.size() &&
            graphicsBuffer.size() >= static_cast<size_t>(screenWidth * screenHeight)) {
            
            for (size_t i = 0; i < graphicsBuffer.size(); i++) {
                uint8_t colorIndex = graphicsBuffer[i];
                if (colorIndex < 16) {
                    // Convert from 16-color palette to RGBA
                    pixelBuffer[i] = {PALETTE[colorIndex].r, PALETTE[colorIndex].g, 
                                     PALETTE[colorIndex].b, PALETTE[colorIndex].a};
                } else {
                    // For 256-color modes, use a simplified conversion
                    pixelBuffer[i] = {colorIndex, colorIndex, colorIndex, 255};
                }
            }
        }
        
        // Render pixel buffer directly with scaling
        if (!pixelBuffer.empty() && pixelBuffer.size() >= static_cast<size_t>(screenWidth * screenHeight)) {
            float scaleX, scaleY;
            getScaleFactors(scaleX, scaleY);
            
            for (int y = 0; y < screenHeight; y++) {
                for (int x = 0; x < screenWidth; x++) {
                    size_t index = y * screenWidth + x;
                    if (index < pixelBuffer.size()) {
                        const auto& pixel = pixelBuffer[index];
                        if (pixel.r || pixel.g || pixel.b) {  // Only draw non-black pixels for performance
                            SDL_SetRenderDrawColor(renderer, pixel.r, pixel.g, pixel.b, pixel.a);
                            SDL_FRect pixelRect = {
                                static_cast<float>(x * scaleX),
                                static_cast<float>(y * scaleY),
                                scaleX, scaleY
                            };
                            SDL_RenderFillRect(renderer, &pixelRect);
                        }
                    }
                }
            }
        }
    }
    
    void renderTextOverlay() {
        // Render text overlay for graphics modes with window scaling
        float scaleX, scaleY;
        getScaleFactors(scaleX, scaleY);
        
        // Calculate base text scaling for graphics modes
        float baseTextScale = 1.0f;
        if (screenWidth < 640) {
            baseTextScale = static_cast<float>(screenWidth) / 640.0f;
        }
        
        // Apply both base scaling and window scaling
        float finalScaleX = baseTextScale * scaleX;
        float finalScaleY = baseTextScale * scaleY;
        
        int scaledCharW = static_cast<int>(CHAR_W * finalScaleX);
        int scaledCharH = static_cast<int>(CHAR_H * finalScaleY);
        
        // Only render non-space characters to allow graphics to show through
        for (int y = 0; y < rows; y++) {
            for (int x = 0; x < cols; x++) {
                const auto& screenChar = screen[y][x];
                if (screenChar.ch != ' ' && y < MAX_ROWS && x < MAX_COLS) {
                    // Check if character would fit in the window
                    int charPixelX = static_cast<int>(x * CHAR_W * finalScaleX);
                    int charPixelY = static_cast<int>(y * CHAR_H * finalScaleY);
                    
                    if (charPixelX + scaledCharW <= windowWidth && charPixelY + scaledCharH <= windowHeight) {
                        renderCharScaled(x, y, screenChar, finalScaleX, finalScaleY);
                    }
                }
            }
        }
        
        // Render cursor in graphics mode
        if (cursorVisible && cursorX >= 0 && cursorY >= 0 && cursorX < cols && cursorY < rows) {
            int cursorPixelX = static_cast<int>(cursorX * CHAR_W * finalScaleX);
            int cursorPixelY = static_cast<int>(cursorY * CHAR_H * finalScaleY);
            
            if (cursorPixelX + scaledCharW <= windowWidth && cursorPixelY + scaledCharH <= windowHeight) {
                SDL_SetRenderDrawColor(renderer, WHITE.r, WHITE.g, WHITE.b, WHITE.a);
                SDL_FRect cursorRect = {
                    static_cast<float>(cursorPixelX),
                    static_cast<float>(cursorPixelY + scaledCharH - 2 * finalScaleY),
                    static_cast<float>(scaledCharW),
                    2.0f * finalScaleY
                };
                SDL_RenderFillRect(renderer, &cursorRect);
            }
        }
    }
    
    void renderCharScaled(int x, int y, const ScreenChar& screenChar, float scale) {
        int scaledCharW = static_cast<int>(CHAR_W * scale);
        int scaledCharH = static_cast<int>(CHAR_H * scale);
        
        // Calculate pixel position
        int charPixelX = x * scaledCharW;
        int charPixelY = y * scaledCharH;
        
        // Bounds check
        if (charPixelX + scaledCharW > screenWidth || charPixelY + scaledCharH > screenHeight) {
            return;
        }
        
        if (screenChar.ch == ' ') {
            // Still render background for spaces
            if (screenChar.bg.r != 0 || screenChar.bg.g != 0 || screenChar.bg.b != 0) {
                SDL_SetRenderDrawColor(renderer, screenChar.bg.r, screenChar.bg.g, screenChar.bg.b, screenChar.bg.a);
                SDL_FRect bgRect = {
                    static_cast<float>(charPixelX),
                    static_cast<float>(charPixelY),
                    static_cast<float>(scaledCharW),
                    static_cast<float>(scaledCharH)
                };
                SDL_RenderFillRect(renderer, &bgRect);
            }
            return;
        }
        
        // Background
        if (screenChar.bg.r != 0 || screenChar.bg.g != 0 || screenChar.bg.b != 0) {
            SDL_SetRenderDrawColor(renderer, screenChar.bg.r, screenChar.bg.g, screenChar.bg.b, screenChar.bg.a);
            SDL_FRect bgRect = {
                static_cast<float>(charPixelX),
                static_cast<float>(charPixelY),
                static_cast<float>(scaledCharW),
                static_cast<float>(scaledCharH)
            };
            SDL_RenderFillRect(renderer, &bgRect);
        }
        
        // Render character using bitmap font with scaling
        const uint8_t* charData = BitmapFont::getCharData(screenChar.ch);
        SDL_SetRenderDrawColor(renderer, screenChar.fg.r, screenChar.fg.g, screenChar.fg.b, screenChar.fg.a);
        
        // Scale font data
        float pixelScaleX = static_cast<float>(scaledCharW) / CHAR_W;
        float pixelScaleY = static_cast<float>(scaledCharH) / CHAR_H;
        
        for (int row = 0; row < CHAR_H; row++) {
            uint8_t rowData = charData[row];
            for (int col = 0; col < CHAR_W; col++) {
                if (rowData & (0x80 >> col)) {
                    // Draw scaled pixel(s)
                    SDL_FRect pixelRect = {
                        static_cast<float>(charPixelX + col * pixelScaleX),
                        static_cast<float>(charPixelY + row * pixelScaleY),
                        pixelScaleX,
                        pixelScaleY
                    };
                    SDL_RenderFillRect(renderer, &pixelRect);
                }
            }
        }
    }
    
    // Overloaded version for separate X and Y scaling
    void renderCharScaled(int x, int y, const ScreenChar& screenChar, float scaleX, float scaleY) {
        int scaledCharW = static_cast<int>(CHAR_W * scaleX);
        int scaledCharH = static_cast<int>(CHAR_H * scaleY);
        
        // Calculate pixel position
        int charPixelX = static_cast<int>(x * CHAR_W * scaleX);
        int charPixelY = static_cast<int>(y * CHAR_H * scaleY);
        
        // Bounds check - use window dimensions instead of screen dimensions
        if (charPixelX + scaledCharW > windowWidth || charPixelY + scaledCharH > windowHeight) {
            return;
        }
        
        if (screenChar.ch == ' ') {
            // Still render background for spaces
            if (screenChar.bg.r != 0 || screenChar.bg.g != 0 || screenChar.bg.b != 0) {
                SDL_SetRenderDrawColor(renderer, screenChar.bg.r, screenChar.bg.g, screenChar.bg.b, screenChar.bg.a);
                SDL_FRect bgRect = {
                    static_cast<float>(charPixelX),
                    static_cast<float>(charPixelY),
                    static_cast<float>(scaledCharW),
                    static_cast<float>(scaledCharH)
                };
                SDL_RenderFillRect(renderer, &bgRect);
            }
            return;
        }
        
        // Background
        if (screenChar.bg.r != 0 || screenChar.bg.g != 0 || screenChar.bg.b != 0) {
            SDL_SetRenderDrawColor(renderer, screenChar.bg.r, screenChar.bg.g, screenChar.bg.b, screenChar.bg.a);
            SDL_FRect bgRect = {
                static_cast<float>(charPixelX),
                static_cast<float>(charPixelY),
                static_cast<float>(scaledCharW),
                static_cast<float>(scaledCharH)
            };
            SDL_RenderFillRect(renderer, &bgRect);
        }
        
        // Render character using bitmap font with scaling
        const uint8_t* charData = BitmapFont::getCharData(screenChar.ch);
        SDL_SetRenderDrawColor(renderer, screenChar.fg.r, screenChar.fg.g, screenChar.fg.b, screenChar.fg.a);
        
        // Scale font data
        float pixelScaleX = scaleX;
        float pixelScaleY = scaleY;
        
        for (int row = 0; row < CHAR_H; row++) {
            uint8_t rowData = charData[row];
            for (int col = 0; col < CHAR_W; col++) {
                if (rowData & (0x80 >> col)) {
                    // Draw scaled pixel(s)
                    SDL_FRect pixelRect = {
                        static_cast<float>(charPixelX + col * pixelScaleX),
                        static_cast<float>(charPixelY + row * pixelScaleY),
                        pixelScaleX,
                        pixelScaleY
                    };
                    SDL_RenderFillRect(renderer, &pixelRect);
                }
            }
        }
    }
    
    // Change screen mode (to be called by SCREEN statement)
    bool setScreenMode(int mode) {
        bool oldGraphicsMode = graphicsMode;
        int oldWidth = screenWidth;
        int oldHeight = screenHeight;
        int oldCols = cols;
        int oldRows = rows;
        
        switch (mode) {
            case 0:  // Text mode 80x25
                screenWidth = 720;
                screenHeight = 400;
                cols = 80;
                rows = 25;
                graphicsMode = false;
                break;
                
            case 1:  // CGA 320x200, 4 colors
                screenWidth = 320;
                screenHeight = 200;
                cols = 40;  // Text overlay capability
                rows = 25;
                graphicsMode = true;
                break;
                
            case 2:  // CGA 640x200, 2 colors
                screenWidth = 640;
                screenHeight = 200;
                cols = 80;
                rows = 25;
                graphicsMode = true;
                break;
                
            case 7:  // EGA 320x200, 16 colors
                screenWidth = 320;
                screenHeight = 200;
                cols = 40;
                rows = 25;
                graphicsMode = true;
                break;
                
            case 8:  // EGA 640x200, 16 colors
                screenWidth = 640;
                screenHeight = 200;
                cols = 80;
                rows = 25;
                graphicsMode = true;
                break;
                
            case 9:  // EGA 640x350, 16 colors
                screenWidth = 640;
                screenHeight = 350;
                cols = 80;
                rows = 43;
                graphicsMode = true;
                break;
                
            case 10:  // EGA 640x350, 4 colors
                screenWidth = 640;
                screenHeight = 350;
                cols = 80;
                rows = 43;
                graphicsMode = true;
                break;
                
            case 11:  // VGA 640x480, 2 colors
                screenWidth = 640;
                screenHeight = 480;
                cols = 80;
                rows = 60;
                graphicsMode = true;
                break;
                
            case 12:  // VGA 640x480, 16 colors
                screenWidth = 640;
                screenHeight = 480;
                cols = 80;
                rows = 60;
                graphicsMode = true;
                break;
                
            case 13:  // VGA 320x200, 256 colors
                screenWidth = 320;
                screenHeight = 200;
                cols = 40;
                rows = 25;
                graphicsMode = true;
                break;
                
            default:
                return false;  // Invalid mode
        }
        
        currentScreenMode = mode;
        
        // Resize window if mode changed
        if (screenWidth != oldWidth || screenHeight != oldHeight) {
            // Update window tracking variables
            windowWidth = screenWidth;
            windowHeight = screenHeight;
            
            if (window) {
                SDL_SetWindowSize(window, screenWidth, screenHeight);
            }
        }
        
        // Initialize pixel buffer for graphics modes
        if (graphicsMode) {
            try {
                size_t bufferSize = screenWidth * screenHeight;
                pixelBuffer.assign(bufferSize, {0, 0, 0, 255});
                graphicsBuffer.assign(bufferSize, 0); // 8-bit color indices, initialize to black
            } catch (const std::exception& e) {
                std::cerr << "Error allocating pixel buffer: " << e.what() << std::endl;
                return false;
            }
        } else {
            pixelBuffer.clear();
            graphicsBuffer.clear();
        }
        
        // Clear screen in new mode
        clearScreen();
        
        return true;
    }
    
    void renderChar(int x, int y, const ScreenChar& screenChar) {
        // Bounds check - don't render characters outside window
        int charPixelX = x * CHAR_W;
        int charPixelY = y * CHAR_H;
        
        if (charPixelX + CHAR_W > screenWidth || charPixelY + CHAR_H > screenHeight) {
            return;  // Character would be outside window bounds
        }
        
        if (screenChar.ch == ' ') {
            // Still render background for spaces
            if (screenChar.bg.r != 0 || screenChar.bg.g != 0 || screenChar.bg.b != 0) {
                SDL_SetRenderDrawColor(renderer, screenChar.bg.r, screenChar.bg.g, screenChar.bg.b, screenChar.bg.a);
                SDL_FRect bgRect = {
                    static_cast<float>(charPixelX),
                    static_cast<float>(charPixelY),
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
                static_cast<float>(charPixelX),
                static_cast<float>(charPixelY),
                static_cast<float>(CHAR_W),
                static_cast<float>(CHAR_H)
            };
            SDL_RenderFillRect(renderer, &bgRect);
        }
        
        // Render character using bitmap font
        const uint8_t* charData = BitmapFont::getCharData(screenChar.ch);
        SDL_SetRenderDrawColor(renderer, screenChar.fg.r, screenChar.fg.g, screenChar.fg.b, screenChar.fg.a);
        
        int baseX = charPixelX;
        int baseY = charPixelY;
        
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
    
    // Initialize default soft keys (based on original GW-BASIC defaults)
    void initializeSoftKeys() {
        softKeys[0] = "LIST";        // F1
        softKeys[1] = "RUN\r";       // F2 - CR auto-executes
        softKeys[2] = "LOAD\"";      // F3
        softKeys[3] = "SAVE\"";      // F4
        softKeys[4] = "CONT\r";      // F5 - CR auto-executes
        softKeys[5] = "\"LPT1:\"";   // F6
        softKeys[6] = "TRON\r";      // F7 - CR auto-executes
        softKeys[7] = "TROFF\r";     // F8 - CR auto-executes
        softKeys[8] = "KEY";         // F9
        softKeys[9] = "SCREEN 0,0,0\r"; // F10 - CR auto-executes
    }
    
    // Handle function key press
    void handleFunctionKey(int keyNumber) {
        if (keyNumber < 1 || keyNumber > NUM_FUNCTION_KEYS) {
            return;
        }
        
        // Convert to 0-based index for trap system (KEY(1) = keyIndex 1)
        uint8_t keyIndex = keyNumber;
        
        // Check if there's an active trap for this key
        if (dispatcher && dispatcher->getEventTrapSystem() && 
            dispatcher->getEventTrapSystem()->isKeyTrapEnabled(keyIndex)) {
            // Don't expand soft key if trap is active - the event system will handle it
            return;
        }
        
        // No active trap, so expand the soft key
        int softKeyIndex = keyNumber - 1; // Convert to 0-based index for soft key array
        const std::string& keyText = softKeys[softKeyIndex];
        if (!keyText.empty()) {
            // Simulate typing the soft key text
            for (char ch : keyText) {
                if (ch == '\r') {
                    // Handle carriage return as enter
                    handleEnter();
                } else {
                    // Type the character
                    handleCharInput(ch);
                }
            }
        }
    }
    
    // Set a soft key definition (called by KEY statement)
    void setSoftKey(int keyNumber, const std::string& text) {
        if (keyNumber < 1 || keyNumber > NUM_FUNCTION_KEYS) {
            return;
        }
        
        int keyIndex = keyNumber - 1;
        softKeys[keyIndex] = text.substr(0, SOFT_KEY_LENGTH);
    }
    
    // Get a soft key definition
    std::string getSoftKey(int keyNumber) const {
        if (keyNumber < 1 || keyNumber > NUM_FUNCTION_KEYS) {
            return "";
        }
        
        int keyIndex = keyNumber - 1;
        return softKeys[keyIndex];
    }
    
    // Get graphics buffer for drawing operations
    uint8_t* getGraphicsBuffer() {
        if (graphicsMode && !graphicsBuffer.empty()) {
            return graphicsBuffer.data();
        }
        return nullptr;
    }
    
    // Display function keys on bottom line (like original GW-BASIC)
    void displayFunctionKeys() {
        if (!functionKeysEnabled || graphicsMode) {
            return;
        }
        
        // Clear the bottom line
        for (int x = 0; x < cols; x++) {
            screen[rows - 1][x] = {' ', WHITE, BLACK};
        }
        
        // Display function keys F1-F10 with their first 6 characters
        int col = 0;
        for (int i = 0; i < NUM_FUNCTION_KEYS && col < cols - 8; i++) {
            // Display key number (F1-F10, but F10 shows as F0)
            char keyLabel = (i == 9) ? '0' : '1' + i;
            screen[rows - 1][col++] = {'F', BLACK, WHITE};  // Reverse video for key labels
            screen[rows - 1][col++] = {keyLabel, BLACK, WHITE};
            
            // Display up to 6 characters of the soft key content
            const std::string& keyText = softKeys[i];
            for (int j = 0; j < 6 && col < cols; j++) {
                char ch = (static_cast<size_t>(j) < keyText.length()) ? keyText[j] : ' ';
                if (ch == '\r') ch = '_';  // Show CR as underscore
                screen[rows - 1][col++] = {ch, WHITE, BLACK};
            }
            
            // Add a space separator if not at end
            if (i < NUM_FUNCTION_KEYS - 1 && col < cols) {
                screen[rows - 1][col++] = {' ', WHITE, BLACK};
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
    // Console color state
    int consoleForeground = 7;  // Default light gray
    int consoleBackground = 0;  // Default black
    
    // ANSI color mapping for 16-color palette
    auto getANSIColor = [](int color, bool background) -> std::string {
        // Map GW-BASIC colors (0-15) to ANSI colors
        int ansiColors[] = {0, 4, 2, 6, 1, 5, 3, 7, 8, 12, 10, 14, 9, 13, 11, 15};
        if (color < 0 || color > 15) return "";
        
        int ansiColor = ansiColors[color];
        if (background) {
            return "\033[" + std::to_string(40 + (ansiColor & 7)) + "m";
        } else {
            if (ansiColor >= 8) {
                return "\033[" + std::to_string(90 + (ansiColor - 8)) + "m"; // Bright colors
            } else {
                return "\033[" + std::to_string(30 + ansiColor) + "m";
            }
        }
    };
    
    // Initialize GW-BASIC components
    auto tokenizer = std::make_shared<Tokenizer>();
    auto programStore = std::make_shared<ProgramStore>();
    auto dispatcher = std::make_unique<BasicDispatcher>(tokenizer, programStore, 
        [&](const std::string& text) { 
            // Apply current console colors before printing
            std::cout << getANSIColor(consoleForeground, false) 
                      << getANSIColor(consoleBackground, true) 
                      << text << "\033[0m"; // Reset colors after text
        },  // print callback
        [](const std::string& prompt) -> std::string {       // input callback
            std::cout << prompt;
            std::string input;
            std::getline(std::cin, input);
            return input;
        },
        nullptr,  // screen mode callback (not supported in console mode)
        [&](int foreground, int background) -> bool {         // color callback
            // Update console colors and apply ANSI escape codes
            if (foreground >= 0 && foreground <= 15) {
                consoleForeground = foreground;
            }
            if (background >= 0 && background <= 7) {
                consoleBackground = background;
            }
            
            // Apply the color change immediately
            std::cout << getANSIColor(consoleForeground, false) 
                      << getANSIColor(consoleBackground, true);
            
            return true;
        },
        nullptr,  // graphics buffer callback (not supported in console mode)
        [&](int /*columns*/) -> bool { return true; }, // width callback (no-op in console mode)
        // LOCATE callback: console mode can't reposition the input cursor reliably
        // for piped input; accept and ignore to keep semantics tolerant.
        [&](int row, int col, int cursor, int start, int stop) -> bool {
            (void)row; (void)col; (void)cursor; (void)start; (void)stop;
            return true;
        },
        // CLS callback in console mode: ANSI clear screen and home
        [&]() -> bool {
            std::cout << "\033[2J\033[H";
            return true;
        },
        // INKEY$ callback for console mode: always return empty string (non-blocking not easily supported in console)
        []() -> std::string {
            return "";
        },
        // Sound callback for console mode: no audio support, just print
        [&](double frequency, int durationMs) {
            if (frequency > 0) {
                std::cout << "BEEP: " << frequency << "Hz for " << durationMs << "ms" << std::endl;
            } else {
                std::cout << "PAUSE: " << durationMs << "ms" << std::endl;
            }
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
    
    // Reset console colors before exiting
    std::cout << "\033[0m";
    
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
