#include "../include/assembler.h"

SourceLine Parser::parseLine(const std::string &line) {
    SourceLine result;
    result.label = "";
    result.opcode = "";
    result.operand = "";
    result.isFormat4 = false;

    if (line.empty() || line[0] == '#') {
        return result;
    }
    std::istringstream iss(line);
    std::string first, second;

    // 첫 번째 단어 읽기
    iss >> first;
    if (first.empty())
        return result;
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
    // Format 4 체크 (opcode가 '+'로 시작)
    if (!result.opcode.empty() && result.opcode[0] == '+') {
        result.isFormat4 = true;
        result.opcode = result.opcode.substr(1); // '+' 제거
    }
    return result;
}

std::string Parser::trim(const std::string &str) {
    size_t first = str.find_first_not_of(" \t\r\n");
    if (first == std::string::npos)
        return "";
    size_t last = str.find_last_not_of(" \t\r\n");
    return str.substr(first, (last - first + 1));
}

bool Parser::startsWithWhitespace(const std::string &line) {
    return !line.empty() && (line[0] == ' ' || line[0] == '\t');
}

// 피연산자 파싱 (숫자 또는 심볼)
int Parser::parseOperand(const std::string &operand, SYMTAB *symtab) {
    std::string op = trim(operand);

    // 16진수 체크 (0x 접두사)
    if (op.size() > 2 && op.substr(0, 2) == "0x") {
        return std::stoi(op.substr(2), nullptr, 16);
    }

    // 숫자인지 확인
    bool isNumber = true;
    for (char c : op) {
        if (!isdigit(c) && c != '-') {
            isNumber = false;
            break;
        }
    }

    if (isNumber) {
        return std::stoi(op);
    }

    // 심볼인 경우
    if (symtab->exists(op)) {
        return symtab->lookup(op);
    }

    std::cerr << "Error: Undefined symbol or invalid operand: " << op << std::endl;
    return 0;
}

// 표현식 평가 (예: "BUFEND-BUFFER", "LENGTH+10", "MAXLEN-1")
int Parser::evaluateExpression(const std::string &expr, SYMTAB *symtab) {
    std::string expression = trim(expr);

    // 연산자 찾기 (우선순위: +, -, *, /)
    // 간단한 구현: 왼쪽에서 오른쪽으로 순차 처리

    // + 또는 - 찾기 (낮은 우선순위)
    for (size_t i = 1; i < expression.length(); i++) { // i=1부터 시작 (음수 방지)
        if (expression[i] == '+' || expression[i] == '-') {
            std::string left = expression.substr(0, i);
            std::string right = expression.substr(i + 1);

            int leftVal = evaluateExpression(left, symtab);
            int rightVal = evaluateExpression(right, symtab);

            if (expression[i] == '+') {
                return leftVal + rightVal;
            } else {
                return leftVal - rightVal;
            }
        }
    }

    // * 또는 / 찾기 (높은 우선순위)
    for (size_t i = 1; i < expression.length(); i++) {
        if (expression[i] == '*' || expression[i] == '/') {
            std::string left = expression.substr(0, i);
            std::string right = expression.substr(i + 1);

            int leftVal = evaluateExpression(left, symtab);
            int rightVal = evaluateExpression(right, symtab);

            if (expression[i] == '*') {
                return leftVal * rightVal;
            } else {
                if (rightVal == 0) {
                    std::cerr << "Error: Division by zero" << std::endl;
                    return 0;
                }
                return leftVal / rightVal;
            }
        }
    }

    // 연산자가 없으면 단일 피연산자
    return parseOperand(expression, symtab);
}