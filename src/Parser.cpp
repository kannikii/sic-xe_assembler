#include "../include/assembler.h"

SourceLine Parser::parseLine(const std::string& line) {
    SourceLine result;
    result.label = "";
    result.opcode = "";
    result.operand = "";
    
    if (line.empty() || line[0] == '#') {
        return result;
    }
    
    std::istringstream iss(line);
    std::string first, second;
    
    // 첫 번째 단어 읽기
    iss >> first;
    if (first.empty()) return result;
    
    // 라벨이 있는지 확인 (라인이 공백으로 시작하지 않으면 라벨)
    if (!startsWithWhitespace(line)) {
        result.label = first;
        iss >> second;
        result.opcode = second;
        // 나머지는 operand
        std::string rest;
        std::getline(iss, rest);
        result.operand = trim(rest);
    } else {
        result.opcode = first;
        // 나머지는 operand
        std::string rest;
        std::getline(iss, rest);
        result.operand = trim(rest);
    }
    
    return result;
}

std::string Parser::trim(const std::string& str) {
    size_t first = str.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) return "";
    size_t last = str.find_last_not_of(" \t\r\n");
    return str.substr(first, (last - first + 1));
}

bool Parser::startsWithWhitespace(const std::string& line) {
    return !line.empty() && (line[0] == ' ' || line[0] == '\t');
}
