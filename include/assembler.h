#ifndef ASSEMBLER_H
#define ASSEMBLER_H

#include <string>
#include <map>
#include <vector>
#include <fstream>
#include <sstream>
#include <iostream>
#include <iomanip>
#include <algorithm>

// ==================== OPTAB ====================
struct InstructionInfo {
    std::string opcode;
    int format;  // 1, 2, 3/4
};

class OPTAB {
private:
    std::map<std::string, InstructionInfo> table;
    
    // 자동으로 형식 결정
    int determineFormat(const std::string& mnemonic);

public:
    OPTAB();
    bool load(const std::string& filename);
    bool isInstruction(const std::string& mnemonic) const;
    std::string getOpcode(const std::string& mnemonic) const;
    int getFormat(const std::string& mnemonic) const;
    void printTable() const;
};

// ==================== SYMTAB ====================
class SYMTAB {
private:
    std::map<std::string, int> table;

public:
    SYMTAB();
    bool insert(const std::string& symbol, int address);
    int lookup(const std::string& symbol) const;
    bool exists(const std::string& symbol) const;
    void print() const;
    void writeToFile(const std::string& filename) const;
};

// ==================== Parser ====================
struct SourceLine {
    std::string label;
    std::string opcode;
    std::string operand;
};

class Parser {
public:
    static SourceLine parseLine(const std::string& line);
    static std::string trim(const std::string& str);
    static bool startsWithWhitespace(const std::string& line);
};

// ==================== Pass1 ====================
struct IntermediateLine {
    int location;
    std::string label;
    std::string opcode;
    std::string operand;
    std::string objcode;
    bool hasLocation;
};

class Pass1 {
private:
    OPTAB* optab;
    SYMTAB* symtab;
    std::vector<IntermediateLine> intFile;
    int locctr;
    int startAddr;
    std::string programName;
    
    int getInstructionLength(const std::string& mnemonic, const std::string& operand);
    int getDirectiveLength(const std::string& directive, const std::string& operand, SYMTAB* symtab);

public:
    Pass1(OPTAB* opt, SYMTAB* sym);
    bool execute(const std::string& srcFilename);
    void writeIntFile(const std::string& intFilename);
    void printIntFile() const;
    int getProgramLength() const;
    int getStartAddress() const;
    int getFinalLocctr() const;
    // ======== [추가] Pass 2에 데이터를 전달하기 위한 함수 ========
    const std::vector<IntermediateLine>& getIntFile() const;
    std::string getProgramName() const;
    // =======================================================
};

// ==================== [신규] Pass2 ====================
class Pass2 {
private:
    OPTAB* optab;
    SYMTAB* symtab;
    std::vector<IntermediateLine> intFile; // Pass1로부터 복사본
    int startAddr;
    int programLength;
    std::string programName;
    int firstExecAddr; // E 레코드용

    // H, T, E 레코드
    std::string headerRecord;
    std::vector<std::string> textRecords;
    std::string endRecord;

    // T 레코드 생성을 위한 버퍼
    std::string currentTextRecord;
    int currentTextRecordStartAddr;
    int currentTextRecordLength; // 바이트 단위
    
    // 레지스터 번호
    std::map<std::string, int> registers;

    // 목적 코드 생성
    std::string generateObjectCode(IntermediateLine& line, int nextLoc);
    std::string handleFormat1(const IntermediateLine& line);
    std::string handleFormat2(const IntermediateLine& line);
    std::string handleFormat3(const IntermediateLine& line, int nextLoc);
    std::string handleDirective(const IntermediateLine& line);

    // T 레코드 관리
    void startNewTextRecord(int loc);
    void appendToTextRecord(const std::string& objCode, int loc);
    void flushTextRecord();

    // 유틸리티
    std::string intToHex(int val, int width) const;
    int hexStringToInt(const std::string& hexStr) const;
    int getRegisterNum(const std::string& reg) const;

public:
    Pass2(OPTAB* opt, SYMTAB* sym, const std::vector<IntermediateLine>& intF, 
          int start, int length, const std::string& progName);
    bool execute();
    void writeObjFile(const std::string& objFilename) const;
    void printObjFile() const;
    void printListingFile() const; // INTFILE에 objcode가 채워진 것을 출력
};

#endif