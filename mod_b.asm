// Modulo B: soma r1 + r2 e grava em dado local (tmp) e em dado do modulo A (result)
extern result
global rotina_soma
tmp .byte 4
rotina_soma add r1 r2
 st tmp r1
 st result r1
 out result
 out tmp
 stp
