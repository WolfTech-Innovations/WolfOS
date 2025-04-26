; ATOBoot - Alpha To Omega Bootloader
; x64 Linux Kernel Loader
; Loads vmlinuz and initrd for 64-bit Linux boot process

[BITS 16]               ; Start in 16-bit Real Mode
[ORG 0x7C00]            ; Bootloader loaded at this address

; Constants for memory locations
KERNEL_LOAD_SEGMENT     equ 0x1000    ; Kernel load segment (0x10000 physical)
KERNEL_LOAD_OFFSET      equ 0x0000    ; Kernel load offset
INITRD_LOAD_SEGMENT     equ 0x2000    ; InitRD load segment (0x20000 physical)
INITRD_LOAD_OFFSET      equ 0x0000    ; InitRD load offset
BOOT_PARAMS_SEGMENT     equ 0x0000    ; Boot parameters segment
BOOT_PARAMS_OFFSET      equ 0x8000    ; Boot parameters offset (0x8000 physical)

; ============================
; BOOTLOADER ENTRY POINT
; ============================
start:
    cli                         ; Disable interrupts
    xor ax, ax                  ; Zero AX register
    mov ds, ax                  ; Set Data Segment to 0
    mov es, ax                  ; Set Extra Segment to 0
    mov ss, ax                  ; Set Stack Segment to 0
    mov sp, 0x7C00              ; Set Stack Pointer right below where we're loaded
    sti                         ; Enable interrupts
    
    mov [boot_drive], dl        ; Save the boot drive number (passed by BIOS in DL)
    
    ; Display welcome message
    mov si, msg_welcome
    call print_string
    
    ; Check for A20 line
    call check_a20
    
    ; Get memory map
    call get_memory_map
    
    ; Load kernel
    mov si, msg_loading_kernel
    call print_string
    call load_kernel
    
    ; Load initrd
    mov si, msg_loading_initrd
    call print_string
    call load_initrd
    
    ; Set up boot parameters
    call setup_boot_params
    
    ; Switch to protected mode and then to long mode
    mov si, msg_switching_mode
    call print_string
    call switch_to_protected_mode
    
    ; We should never get here - the above function doesn't return
    mov si, msg_boot_failed
    call print_string
    jmp $                       ; Infinite loop

; ============================
; SUBROUTINES
; ============================

; Print a null-terminated string with colors
; Input: SI points to the string
print_string:
    push ax
    push bx
    
    mov ah, 0x0E                ; BIOS teletype function
    mov bh, 0x00                ; Page number
    mov bl, 0x0A                ; Text attribute (bright green)
    
.loop:
    lodsb                       ; Load byte from SI into AL
    or al, al                   ; Test if character is 0 (end of string)
    jz .done                    ; If zero, we're done
    int 0x10                    ; Otherwise, print the character
    jmp .loop                   ; And continue looping
    
.done:
    ; Add a small delay for visual effect
    push cx
    mov cx, 0x2000
.delay:
    loop .delay
    pop cx
    
    pop bx
    pop ax
    ret

; Check if A20 line is enabled
check_a20:
    push ax
    push si
    
    mov si, msg_checking_a20
    call print_string
    
    ; Simple A20 check
    xor ax, ax
    mov es, ax
    mov di, 0x7DFE              ; Address that wraps around without A20
    
    mov ax, 0xFFFF
    mov ds, ax
    mov si, 0x7E0E              ; Same address + 0x10 with A20 enabled
    
    mov al, [es:di]             ; Save original value
    mov ah, [ds:si]             ; Save original value
    
    mov byte [es:di], 0x00      ; Write different values
    mov byte [ds:si], 0xFF
    
    cmp byte [es:di], 0xFF      ; If they're the same, A20 is off
    
    ; Restore original values
    mov [es:di], al
    mov [ds:si], ah
    
    mov ax, 0
    mov ds, ax                  ; Reset DS to 0
    
    jne .a20_enabled
    
    mov si, msg_a20_disabled
    call print_string
    
    ; Try to enable A20 using BIOS
    mov ax, 0x2401
    int 0x15
    
    ; Check if successful
    call check_a20
    jmp .done
    
.a20_enabled:
    mov si, msg_a20_enabled
    call print_string
    
.done:
    pop si
    pop ax
    ret

; Get memory map using int 0x15, function 0xE820
get_memory_map:
    push eax
    push ebx
    push ecx
    push edx
    push di
    
    mov si, msg_memory_map
    call print_string
    
    mov di, 0x8004              ; Start storing at 0x8004 (after count)
    xor ebx, ebx                ; Clear EBX (continuation value starts at 0)
    xor bp, bp                  ; Use BP to count entries
    
.get_entry:
    mov eax, 0xE820             ; Function number
    mov ecx, 24                 ; Entry size to request
    mov edx, 0x534D4150         ; 'SMAP' signature
    int 0x15
    
    jc .failed                  ; Carry set means unsupported or error
    
    cmp eax, 0x534D4150         ; Check for SMAP signature in EAX
    jne .failed
    
    test ebx, ebx               ; If EBX is 0, we're done
    je .success
    
    add di, 24                  ; Advance DI to next entry position
    inc bp                      ; Increment entry count
    jmp .get_entry              ; Get next entry
    
.success:
    inc bp                      ; Count the final entry
    mov [0x8000], bp            ; Store entry count at 0x8000
    
    mov si, msg_memory_detected
    call print_string
    jmp .done
    
.failed:
    mov si, msg_memory_failed
    call print_string
    
.done:
    pop di
    pop edx
    pop ecx
    pop ebx
    pop eax
    ret

; Load the Linux kernel into memory
load_kernel:
    push ax
    push bx
    push cx
    push dx
    push es
    
    ; Set up es:bx for the load destination
    mov ax, KERNEL_LOAD_SEGMENT
    mov es, ax
    xor bx, bx
    
    ; Define disk read parameters
    mov ah, 0x42                ; Extended read function
    mov dl, [boot_drive]        ; Boot drive number
    
    ; Set up disk address packet on the stack
    push dword 0                ; Upper 32 bits of 48-bit starting LBA (unused)
    push dword 2                ; Lower 32 bits of 48-bit starting LBA (sector 2)
    push KERNEL_LOAD_SEGMENT    ; Memory buffer destination segment
    push KERNEL_LOAD_OFFSET     ; Memory buffer destination offset
    push word 128               ; Number of sectors to transfer (64KB)
    push word 16                ; Size of packet (16 bytes)
    
    mov si, sp                  ; DS:SI now points to disk address packet
    int 0x13                    ; Call BIOS disk function
    
    add sp, 16                  ; Clean up the stack
    
    jnc .success                ; If carry flag is clear, read was successful
    
    mov si, msg_kernel_error
    call print_string
    jmp hang                    ; Fatal error
    
.success:
    mov si, msg_kernel_success
    call print_string
    
    pop es
    pop dx
    pop cx
    pop bx
    pop ax
    ret

; Load the initial RAM disk into memory
load_initrd:
    push ax
    push bx
    push cx
    push dx
    push es
    
    ; Set up es:bx for the load destination
    mov ax, INITRD_LOAD_SEGMENT
    mov es, ax
    xor bx, bx
    
    ; Define disk read parameters
    mov ah, 0x42                ; Extended read function
    mov dl, [boot_drive]        ; Boot drive number
    
    ; Set up disk address packet on the stack
    push dword 0                ; Upper 32 bits of 48-bit starting LBA (unused)
    push dword 130              ; Lower 32 bits of 48-bit starting LBA (sector 130, after kernel)
    push INITRD_LOAD_SEGMENT    ; Memory buffer destination segment
    push INITRD_LOAD_OFFSET     ; Memory buffer destination offset
    push word 512               ; Number of sectors to transfer (256KB)
    push word 16                ; Size of packet (16 bytes)
    
    mov si, sp                  ; DS:SI now points to disk address packet
    int 0x13                    ; Call BIOS disk function
    
    add sp, 16                  ; Clean up the stack
    
    jnc .success                ; If carry flag is clear, read was successful
    
    mov si, msg_initrd_error
    call print_string
    jmp hang                    ; Fatal error
    
.success:
    mov si, msg_initrd_success
    call print_string
    
    pop es
    pop dx
    pop cx
    pop bx
    pop ax
    ret

; Set up Linux boot parameters structure
setup_boot_params:
    push ax
    push bx
    push cx
    push dx
    push si
    push di
    push es
    
    mov si, msg_setup_params
    call print_string
    
    ; Setup Boot Parameters structure at 0x8000
    mov ax, BOOT_PARAMS_SEGMENT
    mov es, ax
    mov di, BOOT_PARAMS_OFFSET
    
    ; Clear the structure first
    xor ax, ax
    mov cx, 1024                ; 2KB structure size (to be sure)
    rep stosb
    
    ; Position cursor back to the beginning of the structure
    mov di, BOOT_PARAMS_OFFSET
    
    ; Set magic signature "HdrS" (0x48647253)
    mov dword [es:di+0x202], 0x53726448
    
    ; Set protocol version to 2.10 (0x20A)
    mov word [es:di+0x206], 0x20A
    
    ; Set up initrd load address and size
    mov dword [es:di+0x218], 0x00200000   ; InitRD address (2MB)
    mov dword [es:di+0x21C], 0x00040000   ; InitRD size (256KB)
    
    ; Set command line pointer
    mov dword [es:di+0x228], 0x00000800   ; Command line at 0x800
    
    ; Copy kernel command line
    mov si, kernel_cmdline
    mov di, 0x800
    mov cx, 256                 ; Maximum command line size
    call copy_string
    
    mov si, msg_params_success
    call print_string
    
    pop es
    pop di
    pop si
    pop dx
    pop cx
    pop bx
    pop ax
    ret

; Copy a string with length limit
; SI = source, DI = destination, CX = max length
copy_string:
    push ax
    push cx
    push si
    push di
    
.loop:
    lodsb                       ; Load byte from DS:SI into AL
    stosb                       ; Store byte from AL into ES:DI
    or al, al                   ; Check if it's null terminator
    jz .done                    ; If yes, we're done
    loop .loop                  ; Decrement CX and continue if not zero
    
.done:
    pop di
    pop si
    pop cx
    pop ax
    ret

; Switch to protected mode and prepare for long mode
switch_to_protected_mode:
    cli                         ; Disable interrupts
    
    ; Load GDT
    lgdt [gdt_descriptor]
    
    ; Enable protected mode
    mov eax, cr0
    or eax, 1
    mov cr0, eax
    
    ; Far jump to flush the pipeline and load CS with 32-bit code segment
    jmp 0x08:protected_mode_entry

[BITS 32]
protected_mode_entry:
    ; Set up segment registers with data selector
    mov ax, 0x10                ; Data segment selector
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax
    
    ; Set up a stack
    mov esp, 0x90000
    
    ; Check for long mode support
    call check_long_mode
    
    ; Set up paging for long mode
    call setup_paging
    
    ; Enable long mode
    mov ecx, 0xC0000080         ; EFER MSR
    rdmsr
    or eax, (1 << 8)            ; Set LME bit
    wrmsr
    
    ; Enable paging (this activates long mode)
    mov eax, cr0
    or eax, (1 << 31)
    mov cr0, eax
    
    ; Jump to 64-bit code segment
    jmp 0x18:long_mode_entry

; Check if CPU supports long mode
check_long_mode:
    ; Check CPUID support
    pushfd
    pop eax
    mov ecx, eax
    xor eax, 0x200000
    push eax
    popfd
    pushfd
    pop eax
    push ecx
    popfd
    
    xor eax, ecx
    jz no_long_mode            ; CPUID not supported
    
    ; Check if extended CPUID functions are available
    mov eax, 0x80000000
    cpuid
    cmp eax, 0x80000001
    jb no_long_mode            ; Extended functions not supported
    
    ; Check for long mode support
    mov eax, 0x80000001
    cpuid
    test edx, (1 << 29)
    jz no_long_mode            ; Long mode not supported
    
    ret

; Set up paging structures for long mode
setup_paging:
    ; Clear page tables area
    mov edi, 0x70000           ; Page tables will be at 0x70000
    xor eax, eax
    mov ecx, 0x4000            ; Clear 16KB (4 pages of page tables)
    rep stosd
    
    ; Set up Page Map Level 4
    mov edi, 0x70000
    mov eax, 0x71000 | 3       ; Present + Writable
    mov [edi], eax
    
    ; Set up Page Directory Pointer Table
    mov edi, 0x71000
    mov eax, 0x72000 | 3       ; Present + Writable
    mov [edi], eax
    
    ; Set up Page Directory (identity map first 2MB)
    mov edi, 0x72000
    mov eax, 0x73000 | 3       ; Present + Writable
    mov [edi], eax
    
    ; Set up Page Table (identity map 2MB with 4KB pages)
    mov edi, 0x73000
    mov eax, 3                 ; Present + Writable
    mov ecx, 512               ; 512 entries = 2MB
    
.map_page:
    mov [edi], eax
    add eax, 0x1000            ; Next page (4KB)
    add edi, 8                 ; Next entry
    loop .map_page
    
    ; Load PML4 address into CR3
    mov eax, 0x70000
    mov cr3, eax
    
    ; Enable PAE
    mov eax, cr4
    or eax, (1 << 5)           ; Set PAE bit
    mov cr4, eax
    
    ret

; Error handling for no long mode support
no_long_mode:
    ; we'll just halt the system
    jmp hang

[BITS 64]
long_mode_entry:
    ; Now we're in 64-bit long mode!
    ; Set up segment registers (most don't matter in long mode)
    xor rax, rax
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov fs, ax
    mov gs, ax
    
    ; Set up stack
    mov rsp, 0x90000
    
    ; Jump to Linux kernel
    ; The kernel expects specific register values:
    ; rsi = pointer to boot_params structure (0x8000)
    mov rsi, 0x8000
    
    ; Jump to kernel entry point
    mov rax, 0x10000           ; Kernel load address
    jmp rax
    
; Halt the system
hang:
    cli                         ; Disable interrupts
    hlt                         ; Halt the CPU
    jmp hang                    ; Just in case interrupt wakes it up

; ============================
; DATA SECTION
; ============================
boot_drive      db 0

; String messages with color codes
msg_welcome         db 0x0D, 0x0A, '=== ATOBoot - Alpha To Omega Bootloader v1.0 ===', 0x0D, 0x0A, 0
msg_checking_a20    db 'Checking A20 line...', 0x0D, 0x0A, 0
msg_a20_enabled     db '  > A20 line is enabled', 0x0D, 0x0A, 0
msg_a20_disabled    db '  > A20 line is disabled, attempting to enable...', 0x0D, 0x0A, 0
msg_memory_map      db 'Detecting system memory...', 0x0D, 0x0A, 0
msg_memory_detected db '  > Memory map successfully detected', 0x0D, 0x0A, 0
msg_memory_failed   db '  > Failed to detect memory map!', 0x0D, 0x0A, 0
msg_loading_kernel  db 'Loading Linux kernel...', 0x0D, 0x0A, 0
msg_kernel_success  db '  > Kernel loaded successfully at 0x10000', 0x0D, 0x0A, 0
msg_kernel_error    db '  > ERROR: Failed to load kernel!', 0x0D, 0x0A, 0
msg_loading_initrd  db 'Loading initial RAM disk...', 0x0D, 0x0A, 0
msg_initrd_success  db '  > InitRD loaded successfully at 0x20000', 0x0D, 0x0A, 0
msg_initrd_error    db '  > ERROR: Failed to load initrd!', 0x0D, 0x0A, 0
msg_setup_params    db 'Setting up boot parameters...', 0x0D, 0x0A, 0
msg_params_success  db '  > Boot parameters initialized', 0x0D, 0x0A, 0
msg_switching_mode  db 'Preparing to switch to 64-bit long mode...', 0x0D, 0x0A, 0
msg_boot_failed     db 'BOOT FAILED! System halted.', 0x0D, 0x0A, 0

; Linux kernel command line
kernel_cmdline      db 'init=/init ro quiet', 0

; GDT (Global Descriptor Table) for protected and long mode
gdt_start:
    ; Null descriptor
    dq 0
    
    ; 32-bit code segment descriptor
    dw 0xFFFF       ; Limit (bits 0-15)
    dw 0x0000       ; Base (bits 0-15)
    db 0x00         ; Base (bits 16-23)
    db 10011010b    ; Access byte
    db 11001111b    ; Flags (4 bits) + Limit (bits 16-19)
    db 0x00         ; Base (bits 24-31)
    
    ; 32-bit data segment descriptor
    dw 0xFFFF       ; Limit (bits 0-15)
    dw 0x0000       ; Base (bits 0-15)
    db 0x00         ; Base (bits 16-23)
    db 10010010b    ; Access byte
    db 11001111b    ; Flags (4 bits) + Limit (bits 16-19)
    db 0x00         ; Base (bits 24-31)
    
    ; 64-bit code segment descriptor
    dw 0x0000       ; Limit (bits 0-15)
    dw 0x0000       ; Base (bits 0-15)
    db 0x00         ; Base (bits 16-23)
    db 10011010b    ; Access byte
    db 00100000b    ; Flags (4 bits) + Limit (bits 16-19)
    db 0x00         ; Base (bits 24-31)
    
    ; 64-bit data segment descriptor
    dw 0x0000       ; Limit (bits 0-15)
    dw 0x0000       ; Base (bits 0-15)
    db 0x00         ; Base (bits 16-23)
    db 10010010b    ; Access byte
    db 00000000b    ; Flags (4 bits) + Limit (bits 16-19)
    db 0x00         ; Base (bits 24-31)
gdt_end:

; GDT descriptor
gdt_descriptor:
    dw gdt_end - gdt_start - 1  ; Size of GDT
    dd gdt_start                ; Address of GDT

; ============================
; BOOT SIGNATURE
; ============================
times 8048-($-$$) db 0           ; Pad the boot sector with zeros
dw 0xAA55                       ; Boot signature
