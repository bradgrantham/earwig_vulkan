#include <iostream>
#include <cstdio>
#include <fstream>
#include <variant>
#include <chrono>
#include <thread>
#include <algorithm>
#include <atomic>
#include <sstream>
#include <iomanip>
#include <memory>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef USE_CPP17_FILESYSTEM
// filesystem still not available in XCode 2019/04/04
#include <filesystem>
#else
#include <libgen.h>
#endif

#include <StandAlone/ResourceLimits.h>
#include <glslang/MachineIndependent/localintermediate.h>
#include <glslang/Public/ShaderLang.h>
#include <glslang/Include/intermediate.h>
#include <SPIRV/GlslangToSpv.h>
#include <SPIRV/disassemble.h>
#include <spirv-tools/libspirv.h>
#include "spirv.h"
#include "GLSL.std.450.h"
#include "json.hpp"
#include "basic_types.h"
#include "interpreter.h"

struct Image
{
    enum Format {
        // Matching Vulkan 1.0
        UNDEFINED = 0,
        FORMAT_R8G8B8_UNORM = 23,
        FORMAT_R8G8B8A8_UNORM = 37,
        FORMAT_R32G32B32_SFLOAT = 106,
        FORMAT_R32G32B32A32_SFLOAT = 109
    };

    static size_t getPixelSize(Format fmt)
    {
        switch(fmt) {
            case FORMAT_R8G8B8_UNORM: return 3; break;
            case FORMAT_R8G8B8A8_UNORM: return 4; break;
            case FORMAT_R32G32B32_SFLOAT: return 12; break;
            case FORMAT_R32G32B32A32_SFLOAT: return 16; break;

            default:
            case UNDEFINED:
                throw std::runtime_error("unimplemented image format " + std::to_string(fmt));
                break;
        };
    };

    enum Dim {
        // Matching Vulkan 1.0
        DIM_1D = 0,
        DIM_2D = 1,
        DIM_3D = 2,
        DIM_CUBE = 3
    };

    Format format;
    size_t pixelSize;
    Dim dim;
    uint32_t width, height, depth, slices;
    unsigned char *storage;

    unsigned char *getPixelAddress(int s, int t, int r, int q)
    {
        return storage + (q * depth * width * height + r * width * height + t * width + s) * pixelSize;
    }
    unsigned char *getPixelAddress(int s, int t, int r)
    {
        return storage + (r * width * height + t * width + s) * pixelSize;
    }
    unsigned char *getPixelAddress(int s, int t)
    {
        return storage + (t * width + s) * pixelSize;
    }

    Image() :
        format(UNDEFINED),
        pixelSize(0),
        dim(DIM_2D),
        storage(nullptr)
    {}
    Image(Format format_, Dim dim_, uint32_t w_) {}
    Image(Format format_, Dim dim_, uint32_t w_, uint32_t h_, uint32_t d_) {}
    Image(Format format_, Dim dim_, uint32_t w_, uint32_t h_) :
        format(format_),
        pixelSize(getPixelSize(format_)),
        dim(dim_),
        width(w_),
        height(h_),
        depth(1),
        slices(1),
        storage(new unsigned char [width * height * depth * slices * pixelSize])
    {
        assert(dim == DIM_2D);
    }
    ~Image()
    {
        delete[] storage;
    }

#if 0
    static get(const unsigned char *pixel, const v4float& v)
    {
        switch(format) {
            case FORMAT_R8G8U8_UNORM: {
                for(int c = 0; c < 3; c++) {
                    v[0] = pixel[c] / 255.99;
                }
                v[3] = 1.0;
                break;
            }
            default: {
                throw std::runtime_error("get() : unimplemented image format " + std::to_string(fmt));
                break; // not reached
            }
        }
    }
#endif

    // There's probably a clever C++ way to do this with variadic templates...
    // void setPixel(int s, int t, int r, int q, const v4float& v) {}
    // void setPixel(int s, int t, int r,  const v4float& v) {}
    void set(int s, int t, const v4float& v)
    {
        assert((s >= 0) || (t >= 0) || (s < width) || (t < height));
        assert(dim == DIM_2D);
        set(getPixelAddress(s, t), v);
    }
    // void get(int s, int t, int r, int q, v4float& v) {}
    // void get(int s, int t, int r, v4float& v) {}
    // void get(int s, int t, v4float& v) {}

    // implement first only rgb, rgba and f32 and ub8
    // Read(filename, format); // XXX should construct an image with this and use move semantics
    // Write(filename);

private:
    void set(unsigned char *pixel, const v4float& v)
    {
        switch(format) {
            case FORMAT_R8G8B8_UNORM: {
                for(int c = 0; c < 3; c++) {
                    // ShaderToy clamps the color.
                    pixel[c] = std::clamp(int(v[c]*255.99), 0, 255);
                }
                break;
            }
            default: {
                throw std::runtime_error("set() : unimplemented image format " + std::to_string(format));
                break; // not reached
            }
        }
    }
};

static void skipComments(FILE *fp, char ***comments, size_t *commentCount)
{
    int c;
    char line[512];

    while((c = fgetc(fp)) == '#') {
        fgets(line, sizeof(line) - 1, fp);
	line[strlen(line) - 1] = '\0';
	if(comments != NULL) {
	    *comments = (char **)
	        realloc(*comments, sizeof(char *) * (*commentCount + 1));
	    (*comments)[*commentCount] = strdup(line);
	}
	if(commentCount != NULL) {
	    (*commentCount) ++;
	}
    }
    ungetc(c, fp);
}


int pnmRead(FILE *file, unsigned int *w, unsigned int *h, float **pixels,
    char ***comments, size_t *commentCount)
{
    unsigned char	dummyByte;
    int			i;
    float		max;
    char		token;
    int			width, height;
    float		*rgbPixels;

    if(commentCount != NULL)
	*commentCount = 0;
    if(comments != NULL)
	*comments = NULL;

    fscanf(file, " ");

    skipComments(file, comments, commentCount);

    if(fscanf(file, "P%c ", &token) != 1) {
         fprintf(stderr, "pnmRead: Had trouble reading PNM tag\n");
	 return 0;
    }

    skipComments(file, comments, commentCount);

    if(fscanf(file, "%d ", &width) != 1) {
         fprintf(stderr, "pnmRead: Had trouble reading PNM width\n");
	 return 0;
    }

    skipComments(file, comments, commentCount);

    if(fscanf(file, "%d ", &height) != 1) {
         fprintf(stderr, "pnmRead: Had trouble reading PNM height\n");
	 return 0;
    }

    skipComments(file, comments, commentCount);

    if(token != '1' && token != '4')
        if(fscanf(file, "%f", &max) != 1) {
             fprintf(stderr, "pnmRead: Had trouble reading PNM max value\n");
	     return 0;
        }

    rgbPixels = (float*) malloc(width * height * 4 * sizeof(float));
    if(rgbPixels == NULL) {
         fprintf(stderr, "pnmRead: Couldn't allocate %zd bytes\n",
	     width * height * 4 * sizeof(float));
         fprintf(stderr, "pnmRead: (For a %u by %u image)\n", width,
	     height);
	 return 0;
    }

    if(token != '4')
	skipComments(file, comments, commentCount);

    if(token != '4')
    fread(&dummyByte, 1, 1, file);	/* chuck white space */

    if(token == '1') {
	for(i = 0; i < width * height; i++) {
	    int pixel;
	    fscanf(file, "%d", &pixel);
	    pixel = 1 - pixel;
	    rgbPixels[i * 4 + 0] = pixel;
	    rgbPixels[i * 4 + 1] = pixel;
	    rgbPixels[i * 4 + 2] = pixel;
	    rgbPixels[i * 4 + 3] = 1.0;
	}
    } else if(token == '2') {
	for(i = 0; i < width * height; i++) {
	    int pixel;
	    fscanf(file, "%d", &pixel);
	    rgbPixels[i * 4 + 0] = pixel / max;
	    rgbPixels[i * 4 + 1] = pixel / max;
	    rgbPixels[i * 4 + 2] = pixel / max;
	    rgbPixels[i * 4 + 3] = 1.0;
	}
    } else if(token == '3') {
	for(i = 0; i < width * height; i++) {
	    int r, g, b;
	    fscanf(file, "%d %d %d", &r, &g, &b);
	    rgbPixels[i * 4 + 0] = r / max;
	    rgbPixels[i * 4 + 1] = g / max;
	    rgbPixels[i * 4 + 2] = b / max;
	    rgbPixels[i * 4 + 3] = 1.0;
	}
    } else if(token == '4') {
        int bitnum = 0;
        unsigned char value = 0;

	for(i = 0; i < width * height; i++) {
	    unsigned char pixel;

	    if(bitnum == 0)
	        fread(&value, 1, 1, file);

	    pixel = (1 - ((value >> (7 - bitnum)) & 1));
	    rgbPixels[i * 4 + 0] = pixel;
	    rgbPixels[i * 4 + 1] = pixel;
	    rgbPixels[i * 4 + 2] = pixel;
	    rgbPixels[i * 4 + 3] = 1.0;

	    if(++bitnum == 8 || ((i + 1) % width) == 0)
	        bitnum = 0;
	}
    } else if(token == '5') {
	for(i = 0; i < width * height; i++) {
	    unsigned char pixel;
	    fread(&pixel, 1, 1, file);
	    rgbPixels[i * 4 + 0] = pixel / max;
	    rgbPixels[i * 4 + 1] = pixel / max;
	    rgbPixels[i * 4 + 2] = pixel / max;
	    rgbPixels[i * 4 + 3] = 1.0;
	}
    } else if(token == '6') {
	for(i = 0; i < width * height; i++) {
	    unsigned char rgb[3];
	    fread(rgb, 3, 1, file);
	    rgbPixels[i * 4 + 0] = rgb[0] / max;
	    rgbPixels[i * 4 + 1] = rgb[1] / max;
	    rgbPixels[i * 4 + 2] = rgb[2] / max;
	    rgbPixels[i * 4 + 3] = 1.0;
	}
    }
    *w = width;
    *h = height;
    *pixels = rgbPixels;
    return 1;
}

using json = nlohmann::json;

std::map<uint32_t, std::string> OpcodeToString = {
#include "opcode_to_string.h"
};

std::map<uint32_t, std::string> GLSLstd450OpcodeToString = {
#include "GLSLstd450_opcode_to_string.h"
};

#include "opcode_structs.h"

const uint32_t NO_MEMORY_ACCESS_SEMANTIC = 0xFFFFFFFF;

const uint32_t SOURCE_NO_FILE = 0xFFFFFFFF;
const uint32_t NO_INITIALIZER = 0xFFFFFFFF;
const uint32_t NO_ACCESS_QUALIFIER = 0xFFFFFFFF;
const uint32_t EXECUTION_ENDED = 0xFFFFFFFF;
const uint32_t NO_RETURN_REGISTER = 0xFFFFFFFF;

// Section of memory for a specific use.
struct MemoryRegion
{
    // Offset within the "memory" array.
    size_t base;

    // Size of the region.
    size_t size;

    // Offset within the "memory" array of next allocation.
    size_t top;

    MemoryRegion() :
        base(0),
        size(0),
        top(0)
    {}
    MemoryRegion(size_t base_, size_t size_) :
        base(base_),
        size(size_),
        top(base_)
    {}
};

// helper type for visitor
template<class... Ts> struct overloaded : Ts... { using Ts::operator()...; };
template<class... Ts> overloaded(Ts...) -> overloaded<Ts...>;

template <class T>
T& objectAt(unsigned char* data)
{
    return *reinterpret_cast<T*>(data);
}

// The static state of the program.
struct Program 
{
    bool throwOnUnimplemented;
    bool hasUnimplemented;
    bool verbose;
    std::set<uint32_t> capabilities;

    // main id-to-thingie map containing extinstsets, types, variables, etc
    // secondary maps of entryPoint, decorations, names, etc

    std::map<uint32_t, std::string> extInstSets;
    uint32_t ExtInstGLSL_std_450_id;
    std::map<uint32_t, std::string> strings;
    uint32_t memoryModel;
    uint32_t addressingModel;
    std::map<uint32_t, EntryPoint> entryPoints;
    std::map<uint32_t, Decoration> decorations;
    std::map<uint32_t, std::string> names;
    std::vector<Source> sources;
    std::vector<std::string> processes;
    std::map<uint32_t, std::map<uint32_t, std::string> > memberNames;
    std::map<uint32_t, std::map<uint32_t, Decoration> > memberDecorations;
    std::map<uint32_t, Type> types;
    std::map<uint32_t, size_t> typeSizes; // XXX put into Type
    std::map<uint32_t, Variable> variables;
    std::map<uint32_t, Function> functions;
    std::vector<std::unique_ptr<Instruction>> instructions;
    // Map from label ID to block object.
    std::map<uint32_t, std::unique_ptr<Block>> blocks;
    // Map from virtual register ID to physical register ID.
    std::map<uint32_t, uint32_t> virtToPhy;

    Function* currentFunction;
    // Map from label ID to index into instructions vector.
    std::map<uint32_t, uint32_t> labels;
    Function* mainFunction; 

    size_t memorySize;

    std::map<uint32_t, MemoryRegion> memoryRegions;

    std::map<uint32_t, uint32_t> resultsCreated;
    std::map<uint32_t, Register> constants;
    Register& allocConstantObject(uint32_t id, uint32_t type)
    {
        Register r {type, typeSizes[type]};
        constants[id] = r;
        return constants[id];
    }

    Program(bool throwOnUnimplemented_, bool verbose_) :
        throwOnUnimplemented(throwOnUnimplemented_),
        hasUnimplemented(false),
        verbose(verbose_),
        currentFunction(nullptr)
    {
        memorySize = 0;
        auto anotherRegion = [this](size_t size){MemoryRegion r(memorySize, size); memorySize += size; return r;};
        memoryRegions[SpvStorageClassUniformConstant] = anotherRegion(1024);
        memoryRegions[SpvStorageClassInput] = anotherRegion(1024);
        memoryRegions[SpvStorageClassUniform] = anotherRegion(1024);
        memoryRegions[SpvStorageClassOutput] = anotherRegion(1024);
        memoryRegions[SpvStorageClassWorkgroup] = anotherRegion(0);
        memoryRegions[SpvStorageClassCrossWorkgroup] = anotherRegion(0);
        memoryRegions[SpvStorageClassPrivate] = anotherRegion(65536); // XXX still needed?
        memoryRegions[SpvStorageClassFunction] = anotherRegion(16384);
        memoryRegions[SpvStorageClassGeneric] = anotherRegion(0);
        memoryRegions[SpvStorageClassPushConstant] = anotherRegion(0);
        memoryRegions[SpvStorageClassAtomicCounter] = anotherRegion(0);
        memoryRegions[SpvStorageClassImage] = anotherRegion(0);
        memoryRegions[SpvStorageClassStorageBuffer] = anotherRegion(0);
    }

    size_t allocate(SpvStorageClass clss, uint32_t type)
    {
        MemoryRegion& reg = memoryRegions[clss];
        if(false) {
            std::cout << "allocate from " << clss << " type " << type << "\n";
            std::cout << "region at " << reg.base << " size " << reg.size << " top " << reg.top << "\n";
            std::cout << "object is size " << typeSizes[type] << "\n";
        }
        assert(reg.top + typeSizes[type] <= reg.base + reg.size);
        size_t offset = reg.top;
        reg.top += typeSizes[type];
        return offset;
    }
    size_t allocate(uint32_t clss, uint32_t type)
    {
        return allocate(static_cast<SpvStorageClass>(clss), type);
    }

    // Returns the type of the member of "t" at index "i". For vectors,
    // "i" is ignored and the type of any element is returned. For matrices,
    // "i" is ignored and the type of any column is returned. For structs,
    // the type of field "i" (0-indexed) is returned.
    uint32_t getConstituentType(uint32_t t, int i) const
    {
        const Type& type = types.at(t);
        if(std::holds_alternative<TypeVector>(type)) {
            return std::get<TypeVector>(type).type;
        } else if(std::holds_alternative<TypeMatrix>(type)) {
            return std::get<TypeMatrix>(type).columnType;
        } else if (std::holds_alternative<TypeStruct>(type)) {
            return std::get<TypeStruct>(type).memberTypes[i];
        // XXX } else if (std::holds_alternative<TypeArray>(type)) {
        } else {
            std::cout << type.index() << "\n";
            assert(false && "getConstituentType of invalid type?!");
        }
        return 0; // not reached
    }

    void dumpTypeAt(const Type& type, unsigned char *ptr) const
    {
        std::visit(overloaded {
            [&](const TypeVoid& type) { std::cout << "{}"; },
            [&](const TypeBool& type) { std::cout << objectAt<bool>(ptr); },
            [&](const TypeFloat& type) { std::cout << objectAt<float>(ptr); },
            [&](const TypeInt& type) {
                if(type.signedness) {
                    std::cout << objectAt<int32_t>(ptr);
                } else {
                    std::cout << objectAt<uint32_t>(ptr);
                }
            },
            [&](const TypePointer& type) { std::cout << "(ptr)" << objectAt<uint32_t>(ptr); },
            [&](const TypeFunction& type) { std::cout << "function"; },
            [&](const TypeImage& type) { std::cout << "image"; },
            [&](const TypeSampledImage& type) { std::cout << "sampledimage"; },
            [&](const TypeVector& type) {
                std::cout << "<";
                for(int i = 0; i < type.count; i++) {
                    dumpTypeAt(types.at(type.type), ptr);
                    ptr += typeSizes.at(type.type);
                    if(i < type.count - 1)
                        std::cout << ", ";
                }
                std::cout << ">";
            },
            [&](const TypeMatrix& type) {
                std::cout << "<";
                for(int i = 0; i < type.columnCount; i++) {
                    dumpTypeAt(types.at(type.columnType), ptr);
                    ptr += typeSizes.at(type.columnType);
                    if(i < type.columnCount - 1)
                        std::cout << ", ";
                }
                std::cout << ">";
            },
            [&](const TypeStruct& type) {
                std::cout << "{";
                for(int i = 0; i < type.memberTypes.size(); i++) {
                    dumpTypeAt(types.at(type.memberTypes[i]), ptr);
                    ptr += typeSizes.at(type.memberTypes[i]);
                    if(i < type.memberTypes.size() - 1)
                        std::cout << ", ";
                }
                std::cout << "}";
            },
        }, type);
    }

    static spv_result_t handleHeader(void* user_data, spv_endianness_t endian,
                               uint32_t /* magic */, uint32_t version,
                               uint32_t generator, uint32_t id_bound,
                               uint32_t schema)
    {
        // auto pgm = static_cast<Program*>(user_data);
        return SPV_SUCCESS;
    }

    static spv_result_t handleInstruction(void* user_data, const spv_parsed_instruction_t* insn)
    {
        auto pgm = static_cast<Program*>(user_data);

        auto opds = insn->operands;

        int which = 0;

        // Read the next uint32_t.
        auto nextu = [insn, opds, &which](uint32_t deflt = 0xFFFFFFFF) {
            return (which < insn->num_operands) ? insn->words[opds[which++].offset] : deflt;
        };

        // Read the next string.
        auto nexts = [insn, opds, &which]() {
            const char *s = reinterpret_cast<const char *>(&insn->words[opds[which].offset]);
            which++;
            return s;
        };

        // Read the next vector.
        auto nextv = [insn, opds, &which]() {
            std::vector<uint32_t> v;
            if(which < insn->num_operands) {
                v = std::vector<uint32_t> {&insn->words[opds[which].offset], &insn->words[opds[which].offset] + opds[which].num_words};
            } else {
                v = {};
            }
            which++; // XXX advance by opds[which].num_words? And move up into the "if"?
            return v;
        };

        // Read the rest of the operands as uint32_t.
        auto restv = [insn, opds, &which]() {
            std::vector<uint32_t> v;
            while(which < insn->num_operands) {
                v.push_back(insn->words[opds[which++].offset]);
            }
            return v;
        };

        switch(insn->opcode) {

            case SpvOpCapability: {
                uint32_t cap = nextu();
                assert(cap == SpvCapabilityShader);
                pgm->capabilities.insert(cap);
                if(pgm->verbose) {
                    std::cout << "OpCapability " << cap << " \n";
                }
                break;
            }

            case SpvOpExtInstImport: {
                // XXX result id
                uint32_t id = nextu();
                const char *name = nexts();
                if(strcmp(name, "GLSL.std.450") == 0) {
                    pgm->ExtInstGLSL_std_450_id = id;
                } else {
                    throw std::runtime_error("unimplemented extension instruction set \"" + std::string(name) + "\"");
                }
                pgm->extInstSets[id] = name;
                if(pgm->verbose) {
                    std::cout << "OpExtInstImport " << insn->words[opds[0].offset] << " " << name << "\n";
                }
                break;
            }

            case SpvOpMemoryModel: {
                pgm->addressingModel = nextu();
                pgm->memoryModel = nextu();
                assert(pgm->addressingModel == SpvAddressingModelLogical);
                assert(pgm->memoryModel == SpvMemoryModelGLSL450);
                if(pgm->verbose) {
                    std::cout << "OpMemoryModel " << pgm->addressingModel << " " << pgm->memoryModel << "\n";
                }
                break;
            }

            case SpvOpEntryPoint: {
                // XXX not result id but id must eventually be Function result id
                uint32_t executionModel = nextu();
                uint32_t id = nextu();
                std::string name = nexts();
                std::vector<uint32_t> interfaceIds = restv();
                assert(executionModel == SpvExecutionModelFragment);
                pgm->entryPoints[id] = {executionModel, name, interfaceIds};
                if(pgm->verbose) {
                    std::cout << "OpEntryPoint " << executionModel << " " << id << " " << name;
                    for(auto& i: interfaceIds)
                        std::cout << " " << i;
                    std::cout << "\n";
                }
                break;
            }

            case SpvOpExecutionMode: {
                uint32_t entryPointId = nextu();
                uint32_t executionMode = nextu();
                std::vector<uint32_t> operands = nextv();
                pgm->entryPoints[entryPointId].executionModesToOperands[executionMode] = operands;

                if(pgm->verbose) {
                    std::cout << "OpExecutionMode " << entryPointId << " " << executionMode;
                    for(auto& i: pgm->entryPoints[entryPointId].executionModesToOperands[executionMode])
                        std::cout << " " << i;
                    std::cout << "\n";
                }
                break;
            }

            case SpvOpString: {
                // XXX result id
                uint32_t id = nextu();
                std::string name = nexts();
                pgm->strings[id] = name;
                if(pgm->verbose) {
                    std::cout << "OpString " << id << " " << name << "\n";
                }
                break;
            }

            case SpvOpName: {
                uint32_t id = nextu();
                std::string name = nexts();
                pgm->names[id] = name;
                if(pgm->verbose) {
                    std::cout << "OpName " << id << " " << name << "\n";
                }
                break;
            }

            case SpvOpSource: {
                uint32_t language = nextu();
                uint32_t version = nextu();
                uint32_t file = nextu(SOURCE_NO_FILE);
                std::string source = (insn->num_operands > 3) ? nexts() : "";
                pgm->sources.push_back({language, version, file, source});
                if(pgm->verbose) {
                    std::cout << "OpSource " << language << " " << version << " " << file << " " << ((source.size() > 0) ? "with source" : "without source") << "\n";
                }
                break;
            }

            case SpvOpMemberName: {
                uint32_t type = nextu();
                uint32_t member = nextu();
                std::string name = nexts();
                pgm->memberNames[type][member] = name;
                if(pgm->verbose) {
                    std::cout << "OpMemberName " << type << " " << member << " " << name << "\n";
                }
                break;
            }

            case SpvOpModuleProcessed: {
                std::string process = nexts();
                pgm->processes.push_back(process);
                if(pgm->verbose) {
                    std::cout << "OpModulesProcessed " << process << "\n";
                }
                break;
            }

            case SpvOpDecorate: {
                uint32_t id = nextu();
                uint32_t decoration = nextu();
                std::vector<uint32_t> operands = nextv();
                pgm->decorations[id] = {decoration, operands};
                if(pgm->verbose) {
                    std::cout << "OpDecorate " << id << " " << decoration;
                    for(auto& i: pgm->decorations[id].operands)
                        std::cout << " " << i;
                    std::cout << "\n";
                }
                break;
            }

            case SpvOpMemberDecorate: {
                uint32_t id = nextu();
                uint32_t member = nextu();
                uint32_t decoration = nextu();
                std::vector<uint32_t> operands = nextv();
                pgm->memberDecorations[id][member] = {decoration, operands};
                if(pgm->verbose) {
                    std::cout << "OpMemberDecorate " << id << " " << member << " " << decoration;
                    for(auto& i: pgm->memberDecorations[id][member].operands)
                        std::cout << " " << i;
                    std::cout << "\n";
                }
                break;
            }

            case SpvOpTypeVoid: {
                // XXX result id
                uint32_t id = nextu();
                pgm->types[id] = TypeVoid {};
                pgm->typeSizes[id] = 0;
                if(pgm->verbose) {
                    std::cout << "TypeVoid " << id << "\n";
                }
                break;
            }

            case SpvOpTypeBool: {
                // XXX result id
                uint32_t id = nextu();
                pgm->types[id] = TypeBool {};
                pgm->typeSizes[id] = sizeof(bool);
                if(pgm->verbose) {
                    std::cout << "TypeBool " << id << "\n";
                }
                break;
            }

            case SpvOpTypeFloat: {
                // XXX result id
                uint32_t id = nextu();
                uint32_t width = nextu();
                assert(width <= 32); // XXX deal with larger later
                pgm->types[id] = TypeFloat {width};
                pgm->typeSizes[id] = ((width + 31) / 32) * 4;
                if(pgm->verbose) {
                    std::cout << "TypeFloat " << id << " " << width << "\n";
                }
                break;
            }

            case SpvOpTypeInt: {
                // XXX result id
                uint32_t id = nextu();
                uint32_t width = nextu();
                uint32_t signedness = nextu();
                assert(width <= 32); // XXX deal with larger later
                pgm->types[id] = TypeInt {width, signedness};
                pgm->typeSizes[id] = ((width + 31) / 32) * 4;
                if(pgm->verbose) {
                    std::cout << "TypeInt " << id << " width " << width << " signedness " << signedness << "\n";
                }
                break;
            }

            case SpvOpTypeFunction: {
                // XXX result id
                uint32_t id = nextu();
                uint32_t returnType = nextu();
                std::vector<uint32_t> params = restv();
                pgm->types[id] = TypeFunction {returnType, params};
                pgm->typeSizes[id] = 4; // XXX ?!?!?
                if(pgm->verbose) {
                    std::cout << "TypeFunction " << id << " returning " << returnType;
                    if(params.size() > 1) {
                        std::cout << " with parameter types"; 
                        for(int i = 0; i < params.size(); i++)
                            std::cout << " " << params[i];
                    }
                    std::cout << "\n";
                }
                break;
            }

            case SpvOpTypeVector: {
                // XXX result id
                uint32_t id = nextu();
                uint32_t type = nextu();
                uint32_t count = nextu();
                pgm->types[id] = TypeVector {type, count};
                pgm->typeSizes[id] = pgm->typeSizes[type] * count;
                if(pgm->verbose) {
                    std::cout << "TypeVector " << id << " of " << type << " count " << count << "\n";
                }
                break;
            }

            case SpvOpTypeMatrix: {
                // XXX result id
                uint32_t id = nextu();
                uint32_t columnType = nextu();
                uint32_t columnCount = nextu();
                pgm->types[id] = TypeMatrix {columnType, columnCount};
                pgm->typeSizes[id] = pgm->typeSizes[columnType] * columnCount;
                if(pgm->verbose) {
                    std::cout << "TypeMatrix " << id << " of " << columnType << " count " << columnCount << "\n";
                }
                break;
            }

            case SpvOpTypePointer: {
                // XXX result id
                uint32_t id = nextu();
                uint32_t storageClass = nextu();
                uint32_t type = nextu();
                pgm->types[id] = TypePointer {type, storageClass};
                pgm->typeSizes[id] = sizeof(uint32_t);
                if(pgm->verbose) {
                    std::cout << "TypePointer " << id << " class " << storageClass << " type " << type << "\n";
                }
                break;
            }

            case SpvOpTypeStruct: {
                // XXX result id
                uint32_t id = nextu();
                std::vector<uint32_t> memberTypes = restv();
                pgm->types[id] = TypeStruct {memberTypes};
                size_t size = 0;
                for(auto& t: memberTypes) {
                    size += pgm->typeSizes[t];
                }
                pgm->typeSizes[id] = size;
                if(pgm->verbose) {
                    std::cout << "TypeStruct " << id;
                    if(memberTypes.size() > 0) {
                        std::cout << " members"; 
                        for(auto& i: memberTypes)
                            std::cout << " " << i;
                    }
                    std::cout << "\n";
                }
                break;
            }

            case SpvOpVariable: {
                // XXX result id
                uint32_t type = nextu();
                uint32_t id = nextu();
                uint32_t storageClass = nextu();
                uint32_t initializer = nextu(NO_INITIALIZER);
                uint32_t pointedType = std::get<TypePointer>(pgm->types[type]).type;
                size_t offset = pgm->allocate(storageClass, pointedType);
                pgm->variables[id] = {pointedType, storageClass, initializer, offset};
                if(pgm->verbose) {
                    std::cout << "Variable " << id << " type " << type << " to type " << pointedType << " storageClass " << storageClass << " offset " << offset;
                    if(initializer != NO_INITIALIZER)
                        std::cout << " initializer " << initializer;
                    std::cout << "\n";
                }
                break;
            }

            case SpvOpConstant: {
                // XXX result id
                uint32_t typeId = nextu();
                uint32_t id = nextu();
                assert(opds[2].num_words == 1); // XXX limit to 32 bits for now
                uint32_t value = nextu();
                Register& r = pgm->allocConstantObject(id, typeId);
                const unsigned char *data = reinterpret_cast<const unsigned char*>(&value);
                std::copy(data, data + pgm->typeSizes[typeId], r.data);
                if(pgm->verbose) {
                    std::cout << "Constant " << id << " type " << typeId << " value " << value << "\n";
                }
                break;
            }

            case SpvOpConstantTrue: {
                // XXX result id
                uint32_t typeId = nextu();
                uint32_t id = nextu();
                Register& r = pgm->allocConstantObject(id, typeId);
                bool value = true;
                const unsigned char *data = reinterpret_cast<const unsigned char*>(&value);
                std::copy(data, data + pgm->typeSizes[typeId], r.data);
                if(pgm->verbose) {
                    std::cout << "Constant " << id << " type " << typeId << " value " << value << "\n";
                }
                break;
            }

            case SpvOpConstantFalse: {
                // XXX result id
                uint32_t typeId = nextu();
                uint32_t id = nextu();
                Register& r = pgm->allocConstantObject(id, typeId);
                bool value = false;
                const unsigned char *data = reinterpret_cast<const unsigned char*>(&value);
                std::copy(data, data + pgm->typeSizes[typeId], r.data);
                if(pgm->verbose) {
                    std::cout << "Constant " << id << " type " << typeId << " value " << value << "\n";
                }
                break;
            }

            case SpvOpConstantComposite: {
                // XXX result id
                uint32_t typeId = nextu();
                uint32_t id = nextu();
                std::vector<uint32_t> operands = restv();
                Register& r = pgm->allocConstantObject(id, typeId);
                uint32_t offset = 0;
                for(uint32_t operand : operands) {
                    // Copy each operand from a constant into our new composite constant.
                    const Register &src = pgm->constants[operand];
                    uint32_t size = pgm->typeSizes[src.type];
                    std::copy(src.data, src.data + size, r.data + offset);
                    offset += size;
                }
                assert(offset = pgm->typeSizes[typeId]);
                break;
            }

            case SpvOpTypeSampledImage: {
                // XXX result id
                uint32_t id = nextu();
                uint32_t imageType = nextu();
                pgm->types[id] = TypeSampledImage { imageType };
                if(pgm->verbose) {
                    std::cout << "TypeSampledImage " << id
                        << " imageType " << imageType
                        << "\n";
                }
                break;
            }

            case SpvOpTypeImage: {
                // XXX result id
                uint32_t id = nextu();
                uint32_t sampledType = nextu();
                uint32_t dim = nextu();
                uint32_t depth = nextu();
                uint32_t arrayed = nextu();
                uint32_t ms = nextu();
                uint32_t sampled = nextu();
                uint32_t imageFormat = nextu();
                uint32_t accessQualifier = nextu(NO_ACCESS_QUALIFIER);
                pgm->types[id] = TypeImage { sampledType, dim, depth, arrayed, ms, sampled, imageFormat, accessQualifier };
                if(pgm->verbose) {
                    std::cout << "TypeImage " << id
                        << " sampledType " << sampledType
                        << " dim " << dim
                        << " depth " << depth
                        << " arrayed " << arrayed
                        << " ms " << ms
                        << " sampled " << sampled
                        << " imageFormat " << imageFormat
                        << " accessQualifier " << accessQualifier
                        << "\n";
                }
                break;
            }

            case SpvOpFunction: {
                uint32_t resultType = nextu();
                uint32_t id = nextu();
                uint32_t functionControl = nextu(); // bitfield hints for inlining, pure, etc.
                uint32_t functionType = nextu();
                uint32_t start = pgm->instructions.size();
                pgm->functions[id] = Function {id, resultType, functionControl, functionType, start };
                pgm->currentFunction = &pgm->functions[id];
                if(pgm->verbose) {
                    std::cout << "Function " << id
                        << " resultType " << resultType
                        << " functionControl " << functionControl
                        << " functionType " << functionType
                        << "\n";
                }
                break;
            }

            case SpvOpLabel: {
                uint32_t id = nextu();
                pgm->labels[id] = pgm->instructions.size();
                if(pgm->verbose) {
                    std::cout << "Label " << id
                        << " at " << pgm->labels[id]
                        << "\n";
                }
                break;
            }

            case SpvOpFunctionEnd: {
                pgm->currentFunction = NULL;
                if(pgm->verbose) {
                    std::cout << "FunctionEnd\n";
                }
                break;
            }

            case SpvOpSelectionMerge: {
                // We don't do anything with this information.
                // https://www.khronos.org/registry/spir-v/specs/unified1/SPIRV.html#StructuredControlFlow
                break;
            }

            case SpvOpLoopMerge: {
                // We don't do anything with this information.
                // https://www.khronos.org/registry/spir-v/specs/unified1/SPIRV.html#StructuredControlFlow
                break;
            }

            // Decode the instructions.
#include "opcode_decode.h"
            
            default: {
                if(pgm->throwOnUnimplemented) {
                    throw std::runtime_error("unimplemented opcode " + OpcodeToString[insn->opcode] + " (" + std::to_string(insn->opcode) + ")");
                } else {
                    std::cout << "unimplemented opcode " << OpcodeToString[insn->opcode] << " (" << insn->opcode << ")\n";
                    pgm->hasUnimplemented = true;
                }
                break;
            }
        }

        return SPV_SUCCESS;
    }

    void assignRegisters(Block *block, const std::set<uint32_t> &allPhy) {
        std::cout << "assignRegisters(" << block->labelId << ")\n";

        // Assigned physical registers.
        std::set<uint32_t> assigned;

        // Registers that are live going into this block have already been
        // assigned. XXX not sure how that can be true.
        for (auto regId : instructions.at(block->begin)->livein) {
            auto r = virtToPhy.find(regId);
            if (r == virtToPhy.end()) {
                std::cout << "Warning: Expected physical register for "
                    << regId << " in block " << block->labelId << ".\n";
            } else {
                assigned.insert(r->second);
            }
        }

        for (int pc = block->begin; pc < block->end; pc++) {
            Instruction *instruction = instructions.at(pc).get();

            // Free up now-unused physical registers.
            for (auto argId : instruction->argIds) {
                // If this virtual register doesn't survive past this line, then we
                // can use its physical register again.
                if (instruction->liveout.find(argId) == instruction->liveout.end() &&
                        virtToPhy.find(argId) != virtToPhy.end()) {

                    assigned.erase(virtToPhy.at(argId));
                }
            }

            // Assign result registers to physical registers.
            if (instruction->resId != NO_REGISTER) {
                // Find an available physical register for this virtual register.
                bool found = false;
                for (uint32_t phy : allPhy) {
                    if (assigned.find(phy) == assigned.end()) {
                        virtToPhy[instruction->resId] = phy;
                        assigned.insert(phy);
                        found = true;
                        break;
                    }
                }
                if (!found) {
                    std::cout << "Warning: No physical register available for "
                        << instruction->resId << " on line " << pc << ".\n";
                }
            }
        }

        // Go down dominance tree.
        for (auto& [labelId, subBlock] : blocks) {
            if (subBlock->idom == block->labelId) {
                assignRegisters(subBlock.get(), allPhy);
            }
        }
    }

    // Post-parsing work.
    void postParse() {
        // Find the main function.
        mainFunction = nullptr;
        for(auto& e: entryPoints) {
            if(e.second.name == "main") {
                mainFunction = &functions[e.first];
            }
        }

        // Figure out our basic blocks. These start on an OpLabel and end on
        // a terminating instruction.
        for (auto [labelId, codeIndex] : labels) {
            bool found = false;
            for (int i = codeIndex; i < instructions.size(); i++) {
                if (instructions[i]->isTermination()) {
                    blocks[labelId] = std::make_unique<Block>(labelId, codeIndex, uint32_t(i + 1));
                    found = true;
                    break;
                }
            }
            if (!found) {
                std::cout << "Error: Terminating instruction for label "
                    << labelId << " not found.\n";
                exit(EXIT_FAILURE);
            }
        }

        // Compute successor blocks.
        for (auto& [labelId, block] : blocks) {
            Instruction *instruction = instructions[block->end - 1].get();
            assert(instruction->isTermination());
            block->succ = instruction->targetLabelIds;
            for (uint32_t labelId : block->succ) {
                blocks[labelId]->pred.insert(block->labelId);
            }
        }

        // Record the block ID for each instruction. Note a problem
        // here is that the OpFunctionParameter instruction gets put into the
        // block at the end of the previous function. I don't think this
        // matters in practice because there's never a Phi at the top of a
        // function.
        for (auto& [labelId, block] : blocks) {
            for (int pc = block->begin; pc < block->end; pc++) {
                instructions[pc]->blockId = block->labelId;
            }
        }

        // Compute predecessor and successor instructions for each instruction.
        // For most instructions this is just the previous or next line, except
        // for the ones at the beginning or end of each block.
        for (auto& [labelId, block] : blocks) {
            for (auto predBlockId : block->pred) {
                instructions[block->begin]->pred.insert(blocks[predBlockId]->end - 1);
            }
            if (block->begin != block->end - 1) {
                instructions[block->begin]->succ.insert(block->begin + 1);
            }
            for (int pc = block->begin + 1; pc < block->end - 1; pc++) {
                instructions[pc]->pred.insert(pc - 1);
                instructions[pc]->succ.insert(pc + 1);
            }
            if (block->begin != block->end - 1) {
                instructions[block->end - 1]->pred.insert(block->end - 2);
            }
            for (auto succBlockId : block->succ) {
                instructions[block->end - 1]->succ.insert(blocks[succBlockId]->begin);
            }
        }

        // Compute livein and liveout registers for each line. This is an inefficient
        // way to do this, we should use a worklist algorithm.
        bool changed = true;
        while (changed) {
            changed = false;

            for (int pc = 0; pc < instructions.size(); pc++) {
                Instruction *instruction = instructions[pc].get();
                std::set<uint32_t> oldLivein = instruction->livein;
                std::set<uint32_t> oldLiveout = instruction->liveout;

                instruction->liveout.clear();
                for (uint32_t succPc : instruction->succ) {
                    Instruction *succInstruction = instructions[succPc].get();
                    instruction->liveout.insert(succInstruction->livein.begin(),
                        succInstruction->livein.end());
                }

                instruction->livein = instruction->liveout;
                if (instruction->resId != NO_REGISTER) {
                    instruction->livein.erase(instruction->resId);
                }
                for (uint32_t argId : instruction->argIds) {
                    // Don't consider constants or variables, they're never in registers.
                    if (constants.find(argId) == constants.end() &&
                            variables.find(argId) == variables.end()) {

                        instruction->livein.insert(argId);
                    }
                }

                if (oldLivein != instruction->livein || oldLiveout != instruction->liveout) {
                    changed = true;
                }
            }
        }

        // Compute the dominance tree for blocks. Use a worklist. Do all blocks
        // simultaneously (across functions).
        std::vector<uint32_t> worklist; // block IDs.
        // Make set of all label IDs.
        std::set<uint32_t> allLabelIds;
        for (auto& [labelId, block] : blocks) {
            allLabelIds.insert(labelId);
        }
        // Prepare every block.
        for (auto& [labelId, block] : blocks) {
            if (block->pred.empty()) {
                // First block of a function.
                worklist.push_back(labelId);
            }
            block->dom = allLabelIds;
        }
        while (!worklist.empty()) {
            // Take any item from worklist.
            uint32_t labelId = worklist.back();
            worklist.pop_back();

            Block *block = blocks.at(labelId).get();

            // Intersection of all predecessor doms.
            std::set<uint32_t> dom;
            bool first = true;
            for (auto predBlockId : block->pred) {
                Block *pred = blocks.at(predBlockId).get();

                if (first) {
                    dom = pred->dom;
                    first = false;
                } else {
                    // I love C++.
                    std::set<uint32_t> intersection;
                    std::set_intersection(dom.begin(), dom.end(),
                            pred->dom.begin(), pred->dom.end(),
                            std::inserter(intersection, intersection.begin()));
                    std::swap(intersection, dom);
                }
            }
            // Add ourselves.
            dom.insert(labelId);

            // If the set changed, update it and put the
            // successors in the work queue.
            if (dom != block->dom) {
                block->dom = dom;
                worklist.insert(worklist.end(), block->succ.begin(), block->succ.end());
            }
        }

        // Compute immediate dom for each block.
        for (auto& [labelId, block] : blocks) {
            block->idom = NO_BLOCK_ID;

            // Try each block in the block's dom.
            for (auto idomCandidate : block->dom) {
                bool valid = idomCandidate != labelId;

                // Can't be the idom if it's dominated by another dom.
                for (auto otherDom : block->dom) {
                    if (otherDom != idomCandidate &&
                            otherDom != labelId &&
                            blocks[otherDom]->isDominatedBy(idomCandidate)) {

                        valid = false;
                        break;
                    }
                }

                if (valid) {
                    block->idom = idomCandidate;
                    break;
                }
            }

            // It's okay to have no idom as long as you're the first block in the function.
            assert((block->idom == NO_BLOCK_ID) == block->pred.empty());
        }

        // Perform physical register assignment.

        // Fake ones for now, pretend we have 32 of them.
        std::set<uint32_t> PHY_REGS;
        for (int i = 0; i < 32; i++) {
            PHY_REGS.insert(i);
        }

        // Look for blocks at the start of functions.
        for (auto& [labelId, block] : blocks) {
            if (block->pred.empty()) {
                assignRegisters(block.get(), PHY_REGS);
            }
        }

        // Dump block info.
        if (verbose) {
            std::cout << "----------------------- Block info\n";
            for (auto& [labelId, block] : blocks) {
                std::cout << "Block " << labelId << ", instructions "
                    << block->begin << "-" << block->end << ":\n";
                std::cout << "    Pred:";
                for (auto labelId : block->pred) {
                    std::cout << " " << labelId;
                }
                std::cout << "\n";
                std::cout << "    Succ:";
                for (auto labelId : block->succ) {
                    std::cout << " " << labelId;
                }
                std::cout << "\n";
                std::cout << "    Dom:";
                for (auto labelId : block->dom) {
                    std::cout << " " << labelId;
                }
                std::cout << "\n";
                if (block->idom != NO_BLOCK_ID) {
                    std::cout << "    Immediate Dom: " << block->idom << "\n";
                }
            }
            std::cout << "-----------------------\n";
        }

        // Dump instruction info.
        if (verbose) {
            std::cout << "----------------------- Instruction info\n";
            for (int pc = 0; pc < instructions.size(); pc++) {
                Instruction *instruction = instructions[pc].get();

                for(auto &function : functions) {
                    if(pc == function.second.start) {
                        std::string name = cleanUpFunctionName(function.first);
                        std::cout << name << ":\n";
                    }
                }

                for(auto &label : labels) {
                    if(pc == label.second) {
                        std::cout << label.first << ":\n";
                    }
                }

                std::ios oldState(nullptr);
                oldState.copyfmt(std::cout);

                std::cout
                    << std::setw(5) << pc;
                if (instruction->blockId == NO_BLOCK_ID) {
                    std::cout << "  ---";
                } else {
                    std::cout << std::setw(5) << instruction->blockId;
                }
                if (instruction->resId == NO_REGISTER) {
                    std::cout << "        ";
                } else {
                    std::cout << std::setw(5) << instruction->resId << " <-";
                }

                std::cout << std::setw(0) << " " << instruction->name();

                for (auto argId : instruction->argIds) {
                    std::cout << " " << argId;
                }

                std::cout << " (pred";
                for (auto line : instruction->pred) {
                    std::cout << " " << line;
                }
                std::cout << ", succ";
                for (auto line : instruction->succ) {
                    std::cout << " " << line;
                }
                std::cout << ", live";
                for (auto regId : instruction->livein) {
                    std::cout << " " << regId;
                }

                std::cout << ")\n";

                std::cout.copyfmt(oldState);
            }
            std::cout << "-----------------------\n";
        }
    }

    // Take "mainImage(vf4;vf2;" and return "mainImage$v4f$vf2".
    std::string cleanUpFunctionName(int nameId) const {
        std::string name = names.at(nameId);

        // Replace "mainImage(vf4;vf2;" with "mainImage$v4f$vf2$"
        for (int i = 0; i < name.length(); i++) {
            if (name[i] == '(' || name[i] == ';') {
                name[i] = '$';
            }
        }

        // Strip trailing dollar sign.
        if (name.length() > 0 && name[name.length() - 1] == '$') {
            name = name.substr(0, name.length() - 1);
        }

        return name;
    }
};

Interpreter::Interpreter(const Program *pgm)
    : pgm(pgm)
{
    memory = new unsigned char[pgm->memorySize];

    // So we can catch errors.
    std::fill(memory, memory + pgm->memorySize, 0xFF);

    // Allocate registers so they aren't allocated during run()
    for(auto [id, type]: pgm->resultsCreated) {
        allocRegister(id, type);
    }
}

Register& Interpreter::allocRegister(uint32_t id, uint32_t type)
{
    Register r {type, pgm->typeSizes.at(type)};
    registers[id] = r;
    return registers[id];
}

void Interpreter::copy(uint32_t type, size_t src, size_t dst)
{
    std::copy(memory + src, memory + src + pgm->typeSizes.at(type), memory + dst);
}

template <class T>
T& Interpreter::objectInClassAt(SpvStorageClass clss, size_t offset)
{
    return *reinterpret_cast<T*>(memory + pgm->memoryRegions.at(clss).base + offset);
}

template <class T>
T& Interpreter::registerAs(int id)
{
    return *reinterpret_cast<T*>(registers[id].data);
}

void Interpreter::clearPrivateVariables()
{
    // Global variables are cleared for each run.
    const MemoryRegion &mr = pgm->memoryRegions.at(SpvStorageClassPrivate);
    std::fill(memory + mr.base, memory + mr.top, 0x00);
}

void Interpreter::stepLoad(const InsnLoad& insn)
{
    Pointer& ptr = pointers.at(insn.pointerId);
    Register& obj = registers[insn.resultId];
    std::copy(memory + ptr.offset, memory + ptr.offset + pgm->typeSizes.at(insn.type), obj.data);
    if(false) {
        std::cout << "load result is";
        pgm->dumpTypeAt(pgm->types.at(insn.type), obj.data);
        std::cout << "\n";
    }
}

void Interpreter::stepStore(const InsnStore& insn)
{
    Pointer& ptr = pointers.at(insn.pointerId);
    Register& obj = registers[insn.objectId];
    std::copy(obj.data, obj.data + obj.size, memory + ptr.offset);
}

void Interpreter::stepCompositeExtract(const InsnCompositeExtract& insn)
{
    Register& obj = registers[insn.resultId];
    Register& src = registers[insn.compositeId];
    /* use indexes to walk blob */
    uint32_t type = src.type;
    size_t offset = 0;
    for(auto& j: insn.indexesId) {
        for(int i = 0; i < j; i++) {
            offset += pgm->typeSizes.at(pgm->getConstituentType(type, i));
        }
        type = pgm->getConstituentType(type, j);
    }
    std::copy(src.data + offset, src.data + offset + pgm->typeSizes.at(obj.type), obj.data);
    if(false) {
        std::cout << "extracted from ";
        pgm->dumpTypeAt(pgm->types.at(src.type), src.data);
        std::cout << " result is ";
        pgm->dumpTypeAt(pgm->types.at(insn.type), obj.data);
        std::cout << "\n";
    }
}

void Interpreter::stepCompositeConstruct(const InsnCompositeConstruct& insn)
{
    Register& obj = registers[insn.resultId];
    size_t offset = 0;
    for(auto& j: insn.constituentsId) {
        Register& src = registers[j];
        std::copy(src.data, src.data + pgm->typeSizes.at(src.type), obj.data + offset);
        offset += pgm->typeSizes.at(src.type);
    }
    if(false) {
        std::cout << "constructed ";
        pgm->dumpTypeAt(pgm->types.at(obj.type), obj.data);
        std::cout << "\n";
    }
}

void Interpreter::stepIAdd(const InsnIAdd& insn)
{
    std::visit([this, &insn](auto&& type) {

        using T = std::decay_t<decltype(type)>;

        if constexpr (std::is_same_v<T, TypeInt>) {

            uint32_t operand1 = registerAs<uint32_t>(insn.operand1Id);
            uint32_t operand2 = registerAs<uint32_t>(insn.operand2Id);
            uint32_t result = operand1 + operand2;
            registerAs<uint32_t>(insn.resultId) = result;

        } else if constexpr (std::is_same_v<T, TypeVector>) {

            uint32_t* operand1 = &registerAs<uint32_t>(insn.operand1Id);
            uint32_t* operand2 = &registerAs<uint32_t>(insn.operand2Id);
            uint32_t* result = &registerAs<uint32_t>(insn.resultId);
            for(int i = 0; i < type.count; i++) {
                result[i] = operand1[i] + operand2[i];
            }

        } else {

            std::cout << "Unknown type for IAdd\n";

        }
    }, pgm->types.at(insn.type));
}

void Interpreter::stepFAdd(const InsnFAdd& insn)
{
    std::visit([this, &insn](auto&& type) {

        using T = std::decay_t<decltype(type)>;

        if constexpr (std::is_same_v<T, TypeFloat>) {

            float operand1 = registerAs<float>(insn.operand1Id);
            float operand2 = registerAs<float>(insn.operand2Id);
            float result = operand1 + operand2;
            registerAs<float>(insn.resultId) = result;

        } else if constexpr (std::is_same_v<T, TypeVector>) {

            float* operand1 = &registerAs<float>(insn.operand1Id);
            float* operand2 = &registerAs<float>(insn.operand2Id);
            float* result = &registerAs<float>(insn.resultId);
            for(int i = 0; i < type.count; i++) {
                result[i] = operand1[i] + operand2[i];
            }

        } else {

            std::cout << "Unknown type for FAdd\n";

        }
    }, pgm->types.at(insn.type));
}

void Interpreter::stepFSub(const InsnFSub& insn)
{
    std::visit([this, &insn](auto&& type) {

        using T = std::decay_t<decltype(type)>;

        if constexpr (std::is_same_v<T, TypeFloat>) {

            float operand1 = registerAs<float>(insn.operand1Id);
            float operand2 = registerAs<float>(insn.operand2Id);
            float result = operand1 - operand2;
            registerAs<float>(insn.resultId) = result;

        } else if constexpr (std::is_same_v<T, TypeVector>) {

            float* operand1 = &registerAs<float>(insn.operand1Id);
            float* operand2 = &registerAs<float>(insn.operand2Id);
            float* result = &registerAs<float>(insn.resultId);
            for(int i = 0; i < type.count; i++) {
                result[i] = operand1[i] - operand2[i];
            }

        } else {

            std::cout << "Unknown type for FSub\n";

        }
    }, pgm->types.at(insn.type));
}

void Interpreter::stepFMul(const InsnFMul& insn)
{
    std::visit([this, &insn](auto&& type) {

        using T = std::decay_t<decltype(type)>;

        if constexpr (std::is_same_v<T, TypeFloat>) {

            float operand1 = registerAs<float>(insn.operand1Id);
            float operand2 = registerAs<float>(insn.operand2Id);
            float result = operand1 * operand2;
            registerAs<float>(insn.resultId) = result;

        } else if constexpr (std::is_same_v<T, TypeVector>) {

            float* operand1 = &registerAs<float>(insn.operand1Id);
            float* operand2 = &registerAs<float>(insn.operand2Id);
            float* result = &registerAs<float>(insn.resultId);
            for(int i = 0; i < type.count; i++) {
                result[i] = operand1[i] * operand2[i];
            }

        } else {

            std::cout << "Unknown type for FMul\n";

        }
    }, pgm->types.at(insn.type));
}

void Interpreter::stepFDiv(const InsnFDiv& insn)
{
    std::visit([this, &insn](auto&& type) {

        using T = std::decay_t<decltype(type)>;

        if constexpr (std::is_same_v<T, TypeFloat>) {

            float operand1 = registerAs<float>(insn.operand1Id);
            float operand2 = registerAs<float>(insn.operand2Id);
            float result = operand1 / operand2;
            registerAs<float>(insn.resultId) = result;

        } else if constexpr (std::is_same_v<T, TypeVector>) {

            float* operand1 = &registerAs<float>(insn.operand1Id);
            float* operand2 = &registerAs<float>(insn.operand2Id);
            float* result = &registerAs<float>(insn.resultId);
            for(int i = 0; i < type.count; i++) {
                result[i] = operand1[i] / operand2[i];
            }

        } else {

            std::cout << "Unknown type for FDiv\n";

        }
    }, pgm->types.at(insn.type));
}

void Interpreter::stepFMod(const InsnFMod& insn)
{
    std::visit([this, &insn](auto&& type) {

        using T = std::decay_t<decltype(type)>;

        if constexpr (std::is_same_v<T, TypeFloat>) {

            float operand1 = registerAs<float>(insn.operand1Id);
            float operand2 = registerAs<float>(insn.operand2Id);
            float result = operand1 - floor(operand1/operand2)*operand2;
            registerAs<float>(insn.resultId) = result;

        } else if constexpr (std::is_same_v<T, TypeVector>) {

            float* operand1 = &registerAs<float>(insn.operand1Id);
            float* operand2 = &registerAs<float>(insn.operand2Id);
            float* result = &registerAs<float>(insn.resultId);
            for(int i = 0; i < type.count; i++) {
                result[i] = operand1[i] - floor(operand1[i]/operand2[i])*operand2[i];
            }

        } else {

            std::cout << "Unknown type for FMod\n";

        }
    }, pgm->types.at(insn.type));
}

void Interpreter::stepFOrdLessThan(const InsnFOrdLessThan& insn)
{
    std::visit([this, &insn](auto&& type) {

        using T = std::decay_t<decltype(type)>;

        if constexpr (std::is_same_v<T, TypeFloat>) {

            float operand1 = registerAs<float>(insn.operand1Id);
            float operand2 = registerAs<float>(insn.operand2Id);
            bool result = operand1 < operand2;
            registerAs<bool>(insn.resultId) = result;

        } else if constexpr (std::is_same_v<T, TypeVector>) {

            float* operand1 = &registerAs<float>(insn.operand1Id);
            float* operand2 = &registerAs<float>(insn.operand2Id);
            bool* result = &registerAs<bool>(insn.resultId);
            for(int i = 0; i < type.count; i++) {
                result[i] = operand1[i] < operand2[i];
            }

        } else {

            std::cout << "Unknown type for FOrdLessThan\n";

        }
    }, pgm->types.at(registers[insn.operand1Id].type));
}

void Interpreter::stepFOrdGreaterThan(const InsnFOrdGreaterThan& insn)
{
    std::visit([this, &insn](auto&& type) {

        using T = std::decay_t<decltype(type)>;

        if constexpr (std::is_same_v<T, TypeFloat>) {

            float operand1 = registerAs<float>(insn.operand1Id);
            float operand2 = registerAs<float>(insn.operand2Id);
            bool result = operand1 > operand2;
            registerAs<bool>(insn.resultId) = result;

        } else if constexpr (std::is_same_v<T, TypeVector>) {

            float* operand1 = &registerAs<float>(insn.operand1Id);
            float* operand2 = &registerAs<float>(insn.operand2Id);
            bool* result = &registerAs<bool>(insn.resultId);
            for(int i = 0; i < type.count; i++) {
                result[i] = operand1[i] > operand2[i];
            }

        } else {

            std::cout << "Unknown type for FOrdGreaterThan\n";

        }
    }, pgm->types.at(registers[insn.operand1Id].type));
}

void Interpreter::stepFOrdLessThanEqual(const InsnFOrdLessThanEqual& insn)
{
    std::visit([this, &insn](auto&& type) {

        using T = std::decay_t<decltype(type)>;

        if constexpr (std::is_same_v<T, TypeFloat>) {

            float operand1 = registerAs<float>(insn.operand1Id);
            float operand2 = registerAs<float>(insn.operand2Id);
            bool result = operand1 <= operand2;
            registerAs<bool>(insn.resultId) = result;

        } else if constexpr (std::is_same_v<T, TypeVector>) {

            float* operand1 = &registerAs<float>(insn.operand1Id);
            float* operand2 = &registerAs<float>(insn.operand2Id);
            bool* result = &registerAs<bool>(insn.resultId);
            for(int i = 0; i < type.count; i++) {
                result[i] = operand1[i] <= operand2[i];
            }

        } else {

            std::cout << "Unknown type for FOrdLessThanEqual\n";

        }
    }, pgm->types.at(registers[insn.operand1Id].type));
}

void Interpreter::stepFOrdEqual(const InsnFOrdEqual& insn)
{
    std::visit([this, &insn](auto&& type) {

        using T = std::decay_t<decltype(type)>;

        if constexpr (std::is_same_v<T, TypeFloat>) {

            float operand1 = registerAs<float>(insn.operand1Id);
            float operand2 = registerAs<float>(insn.operand2Id);
            // XXX I don't know the difference between ordered and equal
            // vs. unordered and equal, so I don't know which this is.
            bool result = operand1 == operand2;
            registerAs<bool>(insn.resultId) = result;

        } else if constexpr (std::is_same_v<T, TypeVector>) {

            float* operand1 = &registerAs<float>(insn.operand1Id);
            float* operand2 = &registerAs<float>(insn.operand2Id);
            bool* result = &registerAs<bool>(insn.resultId);
            for(int i = 0; i < type.count; i++) {
                result[i] = operand1[i] == operand2[i];
            }

        } else {

            std::cout << "Unknown type for FOrdEqual\n";

        }
    }, pgm->types.at(registers[insn.operand1Id].type));
}

void Interpreter::stepFNegate(const InsnFNegate& insn)
{
    std::visit([this, &insn](auto&& type) {

        using T = std::decay_t<decltype(type)>;

        if constexpr (std::is_same_v<T, TypeFloat>) {

            registerAs<float>(insn.resultId) = -registerAs<float>(insn.operandId);

        } else if constexpr (std::is_same_v<T, TypeVector>) {

            float* operand = &registerAs<float>(insn.operandId);
            float* result = &registerAs<float>(insn.resultId);
            for(int i = 0; i < type.count; i++) {
                result[i] = -operand[i];
            }

        } else {

            // Doesn't seem necessary to do matrices, the assembly
            // extracts the vectors and negates them and contructs
            // a new matrix.

            std::cout << "Unknown type for FNegate\n";

        }
    }, pgm->types.at(insn.type));
}

// Computes the dot product of two vectors.
float dotProduct(float *a, float *b, int count)
{
    float dot = 0.0;

    for (int i = 0; i < count; i++) {
        dot += a[i]*b[i];
    }

    return dot;
}

void Interpreter::stepDot(const InsnDot& insn)
{
    std::visit([this, &insn](auto&& type) {

        using T = std::decay_t<decltype(type)>;

        const Register &r1 = registers[insn.vector1Id];
        const TypeVector &t1 = std::get<TypeVector>(pgm->types.at(r1.type));

        if constexpr (std::is_same_v<T, TypeFloat>) {

            float* vector1 = &registerAs<float>(insn.vector1Id);
            float* vector2 = &registerAs<float>(insn.vector2Id);
            registerAs<float>(insn.resultId) = dotProduct(vector1, vector2, t1.count);

        } else {

            std::cout << "Unknown type for Dot\n";

        }
    }, pgm->types.at(insn.type));
}

void Interpreter::stepFOrdGreaterThanEqual(const InsnFOrdGreaterThanEqual& insn)
{
    std::visit([this, &insn](auto&& type) {

        using T = std::decay_t<decltype(type)>;

        if constexpr (std::is_same_v<T, TypeFloat>) {

            float operand1 = registerAs<float>(insn.operand1Id);
            float operand2 = registerAs<float>(insn.operand2Id);
            bool result = operand1 >= operand2;
            registerAs<bool>(insn.resultId) = result;

        } else if constexpr (std::is_same_v<T, TypeVector>) {

            float* operand1 = &registerAs<float>(insn.operand1Id);
            float* operand2 = &registerAs<float>(insn.operand2Id);
            bool* result = &registerAs<bool>(insn.resultId);
            for(int i = 0; i < type.count; i++) {
                result[i] = operand1[i] >= operand2[i];
            }

        } else {

            std::cout << "Unknown type for FOrdGreaterThanEqual\n";

        }
    }, pgm->types.at(registers[insn.operand1Id].type));
}

void Interpreter::stepSLessThan(const InsnSLessThan& insn)
{
    std::visit([this, &insn](auto&& type) {

        using T = std::decay_t<decltype(type)>;

        if constexpr (std::is_same_v<T, TypeInt>) {

            int32_t operand1 = registerAs<int32_t>(insn.operand1Id);
            int32_t operand2 = registerAs<int32_t>(insn.operand2Id);
            bool result = operand1 < operand2;
            registerAs<bool>(insn.resultId) = result;

        } else if constexpr (std::is_same_v<T, TypeVector>) {

            int32_t* operand1 = &registerAs<int32_t>(insn.operand1Id);
            int32_t* operand2 = &registerAs<int32_t>(insn.operand2Id);
            bool* result = &registerAs<bool>(insn.resultId);
            for(int i = 0; i < type.count; i++) {
                result[i] = operand1[i] < operand2[i];
            }

        } else {

            std::cout << "Unknown type for SLessThan\n";

        }
    }, pgm->types.at(registers[insn.operand1Id].type));
}

void Interpreter::stepSDiv(const InsnSDiv& insn)
{
    std::visit([this, &insn](auto&& type) {

        using T = std::decay_t<decltype(type)>;

        if constexpr (std::is_same_v<T, TypeInt>) {

            int32_t operand1 = registerAs<int32_t>(insn.operand1Id);
            int32_t operand2 = registerAs<int32_t>(insn.operand2Id);
            int32_t result = operand1 / operand2;
            registerAs<int32_t>(insn.resultId) = result;

        } else if constexpr (std::is_same_v<T, TypeVector>) {

            int32_t* operand1 = &registerAs<int32_t>(insn.operand1Id);
            int32_t* operand2 = &registerAs<int32_t>(insn.operand2Id);
            int32_t* result = &registerAs<int32_t>(insn.resultId);
            for(int i = 0; i < type.count; i++) {
                result[i] = operand1[i] / operand2[i];
            }

        } else {

            std::cout << "Unknown type for SDiv\n";

        }
    }, pgm->types.at(registers[insn.operand1Id].type));
}

void Interpreter::stepIEqual(const InsnIEqual& insn)
{
    std::visit([this, &insn](auto&& type) {

        using T = std::decay_t<decltype(type)>;

        if constexpr (std::is_same_v<T, TypeInt>) {

            uint32_t operand1 = registerAs<uint32_t>(insn.operand1Id);
            uint32_t operand2 = registerAs<uint32_t>(insn.operand2Id);
            bool result = operand1 == operand2;
            registerAs<bool>(insn.resultId) = result;

        } else if constexpr (std::is_same_v<T, TypeVector>) {

            uint32_t* operand1 = &registerAs<uint32_t>(insn.operand1Id);
            uint32_t* operand2 = &registerAs<uint32_t>(insn.operand2Id);
            bool* result = &registerAs<bool>(insn.resultId);
            for(int i = 0; i < type.count; i++) {
                result[i] = operand1[i] == operand2[i];
            }

        } else {

            std::cout << "Unknown type for IEqual\n";

        }
    }, pgm->types.at(registers[insn.operand1Id].type));
}

void Interpreter::stepLogicalNot(const InsnLogicalNot& insn)
{
    std::visit([this, &insn](auto&& type) {

        using T = std::decay_t<decltype(type)>;

        if constexpr (std::is_same_v<T, TypeBool>) {

            registerAs<bool>(insn.resultId) = !registerAs<bool>(insn.operandId);

        } else if constexpr (std::is_same_v<T, TypeVector>) {

            bool* operand = &registerAs<bool>(insn.operandId);
            bool* result = &registerAs<bool>(insn.resultId);
            for(int i = 0; i < type.count; i++) {
                result[i] = !operand[i];
            }

        } else {

            std::cout << "Unknown type for LogicalNot\n";

        }
    }, pgm->types.at(registers[insn.operandId].type));
}

void Interpreter::stepLogicalOr(const InsnLogicalOr& insn)
{
    std::visit([this, &insn](auto&& type) {

        using T = std::decay_t<decltype(type)>;

        if constexpr (std::is_same_v<T, TypeBool>) {

            bool operand1 = registerAs<bool>(insn.operand1Id);
            bool operand2 = registerAs<bool>(insn.operand2Id);
            registerAs<bool>(insn.resultId) = operand1 || operand2;

        } else if constexpr (std::is_same_v<T, TypeVector>) {

            bool* operand1 = &registerAs<bool>(insn.operand1Id);
            bool* operand2 = &registerAs<bool>(insn.operand2Id);
            bool* result = &registerAs<bool>(insn.resultId);
            for(int i = 0; i < type.count; i++) {
                result[i] = operand1[i] || operand2[i];
            }

        } else {

            std::cout << "Unknown type for LogicalOr\n";

        }
    }, pgm->types.at(registers[insn.operand1Id].type));
}

void Interpreter::stepSelect(const InsnSelect& insn)
{
    std::visit([this, &insn](auto&& type) {

        using T = std::decay_t<decltype(type)>;

        if constexpr (std::is_same_v<T, TypeFloat>) {

            bool condition = registerAs<bool>(insn.conditionId);
            float object1 = registerAs<float>(insn.object1Id);
            float object2 = registerAs<float>(insn.object2Id);
            float result = condition ? object1 : object2;
            registerAs<float>(insn.resultId) = result;

        } else if constexpr (std::is_same_v<T, TypeVector>) {

            bool* condition = &registerAs<bool>(insn.conditionId);
            // XXX shouldn't assume floats here. Any data is valid.
            float* object1 = &registerAs<float>(insn.object1Id);
            float* object2 = &registerAs<float>(insn.object2Id);
            float* result = &registerAs<float>(insn.resultId);
            for(int i = 0; i < type.count; i++) {
                result[i] = condition[i] ? object1[i] : object2[i];
            }

        } else {

            std::cout << "Unknown type for stepSelect\n";

        }
    }, pgm->types.at(insn.type));
}

void Interpreter::stepVectorTimesScalar(const InsnVectorTimesScalar& insn)
{
    float* vector = &registerAs<float>(insn.vectorId);
    float scalar = registerAs<float>(insn.scalarId);
    float* result = &registerAs<float>(insn.resultId);

    const TypeVector &type = std::get<TypeVector>(pgm->types.at(insn.type));

    for(int i = 0; i < type.count; i++) {
        result[i] = vector[i] * scalar;
    }
}

void Interpreter::stepMatrixTimesVector(const InsnMatrixTimesVector& insn)
{
    float* matrix = &registerAs<float>(insn.matrixId);
    float* vector = &registerAs<float>(insn.vectorId);
    float* result = &registerAs<float>(insn.resultId);

    const Register &vectorReg = registers[insn.vectorId];

    const TypeVector &resultType = std::get<TypeVector>(pgm->types.at(insn.type));
    const TypeVector &vectorType = std::get<TypeVector>(pgm->types.at(vectorReg.type));

    int rn = resultType.count;
    int vn = vectorType.count;

    for(int i = 0; i < rn; i++) {
        float dot = 0.0;

        for(int j = 0; j < vn; j++) {
            dot += matrix[i + j*rn]*vector[j];
        }

        result[i] = dot;
    }
}

void Interpreter::stepVectorTimesMatrix(const InsnVectorTimesMatrix& insn)
{
    float* vector = &registerAs<float>(insn.vectorId);
    float* matrix = &registerAs<float>(insn.matrixId);
    float* result = &registerAs<float>(insn.resultId);

    const Register &vectorReg = registers[insn.vectorId];

    const TypeVector &resultType = std::get<TypeVector>(pgm->types.at(insn.type));
    const TypeVector &vectorType = std::get<TypeVector>(pgm->types.at(vectorReg.type));

    int rn = resultType.count;
    int vn = vectorType.count;

    for(int i = 0; i < rn; i++) {
        result[i] = dotProduct(vector, matrix + vn*i, vn);
    }
}

void Interpreter::stepVectorShuffle(const InsnVectorShuffle& insn)
{
    Register& obj = registers[insn.resultId];
    const Register &r1 = registers[insn.vector1Id];
    const Register &r2 = registers[insn.vector2Id];
    const TypeVector &t1 = std::get<TypeVector>(pgm->types.at(r1.type));
    uint32_t n1 = t1.count;
    uint32_t elementSize = pgm->typeSizes.at(t1.type);

    for(int i = 0; i < insn.componentsId.size(); i++) {
        uint32_t component = insn.componentsId[i];
        unsigned char *src = component < n1
            ? r1.data + component*elementSize
            : r2.data + (component - n1)*elementSize;
        std::copy(src, src + elementSize, obj.data + i*elementSize);
    }
}

void Interpreter::stepConvertSToF(const InsnConvertSToF& insn)
{
    std::visit([this, &insn](auto&& type) {

        using T = std::decay_t<decltype(type)>;

        if constexpr (std::is_same_v<T, TypeFloat>) {

            int32_t src = registerAs<int32_t>(insn.signedValueId);
            registerAs<float>(insn.resultId) = src;

        } else if constexpr (std::is_same_v<T, TypeVector>) {

            int32_t* src = &registerAs<int32_t>(insn.signedValueId);
            float* dst = &registerAs<float>(insn.resultId);
            for(int i = 0; i < type.count; i++) {
                dst[i] = src[i];
            }

        } else {

            std::cout << "Unknown type for ConvertSToF\n";

        }
    }, pgm->types.at(insn.type));
}

void Interpreter::stepConvertFToS(const InsnConvertFToS& insn)
{
    std::visit([this, &insn](auto&& type) {

        using T = std::decay_t<decltype(type)>;

        if constexpr (std::is_same_v<T, TypeInt>) {

            float src = registerAs<float>(insn.floatValueId);
            registerAs<uint32_t>(insn.resultId) = src;

        } else if constexpr (std::is_same_v<T, TypeVector>) {

            float* src = &registerAs<float>(insn.floatValueId);
            uint32_t* dst = &registerAs<uint32_t>(insn.resultId);
            for(int i = 0; i < type.count; i++) {
                dst[i] = src[i];
            }

        } else {

            std::cout << "Unknown type for ConvertFToS\n";

        }
    }, pgm->types.at(insn.type));
}

void Interpreter::stepAccessChain(const InsnAccessChain& insn)
{
    Pointer& basePointer = pointers.at(insn.baseId);
    uint32_t type = basePointer.type;
    size_t offset = basePointer.offset;
    for(auto& id: insn.indexesId) {
        int32_t j = registerAs<int32_t>(id);
        for(int i = 0; i < j; i++) {
            offset += pgm->typeSizes.at(pgm->getConstituentType(type, i));
        }
        type = pgm->getConstituentType(type, j);
    }
    if(false) {
        std::cout << "accesschain of " << basePointer.offset << " yielded " << offset << "\n";
    }
    uint32_t pointedType = std::get<TypePointer>(pgm->types.at(insn.type)).type;
    pointers[insn.resultId] = Pointer { pointedType, basePointer.storageClass, offset };
}

void Interpreter::stepFunctionParameter(const InsnFunctionParameter& insn)
{
    uint32_t sourceId = callstack.back(); callstack.pop_back();
    // XXX is this ever a register?
    pointers[insn.resultId] = pointers[sourceId];
    if(false) std::cout << "function parameter " << insn.resultId << " receives " << sourceId << "\n";
}

void Interpreter::stepReturn(const InsnReturn& insn)
{
    callstack.pop_back(); // return parameter not used.
    pc = callstack.back(); callstack.pop_back();
}

void Interpreter::stepReturnValue(const InsnReturnValue& insn)
{
    // Return value.
    uint32_t returnId = callstack.back(); callstack.pop_back();
    registers[returnId] = registers[insn.valueId];

    pc = callstack.back(); callstack.pop_back();
}

void Interpreter::stepFunctionCall(const InsnFunctionCall& insn)
{
    const Function& function = pgm->functions.at(insn.functionId);

    callstack.push_back(pc);
    callstack.push_back(insn.resultId);
    for(int i = insn.operandId.size() - 1; i >= 0; i--) {
        callstack.push_back(insn.operandId[i]);
    }
    pc = function.start;
}

void Interpreter::stepGLSLstd450Distance(const InsnGLSLstd450Distance& insn)
{
    std::visit([this, &insn](auto&& type) {

        using T = std::decay_t<decltype(type)>;

        if constexpr (std::is_same_v<T, TypeFloat>) {

            float p0 = registerAs<float>(insn.p0Id);
            float p1 = registerAs<float>(insn.p1Id);
            float radicand = (p1 - p0) * (p1 - p0);
            registerAs<float>(insn.resultId) = sqrtf(radicand);

        } else if constexpr (std::is_same_v<T, TypeVector>) {

            float* p0 = &registerAs<float>(insn.p0Id);
            float* p1 = &registerAs<float>(insn.p1Id);
            float radicand = 0;
            for(int i = 0; i < type.count; i++) {
                radicand += (p1[i] - p0[i]) * (p1[i] - p0[i]);
            }
            registerAs<float>(insn.resultId) = sqrtf(radicand);

        } else {

            std::cout << "Unknown type for Distance\n";

        }
    }, pgm->types.at(registers[insn.p0Id].type));
}

void Interpreter::stepGLSLstd450Length(const InsnGLSLstd450Length& insn)
{
    std::visit([this, &insn](auto&& type) {

        using T = std::decay_t<decltype(type)>;

        if constexpr (std::is_same_v<T, TypeFloat>) {

            float x = registerAs<float>(insn.xId);
            registerAs<float>(insn.resultId) = fabsf(x);

        } else if constexpr (std::is_same_v<T, TypeVector>) {

            float* x = &registerAs<float>(insn.xId);
            float length = 0;
            for(int i = 0; i < type.count; i++) {
                length += x[i]*x[i];
            }
            registerAs<float>(insn.resultId) = sqrtf(length);

        } else {

            std::cout << "Unknown type for Length\n";

        }
    }, pgm->types.at(registers[insn.xId].type));
}

void Interpreter::stepGLSLstd450FMax(const InsnGLSLstd450FMax& insn)
{
    std::visit([this, &insn](auto&& type) {

        using T = std::decay_t<decltype(type)>;

        if constexpr (std::is_same_v<T, TypeFloat>) {

            float x = registerAs<float>(insn.xId);
            float y = registerAs<float>(insn.yId);
            registerAs<float>(insn.resultId) = fmaxf(x, y);

        } else if constexpr (std::is_same_v<T, TypeVector>) {

            float* x = &registerAs<float>(insn.xId);
            float* y = &registerAs<float>(insn.yId);
            float* result = &registerAs<float>(insn.resultId);
            for(int i = 0; i < type.count; i++) {
                result[i] = fmaxf(x[i], y[i]);
            }

        } else {

            std::cout << "Unknown type for FMax\n";

        }
    }, pgm->types.at(registers[insn.xId].type));
}

void Interpreter::stepGLSLstd450FMin(const InsnGLSLstd450FMin& insn)
{
    std::visit([this, &insn](auto&& type) {

        using T = std::decay_t<decltype(type)>;

        if constexpr (std::is_same_v<T, TypeFloat>) {

            float x = registerAs<float>(insn.xId);
            float y = registerAs<float>(insn.yId);
            registerAs<float>(insn.resultId) = fminf(x, y);

        } else if constexpr (std::is_same_v<T, TypeVector>) {

            float* x = &registerAs<float>(insn.xId);
            float* y = &registerAs<float>(insn.yId);
            float* result = &registerAs<float>(insn.resultId);
            for(int i = 0; i < type.count; i++) {
                result[i] = fminf(x[i], y[i]);
            }

        } else {

            std::cout << "Unknown type for FMin\n";

        }
    }, pgm->types.at(registers[insn.xId].type));
}

void Interpreter::stepGLSLstd450Pow(const InsnGLSLstd450Pow& insn)
{
    std::visit([this, &insn](auto&& type) {

        using T = std::decay_t<decltype(type)>;

        if constexpr (std::is_same_v<T, TypeFloat>) {

            float x = registerAs<float>(insn.xId);
            float y = registerAs<float>(insn.yId);
            registerAs<float>(insn.resultId) = powf(x, y);

        } else if constexpr (std::is_same_v<T, TypeVector>) {

            float* x = &registerAs<float>(insn.xId);
            float* y = &registerAs<float>(insn.yId);
            float* result = &registerAs<float>(insn.resultId);
            for(int i = 0; i < type.count; i++) {
                result[i] = powf(x[i], y[i]);
            }

        } else {

            std::cout << "Unknown type for Pow\n";

        }
    }, pgm->types.at(registers[insn.xId].type));
}

void Interpreter::stepGLSLstd450Normalize(const InsnGLSLstd450Normalize& insn)
{
    std::visit([this, &insn](auto&& type) {

        using T = std::decay_t<decltype(type)>;

        if constexpr (std::is_same_v<T, TypeFloat>) {

            float x = registerAs<float>(insn.xId);
            registerAs<float>(insn.resultId) = x < 0 ? -1 : 1;

        } else if constexpr (std::is_same_v<T, TypeVector>) {

            float* x = &registerAs<float>(insn.xId);
            float length = 0;
            for(int i = 0; i < type.count; i++) {
                length += x[i]*x[i];
            }
            length = sqrtf(length);

            float* result = &registerAs<float>(insn.resultId);
            for(int i = 0; i < type.count; i++) {
                result[i] = length == 0 ? 0 : x[i]/length;
            }

        } else {

            std::cout << "Unknown type for Normalize\n";

        }
    }, pgm->types.at(registers[insn.xId].type));
}

void Interpreter::stepGLSLstd450Sin(const InsnGLSLstd450Sin& insn)
{
    std::visit([this, &insn](auto&& type) {

        using T = std::decay_t<decltype(type)>;

        if constexpr (std::is_same_v<T, TypeFloat>) {

            float x = registerAs<float>(insn.xId);
            registerAs<float>(insn.resultId) = sin(x);

        } else if constexpr (std::is_same_v<T, TypeVector>) {

            float* x = &registerAs<float>(insn.xId);
            float* result = &registerAs<float>(insn.resultId);
            for(int i = 0; i < type.count; i++) {
                result[i] = sin(x[i]);
            }

        } else {

            std::cout << "Unknown type for Sin\n";

        }
    }, pgm->types.at(registers[insn.xId].type));
}

void Interpreter::stepGLSLstd450Cos(const InsnGLSLstd450Cos& insn)
{
    std::visit([this, &insn](auto&& type) {

        using T = std::decay_t<decltype(type)>;

        if constexpr (std::is_same_v<T, TypeFloat>) {

            float x = registerAs<float>(insn.xId);
            registerAs<float>(insn.resultId) = cos(x);

        } else if constexpr (std::is_same_v<T, TypeVector>) {

            float* x = &registerAs<float>(insn.xId);
            float* result = &registerAs<float>(insn.resultId);
            for(int i = 0; i < type.count; i++) {
                result[i] = cos(x[i]);
            }

        } else {

            std::cout << "Unknown type for Cos\n";

        }
    }, pgm->types.at(registers[insn.xId].type));
}

void Interpreter::stepGLSLstd450Atan(const InsnGLSLstd450Atan& insn)
{
    std::visit([this, &insn](auto&& type) {

        using T = std::decay_t<decltype(type)>;

        if constexpr (std::is_same_v<T, TypeFloat>) {

            float y_over_x = registerAs<float>(insn.y_over_xId);
            registerAs<float>(insn.resultId) = atanf(y_over_x);

        } else if constexpr (std::is_same_v<T, TypeVector>) {

            float* y_over_x = &registerAs<float>(insn.y_over_xId);
            float* result = &registerAs<float>(insn.resultId);
            for(int i = 0; i < type.count; i++) {
                result[i] = atanf(y_over_x[i]);
            }

        } else {

            std::cout << "Unknown type for Atan\n";

        }
    }, pgm->types.at(registers[insn.y_over_xId].type));
}

void Interpreter::stepGLSLstd450Atan2(const InsnGLSLstd450Atan2& insn)
{
    std::visit([this, &insn](auto&& type) {

        using T = std::decay_t<decltype(type)>;

        if constexpr (std::is_same_v<T, TypeFloat>) {

            float y = registerAs<float>(insn.yId);
            float x = registerAs<float>(insn.xId);
            registerAs<float>(insn.resultId) = atan2f(y, x);

        } else if constexpr (std::is_same_v<T, TypeVector>) {

            float* y = &registerAs<float>(insn.yId);
            float* x = &registerAs<float>(insn.xId);
            float* result = &registerAs<float>(insn.resultId);
            for(int i = 0; i < type.count; i++) {
                result[i] = atan2f(y[i], x[i]);
            }

        } else {

            std::cout << "Unknown type for Atan2\n";

        }
    }, pgm->types.at(registers[insn.xId].type));
}

void Interpreter::stepGLSLstd450FAbs(const InsnGLSLstd450FAbs& insn)
{
    std::visit([this, &insn](auto&& type) {

        using T = std::decay_t<decltype(type)>;

        if constexpr (std::is_same_v<T, TypeFloat>) {

            float x = registerAs<float>(insn.xId);
            registerAs<float>(insn.resultId) = fabsf(x);

        } else if constexpr (std::is_same_v<T, TypeVector>) {

            float* x = &registerAs<float>(insn.xId);
            float* result = &registerAs<float>(insn.resultId);
            for(int i = 0; i < type.count; i++) {
                result[i] = fabsf(x[i]);
            }

        } else {

            std::cout << "Unknown type for FAbs\n";

        }
    }, pgm->types.at(registers[insn.xId].type));
}

void Interpreter::stepGLSLstd450Exp(const InsnGLSLstd450Exp& insn)
{
    std::visit([this, &insn](auto&& type) {

        using T = std::decay_t<decltype(type)>;

        if constexpr (std::is_same_v<T, TypeFloat>) {

            float x = registerAs<float>(insn.xId);
            registerAs<float>(insn.resultId) = expf(x);

        } else if constexpr (std::is_same_v<T, TypeVector>) {

            float* x = &registerAs<float>(insn.xId);
            float* result = &registerAs<float>(insn.resultId);
            for(int i = 0; i < type.count; i++) {
                result[i] = expf(x[i]);
            }

        } else {

            std::cout << "Unknown type for Exp\n";

        }
    }, pgm->types.at(registers[insn.xId].type));
}

void Interpreter::stepGLSLstd450Exp2(const InsnGLSLstd450Exp2& insn)
{
    std::visit([this, &insn](auto&& type) {

        using T = std::decay_t<decltype(type)>;

        if constexpr (std::is_same_v<T, TypeFloat>) {

            float x = registerAs<float>(insn.xId);
            registerAs<float>(insn.resultId) = exp2f(x);

        } else if constexpr (std::is_same_v<T, TypeVector>) {

            float* x = &registerAs<float>(insn.xId);
            float* result = &registerAs<float>(insn.resultId);
            for(int i = 0; i < type.count; i++) {
                result[i] = exp2f(x[i]);
            }

        } else {

            std::cout << "Unknown type for Exp2\n";

        }
    }, pgm->types.at(registers[insn.xId].type));
}

void Interpreter::stepGLSLstd450Floor(const InsnGLSLstd450Floor& insn)
{
    std::visit([this, &insn](auto&& type) {

        using T = std::decay_t<decltype(type)>;

        if constexpr (std::is_same_v<T, TypeFloat>) {

            float x = registerAs<float>(insn.xId);
            registerAs<float>(insn.resultId) = floor(x);

        } else if constexpr (std::is_same_v<T, TypeVector>) {

            float* x = &registerAs<float>(insn.xId);
            float* result = &registerAs<float>(insn.resultId);
            for(int i = 0; i < type.count; i++) {
                result[i] = floor(x[i]);
            }

        } else {

            std::cout << "Unknown type for Floor\n";

        }
    }, pgm->types.at(registers[insn.xId].type));
}

void Interpreter::stepGLSLstd450Fract(const InsnGLSLstd450Fract& insn)
{
    std::visit([this, &insn](auto&& type) {

        using T = std::decay_t<decltype(type)>;

        if constexpr (std::is_same_v<T, TypeFloat>) {

            float x = registerAs<float>(insn.xId);
            registerAs<float>(insn.resultId) = x - floor(x);

        } else if constexpr (std::is_same_v<T, TypeVector>) {

            float* x = &registerAs<float>(insn.xId);
            float* result = &registerAs<float>(insn.resultId);
            for(int i = 0; i < type.count; i++) {
                result[i] = x[i] - floor(x[i]);
            }

        } else {

            std::cout << "Unknown type for Fract\n";

        }
    }, pgm->types.at(registers[insn.xId].type));
}

// Returns the value x clamped to the range minVal to maxVal per the GLSL docs.
static float fclamp(float x, float minVal, float maxVal)
{
    return fminf(fmaxf(x, minVal), maxVal);
}

// Compute the smoothstep function per the GLSL docs. They say that the
// results are undefined if edge0 >= edge1, but we allow edge0 > edge1.
static float smoothstep(float edge0, float edge1, float x)
{
    if (edge0 == edge1) {
        return 0;
    }

    float t = fclamp((x - edge0)/(edge1 - edge0), 0.0, 1.0);

    return t*t*(3 - 2*t);
}

// Mixes between x and y according to a.
static float fmix(float x, float y, float a)
{
    return x*(1.0 - a) + y*a;
}

void Interpreter::stepGLSLstd450FClamp(const InsnGLSLstd450FClamp& insn)
{
    std::visit([this, &insn](auto&& type) {

        using T = std::decay_t<decltype(type)>;

        if constexpr (std::is_same_v<T, TypeFloat>) {

            float x = registerAs<float>(insn.xId);
            float minVal = registerAs<float>(insn.minValId);
            float maxVal = registerAs<float>(insn.maxValId);
            registerAs<float>(insn.resultId) = fclamp(x, minVal, maxVal);

        } else if constexpr (std::is_same_v<T, TypeVector>) {

            float* x = &registerAs<float>(insn.xId);
            float* minVal = &registerAs<float>(insn.minValId);
            float* maxVal = &registerAs<float>(insn.maxValId);
            float* result = &registerAs<float>(insn.resultId);
            for(int i = 0; i < type.count; i++) {
                result[i] = fclamp(x[i], minVal[i], maxVal[i]);
            }

        } else {

            std::cout << "Unknown type for FClamp\n";

        }
    }, pgm->types.at(registers[insn.xId].type));
}

void Interpreter::stepGLSLstd450FMix(const InsnGLSLstd450FMix& insn)
{
    std::visit([this, &insn](auto&& type) {

        using T = std::decay_t<decltype(type)>;

        if constexpr (std::is_same_v<T, TypeFloat>) {

            float x = registerAs<float>(insn.xId);
            float y = registerAs<float>(insn.yId);
            float a = registerAs<float>(insn.aId);
            registerAs<float>(insn.resultId) = fmix(x, y, a);

        } else if constexpr (std::is_same_v<T, TypeVector>) {

            float* x = &registerAs<float>(insn.xId);
            float* y = &registerAs<float>(insn.yId);
            float* a = &registerAs<float>(insn.aId);
            float* result = &registerAs<float>(insn.resultId);
            for(int i = 0; i < type.count; i++) {
                result[i] = fmix(x[i], y[i], a[i]);
            }

        } else {

            std::cout << "Unknown type for FMix\n";

        }
    }, pgm->types.at(registers[insn.xId].type));
}

void Interpreter::stepGLSLstd450SmoothStep(const InsnGLSLstd450SmoothStep& insn)
{
    std::visit([this, &insn](auto&& type) {

        using T = std::decay_t<decltype(type)>;

        if constexpr (std::is_same_v<T, TypeFloat>) {

            float edge0 = registerAs<float>(insn.edge0Id);
            float edge1 = registerAs<float>(insn.edge1Id);
            float x = registerAs<float>(insn.xId);
            registerAs<float>(insn.resultId) = smoothstep(edge0, edge1, x);

        } else if constexpr (std::is_same_v<T, TypeVector>) {

            float* edge0 = &registerAs<float>(insn.edge0Id);
            float* edge1 = &registerAs<float>(insn.edge1Id);
            float* x = &registerAs<float>(insn.xId);
            float* result = &registerAs<float>(insn.resultId);
            for(int i = 0; i < type.count; i++) {
                result[i] = smoothstep(edge0[i], edge1[i], x[i]);
            }

        } else {

            std::cout << "Unknown type for SmoothStep\n";

        }
    }, pgm->types.at(registers[insn.xId].type));
}

void Interpreter::stepGLSLstd450Step(const InsnGLSLstd450Step& insn)
{
    std::visit([this, &insn](auto&& type) {

        using T = std::decay_t<decltype(type)>;

        if constexpr (std::is_same_v<T, TypeFloat>) {

            float edge = registerAs<float>(insn.edgeId);
            float x = registerAs<float>(insn.xId);
            registerAs<float>(insn.resultId) = x < edge ? 0.0 : 1.0;

        } else if constexpr (std::is_same_v<T, TypeVector>) {

            float* edge = &registerAs<float>(insn.edgeId);
            float* x = &registerAs<float>(insn.xId);
            float* result = &registerAs<float>(insn.resultId);
            for(int i = 0; i < type.count; i++) {
                result[i] = x[i] < edge[i] ? 0.0 : 1.0;
            }

        } else {

            std::cout << "Unknown type for Step\n";

        }
    }, pgm->types.at(registers[insn.xId].type));
}

void Interpreter::stepGLSLstd450Cross(const InsnGLSLstd450Cross& insn)
{
    std::visit([this, &insn](auto&& type) {

        using T = std::decay_t<decltype(type)>;

        if constexpr (std::is_same_v<T, TypeVector>) {

            float* x = &registerAs<float>(insn.xId);
            float* y = &registerAs<float>(insn.yId);
            float* result = &registerAs<float>(insn.resultId);

            assert(type.count == 3);

            result[0] = x[1]*y[2] - y[1]*x[2];
            result[1] = x[2]*y[0] - y[2]*x[0];
            result[2] = x[0]*y[1] - y[0]*x[1];

        } else {

            std::cout << "Unknown type for Cross\n";

        }
    }, pgm->types.at(registers[insn.xId].type));
}

void Interpreter::stepGLSLstd450Reflect(const InsnGLSLstd450Reflect& insn)
{
    std::visit([this, &insn](auto&& type) {

        using T = std::decay_t<decltype(type)>;

        if constexpr (std::is_same_v<T, TypeFloat>) {

            float i = registerAs<float>(insn.iId);
            float n = registerAs<float>(insn.nId);

            float dot = n*i;

            registerAs<float>(insn.resultId) = i - 2.0*dot*n;

        } else if constexpr (std::is_same_v<T, TypeVector>) {

            float* i = &registerAs<float>(insn.iId);
            float* n = &registerAs<float>(insn.nId);
            float* result = &registerAs<float>(insn.resultId);

            float dot = dotProduct(n, i, type.count);

            for (int k = 0; k < type.count; k++) {
                result[k] = i[k] - 2.0*dot*n[k];
            }

        } else {

            std::cout << "Unknown type for Reflect\n";

        }
    }, pgm->types.at(registers[insn.iId].type));
}

void Interpreter::stepBranch(const InsnBranch& insn)
{
    pc = pgm->labels.at(insn.targetLabelId);
}

void Interpreter::stepBranchConditional(const InsnBranchConditional& insn)
{
    bool condition = registerAs<bool>(insn.conditionId);
    pc = pgm->labels.at(condition ? insn.trueLabelId : insn.falseLabelId);
}

void Interpreter::stepPhi(const InsnPhi& insn)
{
    Register& obj = registers[insn.resultId];
    uint32_t size = pgm->typeSizes.at(obj.type);

    bool found = false;
    for(int i = 0; !found && i < insn.operandId.size(); i += 2) {
        uint32_t srcId = insn.operandId[i];
        uint32_t parentId = insn.operandId[i + 1];

        if (parentId == previousBlockId) {
            const Register &src = registers[srcId];
            std::copy(src.data, src.data + size, obj.data);
            found = true;
        }
    }

    if (!found) {
        std::cout << "Error: Phi didn't find any label, previous " << previousBlockId
            << ", current " << currentBlockId << "\n";
        for(int i = 0; i < insn.operandId.size(); i += 2) {
            std::cout << "    " << insn.operandId[i + 1] << "\n";
        }
    }
}

Image *texture;

void Interpreter::stepImageSampleImplicitLod(const InsnImageSampleImplicitLod& insn)
{
    // uint32_t type; // result type
    // uint32_t resultId; // SSA register for result value
    // uint32_t sampledImageId; // operand from register
    // uint32_t coordinateId; // operand from register
    // uint32_t imageOperands; // ImageOperands (optional)
    float rgba[4];

    std::visit([this, &insn, &rgba](auto&& type) {

        using T = std::decay_t<decltype(type)>;

        if constexpr (std::is_same_v<T, TypeVector>) {

            assert(type.count == 2);

            auto [u, v] = registerAs<v2float>(insn.coordinateId);

            unsigned int s = std::clamp(static_cast<unsigned int>(u * texture->width), 0u, texture->width - 1);
            unsigned int t = std::clamp(static_cast<unsigned int>(v * texture->height), 0u, texture->height - 1);
            unsigned char *address = texture->getPixelAddress(s, t);
            for(int i = 0; i < 4; i++)
                rgba[i] = reinterpret_cast<float*>(address)[i];

        } else {

            std::cout << "Unhandled type for ImageSampleImplicitLod coordinate\n";

        }
    }, pgm->types.at(registers[insn.coordinateId].type));

    uint32_t resultType = std::get<TypeVector>(pgm->types.at(registers[insn.resultId].type)).type;

    std::visit([this, &insn, rgba](auto&& type) {

        using T = std::decay_t<decltype(type)>;

        if constexpr (std::is_same_v<T, TypeFloat>) {

            registerAs<v4float>(insn.resultId) = v4float { rgba[0], rgba[1], rgba[2], rgba[3] };

        // else if constexpr (std::is_same_v<T, TypeInt>) {


        } else {

            std::cout << "Unhandled type for ImageSampleImplicitLod result\n";

        }
    }, pgm->types.at(resultType));
}


void Interpreter::step()
{
    if(false) std::cout << "address " << pc << "\n";

    Instruction *instruction = pgm->instructions.at(pc++).get();

    // Update our idea of what block we're in. If we just switched blocks,
    // remember the previous one (for Phi).
    uint32_t thisBlockId = instruction->blockId;
    if (thisBlockId != currentBlockId) {
        // I'm not sure this is fully correct. For example, when returning
        // from a function this will set previousBlockId to a block in
        // the called function, but that's not right, it should always
        // point to a block within the current function. I don't think that
        // matters in practice because of the restricted locations where Phi
        // is placed.
        if(false) {
            std::cout << "Previous " << previousBlockId << ", current "
                << currentBlockId << ", new " << thisBlockId << "\n";
        }
        previousBlockId = currentBlockId;
        currentBlockId = thisBlockId;
    }

    instruction->step(this);
}

template <class T>
void Interpreter::set(SpvStorageClass clss, size_t offset, const T& v)
{
    objectInClassAt<T>(clss, offset) = v;
}

template <class T>
void Interpreter::get(SpvStorageClass clss, size_t offset, T& v)
{
    v = objectInClassAt<T>(clss, offset);
}

void Interpreter::run()
{
    currentBlockId = NO_BLOCK_ID;
    previousBlockId = NO_BLOCK_ID;

    // Copy constants to memory. They're treated like variables.
    for(auto& [id, constant]: pgm->constants) {
        registers[id] = constant;
    }

    // init Function variables with initializers before each invocation
    // XXX also need to initialize within function calls?
    for(auto& [id, var]: pgm->variables) {
        pointers[id] = Pointer { var.type, var.storageClass, var.offset };
        if(var.storageClass == SpvStorageClassFunction) {
            assert(var.initializer == NO_INITIALIZER); // XXX will do initializers later
        }
    }

    callstack.push_back(EXECUTION_ENDED); // caller PC
    callstack.push_back(NO_RETURN_REGISTER); // return register
    pc = pgm->mainFunction->start;

    do {
        step();
    } while(pc != EXECUTION_ENDED);
}

// -----------------------------------------------------------------------------------

struct Compiler
{
    const Program *pgm;

    Compiler(const Program *pgm) : pgm(pgm)
    {
        // Nothing.
    }

    void compile()
    {
        // pc = pgm->mainFunction->start;

        for(int pc = 0; pc < pgm->instructions.size(); pc++) {
            for(auto &function : pgm->functions) {
                if(pc == function.second.start) {
                    std::string name = pgm->cleanUpFunctionName(function.first);
                    std::cout
                        << "; ---------------------------- function " << name << "\n"
                        << name << ":\n";
                }
            }

            for(auto &label : pgm->labels) {
                if(pc == label.second) {
                    std::cout << "label" << label.first << ":\n";
                }
            }

            pgm->instructions.at(pc)->emit(this);
        }
    }

    // String for a virtual register ("r12" or more).
    std::string reg(uint32_t id) const {
        std::ostringstream ss;

        ss << "r" << id;

        auto name = pgm->names.find(id);
        if (name != pgm->names.end()) {
            ss << "{" << name->second << "}";
        }

        auto constant = pgm->constants.find(id);
        if (constant != pgm->constants.end()) {
            if (std::holds_alternative<TypeFloat>(pgm->types.at(constant->second.type))) {
                const float *f = reinterpret_cast<float *>(constant->second.data);
                ss << "{=" << *f << "}";
            }
        }

        auto r = pgm->virtToPhy.find(id);
        if (r != pgm->virtToPhy.end()) {
            if (r->second != NO_REGISTER) {
                ss << "{%" << r->second << "}";
            }
        }

        return ss.str();
    }

    void emitNotImplemented(const std::string &op)
    {
        std::ostringstream ss;

        ss << op << " not implemented";

        emit("", "nop", ss.str());
    }

    void emitBinaryOp(const std::string &opName, int op1, int op2, int result)
    {
        std::ostringstream ss;
        ss << opName << " " << reg(op1) << ", " << reg(op2) << ", " << reg(result);
        emit("", ss.str(), "");
    }

    void emit(const std::string &label, const std::string &op, const std::string comment)
    {
        std::ios oldState(nullptr);
        oldState.copyfmt(std::cout);

        std::cout
            << std::left
            << std::setw(10) << label
            << std::setw(30) << op
            << std::setw(0);
        if(!comment.empty()) {
            std::cout << "; " << comment;
        }
        std::cout << "\n";

        std::cout.copyfmt(oldState);
    }
};

// -----------------------------------------------------------------------------------

Instruction::Instruction(uint32_t resId)
    : blockId(NO_BLOCK_ID),
      resId(resId)
{
    // Nothing.
}

void Instruction::emit(Compiler *compiler)
{
    compiler->emitNotImplemented(name());
}

void InsnFAdd::emit(Compiler *compiler)
{
    compiler->emitBinaryOp("fadd", resultId, operand1Id, operand2Id);
}

void InsnFMul::emit(Compiler *compiler)
{
    compiler->emitBinaryOp("fmul", resultId, operand1Id, operand2Id);
}

void InsnFunctionCall::emit(Compiler *compiler)
{
    compiler->emit("", "push pc", "");
    for(int i = operandId.size() - 1; i >= 0; i--) {
        compiler->emit("", std::string("push ") + compiler->reg(operandId[i]), "");
    }
    compiler->emit("", std::string("call ") + compiler->pgm->cleanUpFunctionName(functionId), "");
    compiler->emit("", std::string("pop ") + compiler->reg(resultId), "");
}

void InsnFunctionParameter::emit(Compiler *compiler)
{
    compiler->emit("", std::string("pop ") + compiler->reg(resultId), "");
}

void InsnLoad::emit(Compiler *compiler)
{
    std::ostringstream ss;
    ss << "mov " << compiler->reg(resultId) << ", (" << compiler->reg(pointerId) << ")";
    compiler->emit("", ss.str(), "");
}

void InsnStore::emit(Compiler *compiler)
{
    std::ostringstream ss;
    ss << "mov (" << compiler->reg(pointerId) << "), " << compiler->reg(objectId);
    compiler->emit("", ss.str(), "");
}

void InsnBranch::emit(Compiler *compiler)
{
    std::ostringstream ss;
    ss << "jmp label" << targetLabelId;
    compiler->emit("", ss.str(), "");
}

void InsnReturn::emit(Compiler *compiler)
{
    compiler->emit("", "ret r0", "");
}

void InsnReturnValue::emit(Compiler *compiler)
{
    std::ostringstream ss;
    ss << "ret " << compiler->reg(valueId);
    compiler->emit("", ss.str(), "");
}

void InsnFOrdLessThanEqual::emit(Compiler *compiler)
{
    compiler->emitBinaryOp("lte", resultId, operand1Id, operand2Id);
}

void InsnBranchConditional::emit(Compiler *compiler)
{
    std::ostringstream ss;
    ss << "jmp " << compiler->reg(conditionId)
        << ", label" << trueLabelId
        << ", label" << falseLabelId;
    compiler->emit("", ss.str(), "");
}

// -----------------------------------------------------------------------------------

// Returns whether successful.
bool compileProgram(const Program &pgm)
{
    Compiler compiler(&pgm);

    compiler.compile();

    return true;
}

// -----------------------------------------------------------------------------------

void eval(Interpreter &interpreter, float x, float y, v4float& color)
{
    interpreter.clearPrivateVariables();
    interpreter.set(SpvStorageClassInput, 0, v2float {x, y}); // fragCoord is in #0 in preamble
    interpreter.run();
    interpreter.get(SpvStorageClassOutput, 0, color); // color is out #0 in preamble
}


std::string readFileContents(std::string shaderFileName)
{
    std::ifstream shaderFile(shaderFileName.c_str(), std::ios::in | std::ios::binary | std::ios::ate);
    if(!shaderFile.good()) {
        throw std::runtime_error("couldn't open file " + shaderFileName + " for reading");
    }
    std::ifstream::pos_type size = shaderFile.tellg();
    shaderFile.seekg(0, std::ios::beg);

    std::string text(size, '\0');
    shaderFile.read(&text[0], size);

    return text;
}

std::string readStdin()
{
    std::istreambuf_iterator<char> begin(std::cin), end;
    return std::string(begin, end);
}

void usage(const char* progname)
{
    printf("usage: %s [options] shader.frag\n", progname);
    printf("provide \"-\" as a filename to read from stdin\n");
    printf("options:\n");
    printf("\t-f S E    Render frames S through and including E\n");
    printf("\t-d W H    Render frame at size W by H\n");
    printf("\t-v        Print opcodes as they are parsed\n");
    printf("\t-g        Generate debugging information\n");
    printf("\t-O        Run optimizing passes\n");
    printf("\t-t        Throw an exception on first unimplemented opcode\n");
    printf("\t-n        Compile and load shader, but do not shade an image\n");
    printf("\t-S        show the disassembly of the SPIR-V code\n");
    printf("\t-c        compile to our own ISA\n");
    printf("\t--json    input file is a ShaderToy JSON file\n");
}

// Number of rows still left to shade (for progress report).
static std::atomic_int rowsLeft;

// Render rows starting at "startRow" every "skip".
void render(const Program *pgm, int startRow, int skip, Image* output, float when)
{
    Interpreter interpreter(pgm);

    // iResolution is uniform @0 in preamble
    interpreter.set(SpvStorageClassUniform, 0, v2float {static_cast<float>(output->width), static_cast<float>(output->height)});

    // iTime is uniform @8 in preamble
    interpreter.set(SpvStorageClassUniform, 8, when);

    // iMouse is uniform @16 in preamble, but we don't align properly, so ours is at 12.
    interpreter.set(SpvStorageClassUniform, 12, v4float {0, 0, 0, 0});

    for(int y = startRow; y < output->height; y += skip) {
        for(int x = 0; x < output->width; x++) {
            v4float color;
            // XXX what about pixel kill?  C64 demo requires it
            eval(interpreter, x + 0.5f, y + 0.5f, color);
            output->set(x, output->height - 1 - y, color);
        }

        rowsLeft--;
    }
}

// Thread to show progress to the user.
void showProgress(int totalRows, std::chrono::time_point<std::chrono::steady_clock> startTime)
{
    while(true) {
        int left = rowsLeft;
        if (left == 0) {
            break;
        }

        std::cout << left << " rows left of " << totalRows;

        // Estimate time left.
        if (left != totalRows) {
            auto now = std::chrono::steady_clock::now();
            auto elapsedTime = now - startTime;
            auto elapsedSeconds = double(elapsedTime.count())*
                std::chrono::steady_clock::period::num/
                std::chrono::steady_clock::period::den;
            auto secondsLeft = elapsedSeconds*left/(totalRows - left);

            std::cout << " (" << int(secondsLeft) << " seconds left)   ";
        }
        std::cout << "\r";
        std::cout.flush();

        std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    }

    // Clear the line.
    std::cout << "                                                             \r";
}

std::string getFilepathAdjacentToPath(const std::string& filename, std::string adjacent)
{
    std::string result;

#ifdef USE_CPP17_FILESYSTEM

    // filesystem still not available in XCode 2019/04/04
    std::filesystem::path adjacent_path(adjacent);
    std::filesystem::path adjacent_dirname = adjacent_path.parent_path();
    std::filesystem::path code_path(filename);

    if(code_path.is_relative()) {
        std::filesystem::path full_path(adjacent_dirname + code_path);
        result = full_path;
    }

#else

    if(filename[0] != '/') {
        // Assume relative path, get directory from JSON filename
        char *adjacent_copy = strdup(adjacent.c_str());;
        result = std::string(dirname(adjacent_copy)) + "/" + filename;
        free(adjacent_copy);
    }

#endif

    return result;
}

int main(int argc, char **argv)
{
    bool debug = false;
    bool disassemble = false;
    bool optimize = false;
    bool beVerbose = false;
    bool throwOnUnimplemented = false;
    bool doNotShade = false;
    bool inputIsJSON = false;
    bool compile = false;
    int imageStart = 0, imageEnd = 0;
    int imageWidth = 640/2; // ShaderToy default is 640
    int imageHeight = 360/2; // ShaderToy default is 360

    char *progname = argv[0];
    argv++; argc--;

    while(argc > 0 && argv[0][0] == '-') {
        if(strcmp(argv[0], "-g") == 0) {

            debug = true;
            argv++; argc--;

        } else if(strcmp(argv[0], "--json") == 0) {

            inputIsJSON = true;
            argv++; argc--;

        } else if(strcmp(argv[0], "-S") == 0) {

            disassemble = true;
            argv++; argc--;

        } else if(strcmp(argv[0], "-d") == 0) {

            if(argc < 3) {
                usage(progname);
                exit(EXIT_FAILURE);
            }
            imageWidth = atoi(argv[1]);
            imageHeight = atoi(argv[2]);
            argv += 3; argc -= 3;

        } else if(strcmp(argv[0], "-f") == 0) {

            if(argc < 3) {
                usage(progname);
                exit(EXIT_FAILURE);
            }
            imageStart = atoi(argv[1]);
            imageEnd = atoi(argv[2]);
            argv += 3; argc -= 3;

        } else if(strcmp(argv[0], "-v") == 0) {

            beVerbose = true;
            argv++; argc--;

        } else if(strcmp(argv[0], "-t") == 0) {

            throwOnUnimplemented = true;
            argv++; argc--;

        } else if(strcmp(argv[0], "-O") == 0) {

            optimize = true;
            argv++; argc--;

        } else if(strcmp(argv[0], "-n") == 0) {

            doNotShade = true;
            argv++; argc--;

        } else if(strcmp(argv[0], "-c") == 0) {

            compile = true;
            argv++; argc--;

        } else if(strcmp(argv[0], "-h") == 0) {

            usage(progname);
            exit(EXIT_SUCCESS);

        } else if(strcmp(argv[0], "-") == 0) {

            // Read from stdin.
            break;

        } else {

            usage(progname);
            exit(EXIT_FAILURE);

        }
    }

    if(argc < 1) {
        usage(progname);
        exit(EXIT_FAILURE);
    }

    std::string preambleFilename = "preamble.frag";
    std::string preamble = readFileContents(preambleFilename);
    std::string epilogueFilename = "epilogue.frag";
    std::string epilogue = readFileContents(epilogueFilename);

    std::string filename = argv[0];
    std::string text = readFileContents(filename);

    if(inputIsJSON) {
        json j = json::parse(text);

        // Go through the render passes.  If special key "codefile" is
        // present, use that file as the shader code instead of the value
        // of key "code".
        auto& passes = j["Shader"]["renderpass"];
        for (json::iterator it = passes.begin(); it != passes.end(); ++it) {
            auto& pass = *it;
            if(pass.find("codefile") != pass.end()) {
                std::string code_filename = pass["codefile"];

                code_filename = getFilepathAdjacentToPath(code_filename, filename);

                pass["full_code_filename"] = code_filename;
                pass["code"] = readFileContents(code_filename);
            }
        }

        auto& pass0 = j["Shader"]["renderpass"][0];

        // Do special casing for single pass, since that's all we handle now.
        if(pass0.find("full_code_filename") != pass0.end()) {
	    // If we find "full_code_filename", that's the
	    // fully-qualified absolute on-disk path we loaded.  Set
	    // that for debugging.
            filename = pass0["full_code_filename"];
        } else if(pass0.find("codefile") != pass0.end()) {
	    // If we find only "codefile", that's the on-disk path
	    // the JSON specified.  Set that for debugging.
            filename = pass0["codefile"];
        } else {
	    // If we find neither "codefile" nor "full_code_filename",
	    // then the shader came out of the renderpass "code", so
	    // at least use the pass name to aid debugging.
            filename = std::string("shader from pass ") + pass0["name"].get<std::string>();
        }
        text = pass0["code"];

        if(pass0.find("inputs") != pass0.end()) {
            auto& input0 = pass0["inputs"][0];
            if(input0.find("locally_saved") != input0.end()) {
                std::string asset_filename = getFilepathAdjacentToPath(input0["locally_saved"].get<std::string>(), filename);
                FILE *fp = fopen(asset_filename.c_str(), "rb");
                unsigned int textureWidth, textureHeight;
                float *textureData;
                if(!pnmRead(fp, &textureWidth, &textureHeight, &textureData, NULL, NULL)) {
                    std::cerr << "couldn't read image from " << asset_filename;
                    exit(EXIT_FAILURE);
                }
                texture = new Image(Image::FORMAT_R32G32B32A32_SFLOAT, Image::DIM_2D, textureWidth, textureHeight);
                unsigned char* s = reinterpret_cast<unsigned char*>(textureData);
                unsigned char* d = reinterpret_cast<unsigned char*>(texture->storage);
                std::copy(s, s + textureWidth * textureHeight * Image::getPixelSize(Image::FORMAT_R32G32B32A32_SFLOAT), d);
                fclose(fp);
                preamble = preamble + "layout (binding = 1) uniform sampler2D iChannel0;\n";
            }
        }
    }

    glslang::TShader *shader = new glslang::TShader(EShLangFragment);

    {
        const char* strings[3] = { preamble.c_str(), text.c_str(), epilogue.c_str() };
        const char* names[3] = { preambleFilename.c_str(), filename.c_str(), epilogueFilename.c_str() };
        shader->setStringsWithLengthsAndNames(strings, NULL, names, 3);
    }

    shader->setEnvInput(glslang::EShSourceGlsl, EShLangFragment, glslang::EShClientVulkan, glslang::EShTargetVulkan_1_0);

    shader->setEnvClient(glslang::EShClientVulkan, glslang::EShTargetVulkan_1_1);

    shader->setEnvTarget(glslang::EShTargetSpv, glslang::EShTargetSpv_1_3);

    EShMessages messages = (EShMessages)(EShMsgDefault | EShMsgSpvRules | EShMsgVulkanRules | EShMsgDebugInfo);

    glslang::TShader::ForbidIncluder includer;
    TBuiltInResource resources;

    resources = glslang::DefaultTBuiltInResource;

    ShInitialize();

    glslang::TProgram& program = *new glslang::TProgram;

    if (!shader->parse(&resources, 110, false, messages, includer)) {
        std::cerr << "compile failed\n";
        std::cerr << shader->getInfoLog();
        exit(EXIT_FAILURE);
    }

    program.addShader(shader);

    if(!program.link(messages)) {
        std::cerr << "link failed\n";
        std::cerr << program.getInfoLog();
        exit(EXIT_FAILURE);
    }

    std::vector<unsigned int> spirv;
    std::string warningsErrors;
    spv::SpvBuildLogger logger;
    glslang::SpvOptions options;
    options.generateDebugInfo = debug;
    options.disableOptimizer = !optimize;
    options.optimizeSize = false;
    glslang::TIntermediate *shaderInterm = program.getIntermediate(EShLangFragment);
    glslang::GlslangToSpv(*shaderInterm, spirv, &logger, &options);

    if(disassemble) {
        spv::Disassemble(std::cout, spirv);
    }

    if(false) {
        FILE *fp = fopen("spirv", "wb");
        fwrite(spirv.data(), 1, spirv.size() * 4, fp);
        fclose(fp);
    }

    Program pgm(throwOnUnimplemented, beVerbose);
    spv_context context = spvContextCreate(SPV_ENV_UNIVERSAL_1_3);
    spvBinaryParse(context, &pgm, spirv.data(), spirv.size(), Program::handleHeader, Program::handleInstruction, nullptr);

    if (pgm.hasUnimplemented) {
        exit(1);
    }

    pgm.postParse();

    if (compile) {
        bool success = compileProgram(pgm);
        exit(success ? EXIT_SUCCESS : EXIT_FAILURE);
    }

    if (doNotShade) {
        exit(EXIT_SUCCESS);
    }

    int threadCount = std::thread::hardware_concurrency();
    std::cout << "Using " << threadCount << " threads.\n";

    Image image(Image::FORMAT_R8G8B8_UNORM, Image::DIM_2D, imageWidth, imageHeight);

    for(int imageNumber = imageStart; imageNumber <= imageEnd; imageNumber++) {

        auto startTime = std::chrono::steady_clock::now();

        // Workers decrement rowsLeft at the end of each row.
        rowsLeft = image.height;

        std::vector<std::thread *> thread;

        // Generate the rows on multiple threads.
        for (int t = 0; t < threadCount; t++) {
            thread.push_back(new std::thread(render, &pgm, t, threadCount, &image, imageNumber / 60.0));
            // std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }

        // Progress information.
        thread.push_back(new std::thread(showProgress, image.height, startTime));

        // Wait for worker threads to quit.
        for (int t = 0; t < thread.size(); t++) {
            std::thread* td = thread.back();
            thread.pop_back();
            td->join();
        }

        auto endTime = std::chrono::steady_clock::now();
        auto elapsedTime = endTime - startTime;
        auto elapsedSeconds = double(elapsedTime.count())*
            std::chrono::steady_clock::period::num/
            std::chrono::steady_clock::period::den;
        std::cout << "Shading took " << elapsedSeconds << " seconds ("
            << long(image.width*image.height/elapsedSeconds) << " pixels per second)\n";

        std::ostringstream ss;
        ss << "image" << std::setfill('0') << std::setw(4) << imageNumber << std::setw(0) << ".ppm";
        std::ofstream imageFile(ss.str(), std::ios::out | std::ios::binary);
        imageFile << "P6 " << image.width << " " << image.height << " 255\n";
        imageFile.write(reinterpret_cast<char *>(image.getPixelAddress(0, 0)), 3 * image.width * image.height);
        imageFile.close();
    }

    exit(EXIT_SUCCESS);
}
