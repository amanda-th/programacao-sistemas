#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

/*
 * Uso:   ./montador [entrada.asm] [saida.obj]
 *        (padrao: codigo.asm -> codigo.obj)
 *
 * Gera DOIS arquivos:
 *   - programa.txt : somente o codigo numerico (pode ser executado direto na MV)
 *   - saida.obj    : modulo objeto (tamanhos, exports, externs e codigo) para o ligador
 *
 * Diretivas aceitas alem das instrucoes:
 *   nome .byte 4    (ou "nome word 0")  -> declara variavel (1 posicao de dados)
 *   global nome                         -> exporta o simbolo para outros modulos
 *   extern nome                         -> usa um simbolo definido em outro modulo
 */

#define MAX_LINHA    256
#define MAX_SIMBOLOS 100
#define MAX_COD      256   // memoria de codigo da MV
#define MAX_DADOS    64    // memoria de dados da MV
#define MAX_TOKENS   8

// ---------- Tabela de simbolos ----------
typedef struct {
    char nome[50];
    int endereco;
    int tipo;     // 0 = instrucao (rotulo), 1 = dado (variavel)
    int externo;  // 1 se declarado com "extern"
    int global;   // 1 se declarado com "global"
} Simbolo;

Simbolo tabela_simbolos[MAX_SIMBOLOS];
int num_simbolos = 0;

// Nomes declarados com "global" (resolvidos ao final da 1a passagem)
char nomes_globais[MAX_SIMBOLOS][50];
int num_globais = 0;

// Pendencias externas: instrucao (indice local) que usa um simbolo externo
typedef struct {
    char nome[50];
    int linha;
} Pendencia;

Pendencia pendencias[MAX_COD];
int num_pendencias = 0;

// ---------- Contadores (Arquitetura Harvard) ----------
int PC = 0;
int DC = 0;
int contador_erros = 0;

// Codigo gerado na 2a passagem
int codigo_gerado[MAX_COD][4];
int pc_atual = 0;

const char* lista_instrucoes[] = {"add", "sub", "mul", "div", "mv", "st", "br", "jmp", "beq", "bgt", "blt", "out", "in", "stp"};
int codigos_instrucoes[] =       {0,     1,     2,     3,     4,    5,    6,    6,     7,     8,     9,     10,    11,   12};
int total_instrucoes = 14; // 'jmp' e sinonimo de 'br'

// ---------- Utilitarios ----------
void limpar_linha(char* linha) {
    char* comentario = strstr(linha, "//");
    if (comentario != NULL) *comentario = '\0';
    // comentarios entre parenteses, como no exemplo do enunciado: add r1 r2 (r1 = soma)
    char* parenteses = strchr(linha, '(');
    if (parenteses != NULL) *parenteses = '\0';
    linha[strcspn(linha, "\r\n")] = 0;
}

int buscar_indice(const char* nome) {
    for (int i = 0; i < num_simbolos; i++) {
        if (strcmp(tabela_simbolos[i].nome, nome) == 0) return i;
    }
    return -1;
}

void inserir_simbolo(const char* nome, int endereco, int tipo, int externo) {
    if (num_simbolos >= MAX_SIMBOLOS) {
        printf("ERRO: tabela de simbolos cheia\n");
        exit(1);
    }
    strncpy(tabela_simbolos[num_simbolos].nome, nome, 49);
    tabela_simbolos[num_simbolos].nome[49] = '\0';
    tabela_simbolos[num_simbolos].endereco = endereco;
    tabela_simbolos[num_simbolos].tipo = tipo;
    tabela_simbolos[num_simbolos].externo = externo;
    tabela_simbolos[num_simbolos].global = 0;
    num_simbolos++;
}

int is_instrucao(const char* palavra) {
    for (int i = 0; i < total_instrucoes; i++) {
        if (strcmp(palavra, lista_instrucoes[i]) == 0) return 1;
    }
    return 0;
}

// "r0", "r1", "r12"... (r seguido apenas de digitos)
int eh_registrador(const char* s) {
    if (s[0] != 'r' || !isdigit((unsigned char)s[1])) return 0;
    for (int i = 1; s[i]; i++) {
        if (!isdigit((unsigned char)s[i])) return 0;
    }
    return 1;
}

// ---------- Analise de uma linha ----------
enum { L_VAZIA, L_ROTULO, L_DADO, L_INSTR, L_DIRETIVA, L_INVALIDA };

typedef struct {
    int tipo;
    int tem_rotulo;
    char rotulo[50];
    char diretiva[10];   // "global" ou "extern"
    char nome[50];       // nome da diretiva
    char mnem[50];       // mnemonico (ou texto invalido, para a mensagem de erro)
    char ops[3][50];
    int nops;
} Linha;

void analisar_linha(char* texto, Linha* l) {
    char copia[MAX_LINHA];
    char tk[MAX_TOKENS][50];
    int n = 0;

    memset(l, 0, sizeof(*l));
    limpar_linha(texto);
    strncpy(copia, texto, MAX_LINHA - 1);
    copia[MAX_LINHA - 1] = '\0';

    char* t = strtok(copia, " \t,");
    while (t && n < MAX_TOKENS) {
        strncpy(tk[n], t, 49);
        tk[n][49] = '\0';
        n++;
        t = strtok(NULL, " \t,");
    }

    if (n == 0) { l->tipo = L_VAZIA; return; }

    // Diretivas global / extern
    if (strcmp(tk[0], "global") == 0 || strcmp(tk[0], "extern") == 0) {
        if (n < 2) {
            l->tipo = L_INVALIDA;
            strcpy(l->mnem, tk[0]);
            return;
        }
        l->tipo = L_DIRETIVA;
        strcpy(l->diretiva, tk[0]);
        strcpy(l->nome, tk[1]);
        return;
    }

    int i = 0;
    if (!is_instrucao(tk[0])) {
        // Primeira palavra nao e instrucao: e rotulo ou variavel
        strcpy(l->rotulo, tk[0]);
        l->tem_rotulo = 1;
        i = 1;

        if (n == 1) { l->tipo = L_ROTULO; return; }
        if (strcmp(tk[1], "word") == 0 || strcmp(tk[1], ".byte") == 0) {
            l->tipo = L_DADO;
            return;
        }
        if (!is_instrucao(tk[1])) {
            // Ex.: "foo r1 r2" -> o erro e a palavra "foo"
            l->tipo = L_INVALIDA;
            strcpy(l->mnem, tk[0]);
            return;
        }
    }

    l->tipo = L_INSTR;
    strcpy(l->mnem, tk[i]);
    for (int k = i + 1; k < n && l->nops < 3; k++) {
        strcpy(l->ops[l->nops++], tk[k]);
    }
}

// =========================================================
// PRIMEIRA PASSAGEM: mapear rotulos, variaveis e diretivas
// =========================================================
void primeira_passagem(const char* nome_arquivo) {
    FILE* arquivo = fopen(nome_arquivo, "r");
    if (!arquivo) {
        printf("Erro: nao foi possivel abrir '%s'.\n", nome_arquivo);
        exit(1);
    }

    char linha[MAX_LINHA];
    while (fgets(linha, sizeof(linha), arquivo)) {
        Linha l;
        analisar_linha(linha, &l);

        if (l.tipo == L_VAZIA) continue;

        if (l.tipo == L_INVALIDA) {
            printf("ERRO: instrucao invalida -> '%s'\n", l.mnem);
            contador_erros++;
            continue;
        }

        if (l.tipo == L_DIRETIVA) {
            if (strcmp(l.diretiva, "extern") == 0) {
                if (buscar_indice(l.nome) != -1) {
                    printf("ERRO: simbolo redefinido -> '%s'\n", l.nome);
                    contador_erros++;
                } else {
                    inserir_simbolo(l.nome, 0, 0, 1);
                }
            } else if (num_globais < MAX_SIMBOLOS) {
                strcpy(nomes_globais[num_globais++], l.nome);
            }
            continue;
        }

        if (l.tem_rotulo) {
            if (buscar_indice(l.rotulo) != -1) {
                printf("ERRO: simbolo redefinido -> '%s'\n", l.rotulo);
                contador_erros++;
            } else if (l.tipo == L_DADO) {
                inserir_simbolo(l.rotulo, DC, 1, 0);
            } else {
                inserir_simbolo(l.rotulo, PC, 0, 0);
            }
        }

        if (l.tipo == L_DADO) DC++;
        else if (l.tipo == L_INSTR) PC++;
    }
    fclose(arquivo);

    // Confere os simbolos declarados como global
    for (int i = 0; i < num_globais; i++) {
        int idx = buscar_indice(nomes_globais[i]);
        if (idx == -1 || tabela_simbolos[idx].externo) {
            printf("ERRO: simbolo nao definido -> '%s'\n", nomes_globais[i]);
            contador_erros++;
        } else {
            tabela_simbolos[idx].global = 1;
        }
    }

    if (PC > MAX_COD) {
        printf("ERRO: programa excede a memoria de codigo (%d palavras)\n", MAX_COD);
        contador_erros++;
    }
    if (DC > MAX_DADOS) {
        printf("ERRO: programa excede a memoria de dados (%d palavras)\n", MAX_DADOS);
        contador_erros++;
    }
}

// Traduz um operando: registrador (r0..r3), simbolo local ou simbolo externo
int decodificar_operando(const char* op) {
    if (eh_registrador(op)) {
        int n = atoi(op + 1);
        if (n > 3) {
            printf("ERRO: registrador invalido -> '%s'\n", op);
            contador_erros++;
            return 0;
        }
        return n;
    }

    int idx = buscar_indice(op);
    if (idx == -1) {
        printf("ERRO: simbolo nao definido -> '%s'\n", op);
        contador_erros++;
        return 0;
    }

    if (tabela_simbolos[idx].externo) {
        // O ligador preenche o endereco depois: registra a pendencia
        if (num_pendencias < MAX_COD) {
            strcpy(pendencias[num_pendencias].nome, op);
            pendencias[num_pendencias].linha = pc_atual;
            num_pendencias++;
        }
        return -1;
    }
    return tabela_simbolos[idx].endereco;
}

// =========================================================
// SEGUNDA PASSAGEM: traduzir para numeros
// =========================================================
void segunda_passagem(const char* nome_arquivo) {
    FILE* entrada = fopen(nome_arquivo, "r");
    if (!entrada) {
        printf("Erro: nao foi possivel abrir '%s'.\n", nome_arquivo);
        exit(1);
    }

    char linha[MAX_LINHA];
    while (fgets(linha, sizeof(linha), entrada)) {
        Linha l;
        analisar_linha(linha, &l);

        if (l.tipo != L_INSTR) continue; // erros de linha invalida ja foram dados na 1a passagem
        if (pc_atual >= MAX_COD) break;  // ja reportado na 1a passagem

        int codigo = -1;
        for (int i = 0; i < total_instrucoes; i++) {
            if (strcmp(l.mnem, lista_instrucoes[i]) == 0) {
                codigo = codigos_instrucoes[i];
                break;
            }
        }

        int ops[3] = {0, 0, 0};
        for (int k = 0; k < l.nops; k++) ops[k] = decodificar_operando(l.ops[k]);

        codigo_gerado[pc_atual][0] = codigo;
        codigo_gerado[pc_atual][1] = ops[0];
        codigo_gerado[pc_atual][2] = ops[1];
        codigo_gerado[pc_atual][3] = ops[2];
        pc_atual++;
    }
    fclose(entrada);
}

// =========================================================
// SAIDAS
// =========================================================
void escrever_programa_txt() {
    FILE* f = fopen("programa.txt", "w");
    if (!f) { printf("Erro ao criar programa.txt\n"); exit(1); }
    for (int i = 0; i < pc_atual; i++) {
        fprintf(f, "%d %d %d %d\n", codigo_gerado[i][0], codigo_gerado[i][1],
                codigo_gerado[i][2], codigo_gerado[i][3]);
    }
    fclose(f);
}

// Formato lido pelo ligador
void escrever_obj(const char* nome_obj) {
    FILE* f = fopen(nome_obj, "w");
    if (!f) { printf("Erro ao criar %s\n", nome_obj); exit(1); }

    fprintf(f, "TAM_INST %d\n", pc_atual);
    fprintf(f, "TAM_DADOS %d\n", DC);

    int n_exports = 0;
    for (int i = 0; i < num_simbolos; i++) {
        if (tabela_simbolos[i].global && !tabela_simbolos[i].externo) n_exports++;
    }
    fprintf(f, "EXPORTS %d\n", n_exports);
    for (int i = 0; i < num_simbolos; i++) {
        if (tabela_simbolos[i].global && !tabela_simbolos[i].externo) {
            fprintf(f, "%s %d %d\n", tabela_simbolos[i].nome,
                    tabela_simbolos[i].endereco, tabela_simbolos[i].tipo);
        }
    }

    fprintf(f, "EXTERNS %d\n", num_pendencias);
    for (int i = 0; i < num_pendencias; i++) {
        fprintf(f, "%s %d\n", pendencias[i].nome, pendencias[i].linha);
    }

    fprintf(f, "CODIGO\n");
    for (int i = 0; i < pc_atual; i++) {
        fprintf(f, "%d %d %d %d\n", codigo_gerado[i][0], codigo_gerado[i][1],
                codigo_gerado[i][2], codigo_gerado[i][3]);
    }
    fclose(f);
}

int main(int argc, char* argv[]) {
    const char* entrada = (argc >= 2) ? argv[1] : "codigo.asm";
    const char* saida_obj = (argc >= 3) ? argv[2] : "codigo.obj";

    printf("Iniciando montador...\n");

    primeira_passagem(entrada);
    segunda_passagem(entrada);

    if (contador_erros > 0) {
        printf("Montagem abortada: %d erro(s).\n", contador_erros);
        return 1;
    }

    escrever_programa_txt();
    escrever_obj(saida_obj);

    printf("Montagem concluida com sucesso! Gerados 'programa.txt' e '%s'.\n", saida_obj);
    return 0;
}
