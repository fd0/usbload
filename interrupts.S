.extern __init, __vector_2
.global __vector_default, exit
.section .vectors.bootloader

/* micro-jumptable, we are using just reset and int1 vectors */
exit:
__vector_default:
    /* use jmp if available on this architecture */
    #ifdef __AVR_HAVE_JMP_CALL__
        jmp __init          /* reset vector */
        nop                 /* int0 */
        nop
        jmp __vector_2      /* int1 */
    #else
        rjmp __init         /* reset vector */
        nop                 /* int0 */
        rjmp __vector_2     /* int1 */
    #endif
