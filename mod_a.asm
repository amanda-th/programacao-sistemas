// Modulo A: le dois numeros e salta para a rotina do modulo B
extern rotina_soma
global result
val1 .byte 4
val2 .byte 4
result .byte 4
 in val1
 in val2
 mv r1 val1
 mv r2 val2
 jmp rotina_soma
