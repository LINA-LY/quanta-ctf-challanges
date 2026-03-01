#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <time.h>
#include <sys/mman.h>
#include <stdint.h>

#ifdef USE_SECCOMP
#include <seccomp.h>
#endif

#define MAX_SLOTS 16
#define CHUNK_SIZE 0x80
#define HEAP_SIZE 0x20000
#define MIN_CHUNK_SIZE 0x40
#define MAX_PROGRAM_SIZE 512
#define VM_STACK_SIZE 32

unsigned char SESSION_KEY;
void *heap_base = NULL;
void *data_region = NULL;
size_t data_region_size = 0;

// Separate metadata storage (protected from direct user access)
typedef struct chunk_meta {
    size_t size;
    uint32_t flags;
    uint32_t refcount;
    struct chunk_meta *next;
} chunk_meta_t;

typedef struct {
    void *data;
    int in_use;
    int is_entangled;
    uint32_t entangle_target;
    size_t allocated_size;
} quantum_slot_t;

// VM state
typedef struct {
    int64_t stack[VM_STACK_SIZE];
    int sp;
    uint8_t *code;
    size_t code_len;
    size_t ip;
    int running;
} quantum_vm_t;

quantum_slot_t slots[MAX_SLOTS];
chunk_meta_t metadata_store[MAX_SLOTS * 4];
int metadata_count = 0;
chunk_meta_t *freelist = NULL;

// VM opcodes
#define OP_NOP        0x00
#define OP_PUSH       0x01
#define OP_POP        0x02
#define OP_LOAD       0x10
#define OP_STORE      0x11
#define OP_LOAD_REL   0x12
#define OP_STORE_REL  0x13
#define OP_ADD        0x20
#define OP_SUB        0x21
#define OP_XOR        0x22
#define OP_AND        0x23
#define OP_OR         0x24
#define OP_SHL        0x25
#define OP_SHR        0x26
#define OP_ENTANGLE   0x30
#define OP_COLLAPSE   0x31
#define OP_MEASURE    0x32
#define OP_HALT       0xFF

static inline uintptr_t mangle_ptr(void *ptr) {
    if (ptr == NULL) return 0;
    uintptr_t addr = (uintptr_t)ptr;
    uintptr_t mangled = addr ^ ((uintptr_t)heap_base >> 12);
    return mangled;
}

static inline void* demangle_ptr(uintptr_t mangled) {
    if (mangled == 0) return NULL;
    uintptr_t addr = mangled ^ ((uintptr_t)heap_base >> 12);
    return (void*)addr;
}

void setup_seccomp() {
#ifdef USE_SECCOMP
    scmp_filter_ctx ctx = seccomp_init(SCMP_ACT_KILL);
    if (!ctx) {
        exit(1);
    }
    
    // Allow necessary syscalls for flag reading
    seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(read), 0);
    seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(write), 0);
    seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(openat), 0);  // FIXED: Allow openat
    seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(exit), 0);
    seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(exit_group), 0);
    seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(mmap), 0);
    seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(brk), 0);
    seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(munmap), 0);
    seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(mprotect), 0);
    seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(close), 0);
    
    // Block dangerous syscalls
    seccomp_rule_add(ctx, SCMP_ACT_KILL, SCMP_SYS(execve), 0);
    seccomp_rule_add(ctx, SCMP_ACT_KILL, SCMP_SYS(execveat), 0);
    
    if (seccomp_load(ctx) < 0) {
        seccomp_release(ctx);
        exit(1);
    }
    seccomp_release(ctx);
#endif
}

void init_heap() {
    // Allocate heap with separated metadata
    heap_base = mmap(NULL, HEAP_SIZE, PROT_READ | PROT_WRITE,
                     MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (heap_base == MAP_FAILED) {
        exit(1);
    }
    
    // Use first portion for metadata, rest for data
    size_t meta_size = sizeof(chunk_meta_t) * MAX_SLOTS * 4;
    data_region = (void*)((char*)heap_base + meta_size);
    data_region_size = HEAP_SIZE - meta_size;
    
    chunk_meta_t *initial = &metadata_store[0];
    initial->size = data_region_size;
    initial->flags = 0;
    initial->refcount = 0;
    initial->next = NULL;
    freelist = initial;
    metadata_count = 1;
}

chunk_meta_t* find_metadata(void *ptr) {
    if (!ptr || ptr < data_region || ptr >= (void*)((char*)data_region + data_region_size)) {
        return NULL;
    }
    
    for (int i = 0; i < metadata_count; i++) {
        void *chunk_data = (void*)((char*)data_region + 
            ((char*)&metadata_store[i] - (char*)metadata_store) * 8);
        if (chunk_data == ptr) {
            return &metadata_store[i];
        }
    }
    return NULL;
}

void* custom_malloc(size_t size) {
    if (size < MIN_CHUNK_SIZE) {
        size = MIN_CHUNK_SIZE;
    }
    
    size = (size + 0xf) & ~0xf;
    
    chunk_meta_t *current = freelist;
    chunk_meta_t *prev = NULL;
    
    while (current) {
        if (current->size >= size) {
            if (current->size >= size + MIN_CHUNK_SIZE) {
                if (metadata_count >= MAX_SLOTS * 4 - 1) {
                    return NULL;
                }
                
                chunk_meta_t *remainder = &metadata_store[metadata_count++];
                remainder->size = current->size - size;
                remainder->flags = 0;
                remainder->refcount = 0;
                
                chunk_meta_t *next_chunk = current->next ? 
                    (chunk_meta_t*)demangle_ptr((uintptr_t)current->next) : NULL;
                remainder->next = (chunk_meta_t*)mangle_ptr(next_chunk);
                
                current->size = size;
                current->next = (chunk_meta_t*)mangle_ptr(remainder);
            }
            
            if (prev == NULL) {
                freelist = current->next ? 
                    (chunk_meta_t*)demangle_ptr((uintptr_t)current->next) : NULL;
            } else {
                prev->next = current->next;
            }
            
            current->flags = 1;
            current->refcount = 1;
            
            size_t offset = (current - metadata_store) * 8;
            return (void*)((char*)data_region + offset);
        }
        
        prev = current;
        current = current->next ? 
            (chunk_meta_t*)demangle_ptr((uintptr_t)current->next) : NULL;
    }
    
    return NULL;
}

void custom_free(void *ptr) {
    chunk_meta_t *meta = find_metadata(ptr);
    if (!meta || meta->flags == 0) {
        return;
    }
    
    if (meta->refcount > 0) {
        meta->refcount--;
        if (meta->refcount > 0) {
            return;
        }
    }
    
    meta->flags = 0;
    
    chunk_meta_t *next_free = freelist;
    meta->next = (chunk_meta_t*)mangle_ptr(next_free);
    freelist = meta;
}

void init() {
    setvbuf(stdin, NULL, _IONBF, 0);
    setvbuf(stdout, NULL, _IONBF, 0);
    setvbuf(stderr, NULL, _IONBF, 0);
    srand(time(NULL));
    SESSION_KEY = rand() % 255;
    alarm(300);
    
    memset(slots, 0, sizeof(slots));
    memset(metadata_store, 0, sizeof(metadata_store));
    
    init_heap();
    setup_seccomp();
}

void quantum_encrypt(char *buf, int len) {
    for(int i=0; i<len; i++) {
        buf[i] ^= SESSION_KEY;
    }
}

int get_int() {
    char buf[16];
    int r = read(0, buf, 15);
    if (r > 0) {
        buf[r] = '\0';
    }
    return atoi(buf);
}

void vm_push(quantum_vm_t *vm, int64_t val) {
    if (vm->sp >= VM_STACK_SIZE) {
        vm->running = 0;
        return;
    }
    vm->stack[vm->sp++] = val;
}

int64_t vm_pop(quantum_vm_t *vm) {
    if (vm->sp <= 0) {
        vm->running = 0;
        return 0;
    }
    return vm->stack[--vm->sp];
}

uint8_t vm_fetch_byte(quantum_vm_t *vm) {
    if (vm->ip >= vm->code_len) {
        vm->running = 0;
        return OP_HALT;
    }
    return vm->code[vm->ip++];
}

// THE VULNERABILITY: Hidden in the ENTANGLE instruction
// The bug: ENTANGLE uses a signed comparison for the offset calculation
// combined with a missing bounds check on the entangle_target update.
// This allows:
// 1. Type confusion between slot indices
// 2. Use-after-free by entangling to a freed slot
// 3. Out-of-bounds write through carefully crafted offset values
void vm_execute(quantum_vm_t *vm) {
    while (vm->running && vm->ip < vm->code_len) {
        uint8_t op = vm_fetch_byte(vm);
        
        switch(op) {
            case OP_NOP:
                break;
                
            case OP_PUSH: {
                int64_t val = 0;
                for (int i = 0; i < 8; i++) {
                    val |= ((int64_t)vm_fetch_byte(vm)) << (i * 8);
                }
                vm_push(vm, val);
                break;
            }
            
            case OP_POP:
                vm_pop(vm);
                break;
                
            case OP_LOAD: {
                int64_t idx = vm_pop(vm);
                if (idx >= 0 && idx < MAX_SLOTS && slots[idx].in_use) {
                    vm_push(vm, (int64_t)slots[idx].data);
                } else {
                    vm_push(vm, 0);
                }
                break;
            }
            
            case OP_STORE: {
                int64_t val = vm_pop(vm);
                int64_t idx = vm_pop(vm);
                if (idx >= 0 && idx < MAX_SLOTS && slots[idx].in_use) {
                    if (slots[idx].data && val < slots[idx].allocated_size) {
                        ((uint8_t*)slots[idx].data)[val] = vm_pop(vm) & 0xFF;
                    }
                }
                break;
            }
            
            // BUG: LOAD_REL uses signed offset with incorrect comparison
            case OP_LOAD_REL: {
                int64_t offset = vm_pop(vm);  // SIGNED - can be negative!
                int64_t idx = vm_pop(vm);
                
                if (idx >= 0 && idx < MAX_SLOTS && slots[idx].in_use && slots[idx].data) {
                    // TODO: Fix signed comparison issue
                    // BUG: Compares signed offset with signed size
                    // Negative offsets pass the check but cause OOB read
                    if (offset < (int64_t)slots[idx].allocated_size) {
                        uint8_t *ptr = (uint8_t*)slots[idx].data;
                        vm_push(vm, ptr[offset]);
                    } else {
                        vm_push(vm, 0);
                    }
                } else {
                    vm_push(vm, 0);
                }
                break;
            }
            
            // BUG: STORE_REL allows out-of-bounds write via signed comparison
            case OP_STORE_REL: {
                int64_t val = vm_pop(vm);
                int64_t offset = vm_pop(vm);  // SIGNED!
                int64_t idx = vm_pop(vm);
                
                if (idx >= 0 && idx < MAX_SLOTS && slots[idx].in_use && slots[idx].data) {
                    // TODO: Fix signed comparison issue
                    // BUG: Compares signed offset with signed size
                    // Negative offsets (e.g., -0x50) pass: -0x50 < +0x80 is TRUE
                    // But ptr[offset] with negative offset writes BEFORE the chunk (OOB)
                    if (offset < (int64_t)slots[idx].allocated_size) {
                        uint8_t *ptr = (uint8_t*)slots[idx].data;
                        ptr[offset] = val & 0xFF;
                    }
                }
                break;
            }
            
            case OP_ADD: {
                int64_t b = vm_pop(vm);
                int64_t a = vm_pop(vm);
                vm_push(vm, a + b);
                break;
            }
            
            case OP_SUB: {
                int64_t b = vm_pop(vm);
                int64_t a = vm_pop(vm);
                vm_push(vm, a - b);
                break;
            }
            
            case OP_XOR: {
                int64_t b = vm_pop(vm);
                int64_t a = vm_pop(vm);
                vm_push(vm, a ^ b);
                break;
            }
            
            case OP_AND: {
                int64_t b = vm_pop(vm);
                int64_t a = vm_pop(vm);
                vm_push(vm, a & b);
                break;
            }
            
            case OP_OR: {
                int64_t b = vm_pop(vm);
                int64_t a = vm_pop(vm);
                vm_push(vm, a | b);
                break;
            }
            
            case OP_SHL: {
                int64_t b = vm_pop(vm);
                int64_t a = vm_pop(vm);
                vm_push(vm, a << (b & 0x3F));
                break;
            }
            
            case OP_SHR: {
                int64_t b = vm_pop(vm);
                int64_t a = vm_pop(vm);
                vm_push(vm, (uint64_t)a >> (b & 0x3F));
                break;
            }
            
            // THE CRITICAL BUG: ENTANGLE instruction
            // Multiple vulnerabilities here:
            // 1. Doesn't check if target slot is actually allocated
            // 2. Uses bitwise math that can cause integer overflow
            // 3. Updates entangle_target WITHOUT validating the new value
            // 4. The refcount increment happens BEFORE validation
            case OP_ENTANGLE: {
                int64_t target_idx = vm_pop(vm);
                int64_t source_idx = vm_pop(vm);
                
                // Looks safe but isn't - checks are insufficient
                if (source_idx >= 0 && source_idx < MAX_SLOTS && 
                    target_idx >= 0 && target_idx < MAX_SLOTS) {
                    
                    if (slots[source_idx].in_use) {
                        // BUG #1: Doesn't verify target is in_use
                        // BUG #2: Updates entangle_target using XOR which can overflow
                        // BUG #3: The entangle_target calculation is wrong - uses |= instead of =
                        // This allows accumulated corruption across multiple ENTANGLEs
                        slots[source_idx].entangle_target |= (target_idx ^ 0x5A);
                        
                        // BUG #4: Increments refcount even if target isn't valid
                        // This prevents proper cleanup and enables UAF
                        if (slots[target_idx].data) {
                            chunk_meta_t *meta = find_metadata(slots[target_idx].data);
                            if (meta) {
                                meta->refcount++;
                            }
                        }
                        
                        slots[source_idx].is_entangled = 1;
                        
                        // BUG #5: Copies pointer without incrementing source refcount
                        // Creates a dangling pointer scenario
                        if (!slots[source_idx].data && slots[target_idx].in_use) {
                            slots[source_idx].data = slots[target_idx].data;
                        }
                    }
                }
                break;
            }
            
            // COLLAPSE: Deentangle but doesn't fix refcount properly
            case OP_COLLAPSE: {
                int64_t idx = vm_pop(vm);
                if (idx >= 0 && idx < MAX_SLOTS && slots[idx].in_use) {
                    // BUG: Doesn't decrement refcount on target
                    slots[idx].is_entangled = 0;
                    // BUG: Doesn't clear the entangle_target - data lingers
                }
                break;
            }
            
            case OP_MEASURE: {
                int64_t idx = vm_pop(vm);
                if (idx >= 0 && idx < MAX_SLOTS && slots[idx].in_use) {
                    vm_push(vm, (int64_t)slots[idx].is_entangled);
                    vm_push(vm, (int64_t)slots[idx].entangle_target);
                } else {
                    vm_push(vm, 0);
                    vm_push(vm, 0);
                }
                break;
            }
            
            case OP_HALT:
                vm->running = 0;
                return;
                
            default:
                vm->running = 0;
                return;
        }
    }
}

void alloc() {
    int idx = -1;
    for(int i=0; i<MAX_SLOTS; i++) {
        if(!slots[i].in_use) { idx = i; break; }
    }
    if(idx == -1) {
        puts("No slots");
        return;
    }
    
    printf("Size (default %d): ", CHUNK_SIZE);
    int size = get_int();
    if (size <= 0 || size > CHUNK_SIZE * 2) {
        size = CHUNK_SIZE;
    }
    
    slots[idx].data = custom_malloc(size);
    if (!slots[idx].data) {
        puts("Allocation failed");
        return;
    }
    
    memset(slots[idx].data, 0, size);
    slots[idx].in_use = 1;
    slots[idx].is_entangled = 0;
    slots[idx].entangle_target = 0;
    slots[idx].allocated_size = size;
    
    printf("Allocated slot %d\n", idx);
}

void delete() {
    printf("Idx: ");
    int idx = get_int();
    
    if(idx < 0 || idx >= MAX_SLOTS || !slots[idx].in_use) {
        puts("Invalid");
        return;
    }
    
    custom_free(slots[idx].data);
    slots[idx].in_use = 0;
    slots[idx].data = NULL;
    slots[idx].is_entangled = 0;
    slots[idx].entangle_target = 0;
    
    puts("Freed");
}

void program_quantum_state() {
    printf("Program size: ");
    int size = get_int();
    
    if (size <= 0 || size > MAX_PROGRAM_SIZE) {
        puts("Invalid size");
        return;
    }
    
    uint8_t *program = malloc(size);
    if (!program) {
        puts("Failed to allocate program memory");
        return;
    }
    
    printf("Bytecode: ");
    int r = read(0, program, size);
    if (r <= 0) {
        free(program);
        return;
    }
    
    quantum_vm_t vm;
    memset(&vm, 0, sizeof(vm));
    vm.code = program;
    vm.code_len = r;
    vm.ip = 0;
    vm.sp = 0;
    vm.running = 1;
    
    vm_execute(&vm);
    
    puts("Quantum state programmed");
    free(program);
}

void observe() {
    printf("Idx: ");
    int idx = get_int();
    
    if(idx < 0 || idx >= MAX_SLOTS || !slots[idx].in_use) {
        puts("Invalid");
        return;
    }
    
    write(1, slots[idx].data, slots[idx].allocated_size);
    write(1, "\n", 1);
}

void show_info() {
    puts("\n=== Quantum Printer v2.0 ===");
    puts("A quantum-entangled memory management system");
    puts("Use the VM to manipulate quantum states");
    puts("");
    puts("VM Opcodes:");
    puts("  0x01 - PUSH <8 bytes>");
    puts("  0x10 - LOAD (pop idx, push addr)");
    puts("  0x12 - LOAD_REL (pop idx, pop offset, push byte)");
    puts("  0x13 - STORE_REL (pop idx, pop offset, pop val)");
    puts("  0x20-0x26 - Arithmetic ops");
    puts("  0x30 - ENTANGLE (pop src, pop target)");
    puts("  0x31 - COLLAPSE (pop idx)");
    puts("  0x32 - MEASURE (pop idx, push entangled, push target)");
    puts("  0xFF - HALT");
}

int main() {
    init();
    
    show_info();
    
    while(1) {
        printf("\n1.Alloc 2.Free 3.Program 4.View 5.Info\n> ");
        int c = get_int();
        
        if(c==1) alloc();
        else if(c==2) delete();
        else if(c==3) program_quantum_state();
        else if(c==4) observe();
        else if(c==5) show_info();
        else break;
    }
    
    return 0;
}
