val1 .byte 4 // Primeira posicao de memoria
val2 .byte 4 // Segunda posicao de memoria
result .byte 4 // Terceira posicao de memoria
L in val1
 in val2
 mv r1 val1 // Carrega val1 em r1
 mv r2 val2 // Carrega val2 em r2
 add r1 r2 // r1 = r1 + r2
 st result r1 // Salva r1 em result
 out result // Imprime result
 stp
