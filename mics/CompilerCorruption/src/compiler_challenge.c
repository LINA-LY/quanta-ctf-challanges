#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

/*
  PHASE 1: TOKEN DEFINITIONS
*/
typedef enum {
    TOKEN_EOF = 0, TOKEN_IDENTIFIANT, TOKEN_NOMBRE, TOKEN_HEXADECIMAL,
    TOKEN_STRING, TOKEN_PROTOCOL, TOKEN_ENDPROTOCOL, TOKEN_KEYSPACE,
    TOKEN_MAIN, TOKEN_LOOP, TOKEN_FOR, TOKEN_BYTE, TOKEN_PLAIN,
    TOKEN_CIPHER, TOKEN_HASH, TOKEN_KEY256, TOKEN_PLUS, TOKEN_MINUS,
    TOKEN_MULT, TOKEN_DIV, TOKEN_EQUAL, TOKEN_EQUAL_EQUAL, TOKEN_NOT_EQUAL,
    TOKEN_LESS, TOKEN_GREATER, TOKEN_LESS_EQUAL, 
    TOKEN_GREATER_EQUAL, TOKEN_AND, TOKEN_OR,
    TOKEN_ENCRYPT, TOKEN_DECRYPT, TOKEN_HASH_OP,
    TOKEN_DOUBLE_COLON, TOKEN_SEMICOLON, TOKEN_BRACE_OPEN,
    TOKEN_BRACE_CLOSE, TOKEN_PAREN_OPEN, TOKEN_PAREN_CLOSE, 
    TOKEN_BRACKET_OPEN, TOKEN_BRACKET_CLOSE, TOKEN_ARROW_RIGHT, 
    TOKEN_ARROW_LEFT, TOKEN_COMMENT, TOKEN_ERROR
} TokenType;

typedef struct {
    TokenType type;
    char lexeme[256];
    int line;
    int column;
} Token;

/*
  PHASE 1: LEXICAL ANALYZER - GLOBAL VARIABLES
*/
char *source_code;
int pos = 0;
int line_lex = 1;
int column_lex = 1;
int lexical_errors = 0;
int syntax_errors = 0;
int semantic_errors = 0;

Token tokens[1000];
int token_count = 0;

char peek() { return source_code[pos]; }
char peek_next() { return (source_code[pos] == '\0') ? '\0' : source_code[pos + 1]; }
char advance() {
    char c = source_code[pos++];
    column_lex++;
    if (c == '\n') { line_lex++; column_lex = 1; }
    return c;
}
void skip_whitespace() { while (isspace(peek())) advance(); }
int is_letter(char c) { return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z'); }
int is_digit(char c) { return c >= '0' && c <= '9'; }
int is_hex_digit(char c) { return is_digit(c) || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F'); }

TokenType check_keyword(const char *lexeme) {
    if (strcmp(lexeme, "protocol") == 0) return TOKEN_PROTOCOL;
    if (strcmp(lexeme, "endprotocol") == 0) return TOKEN_ENDPROTOCOL;
    if (strcmp(lexeme, "keyspace") == 0) return TOKEN_KEYSPACE;
    if (strcmp(lexeme, "main") == 0) return TOKEN_MAIN;
    if (strcmp(lexeme, "loop") == 0) return TOKEN_LOOP;
    if (strcmp(lexeme, "for") == 0) return TOKEN_FOR;
    if (strcmp(lexeme, "byte") == 0) return TOKEN_BYTE;
    if (strcmp(lexeme, "plain") == 0) return TOKEN_PLAIN;
    if (strcmp(lexeme, "cipher") == 0) return TOKEN_CIPHER;
    if (strcmp(lexeme, "hash") == 0) return TOKEN_HASH;
    if (strcmp(lexeme, "key256") == 0) return TOKEN_KEY256;
    return TOKEN_IDENTIFIANT;
}

Token scan_hexadecimal() {
    Token token = {TOKEN_HEXADECIMAL, "", line_lex, column_lex};
    int i = 0;
    token.lexeme[i++] = advance();
    token.lexeme[i++] = advance();
    
    int has_error = 0;
    while (is_hex_digit(peek()) || (!has_error && !isspace(peek()) && peek() != ';' && peek() != '\n')) {
        char c = peek();
        if (!is_hex_digit(c) && !isspace(c) && c != ';' && c != '\n') {
            has_error = 1;
            printf("[LEXICAL ERROR] Line %d\n", line_lex);
            lexical_errors++;
        }
        token.lexeme[i++] = advance();
    }
    
    token.lexeme[i] = '\0';
    if (has_error) token.type = TOKEN_ERROR;
    return token;
}

Token scan_number() {
    Token token = {TOKEN_NOMBRE, "", line_lex, column_lex};
    int i = 0;
    while (is_digit(peek())) token.lexeme[i++] = advance();
    token.lexeme[i] = '\0';
    return token;
}

Token scan_identifier() {
    Token token = {TOKEN_IDENTIFIANT, "", line_lex, column_lex};
    int i = 0;
    if (is_letter(peek()) || peek() == '_') token.lexeme[i++] = advance();
    while (is_letter(peek()) || is_digit(peek()) || peek() == '_') 
        token.lexeme[i++] = advance();
    token.lexeme[i] = '\0';
    token.type = check_keyword(token.lexeme);
    return token;
}

Token scan_string() {
    Token token = {TOKEN_ERROR, "", line_lex, column_lex};
    int i = 0;
    int start_line = line_lex;
    
    token.lexeme[i++] = advance();
    
    while (peek() != '"' && peek() != '\0' && peek() != '\n') {
        if (i < 255) token.lexeme[i++] = advance();
        else { advance(); }
    }
    
    if (peek() == '"') {
        token.lexeme[i++] = advance();
        token.lexeme[i] = '\0';
        token.type = TOKEN_STRING;
    } else {
        printf("[LEXICAL ERROR] Line %d\n", start_line);
        strcpy(token.lexeme, "unterminated string");
        token.type = TOKEN_ERROR;
        lexical_errors++;
    }
    return token;
}

Token scan_comment() {
    Token token = {TOKEN_COMMENT, "//...", line_lex, column_lex};
    advance(); advance();
    while (peek() != '\n' && peek() != '\0') advance();
    return token;
}

Token get_next_token() {
    skip_whitespace();
    Token token = {TOKEN_ERROR, "", line_lex, column_lex};
    char c = peek();
    
    if (c == '\0') { token.type = TOKEN_EOF; strcpy(token.lexeme, "EOF"); return token; }
    if (c == '/' && peek_next() == '/') return scan_comment();
    if (c == '0' && (peek_next() == 'x' || peek_next() == 'X')) return scan_hexadecimal();
    if (is_digit(c)) return scan_number();
    if (c == '"') return scan_string();
    if (is_letter(c) || c == '_') return scan_identifier();
    
    token.lexeme[0] = advance();
    token.lexeme[1] = '\0';
    
    switch (token.lexeme[0]) {
        case '@':
            if (peek() == '>') { 
                advance(); 
                token.type = TOKEN_ENCRYPT; 
                strcpy(token.lexeme, "@>"); 
            }
            else if (is_letter(peek()) || peek() == '_') {
                Token id = scan_identifier();
                id.type = check_keyword(id.lexeme);
                token.lexeme[0] = '@';
                strncpy(token.lexeme + 1, id.lexeme, sizeof(token.lexeme) - 2);
                token.lexeme[sizeof(token.lexeme) - 1] = '\0';
                token.type = id.type;
                return token;
            }
            break;
        case '<':
            if (peek() == '@') { advance(); token.type = TOKEN_DECRYPT; strcpy(token.lexeme, "<@"); }
            else if (peek() == '=') { advance(); token.type = TOKEN_LESS_EQUAL; strcpy(token.lexeme, "<="); }
            else if (peek() == '-') { advance(); token.type = TOKEN_ARROW_LEFT; strcpy(token.lexeme, "<-"); }
            else token.type = TOKEN_LESS;
            break;
        case '>':
            if (peek() == '=') { advance(); token.type = TOKEN_GREATER_EQUAL; strcpy(token.lexeme, ">="); }
            else token.type = TOKEN_GREATER;
            break;
        case '#':
            if (peek() == '>') { advance(); token.type = TOKEN_HASH_OP; strcpy(token.lexeme, "#>"); }
            break;
        case '&':
            if (peek() == '&') { advance(); token.type = TOKEN_AND; strcpy(token.lexeme, "&&"); }
            break;
        case '|':
            if (peek() == '|') { advance(); token.type = TOKEN_OR; strcpy(token.lexeme, "||"); }
            break;
        case '!':
            if (peek() == '=') { advance(); token.type = TOKEN_NOT_EQUAL; strcpy(token.lexeme, "!="); }
            break;
        case '=':
            if (peek() == '=') { advance(); token.type = TOKEN_EQUAL_EQUAL; strcpy(token.lexeme, "=="); }
            else token.type = TOKEN_EQUAL;
            break;
        case '-':
            if (peek() == '>') { advance(); token.type = TOKEN_ARROW_RIGHT; strcpy(token.lexeme, "->"); }
            else token.type = TOKEN_MINUS;
            break;
        case ':':
            if (peek() == ':') { advance(); token.type = TOKEN_DOUBLE_COLON; strcpy(token.lexeme, "::"); }
            break;
        case '+': token.type = TOKEN_PLUS; break;
        case '*': token.type = TOKEN_MULT; break;
        case '/': token.type = TOKEN_DIV; break;
        case ';': token.type = TOKEN_SEMICOLON; break;
        case '{': token.type = TOKEN_BRACE_OPEN; break;
        case '}': token.type = TOKEN_BRACE_CLOSE; break;
        case '(': token.type = TOKEN_PAREN_OPEN; break;
        case ')': token.type = TOKEN_PAREN_CLOSE; break;
        case '[': token.type = TOKEN_BRACKET_OPEN; break;
        case ']': token.type = TOKEN_BRACKET_CLOSE; break;
    }
    return token;
}

void tokenize(const char* code) {
    source_code = (char*)code;
    pos = 0;
    line_lex = 1;
    column_lex = 1;
    token_count = 0;
    
    Token token;
    do {
        token = get_next_token();
        if (token.type != TOKEN_COMMENT && token.type != TOKEN_ERROR) {
            tokens[token_count++] = token;
        }
    } while (token.type != TOKEN_EOF && token_count < 1000);
}

/*
  PHASE 2: SYMBOL TABLE
*/
typedef struct {
    char name[256];
    char type[50];
    int declaration_line;
    int used;
} Symbol;

Symbol symbol_table[100];
int symbol_count = 0;

void add_symbol(const char* name, const char* type, int line) {
    for (int i = 0; i < symbol_count; i++) {
        if (strcmp(symbol_table[i].name, name) == 0) {
            printf("[SEMANTIC ERROR] Line %d\n", line);
            semantic_errors++;
            return;
        }
    }
    strcpy(symbol_table[symbol_count].name, name);
    strcpy(symbol_table[symbol_count].type, type);
    symbol_table[symbol_count].declaration_line = line;
    symbol_table[symbol_count].used = 0;
    symbol_count++;
}

char* get_type(const char* name, int line) {
    for (int i = 0; i < symbol_count; i++) {
        if (strcmp(symbol_table[i].name, name) == 0) {
            symbol_table[i].used = 1;
            return symbol_table[i].type;
        }
    }
    printf("[SEMANTIC ERROR] Line %d\n", line);
    semantic_errors++;
    return "error";
}

int types_compatible(const char* type1, const char* type2) {
    if (strcmp(type1, "error") == 0 || strcmp(type2, "error") == 0) return 1;
    return strcmp(type1, type2) == 0;
}

/*
  PHASE 3: INTERMEDIATE CODE GENERATION
*/
typedef struct {
    char operator[20];
    char arg1[50];
    char arg2[50];
    char result[50];
} Quadruple;

Quadruple quadruples[500];
int quadruple_count = 0;
int temp_count = 0;
int label_count = 0;

void generate_quadruple(const char* op, const char* arg1, const char* arg2, const char* res) {
    strcpy(quadruples[quadruple_count].operator, op);
    strcpy(quadruples[quadruple_count].arg1, arg1);
    strcpy(quadruples[quadruple_count].arg2, arg2);
    strcpy(quadruples[quadruple_count].result, res);
    quadruple_count++;
}

char* new_temp() {
    static char temp[20];
    sprintf(temp, "T%d", temp_count++);
    return temp;
}

char* new_label() {
    static char label[20];
    sprintf(label, "L%d", label_count++);
    return label;
}

/*
  PHASE 4: SYNTAX ANALYZER
*/
int token_index = 0;
Token current_token;

void advance_token() {
    if (token_index < token_count - 1) {
        token_index++;
        current_token = tokens[token_index];
    }
}

int check_token(TokenType type) {
    return current_token.type == type;
}

void expect_token(TokenType type, const char* message) {
    if (!check_token(type)) {
        printf("[SYNTAX ERROR] Line %d\n", current_token.line);
        syntax_errors++;
    } else {
        advance_token();
    }
}

typedef struct {
    char addr[50];
    char type[50];
} ExpressionInfo;

ExpressionInfo parse_expression();

ExpressionInfo parse_factor() {
    ExpressionInfo info = {"", ""};
    
    if (check_token(TOKEN_NOMBRE)) {
        strcpy(info.addr, current_token.lexeme);
        strcpy(info.type, "byte");
        advance_token();
    }
    else if (check_token(TOKEN_HEXADECIMAL)) {
        strcpy(info.addr, current_token.lexeme);
        strcpy(info.type, "key256");
        advance_token();
    }
    else if (check_token(TOKEN_STRING)) {
        strcpy(info.addr, current_token.lexeme);
        strcpy(info.type, "plain");
        advance_token();
    }
    else if (check_token(TOKEN_IDENTIFIANT)) {
        strcpy(info.addr, current_token.lexeme);
        char* var_type = get_type(current_token.lexeme, current_token.line);
        strcpy(info.type, var_type);
        advance_token();
    }
    else if (check_token(TOKEN_PAREN_OPEN)) {
        advance_token();
        info = parse_expression();
        expect_token(TOKEN_PAREN_CLOSE, "Expected ')'");
    }
    
    return info;
}

ExpressionInfo parse_term() {
    ExpressionInfo left = parse_factor();
    
    while (check_token(TOKEN_MULT) || check_token(TOKEN_DIV)) {
        char op = check_token(TOKEN_MULT) ? '*' : '/';
        advance_token();
        ExpressionInfo right = parse_factor();
        
        char* temp = new_temp();
        char op_str[2] = {op, '\0'};
        generate_quadruple(op_str, left.addr, right.addr, temp);
        
        strcpy(left.addr, temp);
        strcpy(left.type, "byte");
    }
    
    return left;
}

ExpressionInfo parse_arithmetic() {
    ExpressionInfo left = parse_term();
    
    while (check_token(TOKEN_PLUS) || check_token(TOKEN_MINUS)) {
        char op = check_token(TOKEN_PLUS) ? '+' : '-';
        advance_token();
        ExpressionInfo right = parse_term();
        
        char* temp = new_temp();
        char op_str[2] = {op, '\0'};
        generate_quadruple(op_str, left.addr, right.addr, temp);
        
        strcpy(left.addr, temp);
        strcpy(left.type, "byte");
    }
    
    return left;
}

ExpressionInfo parse_expression() {
    ExpressionInfo left = parse_arithmetic();
    
    if (check_token(TOKEN_ENCRYPT)) {
        advance_token();
        ExpressionInfo key = parse_arithmetic();
        char* temp = new_temp();
        generate_quadruple("@>", left.addr, key.addr, temp);
        strcpy(left.addr, temp);
        strcpy(left.type, "cipher");
    }
    else if (check_token(TOKEN_HASH_OP)) {
        advance_token();
        char* temp = new_temp();
        generate_quadruple("#>", left.addr, "", temp);
        strcpy(left.addr, temp);
        strcpy(left.type, "hash");
    }
    
    if (check_token(TOKEN_LESS) || check_token(TOKEN_GREATER) ||
        check_token(TOKEN_EQUAL_EQUAL) || check_token(TOKEN_NOT_EQUAL) ||
        check_token(TOKEN_LESS_EQUAL) || check_token(TOKEN_GREATER_EQUAL)) {
        
        char op[5];
        strcpy(op, current_token.lexeme);
        advance_token();
        ExpressionInfo right = parse_arithmetic();
        
        char* temp = new_temp();
        generate_quadruple(op, left.addr, right.addr, temp);
        strcpy(left.addr, temp);
        strcpy(left.type, "byte");
    }
    
    return left;
}

void parse_statement() {
    if (check_token(TOKEN_BYTE) || check_token(TOKEN_PLAIN) || 
        check_token(TOKEN_CIPHER) || check_token(TOKEN_HASH) ||
        check_token(TOKEN_KEY256)) {
        
        char var_type[50];
        strcpy(var_type, current_token.lexeme);
        advance_token();
        
        if (check_token(TOKEN_DOUBLE_COLON)) {
            advance_token();
            
            if (check_token(TOKEN_IDENTIFIANT)) {
                char var_name[256];
                strcpy(var_name, current_token.lexeme);
                int line = current_token.line;
                advance_token();
                
                add_symbol(var_name, var_type, line);
                
                if (check_token(TOKEN_EQUAL)) {
                    advance_token();
                    ExpressionInfo expr = parse_expression();
                    
                    if (!types_compatible(var_type, expr.type)) {
                        printf("[SEMANTIC ERROR] Line %d: Type mismatch - expected '%s', found '%s'\n", 
                               line, var_type, expr.type);
                        semantic_errors++;
                    }
                    generate_quadruple("=", expr.addr, "", var_name);
                }
            }
            expect_token(TOKEN_SEMICOLON, "Expected ';' after declaration");
        }
    }
    else if (check_token(TOKEN_IDENTIFIANT)) {
        char var_name[256];
        strcpy(var_name, current_token.lexeme);
        int line = current_token.line;
        char* var_type = get_type(var_name, line);
        advance_token();
        
        expect_token(TOKEN_EQUAL, "Expected '=' for assignment");
        ExpressionInfo expr = parse_expression();
        
        if (!types_compatible(var_type, expr.type)) {
            printf("[SEMANTIC ERROR] Line %d\n", line);
            semantic_errors++;
        }
        generate_quadruple("=", expr.addr, "", var_name);
        expect_token(TOKEN_SEMICOLON, "Expected ';' after assignment");
    }
    else if (check_token(TOKEN_ARROW_RIGHT)) {
        advance_token();
        ExpressionInfo expr = parse_expression();
        generate_quadruple("OUT", expr.addr, "", "");
        expect_token(TOKEN_SEMICOLON, "Expected ';' after output");
    }
    else if (check_token(TOKEN_ARROW_LEFT)) {
        advance_token();
        if (check_token(TOKEN_IDENTIFIANT)) {
            char var_name[256];
            strcpy(var_name, current_token.lexeme);
            get_type(var_name, current_token.line);
            advance_token();
            generate_quadruple("IN", "", "", var_name);
        }
        expect_token(TOKEN_SEMICOLON, "Expected ';' after input");
    }
    else if (check_token(TOKEN_LOOP)) {
        advance_token();
        expect_token(TOKEN_BRACKET_OPEN, "Expected '['");
        
        char* start = new_label();
        generate_quadruple("LABEL", start, "", "");
        
        ExpressionInfo cond = parse_expression();
        
        char* end = new_label();
        generate_quadruple("JZ", cond.addr, end, "");
        
        expect_token(TOKEN_BRACKET_CLOSE, "Expected ']'");
        expect_token(TOKEN_BRACE_OPEN, "Expected '{'");
        
        while (!check_token(TOKEN_BRACE_CLOSE) && !check_token(TOKEN_EOF)) {
            parse_statement();
        }
        
        generate_quadruple("JUMP", start, "", "");
        generate_quadruple("LABEL", end, "", "");
        expect_token(TOKEN_BRACE_CLOSE, "Expected '}'");
    }
    else {
        advance_token();
    }
}

void parse_program() {
    expect_token(TOKEN_PROTOCOL, "Expected '@protocol'");
    expect_token(TOKEN_IDENTIFIANT, "Expected program name");
    
    if (check_token(TOKEN_KEYSPACE)) {
        advance_token();
        expect_token(TOKEN_BRACE_OPEN, "Expected '{'");
        while (!check_token(TOKEN_BRACE_CLOSE) && !check_token(TOKEN_EOF)) {
            parse_statement();
        }
        expect_token(TOKEN_BRACE_CLOSE, "Expected '}'");
    }
    
    expect_token(TOKEN_MAIN, "Expected '@main'");
    expect_token(TOKEN_BRACE_OPEN, "Expected '{'");
    while (!check_token(TOKEN_BRACE_CLOSE) && !check_token(TOKEN_EOF)) {
        parse_statement();
    }
    expect_token(TOKEN_BRACE_CLOSE, "Expected '}'");
    expect_token(TOKEN_ENDPROTOCOL, "Expected '@endprotocol'");
}

/*
  MAIN FUNCTION
*/
int main() {
    printf("\n");
    printf("================================================================\n");
    printf("  CRYPTOLANG COMPILER - CTF CHALLENGE\n");
    printf("  Fix all compilation errors to reveal the flag\n");
    printf("================================================================\n");
    
    const char* code_source = 
        "@protocol FlagVault\n"
        "\n"
        "@keyspace {\n"
        "    key256 :: vault_key = 0x7368656C6C6D61746573G7B6C;\n"
        "    byte :: rounds = 10;\n"
        "}\n"
        "\n"
        "@main {\n"
        "    plain :: message = \"Encrypted\"\n"
        "    \n"
        "    cipher :: encrypted = vault_key;\n"
        "    \n"
        "    @loop [ rounds > 0 ] {\n"
        "        rounds = rounds - 1;\n"
        "    }\n"
        "    \n"
        "    hash :: h = message #>;\n"
        "    -> h;\n"
        "}\n"
        "\n"
        "@endprotocol";
    
    printf("\nSOURCE CODE LOADED:\n");
    printf("----------------------------------------------------------------\n");
    printf("%s\n", code_source);
    printf("----------------------------------------------------------------\n");
    
    printf("\n[PHASE 1] LEXICAL ANALYSIS\n");
    printf("----------------------------------------------------------------\n");
    tokenize(code_source);
    
    if (lexical_errors > 0) {
        printf("\nCompilation stopped: %d lexical error(s) detected\n", lexical_errors);
        printf("================================================================\n\n");
        return 1;
    }
    
    printf("Lexical analysis successful - %d tokens generated\n", token_count);
    printf("\n l3x1c4l_ \n");
    printf("----------------------------------------------------------------\n");
    
    printf("\n[PHASE 2] SYNTAX ANALYSIS\n");
    printf("----------------------------------------------------------------\n");
    token_index = 0;
    current_token = tokens[0];
    parse_program();
    
    if (syntax_errors > 0) {
        printf("\nCompilation stopped: %d syntax error(s) detected\n", syntax_errors);
        printf("================================================================\n\n");
        return 1;
    }
    
    printf("Syntax analysis successful\n");
    printf("\n p4rs3r_ \n");
    printf("----------------------------------------------------------------\n");
    
    printf("\n[PHASE 3] SEMANTIC ANALYSIS\n");
    printf("----------------------------------------------------------------\n");
    
    if (semantic_errors > 0) {
        printf("\nCompilation stopped: %d semantic error(s) detected\n", semantic_errors);
        printf("================================================================\n\n");
        return 1;
    }
    
    printf("Semantic analysis successful\n");
    printf("\n m4st3r \n");
    printf("----------------------------------------------------------------\n");
    
    printf("\n");
    printf("  Congratulations! You have mastered the three phases of\n");
    printf("\n");
    printf("================================================================\n");
    
    printf("\nCOMPILATION STATISTICS:\n");
    printf("----------------------------------------------------------------\n");
    printf("  Tokens generated     : %d\n", token_count);
    printf("  Symbols declared     : %d\n", symbol_count);
    printf("  Quadruples generated : %d\n", quadruple_count);
    printf("  Total errors         : 0\n");
    printf("===========\n\n");
    
    return 0;
}
