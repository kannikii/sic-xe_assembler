#include "../include/assembler.h"

Pass1::Pass1(OPTAB *opt, SYMTAB *sym, LITTAB *lit)
    : optab(opt), symtab(sym), littab(lit), locctr(0), startAddr(0), programName("") {}

int Pass1::getInstructionLength(const std::string &mnemonic, const std::string &operand)
{
    if (!optab->isInstruction(mnemonic))
    {
        return 0;
    }

    int format = optab->getFormat(mnemonic);

    // Format 4 체크 (+ 접두사가 있는 경우)
    if (!operand.empty() && operand[0] == '+')
    {
        return 4;
    }

    return format;
}

int Pass1::getDirectiveLength(const std::string &directive, const std::string &operand, SYMTAB *symtab)
{
    int value = 0;

    // 피연산자의 값을 확인 (숫자 or 심볼)
    if (!operand.empty())
    {
        try
        {
            value = Parser::evaluateExpression(operand, symtab);
        }
        catch (const std::exception &)
        {
            std::cerr << "Error: Invalid expression in directive "
                      << directive << ": " << operand << std::endl;
            value = 0;
        }
    }

    // 값(value)을 기반으로 길이 계산
    if (directive == "WORD")
    {
        return 3;
    }
    else if (directive == "RESW")
    {
        return 3 * value; // value = 1 (e.g. RESW 1)
    }
    else if (directive == "BYTE")
    {
        if (operand.size() >= 3 && operand[0] == 'C' && operand[1] == '\'')
        {
            size_t start = operand.find('\'');
            size_t end = operand.rfind('\'');
            if (start != std::string::npos && end != std::string::npos && end > start)
            {
                return end - start - 1;
            }
        }
        else if (operand.size() >= 3 && operand[0] == 'X' && operand[1] == '\'')
        {
            size_t start = operand.find('\'');
            size_t end = operand.rfind('\'');
            if (start != std::string::npos && end != std::string::npos && end > start)
            {
                return (end - start - 1 + 1) / 2;
            }
        }
    }
    else if (directive == "RESB")
    {
        return value; // value = 4096 (e.g. RESB BUFSIZE)
    }
    else if (directive == "EQU")
    {
        return 0; // EQU는 길이 없음
    }
    return 0;
}

bool Pass1::execute(const std::string &srcFilename)
{
    std::ifstream file(srcFilename);
    if (!file.is_open())
    {
        std::cerr << "Error: Cannot open source file: " << srcFilename << std::endl;
        return false;
    }

    std::string line;
    int lineNum = 0;

    while (std::getline(file, line))
    {
        lineNum++;

        // 빈 줄이나 주석 건너뛰기
        if (line.empty())
            continue;

        SourceLine parsed = Parser::parseLine(line);

        if (parsed.opcode.empty())
            continue;

        // START 처리
        if (parsed.opcode == "START")
        {
            programName = parsed.label;
            startAddr = std::stoi(parsed.operand, nullptr, 16);
            locctr = startAddr;

            IntermediateLine intLine;
            intLine.location = locctr;
            intLine.label = parsed.label;
            intLine.opcode = parsed.opcode;
            intLine.operand = parsed.operand;
            intLine.objcode = "";
            intLine.hasLocation = true;
            intLine.isFormat4 = false;
            intFile.push_back(intLine);
            continue;
        }
        // EQU 기계 독립적 기능 1
        if (parsed.opcode == "EQU")
        {
            if (parsed.label.empty())
            {
                std::cerr << "Error at line " << lineNum << ": EQU must have a label" << std::endl;
                continue; // 이 라인 무시
            }

            // (간단한 구현) 피연산자가 숫자라고 가정
            // TODO: 나중에 'Expressions' 기능을 구현할 때 여기를 수정해야 함
            int value = 0;
            try
            {
                value = Parser::evaluateExpression(parsed.operand, symtab);
            }
            catch (const std::exception &e)
            {
                std::cerr << "Error at line " << lineNum
                          << ": Invalid expression for EQU: " << parsed.operand << std::endl;
                continue;
            }

            // SYMTAB에 (레이블, 값) 삽입
            if (!symtab->insert(parsed.label, value))
            {
                std::cerr << "Warning at line " << lineNum
                          << ": Duplicate symbol " << parsed.label << std::endl;
            }

            // INTFILE에 기록
            IntermediateLine intLine;
            intLine.location = 0; // EQU는 특정 주소가 없음 (혹은 현재 LOCCTR)
            intLine.label = parsed.label;
            intLine.opcode = parsed.opcode;
            intLine.operand = parsed.operand;
            intLine.objcode = "";
            intLine.hasLocation = false; // 주소 미출력
            intLine.isFormat4 = false;
            intFile.push_back(intLine);

            continue; // LOCCTR 증가 로직을 건너뜀
        }
        // ORG 기계 독립적 기능 2
        if (parsed.opcode == "ORG")
        {
            // ORG는 LOCCTR를 변경
            int newLoc = 0;

            try
            {

                newLoc = Parser::evaluateExpression(parsed.operand, symtab);
            }
            catch (const std::exception &e)
            {
                std::cerr << "Error at line " << lineNum
                          << ": Invalid operand for ORG " << parsed.operand << std::endl;
                continue;
            }

            // LOCCTR 변경
            locctr = newLoc;

            // INTFILE에 기록
            IntermediateLine intLine;
            intLine.location = locctr;
            intLine.label = parsed.label;
            intLine.opcode = parsed.opcode;
            intLine.operand = parsed.operand;
            intLine.objcode = "";
            intLine.hasLocation = true; // ORG는 주소를 표시
            intLine.isFormat4 = false;
            intFile.push_back(intLine);

            continue; // 명령어 길이 계산 로직 건너뜀
        }
        // LTORG 처리
        if (parsed.opcode == "LTORG")
        {
            processLTORG(); // 리터럴 풀 생성

            // INTFILE에 LTORG 기록
            IntermediateLine intLine;
            intLine.location = 0;
            intLine.label = parsed.label;
            intLine.opcode = parsed.opcode;
            intLine.operand = parsed.operand;
            intLine.objcode = "";
            intLine.hasLocation = false;
            intLine.isFormat4 = false;
            intFile.push_back(intLine);

            continue;
        }

        // END 처리
        if (parsed.opcode == "END")
        {
            processLTORG();
            IntermediateLine intLine;
            intLine.location = 0;
            intLine.label = parsed.label;
            intLine.opcode = parsed.opcode;
            intLine.operand = parsed.operand;
            intLine.objcode = "";
            intLine.hasLocation = false;
            intLine.isFormat4 = false;
            intFile.push_back(intLine);
            break;
        }

        // 현재 위치 저장
        int currentLoc = locctr;

        // 라벨이 있으면 SYMTAB에 추가
        if (!parsed.label.empty())
        {
            if (!symtab->insert(parsed.label, currentLoc))
            {
                std::cerr << "Warning at line " << lineNum
                          << ": Duplicate symbol " << parsed.label << std::endl;
            }
        }
        // 리터럴 검사 (operand가 '='로 시작하면 리터럴)
        if (!parsed.operand.empty() && parsed.operand[0] == '=')
        {
            // 리터럴을 LITTAB에 추가
            std::string op = parsed.operand;

            // Indexed 모드 처리 (,X 제거)
            size_t comma = op.find(',');
            if (comma != std::string::npos)
            {
                op = op.substr(0, comma);
            }

            // Immediate/Indirect 모드 처리 (#, @ 제거)
            if (op[0] == '#' || op[0] == '@')
            {
                op = op.substr(1);
            }

            // 리터럴인지 확인
            if (op[0] == '=')
            {
                littab->insert(op);
            }
        }

        // 명령어 길이 계산
        int length = 0;
        if (optab->isInstruction(parsed.opcode))
        {
            // Format 4인 경우 4바이트, 아니면 기본 format
            if (parsed.isFormat4)
            {
                length = 4;
            }
            else
            {
                int format = optab->getFormat(parsed.opcode);
                length = format;
            }
        }
        else
        {
            length = getDirectiveLength(parsed.opcode, parsed.operand, symtab);
        }

        // 중간파일에 추가
        IntermediateLine intLine;
        intLine.location = currentLoc;
        intLine.label = parsed.label;
        intLine.opcode = parsed.opcode;
        intLine.operand = parsed.operand;
        intLine.objcode = "";
        intLine.hasLocation = true;
        intLine.isFormat4 = parsed.isFormat4; // 추가
        intFile.push_back(intLine);

        // LOCCTR 증가
        locctr += length;
    }

    file.close();
    std::cout << "Pass 1 completed: " << lineNum << " lines processed" << std::endl;
    return true;
}

void Pass1::processLTORG()
{
    // 미할당 리터럴들을 현재 LOCCTR 위치에 할당
    std::vector<Literal> unassigned = littab->getUnassignedLiterals();

    for (const auto &lit : unassigned)
    {
        // 리터럴에 주소 할당
        littab->assignAddress(lit.name, locctr);

        // 중간파일에 추가
        IntermediateLine intLine;
        intLine.location = locctr;
        intLine.label = "*"; // 리터럴 표시
        intLine.opcode = lit.name;
        intLine.operand = lit.value;
        intLine.objcode = "";
        intLine.hasLocation = true;
        intLine.isFormat4 = false;
        intFile.push_back(intLine);

        // LOCCTR 증가
        locctr += lit.length;
    }
}

void Pass1::writeIntFile(const std::string &intFilename)
{
    std::ofstream file(intFilename);
    if (!file.is_open())
    {
        std::cerr << "Error: Cannot write intermediate file" << std::endl;
        return;
    }

    for (const auto &line : intFile)
    {
        if (line.hasLocation)
        {
            file << "0x" << std::hex << std::uppercase
                 << std::setw(4) << std::setfill('0') << line.location << "  "; // 6 -> 4로 변경
        }
        else
        {
            file << "          ";
        }

        file << std::left << std::setfill(' ')
             << std::setw(10) << line.label
             << std::setw(10) << line.opcode
             << std::setw(20) << line.operand
             << line.objcode << std::endl;
    }

    file.close();
    std::cout << "Intermediate file written: " << intFilename << std::endl;
}

void Pass1::printIntFile() const
{
    std::cout << "\n"
              << std::string(80, '=') << std::endl;
    std::cout << "INTERMEDIATE FILE (INTFILE)" << std::endl;
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
        if (line.hasLocation)
        {
            std::cout << "0x" << std::hex << std::uppercase
                      << std::setw(4) << std::setfill('0') << line.location << "  "; // 6 -> 4로 변경
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

int Pass1::getProgramLength() const
{
    return locctr - startAddr;
}

int Pass1::getStartAddress() const
{
    return startAddr;
}

int Pass1::getFinalLocctr() const
{
    return locctr;
}

const std::vector<IntermediateLine> &Pass1::getIntFile() const
{
    return intFile;
}

std::string Pass1::getProgramName() const
{
    return programName;
}