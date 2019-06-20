#include <vector>
#include <array>
#include <fstream>
#include <iostream>
#include <sstream>
#include <iomanip>
#include <cassert>
#include <cstdlib>
#include "gpuemu.h"
#include "timer.h"

extern "C" {
#include "riscv-disas.h"
}

typedef std::map<uint32_t, std::string> AddressToSymbolMap;

void print_inst(uint64_t pc, uint32_t inst, const AddressToSymbolMap& addressesToSymbols)
{
    char buf[80] = { 0 };
    if(addressesToSymbols.find(pc) != addressesToSymbols.end())
        printf("%s:\n", addressesToSymbols.at(pc).c_str());
    disasm_inst(buf, sizeof(buf), rv64, pc, inst);
    printf("        %08" PRIx64 ":  %s\n", pc, buf);
}

    
void dumpGPUCore(const GPUCore& core)
{
    std::cout << std::setfill('0');
    for(int i = 0; i < 32; i++) {
        std::cout << "x" << std::setw(2) << i << ":" << std::hex << std::setw(8) << core.regs.x[i] << std::dec;
        std::cout << (((i + 1) % 4 == 0) ? "\n" : " ");
    }
    for(int i = 0; i < 32; i++) {
        std::cout << "ft" << std::setfill('0') << std::setw(2) << i << ":" << std::setw(8) << std::setfill(' ') << core.regs.f[i];
        std::cout << (((i + 1) % 4 == 0) ? "\n" : " ");
    }
    std::cout << "pc :" << std::hex << std::setw(8) << core.regs.pc << '\n' << std::dec;
    std::cout << std::setfill(' ');
}

void dumpRegsDiff(const GPUCore::Registers& prev, const GPUCore::Registers& cur)
{
    std::cout << std::setfill('0');
    if(prev.pc != cur.pc) {
        std::cout << "pc changed to " << std::hex << std::setw(8) << cur.pc << std::dec << '\n';
    }
    for(int i = 0; i < 32; i++) {
        if(prev.x[i] != cur.x[i]) {
            std::cout << "x" << std::setw(2) << i << " changed to " << std::hex << std::setw(8) << cur.x[i] << std::dec << '\n';
        }
    }
    for(int i = 0; i < 32; i++) {
        bool bothnan = isnan(cur.f[i]) && isnan(prev.f[i]);
        if((prev.f[i] != cur.f[i]) && !bothnan) { // if both NaN, equality test still fails)
            uint32_t x = *reinterpret_cast<const uint32_t*>(&cur.f[i]);
            std::cout << "f" << std::setw(2) << i << " changed to " << std::hex << std::setw(8) << x << std::dec << "(" << cur.f[i] << ")\n";
        }
    }
    std::cout << std::setfill(' ');
}
struct Memory
{
    std::vector<uint8_t> memorybytes;
    bool verbose;
    Memory(const std::vector<uint8_t>& memorybytes_, bool verbose_ = false) :
        memorybytes(memorybytes_),
        verbose(verbose_)
    {}
    uint8_t read8(uint32_t addr)
    {
        if(verbose) printf("read 8 from %08X\n", addr);
        assert(addr < memorybytes.size());
        return memorybytes[addr];
    }
    uint16_t read16(uint32_t addr)
    {
        if(verbose) printf("read 16 from %08X\n", addr);
        assert(addr + 1 < memorybytes.size());
        return *reinterpret_cast<uint16_t*>(memorybytes.data() + addr);
    }
    uint32_t read32(uint32_t addr)
    {
        if(verbose) printf("read 32 from %08X\n", addr);
        assert(addr < memorybytes.size());
        assert(addr + 3 < memorybytes.size());
        return *reinterpret_cast<uint32_t*>(memorybytes.data() + addr);
    }
    float readf(uint32_t addr)
    {
        if(verbose) printf("read float from %08X\n", addr);
        assert(addr < memorybytes.size());
        assert(addr + 3 < memorybytes.size());
        return *reinterpret_cast<float*>(memorybytes.data() + addr);
    }
    void write8(uint32_t addr, uint32_t v)
    {
        if(verbose) printf("write 8 bits of %08X to %08X\n", v, addr);
        assert(addr < memorybytes.size());
        memorybytes[addr] = static_cast<uint8_t>(v);
    }
    void write16(uint32_t addr, uint32_t v)
    {
        if(verbose) printf("write 16 bits of %08X to %08X\n", v, addr);
        assert(addr + 1 < memorybytes.size());
        *reinterpret_cast<uint16_t*>(memorybytes.data() + addr) = static_cast<uint16_t>(v);
    }
    void write32(uint32_t addr, uint32_t v)
    {
        if(verbose) printf("write 32 bits of %08X to %08X\n", v, addr);
        assert(addr < memorybytes.size());
        assert(addr + 3 < memorybytes.size());
        *reinterpret_cast<uint32_t*>(memorybytes.data() + addr) = v;
    }
    void writef(uint32_t addr, float v)
    {
        if(verbose) printf("write float %f to %08X\n", v, addr);
        assert(addr < memorybytes.size());
        assert(addr + 3 < memorybytes.size());
        *reinterpret_cast<float*>(memorybytes.data() + addr) = v;
    }
};

bool ReadBinary(std::ifstream& binaryFile, RunHeader1& header, SymbolTable& symbols, std::vector<uint8_t>& bytes)
{
    binaryFile.read(reinterpret_cast<char*>(&header), sizeof(header));
    if(!binaryFile) {
        std::cerr << "failed to read header, only " << binaryFile.gcount() << " bytes read\n";
        return false;
    }

    if(header.magic != RunHeader1MagicExpected) {
        std::cerr << "magic read did not match magic expected for RunHeader1\n";
        return false;
    }

    for(int i = 0; i < header.symbolCount; i++) {
        uint32_t addressAndLength[2];

        binaryFile.read(reinterpret_cast<char*>(addressAndLength), sizeof(addressAndLength));
        if(!binaryFile) {
            std::cerr << "failed to read address and length for symbol " << i << " only " << binaryFile.gcount() << " bytes read\n";
            return false;
        }

        std::string symbol;
        symbol.resize(addressAndLength[1] - 1);

        binaryFile.read(symbol.data(), addressAndLength[1]);
        if(!binaryFile) {
            std::cerr << "failed to symbol string for symbol " << i << " only " << binaryFile.gcount() << " bytes read\n";
            return false;
        }

        symbols[symbol] = addressAndLength[0];
    }

    std::ifstream::pos_type bytesStart = binaryFile.tellg();
    binaryFile.seekg(0, std::ios::end);
    std::ifstream::pos_type bytesEnd = binaryFile.tellg();
    binaryFile.seekg(bytesStart, std::ios::beg);
    bytes.resize(bytesEnd - bytesStart);
    binaryFile.read(reinterpret_cast<char*>(bytes.data()), bytesEnd - bytesStart);

    return true;
}

template <class MEMORY, class TYPE>
void set(MEMORY& memory, uint32_t address, const TYPE& value)
{
    static_assert(sizeof(TYPE) == sizeof(uint32_t));
    memory.write32(address, *reinterpret_cast<const uint32_t*>(&value));
}

template <class MEMORY, class TYPE, unsigned long N>
void set(MEMORY& memory, uint32_t address, const std::array<TYPE, N>& value)
{
    static_assert(sizeof(TYPE) == sizeof(uint32_t));
    for(int i = 0; i < N; i++)
        memory.write32(address + i * sizeof(uint32_t), *reinterpret_cast<const uint32_t*>(&value[i]));
}

typedef std::array<float,1> v1float;
typedef std::array<uint32_t,1> v1uint;
typedef std::array<int32_t,1> v1int;
typedef std::array<float,2> v2float;
typedef std::array<uint32_t,2> v2uint;
typedef std::array<int32_t,2> v2int;
typedef std::array<float,3> v3float;
typedef std::array<uint32_t,3> v3uint;
typedef std::array<int32_t,3> v3int;
typedef std::array<float,4> v4float;
typedef std::array<uint32_t,4> v4uint;
typedef std::array<int32_t,4> v4int;

// https://stackoverflow.com/a/34571089/211234
static const char *BASE64_ALPHABET = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
std::string base64Encode(const std::string &in) {
    std::string out;
    int val = 0;
    int valb = -6;

    for (uint8_t c : in) {
        val = (val << 8) + c;
        valb += 8;
        while (valb >= 0) {
            out.push_back(BASE64_ALPHABET[(val >> valb) & 0x3F]);
            valb -= 6;
        }
    }
    if (valb > -6) {
        out.push_back(BASE64_ALPHABET[((val << 8) >> (valb + 8)) & 0x3F]);
    }
    while (out.size() % 4 != 0) {
        out.push_back('=');
    }

    return out;
}

void usage(const char* progname)
{
    printf("usage: %s [options] shader.blob\n", progname);
    printf("options:\n");
    printf("\t-f N      Render frame N\n");
    printf("\t-v        Print memory access\n");
    printf("\t-S        show the disassembly of the SPIR-V code\n");
    printf("\t--term    draw output image on terminal (in addition to file)\n");
}


int main(int argc, char **argv)
{
    bool verboseMemory = false;
    bool disassemble = false;
    bool printCoreDiff = false;
    bool imageToTerminal = false;
    bool printSymbols = false;
    float frameTime = 1.5f;

    char *progname = argv[0];
    argv++; argc--;

    while(argc > 0 && argv[0][0] == '-') {
        if(strcmp(argv[0], "-v") == 0) {

            verboseMemory = true;
            argv++; argc--;

        } else if(strcmp(argv[0], "--diff") == 0) {

            printCoreDiff = true;
            argv++; argc--;

        } else if(strcmp(argv[0], "--term") == 0) {

            imageToTerminal = true;
            argv++; argc--;

        } else if(strcmp(argv[0], "-S") == 0) {

            disassemble = true;
            argv++; argc--;

        } else if(strcmp(argv[0], "-f") == 0) {

            if(argc < 2) {
                std::cerr << "Expected frame parameter for \"-f\"\n";
                usage(progname);
                exit(EXIT_FAILURE);
            }
            frameTime = atoi(argv[1]) / 60.0f;
            argv+=2; argc-=2;

        } else if(strcmp(argv[0], "-d") == 0) {

            printSymbols = true;
            argv++; argc--;

        } else if(strcmp(argv[0], "-h") == 0) {

            usage(progname);
            exit(EXIT_SUCCESS);

        } else {

            usage(progname);
            exit(EXIT_FAILURE);

        }
    }

    if(argc < 1) {
        usage(progname);
        exit(EXIT_FAILURE);
    }

    RunHeader1 header;
    SymbolTable symbols;
    std::vector<uint8_t> bytes;

    std::ifstream binaryFile(argv[0], std::ios::in | std::ios::binary);
    if(!binaryFile.good()) {
        throw std::runtime_error(std::string("couldn't open file ") + argv[0] + " for reading");
    }

    if(!ReadBinary(binaryFile, header, symbols, bytes)) {
        exit(EXIT_FAILURE);
    }

    if(printSymbols) {
        for(auto& [symbol, address]: symbols) {
            std::cout << "symbol \"" << symbol << "\" is at " << address << "\n";
        }
    }

    binaryFile.close();

    AddressToSymbolMap addressesToSymbols;
    for(auto& [symbol, address]: symbols)
        addressesToSymbols[address] = symbol;

    bytes.resize(bytes.size() + 0x10000);
    Memory m(bytes, false);

    GPUCore core(symbols);
    GPUCore::Status status;

    int imageWidth = 320;
    int imageHeight = 180;

    float fw = imageWidth;
    float fh = imageHeight;
    // float zero = 0.0;
    float one = 1.0;
    for(auto& s: { "gl_FragCoord", "color"}) {
        if (symbols.find(s) == symbols.end()) {
            std::cerr << "No memory location for required variable " << s << ".\n";
            exit(EXIT_FAILURE);
        }
    }
    for(auto& s: { "iResolution", "iTime", "iMouse"}) {
        if (symbols.find(s) == symbols.end()) {
            // Don't warn, these are in the anonymous params.
            // std::cerr << "Warning: No memory location for variable " << s << ".\n";
        }
    }

    if (symbols.find(".anonymous") != symbols.end()) {
        set(m, symbols[".anonymous"], v3float{fw, fh, one});
        set(m, symbols[".anonymous"] + sizeof(v3float), frameTime);
    }
    unsigned char *img = new unsigned char[imageWidth * imageHeight * 3];

    uint32_t gl_FragCoordAddress = symbols["gl_FragCoord"];
    uint32_t colorAddress = symbols["color"];
    uint32_t iTimeAddress = symbols["iTime"];

    Timer frameElapsed;
    uint64_t dispatchedCount = 0;
    for(int j = 0; j < imageHeight; j++)
        for(int i = 0; i < imageWidth; i++) {

            set(m, gl_FragCoordAddress, v4float{(float)i, (float)j, 0, 0});
            set(m, colorAddress, v4float{1, 1, 1, 1});
            if(false) // XXX TODO: when iTime is exported by compilation
                set(m, iTimeAddress, frameTime);

            core.regs.x[1] = 0xffffffff; // Set PC to unlikely value to catch ret with no caller
            core.regs.x[2] = bytes.size(); // Set SP to end of memory 
            core.regs.pc = header.initialPC;

            if(disassemble) {
                std::cout << "; pixel " << i << ", " << j << '\n';
            }
            try {
                do {
                    GPUCore::Registers oldRegs = core.regs;
                    if(disassemble) {
                        print_inst(core.regs.pc, m.read32(core.regs.pc), addressesToSymbols);
                    }
                    m.verbose = verboseMemory;
                    status = core.step(m);
                    dispatchedCount ++;
                    m.verbose = false;
                    if(printCoreDiff) {
                        dumpRegsDiff(oldRegs, core.regs);
                    }
                } while(core.regs.pc != 0xffffffff && status == GPUCore::RUNNING);
            } catch(const std::exception& e) {
                std::cerr << e.what() << '\n';
                dumpGPUCore(core);
                exit(EXIT_FAILURE);
            }

            if(status != GPUCore::BREAK) {
                std::cerr << "unexpected core step result " << status << '\n';
                dumpGPUCore(core);
                exit(EXIT_FAILURE);
            }

            uint32_t ir = m.read32(symbols["color"] +  0);
            uint32_t ig = m.read32(symbols["color"] +  4);
            uint32_t ib = m.read32(symbols["color"] +  8);
            // uint32_t ia = m.read32(symbols["color"] + 12);
            float r = *reinterpret_cast<float*>(&ir);
            float g = *reinterpret_cast<float*>(&ig);
            float b = *reinterpret_cast<float*>(&ib);
            // float a = *reinterpret_cast<float*>(&ia);
            img[3 * ((imageHeight - 1 - j) * imageWidth + i) + 0] = std::clamp(int(r * 255.99), 0, 255);
            img[3 * ((imageHeight - 1 - j) * imageWidth + i) + 1] = std::clamp(int(g * 255.99), 0, 255);
            img[3 * ((imageHeight - 1 - j) * imageWidth + i) + 2] = std::clamp(int(b * 255.99), 0, 255);
        }
    std::cout << "shading took " << frameElapsed.elapsed() << " seconds.\n";
    std::cout << dispatchedCount << " instructions executed.\n";
    std::cout << (50000000.0f / dispatchedCount) << " fps estimated at 50 MHz.\n";

    FILE *fp = fopen("emulated.ppm", "wb");
    fprintf(fp, "P6 %d %d 255\n", imageWidth, imageHeight);
    fwrite(img, 1, imageWidth * imageHeight * 3, fp);
    fclose(fp);

    if (imageToTerminal) {
        // https://www.iterm2.com/documentation-images.html
        std::ostringstream ss;
        ss << "P6 " << imageWidth << " " << imageHeight << " 255\n";
        ss.write(reinterpret_cast<char *>(img), 3*imageWidth*imageHeight);
        std::cout << "\033]1337;File=width="
            << imageWidth << "px;height="
            << imageHeight << "px;inline=1:"
            << base64Encode(ss.str()) << "\007\n";
    }
}
