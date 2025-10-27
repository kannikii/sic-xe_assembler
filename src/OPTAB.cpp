#include "../include/assembler.h"

OPTAB::OPTAB() {}

int OPTAB::determineFormat(const std::string& mnemonic) {
    // Format 2 명령어들 (레지스터 연산)
    if (mnemonic == "ADDR" || mnemonic == "SUBR" || mnemonic == "MULR" || 
        mnemonic == "DIVR" || mnemonic == "COMPR" || mnemonic == "CLEAR" || 
        mnemonic == "RMO" || mnemonic == "SHIFTL" || mnemonic == "SHIFTR" || 
        mnemonic == "SVC" || mnemonic == "TIXR") {
        return 2;
    }
    // Format 1 명령어들 (피연산자 없음)
    if (mnemonic == "FIX" || mnemonic == "FLOAT" || mnemonic == "HIO" || 
        mnemonic == "NORM" || mnemonic == "SIO" || mnemonic == "TIO") {
        return 1;
    }
    // 나머지는 Format 3/4 (기본 3, + 접두사면 4)
    return 3;
}

bool OPTAB::load(const std::string& filename) {
    std::ifstream file(filename);
    if (!file.is_open()) {
        std::cerr << "Error: Cannot open OPTAB file: " << filename << std::endl;
        return false;
    }
    std::string line;
    int lineNum = 0;
    while (std::getline(file, line)) {
        lineNum++;
        
        // 주석과 빈 줄 제거
        if (line.empty() || line[0] == '#') continue;
        
        std::istringstream iss(line);
        std::string mnemonic, opcode;
        
        if (iss >> mnemonic >> opcode) {
            InstructionInfo info;
            info.opcode = opcode;
            info.format = determineFormat(mnemonic);
            table[mnemonic] = info;
        }
    }
    file.close();
    std::cout << "OPTAB loaded: " << table.size() << " instructions" << std::endl;
    return true;
}

bool OPTAB::isInstruction(const std::string& mnemonic) const {
    return table.find(mnemonic) != table.end();
}

std::string OPTAB::getOpcode(const std::string& mnemonic) const {
    auto it = table.find(mnemonic);
    if (it != table.end()) {
        return it->second.opcode;
    }
    return "";
}

int OPTAB::getFormat(const std::string& mnemonic) const {
    auto it = table.find(mnemonic);
    if (it != table.end()) {
        return it->second.format;
    }
    return 0;
}

void OPTAB::printTable() const {
    std::cout << "\n" << std::string(60, '=') << std::endl;
    std::cout << "OPERATION CODE TABLE (OPTAB)" << std::endl;
    std::cout << std::string(60, '=') << std::endl;
    std::cout << std::left << std::setw(15) << "Mnemonic" 
              << std::setw(10) << "Opcode" 
              << std::setw(10) << "Format" << std::endl;
    std::cout << std::string(60, '-') << std::endl;
    
    for (const auto& entry : table) {
        std::cout << std::left << std::setw(15) << entry.first 
                  << std::setw(10) << entry.second.opcode
                  << "Format " << entry.second.format << std::endl;
    }
    std::cout << std::string(60, '=') << std::endl;
}
