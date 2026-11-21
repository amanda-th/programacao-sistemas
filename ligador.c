#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/*
 * Uso:   ./ligador mod_a.obj mod_b.obj [mod_c.obj ...]
 *        (sem argumentos: liga mod_a.obj e mod_b.obj)
 * Saida: programa.txt  (pronto para ser executado pela MV)
 */

#define MAX_SIMBOLOS 100
#define MAX_EXTERNOS 100
#define MAX_MODULOS  10

// Tabela Global de Simbolos (TGS)
typedef struct {
    char nome[50];
    int endereco;
    int tipo; // 0 para Instrucao, 1 para Dado
} SimboloGlobal;

SimboloGlobal tgs[MAX_SIMBOLOS];
int num_simbolos_globais = 0;

// Metadados extraidos de cada modulo no Passo 1
typedef struct {
    int inst_size;
    int data_size;
    int pc_offset; // onde este modulo comeca na memoria de instrucoes
    int dc_offset; // onde este modulo comeca na memoria de dados
} ModuloInfo;

ModuloInfo modulos[MAX_MODULOS];

// Busca endereco final de um simbolo na TGS (-1 se nao existe)
int buscar_tgs(const char* nome) {
    for (int i = 0; i < num_simbolos_globais; i++) {
        if (strcmp(tgs[i].nome, nome) == 0) return tgs[i].endereco;
    }
    return -1;
}

// Adiciona um simbolo na TGS aplicando o deslocamento do modulo
void adicionar_na_tgs(const char* nome, int endereco_local, int tipo, int offset) {
    if (buscar_tgs(nome) != -1) {
        printf("ERRO DE LINKAGEM: simbolo '%s' definido em mais de um modulo.\n", nome);
        exit(1);
    }
    if (num_simbolos_globais >= MAX_SIMBOLOS) {
        printf("ERRO DE LINKAGEM: tabela global de simbolos cheia.\n");
        exit(1);
    }
    strncpy(tgs[num_simbolos_globais].nome, nome, 49);
    tgs[num_simbolos_globais].nome[49] = '\0';
    tgs[num_simbolos_globais].endereco = endereco_local + offset;
    tgs[num_simbolos_globais].tipo = tipo;
    num_simbolos_globais++;
}

// ==============================================================
// PASSO 1: descobrir tamanhos e montar a Tabela Global
// ==============================================================
void primeiro_passo(const char* arquivo_nome, ModuloInfo* info, int atual_pc_offset, int atual_dc_offset) {
    FILE* file = fopen(arquivo_nome, "r");
    if (!file) {
        printf("Erro ao abrir %s\n", arquivo_nome);
        exit(1);
    }

    info->pc_offset = atual_pc_offset;
    info->dc_offset = atual_dc_offset;

    char buffer[256];
    int num_exports;

    if (fscanf(file, "%255s %d", buffer, &info->inst_size) != 2 ||
        fscanf(file, "%255s %d", buffer, &info->data_size) != 2 ||
        fscanf(file, "%255s %d", buffer, &num_exports) != 2) {
        printf("Erro: cabecalho invalido em %s\n", arquivo_nome);
        exit(1);
    }

    for (int i = 0; i < num_exports; i++) {
        char nome[50];
        int end_local, tipo;
        if (fscanf(file, "%49s %d %d", nome, &end_local, &tipo) != 3) {
            printf("Erro: tabela de exports invalida em %s\n", arquivo_nome);
            exit(1);
        }
        int offset = (tipo == 0) ? info->pc_offset : info->dc_offset;
        adicionar_na_tgs(nome, end_local, tipo, offset);
    }

    fclose(file);
}

// ==============================================================
// PASSO 2: relocar enderecos, resolver pendencias e gerar saida
// ==============================================================
void segundo_passo(const char* arquivo_nome, FILE* saida, ModuloInfo* info) {
    FILE* file = fopen(arquivo_nome, "r");
    if (!file) {
        printf("Erro ao abrir %s\n", arquivo_nome);
        exit(1);
    }
    char buffer[256];

    // Pula o cabecalho que ja foi lido no passo 1
    int lixo, num_exports, num_externs;
    fscanf(file, "%*s %d", &lixo);
    fscanf(file, "%*s %d", &lixo);
    fscanf(file, "%*s %d", &num_exports);
    for (int i = 0; i < num_exports; i++) fscanf(file, "%*s %*d %*d");

    // Pendencias externas (o que este modulo precisa dos outros)
    if (fscanf(file, "%255s %d", buffer, &num_externs) != 2 || num_externs > MAX_EXTERNOS) {
        printf("Erro: secao EXTERNS invalida em %s\n", arquivo_nome);
        exit(1);
    }

    struct { int linha_local; char nome[50]; } pendencias[MAX_EXTERNOS];
    for (int i = 0; i < num_externs; i++) {
        fscanf(file, "%49s %d", pendencias[i].nome, &pendencias[i].linha_local);
    }

    fscanf(file, "%255s", buffer); // pula a palavra "CODIGO"

    for (int i = 0; i < info->inst_size; i++) {
        int opcode, op1, op2, op3;
        if (fscanf(file, "%d %d %d %d", &opcode, &op1, &op2, &op3) != 4) {
            printf("Erro: codigo incompleto em %s\n", arquivo_nome);
            exit(1);
        }

        // Relocacao interna.
        // mv  reg end   -> op2 e endereco de DADOS
        // st  end reg   -> op1 e endereco de DADOS   (corrigido: antes relocava op2)
        // out end / in end -> op1 e endereco de DADOS
        // br  end       -> op1 e endereco de INSTRUCAO
        // beq/bgt/blt r r end -> op3 e endereco de INSTRUCAO
        if (opcode == 4) op2 += info->dc_offset;
        if (opcode == 5 || opcode == 10 || opcode == 11) op1 += info->dc_offset;
        if (opcode == 6) op1 += info->pc_offset;
        if (opcode >= 7 && opcode <= 9) op3 += info->pc_offset;

        // Resolve pendencias externas (instrucao que usa simbolo de outro modulo)
        for (int p = 0; p < num_externs; p++) {
            if (pendencias[p].linha_local == i) {
                int endereco_global = buscar_tgs(pendencias[p].nome);
                if (endereco_global == -1) {
                    printf("ERRO DE LINKAGEM: '%s' nao resolvido.\n", pendencias[p].nome);
                    exit(1);
                }
                switch (opcode) {
                    case 4:                  op2 = endereco_global; break;
                    case 5: case 10: case 11: op1 = endereco_global; break;
                    case 6:                  op1 = endereco_global; break;
                    case 7: case 8: case 9:  op3 = endereco_global; break;
                    default: break;
                }
            }
        }

        fprintf(saida, "%d %d %d %d\n", opcode, op1, op2, op3);
    }

    fclose(file);
}

int main(int argc, char* argv[]) {
    const char* arquivos[MAX_MODULOS];
    int n = 0;

    if (argc >= 2) {
        for (int i = 1; i < argc && n < MAX_MODULOS; i++) arquivos[n++] = argv[i];
    } else {
        arquivos[n++] = "mod_a.obj";
        arquivos[n++] = "mod_b.obj";
    }

    printf("Iniciando Ligador (Passo 1)...\n");
    int pc_off = 0, dc_off = 0;
    for (int i = 0; i < n; i++) {
        primeiro_passo(arquivos[i], &modulos[i], pc_off, dc_off);
        // O proximo modulo comeca exatamente onde este termina
        pc_off += modulos[i].inst_size;
        dc_off += modulos[i].data_size;
    }

    if (pc_off > 256 || dc_off > 64) {
        printf("ERRO DE LINKAGEM: programa final excede a memoria da MV (codigo=%d/256, dados=%d/64).\n", pc_off, dc_off);
        return 1;
    }

    printf("Iniciando Ligador (Passo 2)...\n");
    FILE* saida = fopen("programa.txt", "w");
    if (!saida) {
        printf("Erro ao criar programa.txt\n");
        return 1;
    }

    for (int i = 0; i < n; i++) {
        segundo_passo(arquivos[i], saida, &modulos[i]);
    }
    fclose(saida);

    printf("Linkagem concluida! 'programa.txt' gerado.\n");
    return 0;
}
