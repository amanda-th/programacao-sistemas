<p align="center">
  <img src="https://img.shields.io/badge/C-A8B9CC?style=flat-square&logo=c&logoColor=black" alt="C" />
  <img src="https://img.shields.io/badge/Assembly-6E4C13?style=flat-square&logoColor=white" alt="Assembly" />
</p>

# Máquina Virtual MV: máquina virtual, montador e ligador

**Trabalho de Programação de Sistemas**

Este documento serve como relatório do trabalho e também como roteiro para quem quiser compilar e rodar o projeto do zero. Foi escrito pensando em outros estudantes da disciplina, então além de descrever o que cada arquivo faz, explica o raciocínio por trás das partes que mais geram dúvida (duas passagens do montador, relocação no ligador) e lista os erros mais comuns na hora de executar.

---

## Sumário

1. [Resumo](#1-resumo)
2. [Visão geral](#2-visão-geral)
3. [A máquina virtual (T1)](#3-a-máquina-virtual-t1)
4. [O montador (T2)](#4-o-montador-t2)
5. [O ligador (T3)](#5-o-ligador-t3)
6. [Programas de exemplo](#6-programas-de-exemplo)
7. [Como compilar e executar](#7-como-compilar-e-executar)
8. [Arquivos gerados](#8-arquivos-gerados)
9. [Problemas comuns](#9-problemas-comuns)
10. [Decisões, limitações e observações](#10-decisões-limitações-e-observações)
11. [Considerações finais](#11-considerações-finais)
12. [Referência](#12-referência)

---

## 1. Resumo

O trabalho tem três partes, todas em C:

* **T1, Máquina Virtual (`mv.c`):** interpreta e executa instruções representadas por números.
* **T2, Montador (`montador.c`):** traduz Assembly para código numérico, em duas passagens, e gera arquivos objeto.
* **T3, Ligador (`ligador.c`):** junta vários módulos objeto em um único programa, ajustando endereços e resolvendo referências externas.

Elas formam uma cadeia: um programa escrito em Assembly passa pelo montador (e, se tiver vários módulos, pelo ligador) e termina executando na máquina virtual. Cada peça tem uma função bem delimitada. A MV não sabe o que é um rótulo ou uma variável com nome, só enxerga números. O montador não executa nada, só traduz. O ligador não entende Assembly, trabalha apenas com os arquivos objeto.

## 2. Visão geral

### 2.1 Arquivos do projeto

```text
Maquina-Virtual-MV/
├── mv.c           máquina virtual
├── montador.c     montador de duas passagens
├── ligador.c      ligador de dois passos
├── codigo.asm     exemplo de programa com um único módulo
├── mod_a.asm      exemplo modular, módulo A
├── mod_b.asm      exemplo modular, módulo B
└── README.md
```

### 2.2 Fluxo de trabalho

```text
 programa único:   codigo.asm --> montador --> programa.txt --> MV

 programa modular: mod_a.asm --> montador --> mod_a.obj --+
                   mod_b.asm --> montador --> mod_b.obj --+--> ligador --> programa.txt --> MV
```

Os dois caminhos terminam do mesmo jeito: a MV sempre lê um arquivo chamado `programa.txt`. No caso de um módulo só, o montador já gera esse arquivo. No caso modular, o `programa.txt` que o montador gera para cada módulo não serve para execução, porque ainda tem endereços incompletos. O `programa.txt` correto é o que o ligador produz no final.

## 3. A máquina virtual (T1)

### 3.1 Arquitetura

A MV segue o modelo Harvard, com memórias separadas para instruções e dados:

| Componente | Descrição |
|---|---|
| Memória de instruções | 256 posições |
| Memória de dados | 64 posições |
| PC | Contador de programa (próxima instrução a executar) |
| Registradores gerais | `r0`, `r1`, `r2`, `r3` |

No código, as memórias são os vetores `memoria_instrucoes` e `memoria_dados`, e os registradores são o vetor `registradores[4]`. Todos começam zerados.

Cada instrução ocupa quatro campos inteiros:

```c
struct Instrucao {
    int operador, op1, op2, op3;
};
```

`operador` é o opcode e `op1`, `op2`, `op3` são os operandos. Instruções que usam menos de três operandos deixam os campos restantes em zero.

### 3.2 Conjunto de instruções

| Instrução | Opcode | O que faz |
|---|---:|---|
| `add` | 0 | `r1 = r1 + r2` |
| `sub` | 1 | `r1 = r1 - r2` |
| `mul` | 2 | `r1 = r1 * r2` |
| `div` | 3 | `r1 = r1 / r2` |
| `mv` | 4 | Carrega da memória de dados para um registrador |
| `st` | 5 | Grava um registrador na memória de dados |
| `br` | 6 | Desvio incondicional |
| `beq` | 7 | Desvia se os dois registradores forem iguais |
| `bgt` | 8 | Desvia se o primeiro for maior que o segundo |
| `blt` | 9 | Desvia se o primeiro for menor que o segundo |
| `out` | 10 | Exibe um valor da memória de dados |
| `in` | 11 | Lê um valor do teclado e grava na memória de dados |
| `stp` | 12 | Encerra a execução |

Nas operações aritméticas, o resultado fica sempre no primeiro registrador. As duas instruções de movimentação costumam confundir no começo, então vale fixar:

```asm
mv r1 val1       ; r1 = memoria_dados[endereço de val1]   (lê da memória)
st result r1     ; memoria_dados[endereço de result] = r1  (escreve na memória)
```

Os desvios condicionais têm três operandos: os dois registradores comparados e o destino (`beq r1 r2 destino`). O montador também aceita `jmp` como sinônimo de `br`.

### 3.3 Funcionamento do `mv.c`

* **`carregarInstrucoes()`** abre `programa.txt`, lê quatro inteiros por linha com `fscanf` e guarda cada grupo como uma instrução. Se o arquivo não existir ou tiver mais de 256 instruções, o programa aborta com mensagem de erro.
* **`executarInstrucoes()`** implementa o ciclo clássico: busca a instrução apontada pelo PC, identifica o opcode e executa a operação num `switch`, atualizando o PC em seguida. O ciclo se repete até um `stp` ou um erro. A função também testa divisão por zero, registradores inexistentes e endereços de memória fora dos limites.
* **`exibirDadosFinais()`** mostra, ao terminar, as dez primeiras posições da memória de instruções, as quinze primeiras da memória de dados e os valores finais dos registradores. Foi útil nos testes, porque permite conferir o estado interno da máquina e não só o que o programa imprimiu.

## 4. O montador (T2)

O montador converte Assembly em números. Um trecho como

```asm
mv r1 val1
add r1 r2
st result r1
```

vira algo como

```text
4 1 0 0
0 1 2 0
5 2 1 0
```

onde os endereços de `val1` e `result` dependem de onde essas variáveis caíram na memória de dados.

### 4.1 Por que duas passagens

Considere:

```asm
br inicio
...
inicio add r1 r2
```

Quando o montador lê o `br`, o rótulo `inicio` ainda não apareceu, então ele não sabe o endereço. A solução é percorrer o arquivo duas vezes:

1. **Primeira passagem** (`primeira_passagem`): só descobre onde cada símbolo está e monta a tabela de símbolos.
2. **Segunda passagem** (`segunda_passagem`): com todos os endereços conhecidos, gera o código numérico.

### 4.2 Primeira passagem

São mantidos dois contadores, um para cada memória (reflexo da arquitetura Harvard):

```c
int PC = 0;   // posição das instruções
int DC = 0;   // posição das variáveis
```

Cada instrução incrementa o `PC` e cada variável declarada incrementa o `DC`. Os símbolos ficam guardados nesta estrutura:

```c
typedef struct {
    char nome[50];
    int endereco;
    int tipo;      // 0 = instrução, 1 = dado
    int externo;   // declarado com extern
    int global;    // declarado com global
} Simbolo;
```

Nessa passagem também são tratadas as duas diretivas de programação modular:

* `global nome`: o símbolo será visível para outros módulos.
* `extern nome`: o símbolo é usado aqui, mas definido em outro módulo.

### 4.3 Segunda passagem

Agora cada instrução é traduzida para seu opcode e cada operando é resolvido pela função `decodificar_operando()`, que devolve:

* o índice, se o operando for um registrador;
* o endereço, se for um símbolo local;
* uma pendência registrada, se for um símbolo externo (quem resolve é o ligador).

### 4.4 Erros detectados

| Erro | Quando acontece |
|---|---|
| Símbolo não definido | Usa um nome que nunca foi declarado |
| Símbolo redefinido | Declara o mesmo nome mais de uma vez |
| Instrução inválida | Mnemônico fora do conjunto da MV |
| Registrador inválido | Registrador fora do intervalo permitido |

O montador também confere os limites de 256 instruções e 64 posições de dados.

### 4.5 Saídas

Ao terminar sem erros, o montador gera:

* **`programa.txt`:** apenas as instruções numéricas. É o que a MV lê.
* **Arquivo `.obj`:** o código numérico acompanhado de metadados para o ligador (quantidade de instruções e dados, símbolos exportados e referências externas pendentes).

## 5. O ligador (T3)

### 5.1 Por que não basta concatenar os módulos

Cada módulo é montado como se fosse o único programa: seus endereços começam em zero. Ao juntar dois módulos, o segundo passa a ficar depois do primeiro, e todos os seus endereços mudam. Além disso, referências como `jmp rotina_soma`, em que o símbolo mora em outro arquivo, não puderam ser resolvidas na montagem. O ligador cuida dessas duas coisas: relocação de endereços e resolução de referências externas.

### 5.2 Primeiro passo: Tabela Global de Símbolos

A função `primeiro_passo()` percorre todos os `.obj`, soma os tamanhos de código e dados de cada módulo para descobrir onde cada um começa e monta a Tabela Global de Símbolos (TGS) com os endereços finais dos símbolos exportados.

Exemplo, com dois módulos:

| Módulo | Instruções | Dados | Início do código | Início dos dados |
|---|---:|---:|---:|---:|
| A | 5 | 3 | 0 | 0 |
| B | 6 | 1 | 5 | 3 |

Um símbolo do módulo B com endereço local 2 na memória de dados passa a ter endereço global 2 + 3 = 5.

### 5.3 Segundo passo: relocação e resolução

A função `segundo_passo()` percorre as instruções novamente, soma o deslocamento aos endereços e substitui as referências externas pelos endereços encontrados na TGS.

Nem todo operando é endereço, então o ligador precisa saber qual campo mexer em cada instrução, e se o deslocamento é o do código ou o dos dados:

| Instrução | Campo ajustado | Tipo de endereço |
|---|---|---|
| `mv` | op2 | Dados |
| `st`, `in`, `out` | op1 | Dados |
| `br` / `jmp` | op1 | Instruções |
| `beq`, `bgt`, `blt` | op3 | Instruções |

Se esse mapeamento for aplicado errado, o programa lê variáveis nas posições erradas ou salta para o meio de outra rotina, e o erro costuma ser difícil de enxergar apenas olhando o `programa.txt`.

### 5.4 Erros detectados

Arquivo objeto inexistente, cabeçalho ou tabela de exportações inválidos, símbolo exportado em mais de um módulo, referência externa não resolvida e estouro dos limites de 256 instruções e 64 posições de dados.

O resultado é um único `programa.txt`, pronto para a MV.

## 6. Programas de exemplo

### 6.1 `codigo.asm` (módulo único)

```asm
val1 .byte 4
val2 .byte 4
result .byte 4

L in val1
  in val2

  mv r1 val1
  mv r2 val2

  add r1 r2

  st result r1
  out result

stp
```

Lê dois números, soma e imprime. A declaração `.byte 4` segue a sintaxe do enunciado; nesta implementação, cada declaração ocupa uma posição da memória de dados.

### 6.2 `mod_a.asm` e `mod_b.asm` (dois módulos)

**Módulo A**, que lê os valores e desvia para uma rotina que está em outro arquivo:

```asm
// Modulo A
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
```

**Módulo B**, que contém a rotina de soma e usa uma variável do módulo A:

```asm
// Modulo B
extern result
global rotina_soma

tmp .byte 4

rotina_soma add r1 r2
st tmp r1
st result r1
out result
out tmp

stp
```

A ligação entre os dois acontece só por `global` e `extern`:

* O módulo A usa `rotina_soma`, que é do B. O montador deixa isso como pendência no `.obj` e o ligador preenche o endereço.
* O módulo B usa `result`, que é do A. Mesma coisa no sentido inverso.
* Os valores chegam à rotina pelos registradores `r1` e `r2`, que o módulo A carrega antes do `jmp`.

## 7. Como compilar e executar

Todos os comandos devem ser executados dentro da pasta do projeto.

### 7.1 Instalando o GCC

**Linux (Debian, Ubuntu e derivadas):**

```bash
sudo apt update
sudo apt install build-essential
gcc --version
```

**Windows (MSYS2 + MinGW-w64):**

1. Instale o MSYS2 (https://www.msys2.org/).
2. Abra o terminal **MSYS2 UCRT64** e rode:

   ```bash
   pacman -Syu
   pacman -S --needed mingw-w64-ucrt-x86_64-gcc
   ```

3. Se o terminal pedir para fechar durante o `-Syu`, feche, reabra e continue.
4. Para usar o `gcc` também no PowerShell, adicione `C:\msys64\ucrt64\bin` ao PATH do Windows e reabra o terminal.
5. Confirme com `gcc --version`.

### 7.2 Compilando

Primeiro, compile os três arquivos `.c` para gerar os executáveis.

**Linux:**

```bash
gcc -o montador montador.c
gcc -o ligador ligador.c
gcc -o mv mv.c
```

**Windows (PowerShell):**

```powershell
gcc -o montador.exe montador.c
gcc -o ligador.exe ligador.c
gcc -o mv.exe mv.c
```

### 7.3 Teste 1: programa de um módulo

Para executar o programa `codigo.asm`, primeiro utilize o montador e depois a máquina virtual.

**Linux:**

```bash
./montador codigo.asm codigo.obj
./mv
```

**Windows (PowerShell):**

```powershell
.\montador.exe codigo.asm codigo.obj
.\mv.exe
```

O montador deve avisar que a montagem terminou com sucesso e criar `codigo.obj` e `programa.txt`. Ao rodar a MV, informe dois inteiros:

```text
Input para memoria_dados[0]: 15
Input para memoria_dados[1]: 25

Output (memoria_dados[2]): 40
```

### 7.4 Teste 2: dois módulos com ligador

Para executar o exemplo modular, é necessário montar os dois módulos separadamente e depois utilizar o ligador para gerar o programa final.

**Linux:**

```bash
./montador mod_a.asm mod_a.obj
./montador mod_b.asm mod_b.obj
./ligador mod_a.obj mod_b.obj
./mv
```

**Windows (PowerShell):**

```powershell
.\montador.exe mod_a.asm mod_a.obj
.\montador.exe mod_b.asm mod_b.obj
.\ligador.exe mod_a.obj mod_b.obj
.\mv.exe
```

A ordem importa. O segundo comando do montador sobrescreve o `programa.txt` do primeiro, o que não faz diferença, porque o que o ligador usa são os arquivos `.obj`. O `programa.txt` que vale é o gerado pelo ligador, e por isso a MV só deve ser executada depois dele.

Com as entradas 12 e 8, a MV deve imprimir duas linhas com o valor 20, uma para `result` e outra para `tmp`.

## 8. Arquivos gerados

| Arquivo | Gerado por | Para que serve |
|---|---|---|
| `codigo.obj`, `mod_a.obj`, `mod_b.obj` | Montador | Módulos objeto, entrada do ligador |
| `programa.txt` | Montador ou ligador | Código numérico que a MV executa |

O `programa.txt` tem só números. Os nomes das instruções (`add`, `mv`, `out`) e dos símbolos já não existem nesse estágio.

Um arquivo objeto tem este formato (valores ilustrativos):

```text
TAM_INST 5
TAM_DADOS 3

EXPORTS 1
result 2 1

EXTERNS 1
rotina_soma 4

CODIGO
11 0 0 0
11 1 0 0
4 1 0 0
4 2 1 0
6 -1 0 0
```

* `TAM_INST` e `TAM_DADOS`: quantas posições o módulo ocupa em cada memória.
* `EXPORTS`: símbolos que o módulo oferece, com seus endereços locais.
* `EXTERNS`: referências que ficaram pendentes para o ligador.
* `CODIGO`: as instruções já traduzidas.

## 9. Problemas comuns

| Mensagem ou sintoma | Causa provável | O que fazer |
|---|---|---|
| `gcc não é reconhecido` | GCC não instalado ou fora do PATH | Conferir a instalação e o PATH, reabrir o terminal |
| `Erro ao abrir programa.txt` | MV rodada antes do montador/ligador, ou em outra pasta | Gerar o arquivo e rodar a MV na mesma pasta dele |
| `Erro ao abrir mod_a.obj` | Módulo ainda não foi montado | Montar os módulos antes do ligador |
| Símbolo não definido | Nome usado sem declaração | Conferir rótulos, variáveis e diretivas |
| Símbolo redefinido | Nome declarado duas vezes | Procurar duplicatas |
| Símbolo não resolvido (ligador) | `extern` sem `global` correspondente em outro módulo | Conferir se o símbolo é exportado por algum módulo |
| `PC fora do código carregado` | Programa sem `stp`, ou desvio para endereço inválido | Conferir o `stp` e os destinos dos desvios |

Dois lembretes que evitam a maioria dos erros: a MV sempre procura `programa.txt` no diretório atual do terminal, e em programas modulares a sequência é compilar tudo, montar cada módulo, ligar e só então executar.

## 10. Decisões, limitações e observações

* **Endereços locais e globais.** O ponto que mais exigiu atenção foi entender que o endereço de um símbolo só vale dentro do módulo em que foi calculado. Esse foi o motivo para guardar tamanhos e símbolos exportados no `.obj`, e é o que justifica o ligador existir.
* **Dois espaços de endereçamento.** Por causa da arquitetura Harvard, o montador mantém `PC` e `DC` separados, e o ligador aplica deslocamentos distintos para código e dados. Tratar os dois como um só é o erro mais fácil de cometer ao implementar a relocação.
* **CS e DS.** O enunciado descreve os registradores CS (base do código) e DS (base dos dados) na arquitetura. Na implementação atual de `mv.c` eles não aparecem como registradores explícitos: existem as duas memórias separadas e o PC, mas não esses dois.
* **Declaração de variáveis.** O valor da diretiva `.byte 4` segue a sintaxe do trabalho, mas a MV não trabalha com bytes: cada declaração reserva uma posição de dados, que guarda um inteiro.
* **Comparações nos desvios.** `beq`, `bgt` e `blt` comparam os dois registradores informados na própria instrução.
* **Nomes de arquivo fixos.** A MV só lê `programa.txt`. Para testar outro programa, é preciso gerar o arquivo de novo (ou renomear) antes de rodar.

## 11. Considerações finais

O trabalho mostrou, em escala pequena, como as ferramentas que normalmente usamos sem pensar se encaixam: uma máquina que só entende números, um programa que traduz texto simbólico para esses números e outro que junta peças compiladas separadamente. Entender o que cada etapa precisa saber, e principalmente o que ela ainda não pode saber, foi o que deixou o conjunto coerente. O montador não conhece endereços de outros módulos, o ligador não conhece os nomes das instruções, e a MV não conhece nada além dos números do `programa.txt`.

Para quem for fazer algo parecido, a sugestão é implementar e testar uma parte de cada vez: primeiro a MV com um `programa.txt` escrito à mão, depois o montador com programas de um módulo, e só então o ligador.

