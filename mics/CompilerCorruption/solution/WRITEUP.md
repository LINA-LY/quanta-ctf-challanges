# Writeup: Compiler Corruption

**Category:** Misc  
**Difficulty:** medium
**Points:** 150  
**Flag:** `shellmates{l3x1c4l_p4rs3r_m4st3r}`

---

## Challenge Description

This challenge provides a CryptoLang compiler with intentionally buggy source code embedded within it. Players must identify and fix three distinct types of compilation errors - lexical, syntactic, and semantic - to progressively reveal the complete flag.

## Solution Overview

The challenge tests understanding of the three primary phases of compilation:
1. Lexical Analysis (Tokenization)
2. Syntax Analysis (Parsing)
3. Semantic Analysis (Type Checking)

Each successfully fixed phase reveals one part of the flag.

---

## Initial Setup

Compile the compiler:
```bash
gcc compiler_challenge.c -o compiler
```

Or using the provided Makefile:
```bash
make
```

Run the compiler:
```bash
./compiler
```

---

## Phase 1: Lexical Error

### Error Output

```
[PHASE 1] LEXICAL ANALYSIS
----------------------------------------------------------------
[LEXICAL ERROR] Line 5

Compilation stopped: 1 lexical error(s) detected
```

### Analysis

The lexical analyzer identifies an invalid character in a hexadecimal literal on line 5:

```c
key256 :: vault_key = 0x7368656C6C6D61746573G7B6C;
                                           ^
                                     Invalid character
```

Hexadecimal notation in CryptoLang (as in most programming languages) accepts only:
- Digits: 0-9
- Letters: A-F (case insensitive)

The character 'G' falls outside this valid range and causes a lexical error.

### Fix

Remove or replace the invalid character 'G'. In this case, we replace it with a valid hex digit:

```c
// Before
key256 :: vault_key = 0x7368656C6C6D61746573G7B6C;

// After
key256 :: vault_key = 0x7368656C6C6D617465737B6C;
```

### Result

After fixing and recompiling:
```
Lexical analysis successful - 45 tokens generated

[FLAG PART 1/3]: shellmates{l3x1c4l_
```

---

## Phase 2: Syntax Error

### Error Output

```
[PHASE 2] SYNTAX ANALYSIS
----------------------------------------------------------------
[SYNTAX ERROR] Line 11

Compilation stopped: 1 syntax error(s) detected
```

### Analysis

The parser expected a semicolon to terminate the statement on line 11 but encountered `@loop` instead:

```c
plain :: message = "Encrypted"
                             ^
                       Missing semicolon
```

According to CryptoLang grammar (similar to C), every statement must be terminated with a semicolon. The absence of this token violates the language's syntax rules.

### Fix

Add the required semicolon:

```c
// Before
plain :: message = "Encrypted"

// After
plain :: message = "Encrypted";
```

### Result

After fixing and recompiling:
```
Syntax analysis successful

[FLAG PART 2/3]: p4rs3r_
```

---

## Phase 3: Semantic Error

### Error Output

```
[PHASE 3] SEMANTIC ANALYSIS
----------------------------------------------------------------
[SEMANTIC ERROR] Line 14

Compilation stopped: 1 semantic error(s) detected
```

### Analysis

The semantic analyzer detects a type incompatibility on line 14:

```c
cipher :: encrypted = vault_key;
```

Analysis of variable types:
- `vault_key` is declared as type `key256` (line 5)
- `encrypted` is declared as type `cipher` (line 14)

CryptoLang's type system enforces strict type compatibility. A `key256` value cannot be directly assigned to a `cipher` variable without explicit conversion or encryption operation.

### Fix

Change the type declaration to match the assigned value:

```c
// Before
cipher :: encrypted = vault_key;

// After
key256 :: encrypted = vault_key;
```

### Result

After fixing and recompiling:
```
Semantic analysis successful

[FLAG PART 3/3]: m4st3r}
```

---

## Complete Solution

### All Required Changes

| Phase | Line | Error Type | Before | After |
|-------|------|-----------|--------|-------|
| Lexical | 5 | Invalid hex char | `...74657 3G7B6C` | `...746573 7B6C` |
| Syntax | 11 | Missing semicolon | `"Encrypted"` | `"Encrypted";` |
| Semantic | 14 | Type mismatch | `cipher :: encrypted` | `key256 :: encrypted` |

### Corrected Source Code

```c
@protocol FlagVault

@keyspace {
    key256 :: vault_key = 0x7368656C6C6D617465737B6C;
    byte :: rounds = 10;
}

@main {
    plain :: message = "Encrypted";
    
    key256 :: encrypted = vault_key;
    
    @loop [ rounds > 0 ] {
        rounds = rounds - 1;
    }
    
    hash :: h = message #>;
    -> h;
}

@endprotocol
```

---

## Final Flag

After successfully fixing all three errors, the complete flag is revealed:

```
================================================================
                  COMPILATION SUCCESSFUL
================================================================

                    COMPLETE FLAG:

              shellmates{l3x1c4l_p4rs3r_m4st3r}

  Congratulations! You have mastered the three phases of
  compilation: Lexical, Syntactic, and Semantic Analysis

================================================================
```

**Flag:** `shellmates{l3x1c4l_p4rs3r_m4st3r}`

---

## Key Takeaways

This challenge demonstrates the three fundamental phases of compilation:

1. **Lexical Analysis**
   - Breaks source code into tokens
   - Validates individual lexemes (keywords, identifiers, literals, operators)
   - Detects malformed tokens (invalid hex characters, unterminated strings)

2. **Syntax Analysis**  
   - Verifies grammatical structure
   - Ensures proper use of punctuation and delimiters
   - Validates statement and expression formation

3. **Semantic Analysis**
   - Performs type checking
   - Verifies variable declarations and scopes
   - Ensures logical consistency of operations

Each phase builds upon the previous, and errors at any stage prevent progression to subsequent phases - mirroring real compiler behavior.

---

## Tools Required

- GCC compiler
- Text editor (vim, nano, VS Code, etc.)
- Terminal/command line

## Estimated Solve Time

- Experienced: 20-30 minutes
- Intermediate: 45-90 minutes
- Beginner: 1-2 hours

---

## Author Notes

This challenge was designed to teach compiler fundamentals in a practical, hands-on manner while maintaining CTF entertainment value. The progressive flag reveal mechanic reinforces the sequential nature of compilation phases and provides immediate feedback for correct fixes.

The use of a domain-specific language (CryptoLang) adds thematic relevance to the cryptography-focused CTF while keeping the core concepts accessible to players without deep compiler theory knowledge.
