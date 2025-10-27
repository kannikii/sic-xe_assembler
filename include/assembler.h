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
struct InstructionInfo
{
    std::string opcode;
    int format; // 1, 2, 3/4
};

class OPTAB
{
private:
    std::map<std::string, InstructionInfo> table;

    // 자동으로 형식 결정
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
class SYMTAB
{
private:
    std::map<std::string, int> table;

public:
    SYMTAB();
    bool insert(const std::string &symbol, int address);
    int lookup(const std::string &symbol) const;
    bool exists(const std::string &symbol) const;
    void print() const;
    void writeToFile(const std::string &filename) const;
};

// ==================== LITERAL ====================
struct Literal
{
    std::string name;  // 예: =C'EOF'
    std::string value; // 예: C'EOF'
    int address;       // 할당된 주소
    int length;        // 바이트 길이
    bool assigned;     // 주소 할당 여부
};
// LITTAB 클래스 (SYMTAB 클래스 아래에 추가)
class LITTAB
{
private:
    std::vector<Literal> table;

public:
    LITTAB();
    void insert(const std::string &literal);                  // 리터럴 추가
    bool exists(const std::string &literal) const;            // 리터럴 존재 확인
    void assignAddress(const std::string &literal, int addr); // 주소 할당
    int getAddress(const std::string &literal) const;         // 주소 조회
    int getLength(const std::string &literal) const;          // 길이 조회
    std::string getValue(const std::string &literal) const;   // 값 조회
    std::vector<Literal> getUnassignedLiterals() const;       // 미할당 리터럴 반환
    void print() const;
    void writeToFile(const std::string &filename) const;
};

// ==================== Parser ====================
struct SourceLine
{
    std::string label;
    std::string opcode;
    std::string operand;
    bool isFormat4;
};

class Parser
{
public:
    static SourceLine parseLine(const std::string &line);
    static std::string trim(const std::string &str);
    static bool startsWithWhitespace(const std::string &line);

    // Expression 평가 함수 추가
    static int evaluateExpression(const std::string &expr, SYMTAB *symtab);

private:
    static int parseOperand(const std::string &operand, SYMTAB *symtab);
};

// ==================== Pass1 ====================
struct IntermediateLine
{
    int location;
    std::string label;
    std::string opcode;
    std::string operand;
    std::string objcode;
    bool hasLocation;
    bool isFormat4;
};

class Pass1
{
private:
    OPTAB *optab;
    SYMTAB *symtab;
    LITTAB *littab;
    std::vector<IntermediateLine> intFile;
    int locctr;
    int startAddr;
    std::string programName;

    int getInstructionLength(const std::string &mnemonic, const std::string &operand);
    int getDirectiveLength(const std::string &directive, const std::string &operand, SYMTAB *symtab);
    void processLTORG();

public:
    Pass1(OPTAB *opt, SYMTAB *sym, LITTAB *lit);
    bool execute(const std::string &srcFilename);
    void writeIntFile(const std::string &intFilename);
    void printIntFile() const;
    int getProgramLength() const;
    int getStartAddress() const;
    int getFinalLocctr() const;
    LITTAB *getLittab() const { return littab; }

    const std::vector<IntermediateLine> &getIntFile() const;
    std::string getProgramName() const;
    // =======================================================
};

// ==================== Pass2 ====================
class Pass2
{
private:
    OPTAB *optab;
    SYMTAB *symtab;
    LITTAB *littab;
    std::vector<IntermediateLine> intFile; // Pass1로부터 복사본
    int startAddr;
    int programLength;
    std::string programName;
    int firstExecAddr; // E 레코드용
    int baseRegister;   // Base register 값 (-1이면 미설정)

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
    std::string generateObjectCode(IntermediateLine &line, int nextLoc);
    std::string handleFormat1(const IntermediateLine &line);
    std::string handleFormat2(const IntermediateLine &line);
    std::string handleFormat3(const IntermediateLine &line, int nextLoc);
    std::string handleFormat4(const IntermediateLine& line);
    std::string handleDirective(const IntermediateLine &line);

    // T 레코드 관리
    void startNewTextRecord(int loc);
    void appendToTextRecord(const std::string &objCode, int loc);
    void flushTextRecord();

    // 유틸리티
    std::string intToHex(int val, int width) const;
    int hexStringToInt(const std::string &hexStr) const;
    int getRegisterNum(const std::string &reg) const;

public:
    Pass2(OPTAB *opt, SYMTAB *sym, LITTAB *lit,
          const std::vector<IntermediateLine> &intF,
          int start, int length, const std::string &progName);
    bool execute();
    void writeObjFile(const std::string &objFilename) const;
    void printObjFile() const;
    void printListingFile() const; // INTFILE에 objcode가 채워진 것을 출력
};

#endif