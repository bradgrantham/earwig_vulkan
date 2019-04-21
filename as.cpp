
// RISC-V assembler.

#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <map>
#include <vector>
#include <string>
#include <iomanip>

// Map from label name to address in bytes.
std::map<std::string,uint32_t> gLabels;

// Return the pathname without the extension ("file.x" becomes "file").
// If the pathname does not have an extension, it is returned unchanged.
std::string stripExtension(const std::string &pathname) {
    auto slash = pathname.rfind('/');
    auto dot = pathname.rfind('.');
    if (dot == pathname.npos || (slash != pathname.npos && dot < slash)) {
        // No extension.
        return pathname;
    }

    return pathname.substr(0, dot);
}

std::string readFileContents(const std::string &pathname)
{
    std::ifstream file(pathname, std::ios::in | std::ios::ate);
    if (!file.good()) {
        std::cerr << "Can't open file \"" << pathname << "\".\n";
        exit(EXIT_FAILURE);
    }
    std::ifstream::pos_type size = file.tellg();
    file.seekg(0, std::ios::beg);

    std::string text(size, '\0');
    file.read(&text[0], size);

    return text;
}

// ----------------------------------------------------------------------

// Instruction formats.
enum Format {
    FORMAT_R,
    FORMAT_I,
    FORMAT_S,
    FORMAT_SB,
    FORMAT_U,
    FORMAT_UJ,
};

// Information about each type of operator.
struct Operator {
    Format format;

    // Bits 0 through 6.
    uint32_t opcode;

    // Bits 12 through 14.
    uint32_t funct3;

    // Bits 25 through 31.
    uint32_t funct7;

    // Max number of bits in immediate.
    int bits;
};

// ----------------------------------------------------------------------

// Main assembler class.
class Assembler {
private:
    // Map from operator name to operator information.
    std::map<std::string,Operator> operators;

    // Map from register name to register number.
    std::map<std::string,int> registers;

    std::string inPathname;
    bool verbose;

    // Lines of source code.
    std::vector<std::string> lines;

    // Line number in the input file (0-based).
    uint32_t lineNumber;

    // Pointer we walk through the file.
    const char *s;

    // Pointer to the token we just read.
    const char *previousToken;

    // Output binary.
    std::vector<uint32_t> bin;

    // Parallel array to "bin" indicating which source line emitted the instruction.
    std::vector<size_t> binLine;

public:
    Assembler()
        : inPathname("unspecified") {

        // Build our maps.

        // Basic arithmetic.
        operators["add"]   = Operator{FORMAT_R, 0b0110011, 0b000, 0b0000000, 0};
        operators["sub"]   = Operator{FORMAT_R, 0b0110011, 0b000, 0b0100000, 0};
        operators["sll"]   = Operator{FORMAT_R, 0b0110011, 0b001, 0b0000000, 0};
        operators["slt"]   = Operator{FORMAT_R, 0b0110011, 0b010, 0b0000000, 0};
        operators["sltu"]  = Operator{FORMAT_R, 0b0110011, 0b011, 0b0000000, 0};
        operators["xor"]   = Operator{FORMAT_R, 0b0110011, 0b100, 0b0000000, 0};
        operators["srl"]   = Operator{FORMAT_R, 0b0110011, 0b101, 0b0000000, 0};
        operators["sra"]   = Operator{FORMAT_R, 0b0110011, 0b101, 0b0100000, 0};
        operators["or"]    = Operator{FORMAT_R, 0b0110011, 0b110, 0b0000000, 0};
        operators["and"]   = Operator{FORMAT_R, 0b0110011, 0b111, 0b0000000, 0};

        // Immediates.
        operators["addi"]  = Operator{FORMAT_I, 0b0010011, 0b000, 0b0000000, 12};
        operators["andi"]  = Operator{FORMAT_I, 0b0010011, 0b111, 0b0000000, 12};
        operators["ori"]   = Operator{FORMAT_I, 0b0010011, 0b110, 0b0000000, 12};
        operators["xori"]  = Operator{FORMAT_I, 0b0010011, 0b100, 0b0000000, 12};
        operators["slti"]  = Operator{FORMAT_I, 0b0010011, 0b010, 0b0000000, 12};
        operators["sltiu"] = Operator{FORMAT_I, 0b0010011, 0b011, 0b0000000, 12};

        // Shifts.
        operators["slli"]  = Operator{FORMAT_I, 0b0010011, 0b001, 0b0000000, 5};
        operators["srli"]  = Operator{FORMAT_I, 0b0010011, 0b101, 0b0000000, 5};
        operators["srai"]  = Operator{FORMAT_I, 0b0010011, 0b101, 0b0100000, 5};

        // Uppers.
        operators["lui"]   = Operator{FORMAT_U, 0b0110111, 0b000, 0b0000000, 22};
        operators["auipc"] = Operator{FORMAT_U, 0b0010111, 0b000, 0b0000000, 22};

        // Stores.
        operators["sb"]    = Operator{FORMAT_S, 0b0100011, 0b000, 0b0100000, 12};
        operators["sh"]    = Operator{FORMAT_S, 0b0100011, 0b001, 0b0100000, 12};
        operators["sw"]    = Operator{FORMAT_S, 0b0100011, 0b010, 0b0100000, 12};

        // Registers.
        addRegisters("x", 0, 31, 0);
        registers["ra"] = 1;
        registers["sp"] = 2;
        registers["gp"] = 3;
        registers["tp"] = 4;
        addRegisters("t", 0, 2, 5);
        registers["fp"] = 8;
        addRegisters("s", 0, 1, 8);
        addRegisters("a", 0, 7, 10);
        addRegisters("s", 2, 11, 18);
        addRegisters("t", 3, 6, 28);
        addRegisters("f", 0, 31, 32);
    }

    void setVerbose(bool verbose) {
        this->verbose = verbose;
    }

    void load(const std::string &inPathname) {
        this->inPathname = inPathname;

        // Read the whole assembly file at once.
        std::string in = readFileContents(inPathname);

        // Convert to lines.
        lines.clear();
        const char *s = in.c_str();
        while (true) {
            auto endOfLine = strchr(s, '\n');
            if (endOfLine == nullptr) {
                lines.push_back(s);
                break;
            }
            lines.push_back(std::string(s, endOfLine - s));
            s = endOfLine + 1;
        }
    }

    // Assemble the file to the object file.
    void assemble() {
        // Clear output.
        bin.clear();
        binLine.clear();

        // Process each line.
        for (lineNumber = 0; lineNumber < lines.size(); lineNumber++) {
            parseLine();
        }
    }

    void dumpListing() {
        // Assume that the source and the binary are in the same order.
        std::ios oldState(nullptr);
        oldState.copyfmt(std::cout);

        // Next instruction to display.
        size_t binIndex = 0;

        // We have shown this source line to the user.
        for (size_t sourceLine = 0; sourceLine < lines.size(); sourceLine++) {
            // Next source line to display with an instruction.
            size_t displaySourceLine;

            while (true) {
                // See what source line corresponds to the next instruction to display.
                displaySourceLine = binIndex < bin.size()
                    ? binLine[binIndex]
                    : lines.size();

                // See if previous source line generated multiple instructions.
                if (displaySourceLine < sourceLine) {
                    std::cout
                        << std::hex << std::setw(4) << std::setfill('0') << (binIndex*4)
                        << " "
                        << std::hex << std::setw(8) << std::setfill('0') << bin[binIndex]
                        << "\n";
                    binIndex++;
                } else {
                    // No more catching up to do.
                    break;
                }
            }

            // Found matching source line.
            if (displaySourceLine == sourceLine) {
                std::cout
                    << std::hex << std::setw(4) << std::setfill('0') << (binIndex*4)
                    << " "
                    << std::hex << std::setw(8) << std::setfill('0') << bin[binIndex]
                    << " "
                    << lines[sourceLine] << "\n";
                binIndex++;
            } else {
                // Source line with no instruction. Must be comment, label, blank line, etc.
                std::cout
                    << std::string(14, ' ')
                    << lines[sourceLine] << "\n";
            }
        }

        std::cout.copyfmt(oldState);
    }

    void save(const std::string &outPathname) {
        // Write binary.
        std::ofstream outFile(outPathname, std::ios::out | std::ios::binary);
        if (!outFile.good()) {
            std::cerr << "Can't open file \"" << outPathname << "\".\n";
            exit(EXIT_FAILURE);
        }
        for (uint32_t instruction : bin) {
            // Force little endian.
            outFile
                << uint8_t((instruction >> 0) & 0xFF)
                << uint8_t((instruction >> 8) & 0xFF)
                << uint8_t((instruction >> 16) & 0xFF)
                << uint8_t((instruction >> 24) & 0xFF);
        }
        outFile.close();
    }

private:
    // Add known registers with prefix from "first" to "last" inclusive, starting
    // at physical register "start".
    void addRegisters(const std::string &prefix, int first, int last, int start) {
        for (int i = first; i <= last; i++) {
            std::ostringstream ss;
            ss << prefix << i;
            registers[ss.str()] = i - first + start;
        }
    }

    // Reads one line of input, either one character past
    // the newline, or on the nul if there is no newline.
    void parseLine() {
        s = currentLine().c_str();
        previousToken = nullptr;

        // Start of line. Skip whitespace.
        skipWhitespace();

        // Grab a token. This could be a label or an operator.
        std::string opOrLabel = readToken();

        // See if it's a label.
        if (!opOrLabel.empty()) {
            if (foundChar(':')) {
                if (gLabels.find(opOrLabel) != gLabels.end()) {
                    s = previousToken;
                    std::ostringstream ss;
                    ss << "Label \"" << opOrLabel << "\" is already defined";
                    error(ss.str());
                }

                // It's a label, record it.
                gLabels[opOrLabel] = bin.size()*4;

                // Read the operator.
                opOrLabel = readToken();
            }
        }

        // See if it's an operator.
        if (!opOrLabel.empty()) {
            // Parse parameters.
            auto opItr = operators.find(opOrLabel);
            if (opItr == operators.end()) {
                s = previousToken;
                std::ostringstream ss;
                ss << "Unknown operator \"" << opOrLabel << "\"";
                error(ss.str());
            }
            const Operator &op = opItr->second;

            switch (op.format) {
                case FORMAT_R: {
                    int rd = readRegister("destination");
                    if (!foundChar(',')) {
                        error("Expected comma");
                    }
                    int rs1 = readRegister("source");
                    if (!foundChar(',')) {
                        error("Expected comma");
                    }
                    int rs2 = readRegister("source");
                    emitR(op, rd, rs1, rs2);
                    break;
                }

                case FORMAT_I: {
                    int rd = readRegister("destination");
                    if (!foundChar(',')) {
                        error("Expected comma");
                    }
                    int rs = readRegister("source");
                    if (!foundChar(',')) {
                        error("Expected comma");
                    }
                    uint32_t imm = readImmediate(op.bits);
                    emitI(op, rd, rs, imm);
                    break;
                }

                case FORMAT_S: {
                    int rs2 = readRegister("source");
                    if (!foundChar(',')) {
                        error("Expected comma");
                    }
                    uint32_t imm = readImmediate(op.bits);
                    if (!foundChar('(')) {
                        error("Expected open parenthesis");
                    }
                    int rs1 = readRegister("source");
                    if (!foundChar(')')) {
                        error("Expected close parenthesis");
                    }
                    emitS(op, rs2, imm, rs1);
                    break;
                }

                case FORMAT_U: {
                    int rd = readRegister("destination");
                    if (!foundChar(',')) {
                        error("Expected comma");
                    }
                    uint32_t imm = readImmediate(op.bits);
                    emitU(op, rd, imm);
                    break;
                }

                default: {
                    assert(false);
                }
            }
        }

        // See if there's a comment.
        if (*s == ';') {
            // Skip to end of line.
            while (*s != '\n' && *s != '\0') {
                s++;
            }
        }

        // Make sure the whole line was parsed properly.
        if (*s != '\0') {
            // Unknown error.
            error("Error");
        }
    }

private:
    // Skip non-newline whitespace.
    void skipWhitespace() {
        while (*s == ' ' || *s == '\t' || *s == '\r') {
            s++;
        }
    }

    // Skips a character and subsequent whitespace. Returns whether it found the character.
    bool foundChar(char c) {
        if (*s == c) {
            s++;
            skipWhitespace();
            return true;
        }

        return false;
    }

    // Return the next token, or an empty string if there isn't one.
    // A token is any sequence of alpha-numeric characters, or underscore.
    // Always skips subsequent whitespace.
    std::string readToken() {
        std::string token;

        // Keep track of where we started, for error reporting.
        previousToken = s;

        while (isalnum(*s) || *s == '_') {
            token += *s++;
        }

        skipWhitespace();

        return token;
    }

    // Read an integer immediate value. The immediate must fit in the specified
    // number of bits. Skips subsequent whitespace. The immediate can be in
    // decimal of hex (with a 0x prefix).
    uint32_t readImmediate(int bits) {
        uint32_t imm = 0;
        const char *start = s;

        if (s[0] == '0' && tolower(s[1]) == 'x') {
            // Hex.
            s += 2;
            while (true) {
                char c = tolower(*s);

                uint32_t value;
                if (isdigit(c)) {
                    value = c - '0';
                } else if (c >= 'a' && c <= 'f') {
                    value = c - 'a' + 10;
                } else {
                    break;
                }
                imm = imm*16 + value;

                s++;
            }

            if (s == start + 2) {
                error("Expected immediate");
            }
        } else {
            // Decimal.
            while (isdigit(*s)) {
                imm = imm*10 + (*s - '0');
                s++;
            }

            if (s == start) {
                error("Expected immediate");
            }
        }

        // Make sure we fit in 12 bits.
        if (imm >= (1 << bits)) {
            // Back up over immediate.
            s = start;
            std::ostringstream ss;
            ss << "Immediate " << imm << " (0x"
                << std::hex << imm << std::dec << ") does not fit in "
                << bits << (bits == 1 ? " bit" : " bits");
            error(ss.str());
        }

        skipWhitespace();

        return imm;
    }

    // Error function.
    void error(const std::string &message) {
        int col = s - currentLine().c_str();

        std::cerr << inPathname << ":" << (lineNumber + 1) << ":"
            << (col + 1) << ": " << message << "\n";
        std::cerr << currentLine() << "\n";
        std::cerr << std::string(col, ' ') << "^\n";
        exit(EXIT_FAILURE);
    }

    // Return a reference to the current line.
    const std::string &currentLine() {
        return lines[lineNumber];
    }

    // Read a register name and return its number. Emits an error
    // if the token is missing or is not a register name.
    int readRegister(const std::string &role) {
        std::string regName = readToken();
        if (regName.empty()) {
            std::ostringstream ss;
            ss << "Expected " << role << " register";
            error(ss.str());
        }

        auto reg = registers.find(regName);
        if (reg == registers.end()) {
            s = previousToken;
            std::ostringstream ss;
            ss << "\"" << regName << "\" is not a register name";
            error(ss.str());
        }

        return reg->second;
    }

    // Emit a FORMAT_R instruction.
    void emitR(const Operator &op, int rd, int rs1, int rs2) {
        emit(op.opcode
                | rd << 7
                | op.funct3 << 12
                | rs1 << 15
                | rs2 << 20
                | op.funct7 << 25);
    }

    // Emit a FORMAT_I instruction.
    void emitI(const Operator &op, int rd, int rs, uint32_t imm) {
        emit(op.opcode
                | rd << 7
                | op.funct3 << 12
                | rs << 15
                | imm << 20
                | op.funct7 << 25);
    }

    // Emit a FORMAT_S instruction.
    void emitS(const Operator &op, int rs2, uint32_t imm, int rs1) {
        emit(op.opcode
                | (imm & 0x1F) << 7
                | op.funct3 << 12
                | rs1 << 15
                | rs2 << 20
                | ((imm >> 5) & 0x7F) << 25);
    }

    // Emit a FORMAT_U instruction.
    void emitU(const Operator &op, int rd, uint32_t imm) {
        emit(op.opcode
                | rd << 7
                | imm << 12);
    }

    // Emit an instruction for this source line.
    void emit(uint32_t instruction) {
        bin.push_back(instruction);
        binLine.push_back(lineNumber);
    }
};

// ----------------------------------------------------------------------

void usage(char *progname) {
    std::cerr << "Usage: " << progname << " [options] file.s\n";
    std::cerr << "Options:\n";
    std::cerr << "    -v         verbose output\n";
    std::cerr << "    -o file.o  output object file\n";
}

int main(int argc, char *argv[]) {
    bool verbose = false;
    std::string inPathname;
    std::string outPathname;

    char *progname = argv[0];
    argv++; argc--;

    // Parse parameters.
    while (argc > 0 && argv[0][0] == '-') {
        if (strcmp(argv[0], "-v") == 0) {
            verbose = true;
            argv++; argc--;
        } else if (strcmp(argv[0], "-o") == 0) {
            if(argc < 2) {
                usage(progname);
                exit(EXIT_FAILURE);
            }
            outPathname = argv[1];
            argv += 2; argc -= 2;
        } else {
            usage(progname);
            exit(EXIT_FAILURE);
        }
    }

    if (argc < 1) {
        usage(progname);
        exit(EXIT_FAILURE);
    }

    inPathname = argv[0];

    if (outPathname.empty()) {
        outPathname = stripExtension(inPathname) + ".o";
    }

    Assembler assembler;
    assembler.setVerbose(verbose);
    assembler.load(inPathname);
    assembler.assemble();
    assembler.save(outPathname);
    if (verbose) {
        assembler.dumpListing();
    }
}
