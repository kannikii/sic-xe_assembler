#include "../include/assembler.h"

Pass2::Pass2(OPTAB *opt, SYMTAB *sym, LITTAB *lit,
             const std::vector<IntermediateLine> &intF,
             int start, int length, const std::string &progName,
             const std::map<std::string, ProgramBlock> &blocks)
    : optab(opt), symtab(sym), littab(lit), intFile(intF), startAddr(start),
      programLength(length), programName(progName), firstExecAddr(start),
      currentTextRecordStartAddr(0), currentTextRecordLength(0),
      baseRegister(-1), programBlocks(blocks),
      currentBlockName("DEFAULT"),
      currentBlockStartAddr(start) {
    registers["A"] = 0;
    registers["X"] = 1;
    registers["L"] = 2;
    registers["B"] = 3;
    registers["S"] = 4;
    registers["T"] = 5;
    registers["F"] = 6;
}

int Pass2::getAbsoluteAddress(int blockNum, int offset) const {
    for (const auto &blockPair : programBlocks) {
        if (blockPair.second.number == blockNum) {
            return blockPair.second.startAddress + offset;
        }
    }
    return offset;
}

std::string Pass2::generateObjectCode(IntermediateLine &line, int nextLoc) {
    if (optab->isInstruction(line.opcode)) {
        if (line.isFormat4) {
            return handleFormat4(line);
        }
        int format = optab->getFormat(line.opcode);

        switch (format) {
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
    } else {
        return handleDirective(line);
    }
}

std::string Pass2::handleFormat1(const IntermediateLine &line) {
    return optab->getOpcode(line.opcode);
}

std::string Pass2::handleFormat2(const IntermediateLine &line) {
    std::string obj = optab->getOpcode(line.opcode);
    std::string op = line.operand;

    size_t comma = op.find(',');
    if (comma != std::string::npos) {
        std::string r1_str = Parser::trim(op.substr(0, comma));
        std::string r2_str = Parser::trim(op.substr(comma + 1));

        int r1 = getRegisterNum(r1_str);
        int r2 = 0;

        if (line.opcode == "SHIFTL" || line.opcode == "SHIFTR") {
            r2 = std::stoi(r2_str) - 1;
        } else {
            r2 = getRegisterNum(r2_str);
        }

        obj += intToHex(r1, 1);
        obj += intToHex(r2, 1);
    } else {
        std::string r1_str = Parser::trim(op);
        int r1 = getRegisterNum(r1_str);
        obj += intToHex(r1, 1);
        obj += "0";
    }
    return obj;
}

std::string Pass2::handleFormat3(const IntermediateLine &line, int nextLoc) {
    int opcode_val = hexStringToInt(optab->getOpcode(line.opcode));
    int n = 0, i = 0, x = 0, b = 0, p = 0, e = 0;
    int disp = 0;
    int target_addr = 0;

    std::string op = line.operand;
    std::string clean_op = op;

    int currentAbsAddr = getAbsoluteAddress(line.blockNumber, line.location);
    int nextAbsAddr = getAbsoluteAddress(line.blockNumber, nextLoc);

    if (op.empty()) {
        n = 1;
        i = 1;
        target_addr = 0;
        p = 0;
    } else if (op[0] == '#') {
        n = 0;
        i = 1;
        clean_op = op.substr(1);
    } else if (op[0] == '@') {
        n = 1;
        i = 0;
        clean_op = op.substr(1);
    } else {
        n = 1;
        i = 1;
    }

    size_t comma_x = clean_op.find(",X");
    if (comma_x != std::string::npos) {
        x = 1;
        clean_op = Parser::trim(clean_op.substr(0, comma_x));
    }

    if (line.opcode != "RSUB") {
        if (!clean_op.empty() && clean_op[0] == '=') {
            target_addr = littab->getAddress(clean_op);
        } else if (symtab->exists(clean_op)) {
            target_addr = symtab->lookup(clean_op);
        } else {
            try {
                target_addr = std::stoi(clean_op);
                if (n == 0 && i == 1) {
                    disp = target_addr & 0xFFF;
                    p = 0;
                    b = 0;
                }
            } catch (const std::exception &) {
                std::cerr << "Error at 0x" << std::hex << currentAbsAddr
                          << ": Symbol not found: " << clean_op << std::endl;
                target_addr = 0;
            }
        }
    }

    if (line.opcode == "RSUB") {
        disp = 0;
    } else if (n == 0 && i == 1) {
        if (disp == 0) {
            disp = target_addr & 0xFFF;
        }
    } else {
        int pc = nextAbsAddr;
        int disp_pc = target_addr - pc;

        if (disp_pc >= -2048 && disp_pc <= 2047) {
            p = 1;
            b = 0;
            disp = disp_pc & 0xFFF;
        } else {
            if (baseRegister != -1) {
                int disp_base = target_addr - baseRegister;

                if (disp_base >= 0 && disp_base <= 4095) {
                    p = 0;
                    b = 1;
                    disp = disp_base & 0xFFF;
                } else {
                    std::cerr << "Warning: Address 0x" << std::hex << target_addr
                              << " out of range for both PC and Base relative" << std::dec << std::endl;
                    p = 0;
                    b = 0;
                    disp = target_addr & 0xFFF;
                }
            } else {
                std::cerr << "Warning: PC-relative out of range and BASE not set for address 0x"
                          << std::hex << target_addr << std::dec << std::endl;
                p = 0;
                b = 0;
                disp = target_addr & 0xFFF;
            }
        }
    }

    int first_byte = opcode_val + (n << 1) + i;
    int flags = (x << 3) + (b << 2) + (p << 1) + e;
    int obj = (first_byte << 16) | (flags << 12) | (disp & 0xFFF);

    return intToHex(obj, 6);
}

std::string Pass2::handleFormat4(const IntermediateLine &line) {
    int opcode_val = hexStringToInt(optab->getOpcode(line.opcode));
    int n = 0, i = 0, x = 0, b = 0, p = 0, e = 1;
    int address = 0;

    std::string op = line.operand;
    std::string clean_op = op;

    if (op.empty()) {
        n = 1;
        i = 1;
        address = 0;
    } else if (op[0] == '#') {
        n = 0;
        i = 1;
        clean_op = op.substr(1);
    } else if (op[0] == '@') {
        n = 1;
        i = 0;
        clean_op = op.substr(1);
    } else {
        n = 1;
        i = 1;
    }

    size_t comma_x = clean_op.find(",X");
    if (comma_x != std::string::npos) {
        x = 1;
        clean_op = Parser::trim(clean_op.substr(0, comma_x));
    }

    p = 0;
    b = 0;

    bool needsModification = false;

    if (!clean_op.empty() && clean_op[0] == '=') {
        address = littab->getAddress(clean_op);
        needsModification = true;
    } else if (symtab->exists(clean_op)) {
        address = symtab->lookup(clean_op);
        needsModification = true;
    } else if (!clean_op.empty()) {
        try {
            address = std::stoi(clean_op);
            if (n == 0 && i == 1) {
                needsModification = false;
            } else {
                needsModification = true;
            }
        } catch (const std::exception &) {
            std::cerr << "Error: Invalid operand for Format 4: " << clean_op << std::endl;
            address = 0;
        }
    }

    int first_byte = opcode_val + (n << 1) + i;
    int flags = (x << 3) + (b << 2) + (p << 1) + e;
    unsigned int obj = (first_byte << 24) | (flags << 20) | (address & 0xFFFFF);

    int currentAbsAddr = getAbsoluteAddress(line.blockNumber, line.location);

    if (needsModification) {
        addModificationRecord(currentAbsAddr + 1, 5);
    }

    return intToHex(obj, 8);
}

std::string Pass2::handleDirective(const IntermediateLine &line) {
    std::string op = line.operand;

    if (line.opcode == "WORD") {
        if (symtab->exists(op)) {
            int val = symtab->lookup(op);
            int currentAbsAddr = getAbsoluteAddress(line.blockNumber, line.location);
            addModificationRecord(currentAbsAddr, 6);
            return intToHex(val, 6);
        } else {
            int val = std::stoi(op);
            return intToHex(val, 6);
        }
    } else if (line.opcode == "BYTE") {
        if (op.size() >= 3 && op[0] == 'C' && op[1] == '\'') {
            std::string str_val = op.substr(2, op.length() - 3);
            std::string obj = "";
            for (char c : str_val) {
                obj += intToHex(static_cast<int>(c), 2);
            }
            return obj;
        } else if (op.size() >= 3 && op[0] == 'X' && op[1] == '\'') {
            std::string hex_val = op.substr(2, op.length() - 3);
            return (hex_val.length() % 2 == 0) ? hex_val : "0" + hex_val;
        }
    } else if (line.opcode == "RESW" || line.opcode == "RESB") {
        return "";
    } else if (line.opcode == "ORG") {
        return "";
    }
    return "";
}

void Pass2::startNewTextRecord(int loc) {
    flushTextRecord();
    currentTextRecordStartAddr = loc;
    currentTextRecordLength = 0;
    currentTextRecord = "T" + intToHex(loc, 6);
}

void Pass2::appendToTextRecord(const std::string &objCode, int loc) {
    if (objCode.empty()) {
        flushTextRecord();
        return;
    }

    int codeBytes = objCode.length() / 2;

    if ((currentTextRecordLength + codeBytes > 30) ||
        (currentTextRecordLength > 0 && loc != currentTextRecordStartAddr + currentTextRecordLength)) {
        startNewTextRecord(loc);
    }

    if (currentTextRecordLength == 0) {
        currentTextRecordStartAddr = loc;
        currentTextRecord = "T" + intToHex(loc, 6);
    }

    currentTextRecord += objCode;
    currentTextRecordLength += codeBytes;
}

void Pass2::flushTextRecord() {
    if (currentTextRecordLength > 0) {
        std::string record = currentTextRecord.substr(0, 7) +
                             intToHex(currentTextRecordLength, 2) +
                             currentTextRecord.substr(7);
        textRecords.push_back(record);
    }
    currentTextRecord = "";
    currentTextRecordLength = 0;
    currentTextRecordStartAddr = 0;
}

void Pass2::addModificationRecord(int address, int length) {
    std::string mRecord = "M" + intToHex(address, 6) + intToHex(length, 2);
    modificationRecords.push_back(mRecord);
}

bool Pass2::execute() {
    std::cout << "\n[Step 5] Running Pass 2..." << std::endl;

    std::string progNamePadded = programName;
    progNamePadded.resize(6, ' ');
    headerRecord = "H" + progNamePadded + intToHex(startAddr, 6) + intToHex(programLength, 6);

    for (size_t i = 0; i < intFile.size(); ++i) {
        IntermediateLine &line = intFile[i];

        if (line.opcode == "START" || line.opcode == "ORG" || line.opcode == "LTORG") {
            continue;
        }

        if (line.opcode == "USE") {
            flushTextRecord();
            continue;
        }

        if (line.opcode == "BASE") {
            if (symtab->exists(line.operand)) {
                baseRegister = symtab->lookup(line.operand);
                std::cout << "Base register set to: 0x" << std::hex << baseRegister << std::dec << std::endl;
            } else {
                try {
                    baseRegister = std::stoi(line.operand, nullptr, 16);
                } catch (const std::exception &) {
                    std::cerr << "Error: Invalid BASE operand: " << line.operand << std::endl;
                }
            }
            continue;
        }

        if (line.opcode == "NOBASE") {
            baseRegister = -1;
            std::cout << "Base register unset" << std::endl;
            continue;
        }

        if (line.label == "*") {
            std::string litValue = littab->getValue(line.opcode);
            std::string objCode = "";
            int litLength = littab->getLength(line.opcode);

            if (litValue.size() >= 3 && litValue[0] == 'C' && litValue[1] == '\'') {
                std::string str_val = litValue.substr(2, litValue.length() - 3);
                for (char c : str_val) {
                    objCode += intToHex(static_cast<int>(c), 2);
                }
                while (objCode.length() < litLength * 2) {
                    objCode += "00";
                }
            } else if (litValue.size() >= 3 && litValue[0] == 'X' && litValue[1] == '\'') {
                std::string hex_val = litValue.substr(2, litValue.length() - 3);
                objCode = (hex_val.length() % 2 == 0) ? hex_val : "0" + hex_val;
                while (objCode.length() < litLength * 2) {
                    objCode += "00";
                }
            } else {
                try {
                    int val = std::stoi(litValue);
                    objCode = intToHex(val, 6);
                } catch (const std::exception &e) {
                    std::cerr << "Error: Invalid literal value " << litValue << std::endl;
                    objCode = "000000";
                }
            }

            line.objcode = objCode;
            int absAddr = getAbsoluteAddress(line.blockNumber, line.location);
            appendToTextRecord(objCode, absAddr);
            continue;
        }

        if (line.opcode == "END") {
            if (!line.operand.empty() && symtab->exists(line.operand)) {
                firstExecAddr = symtab->lookup(line.operand);
            }
            endRecord = "E" + intToHex(firstExecAddr, 6);
            break;
        }

        int nextLoc = line.location;
        if (i + 1 < intFile.size()) {
            IntermediateLine &nextLine = intFile[i + 1];
            if (nextLine.blockNumber == line.blockNumber && nextLine.hasLocation) {
                nextLoc = nextLine.location;
            } else {
                if (optab->isInstruction(line.opcode)) {
                    int format = line.isFormat4 ? 4 : optab->getFormat(line.opcode);
                    nextLoc = line.location + format;
                } else {
                    nextLoc = line.location;
                }
            }
        }

        std::string objCode = generateObjectCode(line, nextLoc);

        line.objcode = objCode;

        int absAddr = getAbsoluteAddress(line.blockNumber, line.location);
        appendToTextRecord(objCode, absAddr);
    }

    flushTextRecord();

    std::cout << "Pass 2 completed successfully" << std::endl;
    return true;
}

void Pass2::writeObjFile(const std::string &objFilename) const {
    std::ofstream file(objFilename);
    if (!file.is_open()) {
        std::cerr << "Error: Cannot write object file" << std::endl;
        return;
    }

    file << headerRecord << std::endl;
    for (const auto &tRec : textRecords) {
        file << tRec << std::endl;
    }
    for (const auto &mRec : modificationRecords) {
        file << mRec << std::endl;
    }
    file << endRecord << std::endl;

    file.close();
    std::cout << "\nObject file written: " << objFilename << std::endl;
}

void Pass2::printObjFile() const {
    std::cout << "\n"
              << std::string(80, '=') << std::endl;
    std::cout << "OBJECT PROGRAM (OBJFILE)" << std::endl;
    std::cout << std::string(80, '=') << std::endl;
    std::cout << headerRecord << std::endl;
    for (const auto &tRec : textRecords) {
        std::cout << tRec << std::endl;
    }
    for (const auto &mRec : modificationRecords) {
        std::cout << mRec << std::endl;
    }
    std::cout << endRecord << std::endl;
    std::cout << std::string(80, '=') << std::endl;
}

void Pass2::printListingFile() const {
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

    for (const auto &line : intFile) {
        if (line.opcode == "START" || line.opcode == "END") {
            std::cout << "          "
                      << std::left << std::setfill(' ')
                      << std::setw(10) << line.label
                      << std::setw(10) << line.opcode
                      << std::setw(20) << line.operand << std::endl;
            continue;
        }

        if (line.hasLocation) {
            // 절대 주소 계산 (블록 시작 주소 + 상대 주소)
            int absAddr = getAbsoluteAddress(line.blockNumber, line.location);
            std::cout << "0x" << std::hex << std::uppercase
                      << std::setw(4) << std::setfill('0') << absAddr << "  ";
        } else {
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

std::string Pass2::intToHex(int val, int width) const {
    std::stringstream ss;
    unsigned long long mask = (1ULL << (width * 4)) - 1;
    ss << std::hex << std::uppercase << std::setfill('0') << std::setw(width)
       << (static_cast<unsigned long long>(val) & mask);
    return ss.str();
}

int Pass2::hexStringToInt(const std::string &hexStr) const {
    return std::stoi(hexStr, nullptr, 16);
}

int Pass2::getRegisterNum(const std::string &reg) const {
    auto it = registers.find(reg);
    if (it != registers.end()) {
        return it->second;
    }
    std::cerr << "Warning: Unknown register " << reg << std::endl;
    return 0;
}