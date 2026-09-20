; BleeOS Bootloader - Stage 1 MBR
; Loads kernel from disk sectors 2+ to 0x7E00 and jumps to it

[BITS 16]
[ORG 0x7C00]

start:
    cli
    xor ax, ax
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov sp, 0x7C00
    sti

    mov [boot_drive], dl        ; BIOS passes boot drive in DL

    mov si, msg_loading
    call print_string

    ; Load kernel: 32 sectors starting at CHS 0,0,2 -> 0x0000:0x7E00
    mov ah, 0x02                ; BIOS read sectors
    mov al, 32                  ; sector count (16KB, fits C kernel)
    mov ch, 0                   ; cylinder 0
    mov cl, 2                   ; start at sector 2 (sector 1 = bootloader)
    mov dh, 0                   ; head 0
    mov dl, [boot_drive]
    xor bx, bx
    mov es, bx
    mov bx, 0x7E00              ; destination
    int 0x13
    jc disk_error               ; carry = error

    mov si, msg_ok
    call print_string

    jmp 0x0000:0x7E00           ; far jump to kernel

disk_error:
    mov si, msg_error
    call print_string
    cli
    hlt
.hang:
    jmp .hang

; SI = null-terminated string
print_string:
    pusha
    mov ah, 0x0E
.loop:
    lodsb
    test al, al
    jz .done
    int 0x10
    jmp .loop
.done:
    popa
    ret

msg_loading db 'BleeOS bootloader: loading kernel...', 13, 10, 0
msg_ok      db 'OK', 13, 10, 0
msg_error   db 'Disk read error!', 13, 10, 0

boot_drive db 0

    times 510-($-$$) db 0
    dw 0xAA55
