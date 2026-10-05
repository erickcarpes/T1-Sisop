#ifndef COMUM_H
#define COMUM_H

/* Tipos e utilidades compartilhados pelas versoes sequencial e paralela. */

typedef struct {
    int row;
    int col;
} Posicao;

typedef struct {
    Posicao *itens;
    int topo;
    int capacidade;
} Pilha;

typedef struct {
    int linhas;
    int colunas;
    int *celulas;
} Matriz;

/* Pilha explicita (evita recursao no flood fill). */
int pilha_inicia(Pilha *pilha, int capacidade);
void pilha_libera(Pilha *pilha);
int pilha_vazia(const Pilha *pilha);
int pilha_empilha(Pilha *pilha, Posicao posicao);
Posicao pilha_desempilha(Pilha *pilha);

/* Leitura e liberacao de uma matriz binaria em arquivo texto. */
int matriz_carrega(const char *caminho, Matriz *matriz);
void matriz_libera(Matriz *matriz);

/* Relogio monotonico em segundos, usado na analise de desempenho. */
double agora_segundos(void);

#endif
