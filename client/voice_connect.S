.intel_syntax noprefix

.text

.globl _voice_on_preconnect_asm
_voice_on_preconnect_asm:
    pushfd
    pushad

    lea ecx, dword ptr [ecx+0x2]
    push ecx
    lea eax, dword ptr [ebx+0x12]
    push eax
    push ebx
    call _voice_on_preconnect
    add esp, 0xC
    cmp eax, 0
    je block_event

    popad
    popfd
    jmp dword ptr [_voice_continue_preconnect]

block_event:
    popad
    popfd
    xor eax, eax
    ret
