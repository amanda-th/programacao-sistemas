#include <stdio.h>
#include <stdlib.h>

// Tamanhos definidos no enunciado
#define TAM_COD   256   // memoria de codigo: 256 palavras
#define TAM_DADOS 64    // memoria de dados: 64 palavras

// A struct serve EXCLUSIVAMENTE para a Memoria de Instrucoes
struct Instrucao {
    int operador, op1, op2, op3;
};

int pc = 0;
int total_instrucoes = 0; // quantas instrucoes foram carregadas do programa.txt

struct Instrucao memoria_instrucoes[TAM_COD];
int memoria_dados[TAM_DADOS] = {0};

int registradores[4] = {0, 0, 0, 0};

enum CodigoDeOperacao {
    add = 0,
    sub = 1,
    mul = 2,
    div_op = 3,
    mv  = 4,
    st  = 5,
    br  = 6,
    beq = 7,
    bgt = 8,
    blt = 9,
    out = 10,
    in_op = 11,
    stp = 12
};

int* obterRegistrador(int id) {
    if (id >= 0 && id < 4) {
        return &registradores[id];
    }
    return NULL;
}

int enderecoDadosValido(int end) {
    return end >= 0 && end < TAM_DADOS;
}

// le o arquivo e preenche a memoria_instrucoes
void carregarInstrucoes() {
    FILE *arquivo = fopen("programa.txt", "r");
    if (arquivo == NULL) {
        printf("Erro ao abrir o arquivo de instrucoes.\n");
        exit(1);
    }

    while (total_instrucoes < TAM_COD &&
           fscanf(arquivo, "%d %d %d %d",
                  &memoria_instrucoes[total_instrucoes].operador,
                  &memoria_instrucoes[total_instrucoes].op1,
                  &memoria_instrucoes[total_instrucoes].op2,
                  &memoria_instrucoes[total_instrucoes].op3) == 4) {
        total_instrucoes++;
    }

    // Se ainda sobrou conteudo, o programa nao cabe na memoria de codigo
    if (total_instrucoes == TAM_COD) {
        int sobra;
        if (fscanf(arquivo, "%d", &sobra) == 1) {
            printf("Erro: programa maior que a memoria de codigo (%d palavras).\n", TAM_COD);
            fclose(arquivo);
            exit(1);
        }
    }
    fclose(arquivo);
}

void executarInstrucoes() {
    while (1) {
        // Protecao: PC fora do codigo carregado (por exemplo, programa sem stp)
        if (pc < 0 || pc >= total_instrucoes) {
            printf("Erro: PC=%d fora do codigo carregado (faltou stp?)\n", pc);
            return;
        }

        // FETCH: busca apenas na memoria de instrucoes
        struct Instrucao instrucao = memoria_instrucoes[pc];
        int *reg1, *reg2;

        switch (instrucao.operador) {
            // Instrucoes aritmeticas: r1 = r1 (op) r2   (conforme o enunciado)
            case add:
            case sub:
            case mul:
            case div_op:
                reg1 = obterRegistrador(instrucao.op1);
                reg2 = obterRegistrador(instrucao.op2);
                if (!reg1 || !reg2) {
                    printf("Erro: registrador invalido no endereco %d\n", pc);
                    return;
                }
                if (instrucao.operador == add)      *reg1 = (*reg1) + (*reg2);
                else if (instrucao.operador == sub) *reg1 = (*reg1) - (*reg2);
                else if (instrucao.operador == mul) *reg1 = (*reg1) * (*reg2);
                else {
                    if (*reg2 == 0) {
                        printf("Erro: divisao por zero no endereco %d\n", pc);
                        return;
                    }
                    *reg1 = (*reg1) / (*reg2);
                }
                pc++;
                break;

            case mv: // mv reg endereco  -> reg = memdata[endereco]
                reg1 = obterRegistrador(instrucao.op1);
                if (!reg1 || !enderecoDadosValido(instrucao.op2)) {
                    printf("Erro: operando invalido em mv no endereco %d\n", pc);
                    return;
                }
                *reg1 = memoria_dados[instrucao.op2];
                pc++;
                break;

            case st: // st endereco reg  -> memdata[endereco] = reg
                reg1 = obterRegistrador(instrucao.op2);
                if (!reg1 || !enderecoDadosValido(instrucao.op1)) {
                    printf("Erro: operando invalido em st no endereco %d\n", pc);
                    return;
                }
                memoria_dados[instrucao.op1] = *reg1;
                pc++;
                break;

            case br:
                pc = instrucao.op1;
                break;

            // op1 e op2 sao registradores; op3 e o endereco de destino do salto
            case beq:
            case bgt:
            case blt:
                reg1 = obterRegistrador(instrucao.op1);
                reg2 = obterRegistrador(instrucao.op2);
                if (!reg1 || !reg2) {
                    printf("Erro: registrador invalido no endereco %d\n", pc);
                    return;
                }
                if ((instrucao.operador == beq && *reg1 == *reg2) ||
                    (instrucao.operador == bgt && *reg1 >  *reg2) ||
                    (instrucao.operador == blt && *reg1 <  *reg2)) {
                    pc = instrucao.op3;
                } else {
                    pc++;
                }
                break;

            case out:
                if (!enderecoDadosValido(instrucao.op1)) {
                    printf("Erro: endereco invalido em out no endereco %d\n", pc);
                    return;
                }
                printf("Output (memoria_dados[%d]): %d\n", instrucao.op1, memoria_dados[instrucao.op1]);
                pc++;
                break;

            case in_op:
                if (!enderecoDadosValido(instrucao.op1)) {
                    printf("Erro: endereco invalido em in no endereco %d\n", pc);
                    return;
                }
                printf("Input para memoria_dados[%d]: ", instrucao.op1);
                if (scanf("%d", &memoria_dados[instrucao.op1]) != 1) {
                    printf("\nEntrada encerrada ou invalida. Execucao interrompida.\n");
                    return;
                }
                pc++;
                break;

            case stp:
                return;

            default:
                printf("Instrucao invalida no endereco %d\n", pc);
                return;
        }
    }
}

void exibirDadosFinais() {
    printf("\n--- Estado Final ---\n");

    printf("\nMemoria de Instrucoes (primeiras 10 posicoes):\n");
    for (int i = 0; i < 10; i++) {
        printf("Inst[%02d]: %02d %d %d %d\n",
               i,
               memoria_instrucoes[i].operador,
               memoria_instrucoes[i].op1,
               memoria_instrucoes[i].op2,
               memoria_instrucoes[i].op3);
    }

    printf("\nMemoria de Dados (primeiras 15 posicoes):\n");
    for (int i = 0; i < 15; i++) {
        printf("Dados[%02d]: %d\n", i, memoria_dados[i]);
    }

    printf("\nRegistradores Finais:\n");
    for (int i = 0; i < 4; i++) {
        printf("r%d = %d\n", i, registradores[i]);
    }
}

int main() {
    carregarInstrucoes();
    executarInstrucoes();
    exibirDadosFinais();

    return 0;
}
