#include "../include/assembler.h"

SYMTAB::SYMTAB() {}

bool SYMTAB::insert(const std::string& symbol, int address) {
    if (exists(symbol)) {
        std::cerr << "Error: Duplicate symbol '" << symbol << "'" << std::endl;
        return false;
    }
    table[symbol] = address;
    return true;
}

int SYMTAB::lookup(const std::string& symbol) const {
    auto it = table.find(symbol);
    if (it != table.end()) {
        return it->second;
    }
    return -1;
}

bool SYMTAB::exists(const std::string& symbol) const {
    return table.find(symbol) != table.end();
}

void SYMTAB::print() const {
    std::cout << "\n" << std::string(60, '=') << std::endl;
    std::cout << "SYMBOL TABLE (SYMTAB)" << std::endl;
    std::cout << std::string(60, '=') << std::endl;
    std::cout << std::left << std::setw(25) << "Symbol" 
              << std::setw(20) << "Address (Hex)" 
              << std::setw(15) << "Address (Dec)" << std::endl;
    std::cout << std::string(60, '-') << std::endl;
    
    for (const auto& entry : table) {
        std::cout << std::left << std::setw(25) << entry.first 
                  << "0x" << std::hex << std::uppercase 
                  << std::setw(18) << std::setfill('0') << std::setw(4)
                  << entry.second 
                  << std::dec << std::setw(15) << entry.second 
                  << std::setfill(' ') << std::endl;
    }
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
    file << std::left << std::setw(25) << "Symbol" 
         << std::setw(20) << "Address (Hex)" 
         << std::setw(15) << "Address (Dec)" << std::endl;
    file << std::string(60, '-') << std::endl;
    
    for (const auto& entry : table) {
        file << std::left << std::setw(25) << entry.first 
             << "0x" << std::hex << std::uppercase 
             << std::setw(18) << std::setfill('0') << std::setw(4)
             << entry.second 
             << std::dec << std::setw(15) << entry.second 
             << std::setfill(' ') << std::endl;
    }
    file << std::string(60, '=') << std::endl;
    file.close();
}
