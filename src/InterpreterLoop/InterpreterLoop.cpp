#include "InterpreterLoop.hpp"

#include <stdexcept>
#include <utility>

#include "../ProgramStore/ProgramStore.hpp"
#include "../Tokenizer/Tokenizer.hpp"

InterpreterLoop::InterpreterLoop(std::shared_ptr<ProgramStore> program,
                                 std::shared_ptr<Tokenizer> tokenizer)
    : prog(std::move(program)), tok(std::move(tokenizer)) {}

InterpreterLoop::~InterpreterLoop() = default;

void InterpreterLoop::reset() {
    halted = false;
    currentLine = 0;
}

void InterpreterLoop::run() {
    if (!prog) return;
    halted = false;
    currentLine = prog->getFirstLineNumber();
    prog->setCurrentLine(currentLine);
    while (!halted) {
        auto res = step();
        if (res == StepResult::Halted || res == StepResult::Waiting) break;
    }
}

void InterpreterLoop::cont() {
    if (!prog) return;
    halted = false;
    if (currentLine == 0) {
        currentLine = prog->getFirstLineNumber();
    }
    prog->setCurrentLine(currentLine);
    while (!halted) {
        auto res = step();
        if (res == StepResult::Halted || res == StepResult::Waiting) break;
    }
}

void InterpreterLoop::stop() { halted = true; }

void InterpreterLoop::setCurrentLine(uint16_t lineNumber) {
    currentLine = lineNumber;
    if (prog) prog->setCurrentLine(lineNumber);
}

InterpreterLoop::StepResult InterpreterLoop::step() {
    if (halted || !prog) return StepResult::Halted;

    if (currentLine == 0) {
        // No more lines
        halted = true;
        return StepResult::Halted;
    }

    auto linePtr = prog->getLine(currentLine);
    if (!linePtr) {
        // Line missing, halt
        halted = true;
        return StepResult::Halted;
    }

    const auto& tokens = linePtr->tokens;
    if (traceEnabled) traceLine(currentLine, tokens);

    uint16_t nextOverride = 0;
    if (handler) {
        // Handler can throw to signal runtime error
        nextOverride = handler(tokens, currentLine);
    }

    if (halted) return StepResult::Halted;

    if (nextOverride != 0) {
        // Jump or termination
        if (!prog->hasLine(nextOverride)) {
            // Invalid jump, halt
            halted = true;
            return StepResult::Halted;
        }
        currentLine = nextOverride;
        prog->setCurrentLine(currentLine);
        return StepResult::Jumped;
    }

    // Fall through to next line
    auto nextIt = prog->getNextLine(currentLine);
    if (!nextIt.isValid()) {
        halted = true;
        return StepResult::Halted;
    }
    currentLine = nextIt->lineNumber;
    prog->setCurrentLine(currentLine);
    return StepResult::Continued;
}

bool InterpreterLoop::executeImmediate(const std::string& source) {
    if (!tok || !handler) return false;
    auto bytes = tok->crunch(source);
    if (bytes.empty()) return false;
    if (traceEnabled) traceLine(0, bytes); // line 0 indicates immediate
    handler(bytes, 0); // immediate mode uses line 0
    return true;
}

void InterpreterLoop::traceLine(uint16_t lineNum, const std::vector<uint8_t>& tokens) {
    if (trace) trace(lineNum, tokens);
}
