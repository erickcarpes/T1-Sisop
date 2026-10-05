#include <stdio.h>
#include <stdlib.h>

#include "comum.h"

#define DIRECTIONS 8

/*
 * Flood fill iterativo a partir de (inicio_linha, inicio_coluna).
 * Marca todas as celulas 8-conectadas com valor 1 como visitadas.
 */
static int explora_componente(const Matriz *matriz, int *visitado,
                              Pilha *pilha, int inicio_linha,
                              int inicio_coluna)
{
    Posicao posicao;
    Posicao vizinho;
    int linha_delta[DIRECTIONS] = {-1, -1, -1, 0, 0, 1, 1, 1};
    int coluna_delta[DIRECTIONS] = {-1, 0, 1, -1, 1, -1, 0, 1};
    int direcao;
    int vizinho_linha;
    int vizinho_coluna;

    posicao.row = inicio_linha;
    posicao.col = inicio_coluna;
    visitado[inicio_linha * matriz->colunas + inicio_coluna] = 1;
    if (!pilha_empilha(pilha, posicao)) {
        return 0;
    }

    while (!pilha_vazia(pilha)) {
        posicao = pilha_desempilha(pilha);
        for (direcao = 0; direcao < DIRECTIONS; ++direcao) {
            vizinho_linha = posicao.row + linha_delta[direcao];
            vizinho_coluna = posicao.col + coluna_delta[direcao];
            if (vizinho_linha >= 0 && vizinho_linha < matriz->linhas &&
                vizinho_coluna >= 0 && vizinho_coluna < matriz->colunas &&
                matriz->celulas[vizinho_linha * matriz->colunas +
                                vizinho_coluna] == 1 &&
                !visitado[vizinho_linha * matriz->colunas + vizinho_coluna]) {
                visitado[vizinho_linha * matriz->colunas + vizinho_coluna] = 1;
                vizinho.row = vizinho_linha;
                vizinho.col = vizinho_coluna;
                if (!pilha_empilha(pilha, vizinho)) {
                    return 0;
                }
            }
        }
    }
    return 1;
}

/* Conta os objetos da matriz percorrendo-a linha a linha. */
static int conta_objetos(const Matriz *matriz)
{
    int *visitado;
    Pilha pilha;
    int linha;
    int coluna;
    int objetos;
    int total;

    total = matriz->linhas * matriz->colunas;
    visitado = (int *)calloc((size_t)total, sizeof(int));
    if (visitado == NULL || !pilha_inicia(&pilha, total)) {
        free(visitado);
        return -1;
    }

    objetos = 0;
    for (linha = 0; linha < matriz->linhas; ++linha) {
        for (coluna = 0; coluna < matriz->colunas; ++coluna) {
            if (matriz->celulas[linha * matriz->colunas + coluna] == 1 &&
                !visitado[linha * matriz->colunas + coluna]) {
                objetos = objetos + 1;
                if (!explora_componente(matriz, visitado, &pilha, linha,
                                        coluna)) {
                    pilha_libera(&pilha);
                    free(visitado);
                    return -1;
                }
            }
        }
    }

    pilha_libera(&pilha);
    free(visitado);
    return objetos;
}

int main(int argc, char **argv)
{
    Matriz matriz;
    int objetos;
    double inicio;
    double fim;

    if (argc != 2) {
        fprintf(stderr, "Uso: %s <arquivo-da-matriz>\n", argv[0]);
        return 1;
    }
    if (!matriz_carrega(argv[1], &matriz)) {
        return 1;
    }

    inicio = agora_segundos();
    objetos = conta_objetos(&matriz);
    fim = agora_segundos();
    matriz_libera(&matriz);

    if (objetos < 0) {
        fprintf(stderr, "Nao foi possivel contar os objetos.\n");
        return 1;
    }
    printf("objetos: %d\n", objetos);
    printf("tempo: %.6f\n", fim - inicio);
    return 0;
}
