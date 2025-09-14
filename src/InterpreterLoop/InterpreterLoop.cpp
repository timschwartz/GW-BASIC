#include "InterpreterLoop.hpp"

#include <stdexcept>
#include <utility>
#include <iostream>
#include <ostream>

#include "../ProgramStore/ProgramStore.hpp"
#include "../Tokenizer/Tokenizer.hpp"
#include "../Runtime/EventTraps.hpp"
#include "../Runtime/GWError.hpp"
#include "../Runtime/RuntimeStack.hpp"
#include "../ExpressionEvaluator/ExpressionEvaluator.hpp"

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
    
    // Check for event traps before executing each statement
    if (eventTrapSystem) {
        uint16_t trapLine = eventTrapSystem->checkForEvents();
        if (trapLine != 0) {
            // Event trap triggered - jump to handler
            if (prog->hasLine(trapLine)) {
                currentLine = trapLine;
                prog->setCurrentLine(currentLine);
                return StepResult::Jumped;
            }
        }
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
        try {
            // Handler can throw to signal runtime error
            nextOverride = handler(tokens, currentLine);
        } catch (const gwbasic::GWError& e) {
            // Handle GW-BASIC runtime error
            uint16_t errorResult = handleRuntimeError(e);
            if (errorResult == 0xFFFF) {
                halted = true;
                return StepResult::Halted;
            } else if (errorResult != 0) {
                // Jump to error handler
                currentLine = errorResult;
                if (prog) prog->setCurrentLine(currentLine);
                return StepResult::Jumped;
            }
            // Continue normal execution if error result is 0
        } catch (const expr::BasicError& e) {
            // Convert expression evaluator error to GW-BASIC error
            gwbasic::GWError gwError(e.code, e.what(), currentLine, e.position);
            uint16_t errorResult = handleRuntimeError(gwError);
            if (errorResult == 0xFFFF) {
                halted = true;
                return StepResult::Halted;
            } else if (errorResult != 0) {
                // Jump to error handler
                currentLine = errorResult;
                if (prog) prog->setCurrentLine(currentLine);
                return StepResult::Jumped;
            }
            // Continue normal execution if error result is 0
        } catch (const std::exception& e) {
            // Convert generic exception to GW-BASIC internal error
            gwbasic::GWError gwError(gwbasic::ErrorCodes::INTERNAL_ERROR, e.what(), currentLine, 0);
            uint16_t errorResult = handleRuntimeError(gwError);
            if (errorResult == 0xFFFF) {
                halted = true;
                return StepResult::Halted;
            } else if (errorResult != 0) {
                // Jump to error handler
                currentLine = errorResult;
                if (prog) prog->setCurrentLine(currentLine);
                return StepResult::Jumped;
            }
            // Continue normal execution if error result is 0
        }
    }

    if (halted) return StepResult::Halted;

    if (nextOverride != 0) {
        // Jump or termination
        if (nextOverride == 0xFFFF) {
            // END/STOP sentinel - halt immediately
            halted = true;
            return StepResult::Halted;
        }
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

void InterpreterLoop::injectKeyEvent(uint8_t scanCode, bool pressed) {
    if (eventTrapSystem) {
        eventTrapSystem->injectKeyEvent(scanCode, pressed);
    }
    if (keyEventCallback) {
        keyEventCallback(scanCode, pressed);
    }
}

uint16_t InterpreterLoop::handleRuntimeError(const gwbasic::GWError& error) {
    // Check if we have an active error handler
    if (runtimeStack && runtimeStack->hasErrorHandler()) {
        // Set up error information for RESUME
        gwbasic::ErrFrame frame{};
        frame.errCode = error.getErrorCode();
        frame.resumeLine = currentLine;
        frame.resumeTextPtr = 0; // TODO: Could store position within line if needed
        frame.errorHandlerLine = runtimeStack->getErrorHandlerLine();
        frame.trapEnabled = true;
        
        // Push error frame onto stack
        runtimeStack->pushErr(frame);
        
        // Debug trace for error handling
        std::cerr << "[InterpreterLoop] handleRuntimeError: code=" << frame.errCode
              << " at line=" << frame.resumeLine
              << ", handler=" << frame.errorHandlerLine << std::endl;
        
        // Jump to error handler
        uint16_t handlerLine = runtimeStack->getErrorHandlerLine();
        if (prog && prog->hasLine(handlerLine)) {
            return handlerLine;
        } else {
            // Handler line doesn't exist - disable error handling and fall through to default
            runtimeStack->disableErrorHandler();
        }
    }
    
    // No error handler or handler line invalid - use default behavior
    // For now, just halt the program (could also throw the error)
    halted = true;
    return 0xFFFF; // Halt sentinel
}
