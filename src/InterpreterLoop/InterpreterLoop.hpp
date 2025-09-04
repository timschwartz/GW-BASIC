#pragma once

#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <vector>

class ProgramStore;
class Tokenizer;

/**
 * InterpreterLoop
 * 
 * Minimal execution loop skeleton for the GW-BASIC reimplementation.
 * This class coordinates between ProgramStore (tokenized program lines)
 * and a pluggable statement dispatcher. It does not yet evaluate
 * expressions; it focuses on control flow and integration points.
 */
class InterpreterLoop {
public:
    // Execution result for a single step
    enum class StepResult {
        Continued,   // Proceed to next line
        Jumped,       // Control transferred to another line
        Halted,       // Program ended (END/STOP or error)
        Waiting       // Waiting for input or external event
    };

    // Diagnostic / trace callback: (lineNumber, rawTokens)
    using TraceCallback = std::function<void(uint16_t, const std::vector<uint8_t>&)>;

    // Statement handler callback: given a tokenized line body, execute one statement and
    // return optional next line override (0 = fall-through). Can throw to signal error.
    using StatementHandler = std::function<uint16_t(const std::vector<uint8_t>& tokens)>;

    // Constructors
    InterpreterLoop(std::shared_ptr<ProgramStore> program,
                    std::shared_ptr<Tokenizer> tokenizer);
    ~InterpreterLoop();

    // Configuration
    void setTrace(bool enabled) { traceEnabled = enabled; }
    bool getTrace() const { return traceEnabled; }
    void setTraceCallback(TraceCallback cb) { trace = std::move(cb); }
    void setStatementHandler(StatementHandler cb) { handler = std::move(cb); }

    // Program control
    void reset();
    void run();               // RUN from first line
    void cont();              // CONT from current line
    void stop();              // Asynchronous STOP
    void setCurrentLine(uint16_t lineNumber);

    // Single-step execution (useful for debugger and tests)
    StepResult step();

    // Immediate-mode execution (direct command line)
    // Returns true if executed by handler; false if unrecognized.
    bool executeImmediate(const std::string& source);

private:
    std::shared_ptr<ProgramStore> prog;
    std::shared_ptr<Tokenizer> tok;
    bool halted{false};
    bool traceEnabled{false};
    TraceCallback trace{};
    StatementHandler handler{};

    uint16_t currentLine{0};

    // Helpers
    void traceLine(uint16_t lineNum, const std::vector<uint8_t>& tokens);
};
