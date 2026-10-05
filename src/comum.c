#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#include "comum.h"

int pilha_inicia(Pilha *pilha, int capacidade)
{
    if (capacidade < 1) {
        capacidade = 1;
    }
    pilha->itens = (Posicao *)malloc((size_t)capacidade * sizeof(Posicao));
    if (pilha->itens == NULL) {
        return 0;
    }
    pilha->topo = 0;
    pilha->capacidade = capacidade;
    return 1;
}

void pilha_libera(Pilha *pilha)
{
    free(pilha->itens);
    pilha->itens = NULL;
    pilha->topo = 0;
    pilha->capacidade = 0;
}

int pilha_vazia(const Pilha *pilha)
{
    return pilha->topo == 0;
}

int pilha_empilha(Pilha *pilha, Posicao posicao)
{
    if (pilha->topo == pilha->capacidade) {
        return 0;
    }
    pilha->itens[pilha->topo] = posicao;
    pilha->topo = pilha->topo + 1;
    return 1;
}

Posicao pilha_desempilha(Pilha *pilha)
{
    pilha->topo = pilha->topo - 1;
    return pilha->itens[pilha->topo];
}

int matriz_carrega(const char *caminho, Matriz *matriz)
{
    FILE *arquivo;
    int total;
    int indice;
    int valor;

    arquivo = fopen(caminho, "r");
    if (arquivo == NULL) {
        fprintf(stderr, "Nao foi possivel abrir %s.\n", caminho);
        return 0;
    }
    if (fscanf(arquivo, "%d %d", &matriz->linhas, &matriz->colunas) != 2 ||
        matriz->linhas <= 0 || matriz->colunas <= 0) {
        fprintf(stderr, "Dimensoes invalidas.\n");
        fclose(arquivo);
        return 0;
    }

    total = matriz->linhas * matriz->colunas;
    matriz->celulas = (int *)malloc((size_t)total * sizeof(int));
    if (matriz->celulas == NULL) {
        fprintf(stderr, "Nao foi possivel alocar a matriz.\n");
        fclose(arquivo);
        return 0;
    }

    for (indice = 0; indice < total; ++indice) {
        if (fscanf(arquivo, "%d", &valor) != 1 ||
            (valor != 0 && valor != 1)) {
            fprintf(stderr, "Valor invalido na matriz.\n");
            free(matriz->celulas);
            matriz->celulas = NULL;
            fclose(arquivo);
            return 0;
        }
        matriz->celulas[indice] = valor;
    }
    fclose(arquivo);
    return 1;
}

void matriz_libera(Matriz *matriz)
{
    free(matriz->celulas);
    matriz->celulas = NULL;
    matriz->linhas = 0;
    matriz->colunas = 0;
}

double agora_segundos(void)
{
    struct timespec instante;

    if (clock_gettime(CLOCK_MONOTONIC, &instante) != 0) {
        return 0.0;
    }
    return (double)instante.tv_sec + (double)instante.tv_nsec / 1000000000.0;
}
