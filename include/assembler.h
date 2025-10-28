#ifndef ASSEMBLER_H
#define ASSEMBLER_H

#include <algorithm>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <map>
#include <sstream>
#include <string>
#include <vector>

struct ProgramBlock {
    std::string name;
    int number;
    int startAddress;
    int length;
    int currentLocctr;
};

// ==================== OPTAB ====================
struct InstructionInfo {
    std::string opcode;
    int format;
};

class OPTAB {
private:
    std::map<std::string, InstructionInfo> table;
    int determineFormat(const std::string &mnemonic);

public:
    OPTAB();
    bool load(const std::string &filename);
    bool isInstruction(const std::string &mnemonic) const;
    std::string getOpcode(const std::string &mnemonic) const;
    int getFormat(const std::string &mnemonic) const;
    void printTable() const;
};

// ==================== SYMTAB ====================
class SYMTAB {
private:
    std::map<std::string, std::pair<int, int>> table;
    const std::map<std::string, ProgramBlock> *programBlocks;

public:
    SYMTAB();
    bool insert(const std::string &symbol, int address, int blockNum);
    int lookup(const std::string &symbol) const;
    int getBlockNumber(const std::string &symbol) const;
    bool exists(const std::string &symbol) const;

    std::vector<std::string> getAllSymbols() const;
    void updateAddress(const std::string &symbol, int newAddress);
    void setProgramBlocks(const std::map<std::string, ProgramBlock> *blocks);
    void print() const;
    void writeToFile(const std::string &filename) const;
};

// ==================== LITERAL ====================
struct Literal {
    std::string name;
    std::string value;
    int address;
    int length;
    bool assigned;
};

class LITTAB {
private:
    std::vector<Literal> table;

public:
    LITTAB();
    void insert(const std::string &literal);
    bool exists(const std::string &literal) const;
    void assignAddress(const std::string &literal, int addr);
    int getAddress(const std::string &literal) const;
    int getLength(const std::string &literal) const;
    std::string getValue(const std::string &literal) const;
    std::vector<Literal> getUnassignedLiterals() const;
    void print() const;
    void writeToFile(const std::string &filename) const;
};

// ==================== Parser ====================
struct SourceLine {
    std::string label;
    std::string opcode;
    std::string operand;
    bool isFormat4;
};

class Parser {
public:
    static SourceLine parseLine(const std::string &line);
    static std::string trim(const std::string &str);
    static bool startsWithWhitespace(const std::string &line);
    static int evaluateExpression(const std::string &expr, SYMTAB *symtab);

private:
    static int parseOperand(const std::string &operand, SYMTAB *symtab);
};

// ==================== Pass1 ====================
struct IntermediateLine {
    int location;
    std::string label;
    std::string opcode;
    std::string operand;
    std::string objcode;
    bool hasLocation;
    bool isFormat4;
    int blockNumber;
};

class Pass1 {
private:
    OPTAB *optab;
    SYMTAB *symtab;
    LITTAB *littab;
    std::vector<IntermediateLine> intFile;
    int locctr;
    int startAddr;
    std::string programName;

    std::map<std::string, ProgramBlock> programBlocks;
    std::string currentBlock;
    int blockCounter;

    void processLTORG();
    int getInstructionLength(const std::string &mnemonic, const std::string &operand);
    int getDirectiveLength(const std::string &directive, const std::string &operand, SYMTAB *symtab);
    void initializeBlocks();
    void finalizeBlocks();

public:
    Pass1(OPTAB *opt, SYMTAB *sym, LITTAB *lit);
    bool execute(const std::string &srcFilename);
    void writeIntFile(const std::string &intFilename);
    void printIntFile() const;

    int getProgramLength() const;
    int getStartAddress() const;
    int getFinalLocctr() const;
    const std::vector<IntermediateLine> &getIntFile() const;
    std::string getProgramName() const;
    const std::map<std::string, ProgramBlock> &getProgramBlocks() const;
};

// ==================== Pass2 ====================
class Pass2 {
private:
    OPTAB *optab;
    SYMTAB *symtab;
    LITTAB *littab;
    std::vector<IntermediateLine> intFile;
    int startAddr;
    int programLength;
    std::string programName;
    int firstExecAddr;
    int baseRegister;
    std::map<std::string, ProgramBlock> programBlocks;

    std::string headerRecord;
    std::vector<std::string> textRecords;
    std::vector<std::string> modificationRecords;
    std::string endRecord;

    std::string currentTextRecord;
    int currentTextRecordStartAddr;
    int currentTextRecordLength;

    std::string currentBlockName;
    int currentBlockStartAddr;

    std::map<std::string, int> registers;

    // 🔧 헬퍼 함수 추가
    int getAbsoluteAddress(int blockNum, int offset) const;

    std::string generateObjectCode(IntermediateLine &line, int nextLoc);
    std::string handleFormat1(const IntermediateLine &line);
    std::string handleFormat2(const IntermediateLine &line);
    std::string handleFormat3(const IntermediateLine &line, int nextLoc);
    std::string handleFormat4(const IntermediateLine &line);
    std::string handleDirective(const IntermediateLine &line);

    void startNewTextRecord(int loc);
    void appendToTextRecord(const std::string &objCode, int loc);
    void flushTextRecord();

    std::string intToHex(int val, int width) const;
    int hexStringToInt(const std::string &hexStr) const;
    int getRegisterNum(const std::string &reg) const;
    void addModificationRecord(int address, int length);

public:
    Pass2(OPTAB *opt, SYMTAB *sym, LITTAB *lit,
          const std::vector<IntermediateLine> &intF,
          int start, int length, const std::string &progName,
          const std::map<std::string, ProgramBlock> &blocks);
    bool execute();
    void writeObjFile(const std::string &objFilename) const;
    void printObjFile() const;
    void printListingFile() const;
};

#endif