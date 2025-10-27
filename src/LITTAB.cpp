#include "../include/assembler.h"

LITTAB::LITTAB() {}

void LITTAB::insert(const std::string& literal) {
    if (exists(literal)) {
        return;
    }

    Literal lit;
    lit.name = literal;
    lit.value = literal.substr(1);  // '=' 제거
    lit.address = -1;
    lit.assigned = false;
    // 길이 계산
    std::string val = lit.value;
    int actualLength = 0;
    
    if (val.size() >= 3 && val[0] == 'C' && val[1] == '\'') {
        // C'...'
        size_t start = val.find('\'');
        size_t end = val.rfind('\'');
        actualLength = end - start - 1;
    } else if (val.size() >= 3 && val[0] == 'X' && val[1] == '\'') {
        // X'...'
        size_t start = val.find('\'');
        size_t end = val.rfind('\'');
        actualLength = (end - start - 1 + 1) / 2;
    } else {
        // 숫자 리터럴
        actualLength = 3;  // 기본 WORD
    }
    
    // 3바이트 미만이면 WORD(3바이트)로 처리
    lit.length = (actualLength < 3) ? 3 : actualLength;
    table.push_back(lit);
}

bool LITTAB::exists(const std::string& literal) const {
    for (const auto& lit : table) {
        if (lit.name == literal) {
            return true;
        }
    }
    return false;
}

void LITTAB::assignAddress(const std::string& literal, int addr) {
    for (auto& lit : table) {
        if (lit.name == literal) {
            lit.address = addr;
            lit.assigned = true;
            return;
        }
    }
}

int LITTAB::getAddress(const std::string& literal) const {
    for (const auto& lit : table) {
        if (lit.name == literal) {
            return lit.address;
        }
    }
    return -1;
}

int LITTAB::getLength(const std::string& literal) const {
    for (const auto& lit : table) {
        if (lit.name == literal) {
            return lit.length;
        }
    }
    return 0;
}

std::string LITTAB::getValue(const std::string& literal) const {
    for (const auto& lit : table) {
        if (lit.name == literal) {
            return lit.value;
        }
    }
    return "";
}

std::vector<Literal> LITTAB::getUnassignedLiterals() const {
    std::vector<Literal> unassigned;
    for (const auto& lit : table) {
        if (!lit.assigned) {
            unassigned.push_back(lit);
        }
    }
    return unassigned;
}

void LITTAB::print() const {
    std::cout << "\n" << std::string(70, '=') << std::endl;
    std::cout << "LITERAL TABLE (LITTAB)" << std::endl;
    std::cout << std::string(70, '=') << std::endl;
    std::cout << std::left 
              << std::setw(20) << "Literal" 
              << std::setw(20) << "Value"
              << std::setw(15) << "Address (Hex)"
              << std::setw(10) << "Length" << std::endl;
    std::cout << std::string(70, '-') << std::endl;
    
    for (const auto& lit : table) {
        std::cout << std::left << std::setw(20) << lit.name
                  << std::setw(20) << lit.value;
        if (lit.assigned) {
            std::cout << "0x" << std::hex << std::uppercase 
                      << std::setw(13) << std::setfill('0') << std::setw(4) << lit.address;
        } else {
            std::cout << std::setw(15) << "unassigned";
        }
        std::cout << std::dec << std::setw(10) << lit.length 
                  << std::setfill(' ') << std::endl;
    }
    std::cout << std::string(70, '=') << std::endl;
}

void LITTAB::writeToFile(const std::string& filename) const {
    std::ofstream file(filename);
    if (!file.is_open()) {
        std::cerr << "Error: Cannot write LITTAB file" << std::endl;
        return;
    }
    
    file << std::string(70, '=') << std::endl;
    file << "LITERAL TABLE (LITTAB)" << std::endl;
    file << std::string(70, '=') << std::endl;
    file << std::left 
         << std::setw(20) << "Literal" 
         << std::setw(20) << "Value"
         << std::setw(15) << "Address (Hex)"
         << std::setw(10) << "Length" << std::endl;
    file << std::string(70, '-') << std::endl;
    
    for (const auto& lit : table) {
        file << std::left << std::setw(20) << lit.name
             << std::setw(20) << lit.value;
        if (lit.assigned) {
            file << "0x" << std::hex << std::uppercase 
                 << std::setw(13) << std::setfill('0') << std::setw(4) << lit.address;
        } else {
            file << std::setw(15) << "unassigned";
        }
        file << std::dec << std::setw(10) << lit.length 
             << std::setfill(' ') << std::endl;
    }
    file << std::string(70, '=') << std::endl;
    file.close();
}