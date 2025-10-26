// ========== src/Pass2.cpp (신규 파일) ==========
#include "../include/assembler.h"

Pass2::Pass2(OPTAB *opt, SYMTAB *sym, const std::vector<IntermediateLine> &intF,
             int start, int length, const std::string &progName)
    : optab(opt), symtab(sym), intFile(intF), startAddr(start),
      programLength(length), programName(progName), firstExecAddr(start),
      currentTextRecordStartAddr(0), currentTextRecordLength(0)
{
    // 레지스터 테이블 초기화
    registers["A"] = 0;
    registers["X"] = 1;
    registers["L"] = 2;
    registers["B"] = 3;
    registers["S"] = 4;
    registers["T"] = 5;
    registers["F"] = 6;
}

// ============================================================
// 목적 코드 생성 (메인 로직)
// ============================================================
std::string Pass2::generateObjectCode(IntermediateLine &line, int nextLoc)
{
    if (optab->isInstruction(line.opcode))
    {
        // 명령어 (Instruction)
        int format = optab->getFormat(line.opcode);

        // 참고: 제공된 Pass1 코드는 Format 4를 비표준 방식(operand[0] == '+')으로
        // 처리하려 하나, SRCFILE 예제에는 Format 4가 없으므로 Format 1, 2, 3만 구현합니다.

        switch (format)
        {
        case 1:
            return handleFormat1(line);
        case 2:
            return handleFormat2(line);
        case 3:
            return handleFormat3(line, nextLoc);
        default:
            std::cerr << "Error: Unknown format " << format << " for " << line.opcode << std::endl;
            return "";
        }
    }
    else
    {
        // 지시어 (Directive)
        return handleDirective(line);
    }
}

// ============================================================
// 포맷별 목적 코드 생성
// ============================================================

// Format 1: Opcode (8 bits)
std::string Pass2::handleFormat1(const IntermediateLine &line)
{
    return optab->getOpcode(line.opcode);
}

// Format 2: Opcode (8 bits) + r1 (4 bits) + r2 (4 bits)
std::string Pass2::handleFormat2(const IntermediateLine &line)
{
    std::string obj = optab->getOpcode(line.opcode);
    std::string op = line.operand;

    size_t comma = op.find(',');
    if (comma != std::string::npos)
    {
        // 2-register operand
        std::string r1_str = Parser::trim(op.substr(0, comma));
        std::string r2_str = Parser::trim(op.substr(comma + 1));

        int r1 = getRegisterNum(r1_str);
        int r2 = 0; // r2 기본값

        // SHIFTL/SHIFTR의 두 번째 피연산자는 숫자
        if (line.opcode == "SHIFTL" || line.opcode == "SHIFTR")
        {
            r2 = std::stoi(r2_str) - 1; // n-1 저장
        }
        else
        {
            r2 = getRegisterNum(r2_str);
        }

        obj += intToHex(r1, 1);
        obj += intToHex(r2, 1);
    }
    else
    {
        // 1-register operand (e.g., TIXR X, CLEAR S)
        std::string r1_str = Parser::trim(op);
        int r1 = getRegisterNum(r1_str);
        obj += intToHex(r1, 1);
        obj += "0"; // r2는 0
    }
    return obj;
}

// Format 3: Opcode (6b) + nixbpe (6b) + disp (12b)
std::string Pass2::handleFormat3(const IntermediateLine &line, int nextLoc)
{
    int opcode_val = hexStringToInt(optab->getOpcode(line.opcode));
    int n = 0, i = 0, x = 0, b = 0, p = 0, e = 0;
    int disp = 0;
    int target_addr = 0;

    std::string op = line.operand;
    std::string clean_op = op;

    // 1. n, i 플래그 기본값 설정
    if (op.empty())
    { // RSUB
        n = 1;
        i = 1;
        target_addr = 0;
    }
    else if (op[0] == '#')
    { // Immediate
        n = 0;
        i = 1;
        p = 0; // Immediate는 non-relative
        clean_op = op.substr(1);
    }
    else if (op[0] == '@')
    { // Indirect
        n = 1;
        i = 0;
        p = 1; // PC-relative가 기본
        clean_op = op.substr(1);
    }
    else
    { // Simple
        n = 1;
        i = 1;
        p = 1; // PC-relative가 기본
    }

    // 2. x 플래그 설정 (Indexed)
    size_t comma_x = clean_op.find(",X");
    if (comma_x != std::string::npos)
    {
        x = 1;
        clean_op = Parser::trim(clean_op.substr(0, comma_x));
    }

    // 3. Target Address 계산
    if (line.opcode == "RSUB")
    {
        p = 0; // RSUB는 주소 필드 0, non-relative
    }
    else if (symtab->exists(clean_op))
    {
        // 피연산자가 심볼 (e.g., J begin)
        target_addr = symtab->lookup(clean_op);
        // p=1 (PC-relative)는 위에서 이미 설정됨
    }
    else
    {
        // 피연산자가 심볼이 아님 -> 상수로 시도
        try
        {
            target_addr = std::stoi(clean_op);
            
            // [!] 여기가 핵심 수정 사항입니다.
            // 피연산자가 상수(숫자)이면, Simple/Direct 모드로 취급
            // (n=1, i=1은 이미 설정됨)
            // PC-relative(p=1)가 아닌 12-bit 주소(p=0)를 사용하도록 설정
            p = 0;
        }
        catch (const std::exception &)
        {
            // #, @ 없이 심볼 테이블에도 없는 피연산자
            std::cerr << "Error at 0x" << std::hex << line.location
                      << ": Symbol not found and not a number: " << clean_op << std::endl;
            target_addr = 0;
            p = 0;
        }
    }

    // 4. disp 계산 (모드에 따라)
    if (n == 0 && i == 1)
    { // Mode 1: Immediate (e.g. LDA #0)
        disp = target_addr;
    }
    else if (p == 1)
    { // Mode 2: PC-relative (e.g. J begin)
        int pc = nextLoc;
        int disp_pc = target_addr - pc;

        if (disp_pc >= -2048 && disp_pc <= 2047)
        {
            // PC-relative 성공
            b = 0;
            disp = disp_pc & 0xFFF; // 12비트 2's complement
        }
        else
        {
            // PC-relative 실패 (TODO: Base-relative 시도)
            p = 0;
            b = 0;
            disp = target_addr & 0xFFF; // 12-bit Direct로 fallback
        }
    }
    else
    { // Mode 3: Simple/Direct (p=0, b=0)
        // (e.g. RSUB, 또는 COMP 48)
        disp = target_addr & 0xFFF;
    }

    // 5. 조립
    int first_byte = opcode_val + (n << 1) + i;
    int flags = (x << 3) + (b << 2) + (p << 1) + e;
    int obj = (first_byte << 16) | (flags << 12) | (disp & 0xFFF);

    return intToHex(obj, 6);
}

// 지시어 처리 (WORD, BYTE, RESW, RESB)
std::string Pass2::handleDirective(const IntermediateLine& line) {
    std::string op = line.operand;
    
    if (line.opcode == "WORD") {
        int val = std::stoi(op);
        return intToHex(val, 6);
        
    } else if (line.opcode == "BYTE") {
        if (op.size() >= 3 && op[0] == 'C' && op[1] == '\'') {
            // C'...'
            std::string str_val = op.substr(2, op.length() - 3);
            std::string obj = "";
            for (char c : str_val) {
                obj += intToHex(static_cast<int>(c), 2);
            }
            return obj;
        } else if (op.size() >= 3 && op[0] == 'X' && op[1] == '\'') {
            // X'...'
            std::string hex_val = op.substr(2, op.length() - 3);
            // 헥사 코드가 홀수 길이면 앞에 0을 붙여 짝수로 만듦
            return (hex_val.length() % 2 == 0) ? hex_val : "0" + hex_val;
        }
    } else if (line.opcode == "RESW" || line.opcode == "RESB") {
        // T 레코드 분리
        return "";
    }else if (line.opcode == "ORG") {
        // ORG는 목적 코드 생성 없음, T 레코드 분리만 수행
        return "";
    }
    return "";
}

// ============================================================
// T 레코드 관리 헬퍼
// ============================================================

void Pass2::startNewTextRecord(int loc)
{
    flushTextRecord(); // 이전 레코드가 있다면 완료
    currentTextRecordStartAddr = loc;
    currentTextRecordLength = 0;
    currentTextRecord = "T" + intToHex(loc, 6);
}

void Pass2::appendToTextRecord(const std::string &objCode, int loc)
{
    if (objCode.empty())
    { // RESW, RESB
        flushTextRecord();
        return;
    }

    int codeBytes = objCode.length() / 2;

    // 현재 T 레코드가 꽉 찼거나(최대 30바이트), 주소가 연속적이지 않을 때
    if ((currentTextRecordLength + codeBytes > 30) || (loc != currentTextRecordStartAddr + currentTextRecordLength))
    {
        startNewTextRecord(loc);
    }

    currentTextRecord += objCode;
    currentTextRecordLength += codeBytes;
}

void Pass2::flushTextRecord()
{
    if (currentTextRecordLength > 0)
    {
        // T[주소(6)][길이(2)][코드...]
        currentTextRecord.insert(7, intToHex(currentTextRecordLength, 2));
        textRecords.push_back(currentTextRecord);
    }
    currentTextRecord = "";
    currentTextRecordLength = 0;
    currentTextRecordStartAddr = 0;
}

// ============================================================
// Pass 2 메인 실행 함수
// ============================================================

bool Pass2::execute()
{
    std::cout << "\n[Step 4] Running Pass 2..." << std::endl;

    // 1. H 레코드 생성
    std::string progNamePadded = programName;
    progNamePadded.resize(6, ' ');
    headerRecord = "H" + progNamePadded + intToHex(startAddr, 6) + intToHex(programLength, 6);

    // 2. T, E 레코드 생성
    for (size_t i = 0; i < intFile.size(); ++i)
    {
        IntermediateLine &line = intFile[i]; // objcode 저장을 위해 non-const 참조

        if (line.opcode == "START" || line.opcode == "ORG")
        {
            continue;
        }

        if (line.opcode == "END")
        {
            // E 레코드 생성
            if (!line.operand.empty())
            {
                firstExecAddr = symtab->lookup(line.operand);
            }
            endRecord = "E" + intToHex(firstExecAddr, 6);
            break;
        }

        // 목적 코드 생성
        int nextLoc = (i + 1 < intFile.size()) ? intFile[i + 1].location : line.location;
        std::string objCode = generateObjectCode(line, nextLoc);

        // 중간파일(리스트)에 목적 코드 저장
        line.objcode = objCode;

        // T 레코드에 추가
        appendToTextRecord(objCode, line.location);
    }

    // 3. 마지막 T 레코드 저장
    flushTextRecord();

    std::cout << "Pass 2 completed successfully" << std::endl;
    return true;
}

// ============================================================
// 파일 출력
// ============================================================

void Pass2::writeObjFile(const std::string &objFilename) const
{
    std::ofstream file(objFilename);
    if (!file.is_open())
    {
        std::cerr << "Error: Cannot write object file" << std::endl;
        return;
    }

    file << headerRecord << std::endl;
    for (const auto &tRec : textRecords)
    {
        file << tRec << std::endl;
    }
    file << endRecord << std::endl;

    file.close();
    std::cout << "\nObject file written: " << objFilename << std::endl;
}

void Pass2::printObjFile() const
{
    std::cout << "\n"
              << std::string(80, '=') << std::endl;
    std::cout << "OBJECT PROGRAM (OBJFILE)" << std::endl;
    std::cout << std::string(80, '=') << std::endl;
    std::cout << headerRecord << std::endl;
    for (const auto &tRec : textRecords)
    {
        std::cout << tRec << std::endl;
    }
    std::cout << endRecord << std::endl;
    std::cout << std::string(80, '=') << std::endl;
}

// INTFILE에 목적 코드가 채워진 '리스트 파일' 출력
void Pass2::printListingFile() const
{
    std::cout << "\n"
              << std::string(80, '=') << std::endl;
    std::cout << "PROGRAM LISTING (with Object Code)" << std::endl;
    std::cout << std::string(80, '=') << std::endl;
    std::cout << std::left
              << std::setw(10) << "LOC"
              << std::setw(10) << "LABEL"
              << std::setw(10) << "OPCODE"
              << std::setw(20) << "OPERAND"
              << "OBJCODE" << std::endl;
    std::cout << std::string(80, '-') << std::endl;

    for (const auto &line : intFile)
    {
        if (line.opcode == "START" || line.opcode == "END")
        {
            std::cout << "          " // no loc
                      << std::left << std::setfill(' ')
                      << std::setw(10) << line.label
                      << std::setw(10) << line.opcode
                      << std::setw(20) << line.operand << std::endl;
            continue;
        }

        if (line.hasLocation)
        {
            std::cout << "0x" << std::hex << std::uppercase
                      << std::setw(4) << std::setfill('0') << line.location << "  ";
        }
        else
        {
            std::cout << "          ";
        }

        std::cout << std::dec << std::left << std::setfill(' ')
                  << std::setw(10) << line.label
                  << std::setw(10) << line.opcode
                  << std::setw(20) << line.operand
                  << line.objcode << std::endl;
    }
    std::cout << std::string(80, '=') << std::endl;
}

// ============================================================
// 유틸리티 함수
// ============================================================

std::string Pass2::intToHex(int val, int width) const
{
    std::stringstream ss;
    // C++의 2의 보수 표현을 이용하여 음수도 올바르게 마스킹
    unsigned long long mask = (1ULL << (width * 4)) - 1;
    ss << std::hex << std::uppercase << std::setfill('0') << std::setw(width)
       << (static_cast<unsigned long long>(val) & mask);
    return ss.str();
}

int Pass2::hexStringToInt(const std::string &hexStr) const
{
    return std::stoi(hexStr, nullptr, 16);
}

int Pass2::getRegisterNum(const std::string &reg) const
{
    auto it = registers.find(reg);
    if (it != registers.end())
    {
        return it->second;
    }
    std::cerr << "Warning: Unknown register " << reg << std::endl;
    return 0; // 기본값 0 (A)
}