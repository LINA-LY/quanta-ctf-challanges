# Compiler Corruption

A compiler debugging challenge that tests your understanding of lexical, syntactic, and semantic analysis.

## Challenge Information

**Category:** Misc  
**Difficulty:** medium 
**Points:** 150

## Description

A cryptographic protocol implementation has been corrupted during transmission. The CryptoLang compiler contains buggy source code with three distinct compilation errors. Your objective is to identify and fix each error to progressively reveal the flag.

The compiler will guide you through three analysis phases with minimal diagnostic information - only error type and line number. You must apply your understanding of compiler theory to identify and correct each issue.

## Getting Started

### Compilation

Compile the compiler using GCC:
```bash
gcc compiler_challenge.c -o compiler
```

Alternatively, use the provided Makefile:
```bash
make
```

### Execution

Run the compiled binary:
```bash
./compiler
```

The compiler will analyze the embedded source code and report the first error encountered. Read the error message carefully, fix the issue in the source code within `compiler_challenge.c`, then recompile and run again.

## Error Types

### Lexical Errors
Malformed tokens that the scanner cannot recognize. Common examples include:
- Invalid characters in numeric literals
- Unterminated strings
- Illegal character sequences

### Syntax Errors
Code that violates the grammar rules of CryptoLang. Common examples include:
- Missing punctuation (semicolons, braces, parentheses)
- Incorrect statement structure
- Mismatched delimiters

### Semantic Errors
Code that is syntactically correct but logically inconsistent. Common examples include:
- Type mismatches
- Undeclared variables
- Incompatible operations

## CryptoLang Quick Reference

### Data Types
- `byte` - 8-bit integer value
- `plain` - Plaintext string
- `cipher` - Encrypted data
- `hash` - Cryptographic hash value
- `key256` - 256-bit encryption key

### Operators
- `::` - Variable declaration
- `=` - Assignment
- `@>` - Encryption operator
- `#>` - Hash operator
- `->` - Output operator
- `<-` - Input operator

### Control Structures
```
@loop [ condition ] {
    // Loop body
}
```

### Program Structure
```
@protocol ProgramName

@keyspace {
    // Global variable declarations
}

@main {
    // Main program logic
}

@endprotocol
```

## Solution Workflow

1. Run the compiler
2. Read the error message carefully
3. Locate the error in the source code
4. Fix the error
5. Recompile the compiler
6. Run again and repeat until all errors are fixed

Each successful fix reveals part of the flag. After fixing all three errors, the complete flag will be displayed.

## Debugging Strategy

- Error messages provide only the error type and line number
- Study the CryptoLang language specification carefully
- Understand the differences between lexical, syntax, and semantic errors
- Each error type requires different analysis techniques
- Review the complete source code, not just the error line

## Files Provided

- `compiler_challenge.c` - The CryptoLang compiler with embedded buggy code
- `Makefile` - Build automation
- `README.md` - This file

## Learning Objectives

This challenge teaches:
- The three phases of compilation
- Error detection and debugging
- Type systems in programming languages
- Domain-specific language concepts

## Flag Format

```
shellmates{...}
```

---

For a complete walkthrough, see `WRITEUP.md` after solving the challenge.
