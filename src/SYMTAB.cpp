#include "../include/assembler.h"
#include <sstream> // stringstream을 사용하기 위해 추가

SYMTAB::SYMTAB() : programBlocks(nullptr) {}

bool SYMTAB::insert(const std::string& symbol, int address, int blockNum) {
    if (exists(symbol)) {
        std::cerr << "Error: Duplicate symbol '" << symbol << "'" << std::endl;
        return false;
    }
    table[symbol] = std::make_pair(address, blockNum);
    return true;
}

void SYMTAB::setProgramBlocks(const std::map<std::string, ProgramBlock>* blocks) {
    programBlocks = blocks;
}

int SYMTAB::lookup(const std::string& symbol) const {
    auto it = table.find(symbol);
    if (it != table.end()) {
        return it->second.first;  // address
    }
    return -1;
}

int SYMTAB::getBlockNumber(const std::string& symbol) const {
    auto it = table.find(symbol);
    if (it != table.end()) {
        return it->second.second;  // blockNum
    }
    return -1;
}

bool SYMTAB::exists(const std::string& symbol) const {
    return table.find(symbol) != table.end();
}

std::vector<std::string> SYMTAB::getAllSymbols() const {
    std::vector<std::string> symbols;
    for (const auto& entry : table) {
        symbols.push_back(entry.first);
    }
    return symbols;
}

void SYMTAB::updateAddress(const std::string& symbol, int newAddress) {
    auto it = table.find(symbol);
    if (it != table.end()) {
        it->second.first = newAddress;  // address 업데이트
    }
}

void SYMTAB::print() const {
    std::cout << "\n" << std::string(60, '=') << std::endl;
    std::cout << "SYMBOL TABLE (SYMTAB)" << std::endl;
    std::cout << std::string(60, '=') << std::endl;
    std::cout << std::left << std::setw(20) << "Symbol" 
              << std::setw(15) << "Address"
              << std::setw(10) << "Block" << std::endl; // 헤더 너비
    std::cout << std::string(60, '-') << std::endl;
    
    // ▼▼▼ 수정된 출력 루프 ▼▼▼
    for (const auto& entry : table) {
        // 1. Symbol (width 20)
        std::cout << std::left << std::setw(20) << entry.first;
        
        // 2. Address (width 15)
        // stringstream을 사용해 주소 문자열("0xXXXX")을 먼저 만듭니다.
        std::stringstream ss;
        ss << "0x" << std::hex << std::uppercase 
           << std::setfill('0') << std::setw(4) << entry.second.first;
        
        // 주소 문자열을 왼쪽 정렬, 공백 채우기, 너비 15로 출력합니다.
        std::cout << std::left << std::setfill(' ') << std::setw(15) << ss.str();
        
        // 3. Block (width 10)
        // 10칸 너비로 블록 번호 출력
        std::cout << std::left << std::dec << std::setw(10) << entry.second.second << std::endl;
    }
    // ▲▲▲ 수정된 출력 루프 ▲▲▲

    std::cout << std::string(60, '=') << std::endl;
}

void SYMTAB::writeToFile(const std::string& filename) const {
    std::ofstream file(filename);
    if (!file.is_open()) {
        std::cerr << "Error: Cannot write SYMTAB file" << std::endl;
        return;
    }
    
    file << std::string(60, '=') << std::endl;
    file << "SYMBOL TABLE (SYMTAB)" << std::endl;
    file << std::string(60, '=') << std::endl;
    file << std::left << std::setw(20) << "Symbol" 
         << std::setw(15) << "Address"
         << std::setw(10) << "Block" << std::endl; // 헤더 너비
    file << std::string(60, '-') << std::endl;
    
    // ▼▼▼ 수정된 파일 쓰기 루프 ▼▼▼
    for (const auto& entry : table) {
        // 1. Symbol (width 20)
        file << std::left << std::setw(20) << entry.first;
        
        // 2. Address (width 15)
        std::stringstream ss;
        ss << "0x" << std::hex << std::uppercase 
           << std::setfill('0') << std::setw(4) << entry.second.first;
        
        file << std::left << std::setfill(' ') << std::setw(15) << ss.str();
        
        // 3. Block (width 10)
        file << std::left << std::dec << std::setw(10) << entry.second.second << std::endl;
    }
    // ▲▲▲ 수정된 파일 쓰기 루프 ▲▲▲

    file << std::string(60, '=') << std::endl;
    file.close();
}
