set disassembly-flavor intel
set confirm off

define mode16
    set architecture i8086
end

define mode32
    set architecture i386
end

define r16
    i r ax bx cx dx sp bp si di eip cs ds es ss eflags
end

define r32
    i r eax ebx ecx edx esp ebp esi edi eip cs ds es ss eflags
end

define seg
    i r registers cs ds es ss fs gs
end

define ctrl
    i r cr0 cr2 cr3 cr4
end

define mir
  monitor info registers
end

define code16
    set architecture i8086
    x/10i $pc
end

define code32
    set architecture i386
    x/10i $pc
end

define stack16
    x/16xh $sp
end

define stack32
    x/16xw $esp
end

define mem
    x/32xb $arg0
end

define mim
    monitor info mem
end

define ctx16
    r16
    seg
    code16
end

define ctx32
    r32
    seg
    ctrl
    code32
end

define boot
    b *0x7c00
    c
end
