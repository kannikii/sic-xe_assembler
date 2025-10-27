#include "../include/assembler.h"

Pass1::Pass1(OPTAB *opt, SYMTAB *sym, LITTAB *lit)
    : optab(opt), symtab(sym), littab(lit), locctr(0), startAddr(0),
      programName(""), currentBlock("DEFAULT"), blockCounter(0)
{
    initializeBlocks();
}

void Pass1::initializeBlocks()
{
    ProgramBlock defaultBlock;
    defaultBlock.name = "DEFAULT";
    defaultBlock.number = 0;
    defaultBlock.startAddress = 0;
    defaultBlock.length = 0;
    defaultBlock.currentLocctr = 0;
    programBlocks["DEFAULT"] = defaultBlock;
    blockCounter = 1;
}

void Pass1::finalizeBlocks()
{
    // 1. 마지막으로 사용된 블록의 최종 locctr 저장
    programBlocks[currentBlock].currentLocctr = locctr;
    programBlocks[currentBlock].length = locctr;

    // 2. 모든 블록의 length 확정
    for (auto &blockPair : programBlocks)
    {
        blockPair.second.length = blockPair.second.currentLocctr;
    }
    
    // 3. 블록 번호 순서대로 정렬
    std::vector<ProgramBlock*> sortedBlocks(blockCounter);
    for (auto &blockPair : programBlocks)
    {
        sortedBlocks[blockPair.second.number] = &blockPair.second;
    }

    // 4. 블록 번호순으로 시작 주소 계산
    int currentAddr = startAddr;
    for (ProgramBlock* blockPtr : sortedBlocks)
    {
        if (blockPtr)
        {
            blockPtr->startAddress = currentAddr;
            currentAddr += blockPtr->length;

            std::cout << "Block [" << blockPtr->number << "] " << blockPtr->name
                      << ": Start=0x" << std::hex << std::uppercase << blockPtr->startAddress
                      << ", Length=0x" << blockPtr->length << std::dec << std::endl;
        }
    }
    
    // 5. SYMTAB의 심볼 주소를 절대 주소로 변환
    std::vector<std::string> symbols = symtab->getAllSymbols();
    for (const auto& symbol : symbols)
    {
        int offset = symtab->lookup(symbol);
        int blockNum = symtab->getBlockNumber(symbol);

        for (const auto &blockPair : programBlocks)
        {
            if (blockPair.second.number == blockNum)
            {
                int absoluteAddr = blockPair.second.startAddress + offset;
                symtab->updateAddress(symbol, absoluteAddr);
                break;
            }
        }
    }
}

int Pass1::getInstructionLength(const std::string &mnemonic, const std::string &operand)
{
    if (!optab->isInstruction(mnemonic))
    {
        return 0;
    }

    int format = optab->getFormat(mnemonic);

    if (!operand.empty() && operand[0] == '+')
    {
        return 4;
    }

    return format;
}

int Pass1::getDirectiveLength(const std::string &directive, const std::string &operand, SYMTAB *symtab)
{
    int value = 0;

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

    if (directive == "WORD")
    {
        return 3;
    }
    else if (directive == "RESW")
    {
        return 3 * value;
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
        return value;
    }
    else if (directive == "EQU")
    {
        return 0;
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
            locctr = 0;  // 블록 내부에서는 0부터 시작
            programBlocks[currentBlock].currentLocctr = 0;

            IntermediateLine intLine;
            intLine.location = startAddr;  // START는 절대 주소 표시
            intLine.label = parsed.label;
            intLine.opcode = parsed.opcode;
            intLine.operand = parsed.operand;
            intLine.objcode = "";
            intLine.hasLocation = true;
            intLine.isFormat4 = false;
            intLine.blockNumber = programBlocks[currentBlock].number;
            intFile.push_back(intLine);
            continue;
        }

        // EQU 처리
        if (parsed.opcode == "EQU")
        {
            if (parsed.label.empty())
            {
                std::cerr << "Error at line " << lineNum << ": EQU must have a label" << std::endl;
                continue;
            }

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

            if (!symtab->insert(parsed.label, value, programBlocks[currentBlock].number))
            {
                std::cerr << "Warning at line " << lineNum
                          << ": Duplicate symbol " << parsed.label << std::endl;
            }

            IntermediateLine intLine;
            intLine.location = 0;
            intLine.label = parsed.label;
            intLine.opcode = parsed.opcode;
            intLine.operand = parsed.operand;
            intLine.objcode = "";
            intLine.hasLocation = false;
            intLine.isFormat4 = false;
            intLine.blockNumber = programBlocks[currentBlock].number;
            intFile.push_back(intLine);

            continue;
        }

        // ORG 처리
        if (parsed.opcode == "ORG")
        {
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

            locctr = newLoc;
            programBlocks[currentBlock].currentLocctr = locctr;

            IntermediateLine intLine;
            intLine.location = locctr;
            intLine.label = parsed.label;
            intLine.opcode = parsed.opcode;
            intLine.operand = parsed.operand;
            intLine.objcode = "";
            intLine.hasLocation = true;
            intLine.isFormat4 = false;
            intLine.blockNumber = programBlocks[currentBlock].number;
            intFile.push_back(intLine);

            continue;
        }

        // USE 지시어 처리 (Program Blocks)
        if (parsed.opcode == "USE")
        {
            // 현재 블록의 최종 위치 저장
            programBlocks[currentBlock].currentLocctr = locctr;

            // 새 블록 이름 (비어있으면 DEFAULT)
            std::string newBlock = parsed.operand.empty() ? "DEFAULT" : parsed.operand;

            // 새 블록이 없으면 생성
            if (programBlocks.find(newBlock) == programBlocks.end())
            {
                ProgramBlock newProgramBlock;
                newProgramBlock.name = newBlock;
                newProgramBlock.number = blockCounter++;
                newProgramBlock.startAddress = 0;
                newProgramBlock.length = 0;
                newProgramBlock.currentLocctr = 0;
                programBlocks[newBlock] = newProgramBlock;
            }

            // 블록 전환
            currentBlock = newBlock;
            locctr = programBlocks[currentBlock].currentLocctr;  // 해당 블록의 현재 위치로 복원

            IntermediateLine intLine;
            intLine.location = locctr;
            intLine.label = parsed.label;
            intLine.opcode = parsed.opcode;
            intLine.operand = parsed.operand;
            intLine.objcode = "";
            intLine.hasLocation = false;
            intLine.isFormat4 = false;
            intLine.blockNumber = programBlocks[currentBlock].number;
            intFile.push_back(intLine);

            continue;
        }

        // LTORG 처리
        if (parsed.opcode == "LTORG")
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
            intLine.blockNumber = programBlocks[currentBlock].number;
            intFile.push_back(intLine);

            continue;
        }

        // BASE 지시어 처리
        if (parsed.opcode == "BASE")
        {
            IntermediateLine intLine;
            intLine.location = 0;
            intLine.label = parsed.label;
            intLine.opcode = parsed.opcode;
            intLine.operand = parsed.operand;
            intLine.objcode = "";
            intLine.hasLocation = false;
            intLine.isFormat4 = false;
            intLine.blockNumber = programBlocks[currentBlock].number;
            intFile.push_back(intLine);
            continue;
        }

        // NOBASE 지시어 처리
        if (parsed.opcode == "NOBASE")
        {
            IntermediateLine intLine;
            intLine.location = 0;
            intLine.label = parsed.label;
            intLine.opcode = parsed.opcode;
            intLine.operand = parsed.operand;
            intLine.objcode = "";
            intLine.hasLocation = false;
            intLine.isFormat4 = false;
            intLine.blockNumber = programBlocks[currentBlock].number;
            intFile.push_back(intLine);
            continue;
        }

        // END 처리
        if (parsed.opcode == "END")
        {
            processLTORG();
            finalizeBlocks();
            IntermediateLine intLine;
            intLine.location = 0;
            intLine.label = parsed.label;
            intLine.opcode = parsed.opcode;
            intLine.operand = parsed.operand;
            intLine.objcode = "";
            intLine.hasLocation = false;
            intLine.isFormat4 = false;
            intLine.blockNumber = programBlocks[currentBlock].number;
            intFile.push_back(intLine);
            break;
        }

        // 현재 위치 저장 (블록 내 상대 주소)
        int currentLoc = locctr;

        // 라벨이 있으면 SYMTAB에 추가 (블록 내 상대 주소로)
        if (!parsed.label.empty())
        {
            if (!symtab->insert(parsed.label, currentLoc, programBlocks[currentBlock].number))
            {
                std::cerr << "Warning at line " << lineNum
                          << ": Duplicate symbol " << parsed.label << std::endl;
            }
        }

        // 리터럴 검사
        if (!parsed.operand.empty() && parsed.operand[0] == '=')
        {
            std::string op = parsed.operand;

            size_t comma = op.find(',');
            if (comma != std::string::npos)
            {
                op = op.substr(0, comma);
            }

            if (op[0] == '#' || op[0] == '@')
            {
                op = op.substr(1);
            }

            if (op[0] == '=')
            {
                littab->insert(op);
            }
        }

        // 명령어 길이 계산
        int length = 0;
        if (optab->isInstruction(parsed.opcode))
        {
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

        // 중간파일에 추가 (블록 내 상대 주소로)
        IntermediateLine intLine;
        intLine.location = currentLoc;
        intLine.label = parsed.label;
        intLine.opcode = parsed.opcode;
        intLine.operand = parsed.operand;
        intLine.objcode = "";
        intLine.hasLocation = true;
        intLine.isFormat4 = parsed.isFormat4;
        intLine.blockNumber = programBlocks[currentBlock].number;
        intFile.push_back(intLine);

        // LOCCTR 증가 (블록별로 독립적으로 관리)
        locctr += length;
        programBlocks[currentBlock].currentLocctr = locctr;
    }

    file.close();
    std::cout << "Pass 1 completed: " << lineNum << " lines processed" << std::endl;
    return true;
}

void Pass1::processLTORG() {
    std::vector<Literal> unassigned = littab->getUnassignedLiterals();
    
    for (const auto& lit : unassigned) {
        littab->assignAddress(lit.name, locctr);
        
        IntermediateLine intLine;
        intLine.location = locctr;
        intLine.label = "*";
        intLine.opcode = lit.name;
        intLine.operand = lit.value;
        intLine.objcode = "";
        intLine.hasLocation = true;
        intLine.isFormat4 = false;
        intLine.blockNumber = programBlocks[currentBlock].number;
        intFile.push_back(intLine);
        
        locctr += lit.length;
        programBlocks[currentBlock].currentLocctr = locctr;
    }
}

const std::map<std::string, ProgramBlock>& Pass1::getProgramBlocks() const {
    return programBlocks;
}

int Pass1::getProgramLength() const {
    int totalLength = 0;
    for (const auto& blockPair : programBlocks) {
        totalLength += blockPair.second.length;
    }
    return totalLength;
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
        // START는 절대 주소로 표시, 나머지는 절대 주소 계산하여 표시
        if (line.hasLocation)
        {
            int absAddr;
            if (line.opcode == "START")
            {
                // START는 원래 저장된 주소 사용
                absAddr = line.location;
            }
            else
            {
                // 블록의 시작 주소 찾기
                int blockStartAddr = 0;
                for (const auto& blockPair : programBlocks)
                {
                    if (blockPair.second.number == line.blockNumber)
                    {
                        blockStartAddr = blockPair.second.startAddress;
                        break;
                    }
                }
                // 절대 주소 = 블록 시작 주소 + 블록 내 상대 주소
                absAddr = blockStartAddr + line.location;
            }
            
            file << "0x" << std::hex << std::uppercase
                 << std::setw(4) << std::setfill('0') << absAddr << "  ";
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
        // START는 절대 주소로 표시, 나머지는 블록 내 상대 주소로 표시
        if (line.hasLocation)
        {
            if (line.opcode == "START")
            {
                std::cout << "0x" << std::hex << std::uppercase
                          << std::setw(4) << std::setfill('0') << line.location << "  ";
            }
            else
            {
                std::cout << "0x" << std::hex << std::uppercase
                          << std::setw(4) << std::setfill('0') << line.location << "  ";
            }
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