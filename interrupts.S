.extern __init, __vector_2
.global __vector_default, exit
.section .vectors.bootloader

/* micro-jumptable, we are using just reset and int1 vectors */
exit:
__vector_default:
jmp __init
nop
nop
jmp __vector_2
