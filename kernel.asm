; BleeOS Kernel + Shell (16-bit real mode)
; Loaded by boot.asm at 0x7E00

[BITS 16]
[ORG 0x7E00]

kernel_start:
    cli
    xor ax, ax
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov sp, 0x9000
    sti

    call clear_screen

    mov si, banner
    call print_string

    mov si, msg_help_hint
    call print_string

shell_loop:
    mov si, prompt
    call print_string

    call read_line            ; fills input_buffer

    call handle_command
    jmp shell_loop

; ---------------- Screen / print ----------------

print_string:               ; SI = string
    pusha
    mov ah, 0x0E
.loop:
    lodsb
    test al, al
    jz .done
    cmp al, 10
    je .newline
    int 0x10
    jmp .loop
.newline:
    mov al, 13
    int 0x10
    mov al, 10
    int 0x10
    jmp .loop
.done:
    popa
    ret

print_char:                 ; AL = char
    push ax
    push bx
    mov ah, 0x0E
    mov bh, 0
    int 0x10
    pop bx
    pop ax
    ret

newline:
    push si
    mov si, crlf
    call print_string
    pop si
    ret

clear_screen:
    pusha
    mov ah, 0x00
    mov al, 0x03            ; 80x25 text mode
    int 0x10
    popa
    ret

; ---------------- Input ----------------

read_line:
    pusha
    mov di, input_buffer
    xor cx, cx              ; char count
.loop:
    mov ah, 0x00
    int 0x16                ; AL = key
    cmp al, 13              ; Enter?
    je .done
    cmp al, 8               ; Backspace?
    je .backspace
    cmp cx, 255
    jae .loop               ; buffer full, ignore
    ; echo
    mov ah, 0x0E
    int 0x10
    stosb
    inc cx
    jmp .loop
.backspace:
    cmp cx, 0
    je .loop
    dec di
    dec cx
    ; erase on screen: backspace, space, backspace
    mov ah, 0x0E
    mov al, 8
    int 0x10
    mov al, ' '
    int 0x10
    mov al, 8
    int 0x10
    jmp .loop
.done:
    mov byte [di], 0        ; null terminate
    call newline
    popa
    ret

; ---------------- Command handling ----------------

handle_command:
    pusha
    mov si, input_buffer
    cmp byte [si], 0
    je .done                ; empty line

    mov si, input_buffer
    mov di, cmd_help
    call strcmp
    jc .do_help

    mov si, input_buffer
    mov di, cmd_clear
    call strcmp
    jc .do_clear

    mov si, input_buffer
    mov di, cmd_info
    call strcmp
    jc .do_info

    mov si, input_buffer
    mov di, cmd_ver
    call strcmp
    jc .do_info

    mov si, input_buffer
    mov di, cmd_reboot
    call strcmp
    jc .do_reboot

    mov si, input_buffer
    mov di, cmd_halt
    call strcmp
    jc .do_halt

    mov si, input_buffer
    mov di, cmd_echo
    call startswith_word    ; matches "echo" + space/end?
    jc .do_echo

    ; unknown
    mov si, msg_unknown
    call print_string
    jmp .done

.do_help:
    mov si, msg_help
    call print_string
    jmp .done
.do_clear:
    call clear_screen
    jmp .done
.do_info:
    mov si, msg_info
    call print_string
    jmp .done
.do_echo:
    mov si, input_buffer
    add si, 4               ; skip "echo"
    cmp byte [si], 0
    je .echo_nl
    cmp byte [si], ' '
    jne .echo_nospace
    inc si                  ; skip one space, rest is literal
.echo_print:
    call print_string
.echo_nl:
    call newline
    jmp .done
.echo_nospace:
    ; "echofoo" is not echo command -> unknown? treat as unknown
    mov si, msg_unknown
    call print_string
    jmp .done
.do_reboot:
    mov si, msg_reboot
    call print_string
    xor ax, ax
    int 0x19                ; BIOS reboot; fallback below
    jmp 0xFFFF:0x0000
.do_halt:
    mov si, msg_halt
    call print_string
    cli
    hlt
    jmp $                   ; should never return
.done:
    popa
    ret

; SI, DI = strings. Carry set if equal.
strcmp:
    push si
    push di
    push ax
.loop:
    mov al, [si]
    mov ah, [di]
    cmp al, ah
    jne .no
    cmp al, 0
    je .yes
    inc si
    inc di
    jmp .loop
.yes:
    stc
    jmp .out
.no:
    clc
.out:
    pop ax
    pop di
    pop si
    ret

; Checks if SI starts with word in DI, where next char in SI is ' ' or 0.
; Carry set on match.
startswith_word:
    push si
    push di
    push ax
.loop:
    mov ah, [di]
    cmp ah, 0
    je .check_boundary
    mov al, [si]
    cmp al, ah
    jne .no
    inc si
    inc di
    jmp .loop
.check_boundary:
    mov al, [si]
    cmp al, 0
    je .yes
    cmp al, ' '
    je .yes
    jmp .no
.yes:
    stc
    jmp .out
.no:
    clc
.out:
    pop ax
    pop di
    pop si
    ret

; ---------------- Data ----------------

banner      db '==============================', 10
            db '  BleeOS v0.1 - tiny x86 OS', 10
            db '  bootloader + shell ready', 10
            db '==============================', 10, 0
msg_help_hint db 'Type `help` for commands.', 10, 0
prompt      db 'bleeos> ', 0
crlf        db 10, 0

cmd_help    db 'help', 0
cmd_clear   db 'clear', 0
cmd_info    db 'info', 0
cmd_ver     db 'ver', 0
cmd_reboot  db 'reboot', 0
cmd_halt    db 'halt', 0
cmd_echo    db 'echo', 0

msg_help    db 'Commands:', 10
            db '  help         - show this help', 10
            db '  echo <text>  - print text', 10
            db '  clear        - clear screen', 10
            db '  info / ver   - system info', 10
            db '  reboot       - reboot machine', 10
            db '  halt         - halt CPU', 10, 0
msg_unknown db 'Unknown command. Type `help`.', 10, 0
msg_info    db 'BleeOS v0.1 | 16-bit real mode | bootloader: NASM MBR | shell: BIOS I/O', 10, 0
msg_reboot  db 'Rebooting...', 10, 0
msg_halt    db 'Halted. You can close QEMU.', 10, 0

input_buffer times 256 db 0

; Pad kernel to whole-sector size so os.img sector math stays simple.
; $ current size; pad up to next 512-byte multiple.
times 512 - (($-$$) % 512) db 0
