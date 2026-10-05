#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "comum.h"

#define DIRECTIONS 8

/*
 * Estrategia da versao paralela:
 *   1. A matriz e dividida em uma grade de blocos retangulares (linhas x
 *      colunas). A quantidade de blocos pode ser maior que a de threads.
 *   2. Cada thread retira blocos de uma fila dinamica (contador protegido por
 *      mutex) e faz flood fill apenas dentro do bloco, escrevendo rotulos
 *      locais. Como os blocos sao disjuntos, essa fase nao tem corrida.
 *   3. Na consolidacao, as fronteiras entre blocos (horizontais, verticais e
 *      diagonais, incluindo o encontro de quatro blocos) sao varridas em
 *      paralelo. Cada uniao em um Union-Find compartilhado e protegida por
 *      mutex.
 *   4. O numero de objetos e o numero de raizes distintas do Union-Find.
 */

typedef struct {
    const Matriz *matriz;
    int *rotulos;
    int blocos_linha;
    int blocos_coluna;
    int total_blocos;
    int *proximo_bloco;
    int *falhou;
    pthread_mutex_t *mutex_fila;
} ContextoTrabalho;

typedef struct {
    ContextoTrabalho *contexto;
    int identificador;
} ArgumentoTrabalho;

typedef struct {
    const Matriz *matriz;
    int *rotulos;
    int *pai;
    unsigned char *altura;
    pthread_mutex_t *mutex_uniao;
    int identificador;
    int trabalhadores;
    int blocos_linha;
    int blocos_coluna;
} ArgumentoConsolidacao;

/* ------------------------------------------------------------------ */
/* Union-Find (disjoint-set)                                          */
/* ------------------------------------------------------------------ */

static int uf_encontra(int *pai, int valor)
{
    int raiz;
    int proximo;

    raiz = valor;
    while (pai[raiz] != raiz) {
        raiz = pai[raiz];
    }
    while (pai[valor] != raiz) {
        proximo = pai[valor];
        pai[valor] = raiz;
        valor = proximo;
    }
    return raiz;
}

/* Uniao por altura (rank): mantem as arvores rasas e o find barato.
 * Deve ser chamada com o mutex de uniao ja adquirido. */
static void uf_une(int *pai, unsigned char *altura, int primeiro, int segundo)
{
    int raiz_primeiro;
    int raiz_segundo;

    raiz_primeiro = uf_encontra(pai, primeiro);
    raiz_segundo = uf_encontra(pai, segundo);
    if (raiz_primeiro == raiz_segundo) {
        return;
    }
    if (altura[raiz_primeiro] < altura[raiz_segundo]) {
        pai[raiz_primeiro] = raiz_segundo;
    } else if (altura[raiz_primeiro] > altura[raiz_segundo]) {
        pai[raiz_segundo] = raiz_primeiro;
    } else {
        pai[raiz_segundo] = raiz_primeiro;
        altura[raiz_primeiro] = altura[raiz_primeiro] + 1;
    }
}

/* ------------------------------------------------------------------ */
/* Fase 1: rotulacao local por bloco                                  */
/* ------------------------------------------------------------------ */

static int rotula_bloco(ContextoTrabalho *contexto, int indice_bloco)
{
    const Matriz *matriz;
    int *rotulos;
    int bloco_linha;
    int bloco_coluna;
    int linha_inicio;
    int linha_fim;
    int coluna_inicio;
    int coluna_fim;
    int linha_delta[DIRECTIONS] = {-1, -1, -1, 0, 0, 1, 1, 1};
    int coluna_delta[DIRECTIONS] = {-1, 0, 1, -1, 1, -1, 0, 1};
    Pilha pilha;
    Posicao posicao;
    Posicao vizinho;
    int linha;
    int coluna;
    int direcao;
    int vizinho_linha;
    int vizinho_coluna;
    int rotulo;

    matriz = contexto->matriz;
    rotulos = contexto->rotulos;
    bloco_linha = indice_bloco / contexto->blocos_coluna;
    bloco_coluna = indice_bloco % contexto->blocos_coluna;

    linha_inicio = bloco_linha * matriz->linhas / contexto->blocos_linha;
    linha_fim = (bloco_linha + 1) * matriz->linhas / contexto->blocos_linha;
    coluna_inicio = bloco_coluna * matriz->colunas / contexto->blocos_coluna;
    coluna_fim = (bloco_coluna + 1) * matriz->colunas /
                 contexto->blocos_coluna;

    if (!pilha_inicia(&pilha, (linha_fim - linha_inicio) *
                               (coluna_fim - coluna_inicio))) {
        return 0;
    }

    for (linha = linha_inicio; linha < linha_fim; ++linha) {
        for (coluna = coluna_inicio; coluna < coluna_fim; ++coluna) {
            if (matriz->celulas[linha * matriz->colunas + coluna] == 1 &&
                rotulos[linha * matriz->colunas + coluna] == 0) {
                /* Rotulo unico global: indice da celula de origem + 1. */
                rotulo = linha * matriz->colunas + coluna + 1;
                rotulos[linha * matriz->colunas + coluna] = rotulo;
                posicao.row = linha;
                posicao.col = coluna;
                if (!pilha_empilha(&pilha, posicao)) {
                    pilha_libera(&pilha);
                    return 0;
                }

                while (!pilha_vazia(&pilha)) {
                    posicao = pilha_desempilha(&pilha);
                    for (direcao = 0; direcao < DIRECTIONS; ++direcao) {
                        vizinho_linha = posicao.row + linha_delta[direcao];
                        vizinho_coluna = posicao.col + coluna_delta[direcao];
                        if (vizinho_linha >= linha_inicio &&
                            vizinho_linha < linha_fim &&
                            vizinho_coluna >= coluna_inicio &&
                            vizinho_coluna < coluna_fim &&
                            matriz->celulas[vizinho_linha * matriz->colunas +
                                            vizinho_coluna] == 1 &&
                            rotulos[vizinho_linha * matriz->colunas +
                                    vizinho_coluna] == 0) {
                            rotulos[vizinho_linha * matriz->colunas +
                                    vizinho_coluna] = rotulo;
                            vizinho.row = vizinho_linha;
                            vizinho.col = vizinho_coluna;
                            if (!pilha_empilha(&pilha, vizinho)) {
                                pilha_libera(&pilha);
                                return 0;
                            }
                        }
                    }
                }
            }
        }
    }

    pilha_libera(&pilha);
    return 1;
}

static void *trabalhador_principal(void *argumento)
{
    ArgumentoTrabalho *argumentos;
    ContextoTrabalho *contexto;

    argumentos = (ArgumentoTrabalho *)argumento;
    contexto = argumentos->contexto;

    for (;;) {
        int bloco;
        int interrompido;

        pthread_mutex_lock(contexto->mutex_fila);
        bloco = *contexto->proximo_bloco;
        *contexto->proximo_bloco = bloco + 1;
        interrompido = *contexto->falhou;
        pthread_mutex_unlock(contexto->mutex_fila);

        if (interrompido || bloco >= contexto->total_blocos) {
            break;
        }
        if (!rotula_bloco(contexto, bloco)) {
            pthread_mutex_lock(contexto->mutex_fila);
            *contexto->falhou = 1;
            pthread_mutex_unlock(contexto->mutex_fila);
            break;
        }
    }
    return NULL;
}

/* ------------------------------------------------------------------ */
/* Fase 2: consolidacao das fronteiras                                */
/* ------------------------------------------------------------------ */

static void consolida_horizontais(ArgumentoConsolidacao *argumentos)
{
    const Matriz *matriz;
    int linhas;
    int colunas;
    int faixa;
    int linha_superior;
    int linha_inferior;
    int coluna;
    int deslocamento;
    int primeiro;
    int segundo;

    matriz = argumentos->matriz;
    linhas = matriz->linhas;
    colunas = matriz->colunas;

    /* Para cada fronteira entre faixas de blocos, compara a ultima linha da
     * faixa de cima com a primeira linha da faixa de baixo, em todas as
     * colunas e com deslocamento -1, 0, +1: cobre conexoes verticais e
     * diagonais, inclusive o encontro de quatro blocos. Cada thread trata as
     * colunas cujo indice deixa o mesmo resto que o seu identificador. */
    for (faixa = 0; faixa < argumentos->blocos_linha - 1; ++faixa) {
        linha_inferior = (faixa + 1) * linhas / argumentos->blocos_linha;
        linha_superior = linha_inferior - 1;
        for (coluna = argumentos->identificador; coluna < colunas;
             coluna += argumentos->trabalhadores) {
            primeiro = argumentos->rotulos[linha_superior * colunas + coluna];
            if (primeiro == 0) {
                continue;
            }
            for (deslocamento = -1; deslocamento <= 1; ++deslocamento) {
                int coluna_vizinha;
                coluna_vizinha = coluna + deslocamento;
                if (coluna_vizinha < 0 || coluna_vizinha >= colunas) {
                    continue;
                }
                segundo = argumentos->rotulos[linha_inferior * colunas +
                                              coluna_vizinha];
                if (segundo != 0 && segundo != primeiro) {
                    pthread_mutex_lock(argumentos->mutex_uniao);
                    uf_une(argumentos->pai, argumentos->altura, primeiro,
                           segundo);
                    pthread_mutex_unlock(argumentos->mutex_uniao);
                }
            }
        }
    }
}

static void consolida_verticais(ArgumentoConsolidacao *argumentos)
{
    const Matriz *matriz;
    int linhas;
    int colunas;
    int faixa;
    int coluna_esquerda;
    int coluna_direita;
    int linha;
    int deslocamento;
    int primeiro;
    int segundo;

    matriz = argumentos->matriz;
    linhas = matriz->linhas;
    colunas = matriz->colunas;

    /* Para cada fronteira entre colunas de blocos, compara a ultima coluna da
     * esquerda com a primeira coluna da direita, em todas as linhas e com
     * deslocamento -1, 0, +1: cobre conexoes horizontais e diagonais entre
     * blocos lado a lado. Cada thread trata linhas com indice de mesmo resto
     * que o seu identificador. */
    for (faixa = 0; faixa < argumentos->blocos_coluna - 1; ++faixa) {
        coluna_direita = (faixa + 1) * colunas / argumentos->blocos_coluna;
        coluna_esquerda = coluna_direita - 1;
        for (linha = argumentos->identificador; linha < linhas;
             linha += argumentos->trabalhadores) {
            primeiro = argumentos->rotulos[linha * colunas + coluna_esquerda];
            if (primeiro == 0) {
                continue;
            }
            for (deslocamento = -1; deslocamento <= 1; ++deslocamento) {
                int linha_vizinha;
                linha_vizinha = linha + deslocamento;
                if (linha_vizinha < 0 || linha_vizinha >= linhas) {
                    continue;
                }
                segundo = argumentos->rotulos[linha_vizinha * colunas +
                                              coluna_direita];
                if (segundo != 0 && segundo != primeiro) {
                    pthread_mutex_lock(argumentos->mutex_uniao);
                    uf_une(argumentos->pai, argumentos->altura, primeiro,
                           segundo);
                    pthread_mutex_unlock(argumentos->mutex_uniao);
                }
            }
        }
    }
}

static void *consolidador_principal(void *argumento)
{
    ArgumentoConsolidacao *argumentos;

    argumentos = (ArgumentoConsolidacao *)argumento;
    consolida_horizontais(argumentos);
    consolida_verticais(argumentos);
    return NULL;
}

/* ------------------------------------------------------------------ */
/* Escolha da grade de blocos                                         */
/* ------------------------------------------------------------------ */

static int raiz_quadrada_teto(int valor)
{
    int raiz;

    raiz = 1;
    while (raiz * raiz < valor) {
        raiz = raiz + 1;
    }
    return raiz;
}

static void escolhe_grade(int trabalhadores, int linhas, int colunas,
                          int *blocos_linha, int *blocos_coluna)
{
    int colunas_grade;
    int linhas_grade;

    if (trabalhadores <= 1) {
        *blocos_linha = 1;
        *blocos_coluna = 1;
        return;
    }

    /* Usa cerca de quatro blocos por thread: a granularidade fina ajuda o
     * balanceamento de carga quando os objetos tem tamanhos muito diferentes. */
    colunas_grade = raiz_quadrada_teto(trabalhadores * 4);
    linhas_grade = colunas_grade;

    /* Para pelo menos duas threads, garante um minimo de 2x2 blocos quando a
     * matriz permite, de modo a exercitar o encontro de quatro blocos. */
    if (trabalhadores >= 2) {
        if (linhas_grade < 2 && linhas >= 2) {
            linhas_grade = 2;
        }
        if (colunas_grade < 2 && colunas >= 2) {
            colunas_grade = 2;
        }
    }

    if (linhas_grade > linhas) {
        linhas_grade = linhas;
    }
    if (colunas_grade > colunas) {
        colunas_grade = colunas;
    }
    if (linhas_grade < 1) {
        linhas_grade = 1;
    }
    if (colunas_grade < 1) {
        colunas_grade = 1;
    }

    *blocos_linha = linhas_grade;
    *blocos_coluna = colunas_grade;
}

/* ------------------------------------------------------------------ */

int main(int argc, char **argv)
{
    Matriz matriz;
    int trabalhadores;
    int *rotulos = NULL;
    int *pai = NULL;
    int *raiz_vista = NULL;
    unsigned char *altura = NULL;
    pthread_t *threads = NULL;
    ArgumentoTrabalho *argumentos_trabalho = NULL;
    ArgumentoConsolidacao *argumentos_consolidacao = NULL;
    ContextoTrabalho contexto;
    pthread_mutex_t mutex_fila;
    pthread_mutex_t mutex_uniao;
    int mutex_fila_iniciado = 0;
    int mutex_uniao_iniciado = 0;
    int proximo_bloco = 0;
    int falhou = 0;
    int blocos_linha = 0;
    int blocos_coluna = 0;
    int total;
    int criadas;
    int indice;
    int status;
    int objetos = 0;
    int resultado = 1;
    double inicio;
    double fim;

    if (argc != 3) {
        fprintf(stderr, "Uso: %s <arquivo-da-matriz> <threads>\n", argv[0]);
        return 1;
    }
    if (sscanf(argv[2], "%d", &trabalhadores) != 1 || trabalhadores < 1) {
        fprintf(stderr, "A quantidade de threads deve ser positiva.\n");
        return 1;
    }
    if (!matriz_carrega(argv[1], &matriz)) {
        return 1;
    }

    total = matriz.linhas * matriz.colunas;
    rotulos = (int *)calloc((size_t)total, sizeof(int));
    pai = (int *)malloc((size_t)(total + 1) * sizeof(int));
    raiz_vista = (int *)calloc((size_t)(total + 1), sizeof(int));
    altura = (unsigned char *)calloc((size_t)(total + 1),
                                     sizeof(unsigned char));
    threads = (pthread_t *)malloc((size_t)trabalhadores * sizeof(pthread_t));
    argumentos_trabalho = (ArgumentoTrabalho *)malloc(
        (size_t)trabalhadores * sizeof(ArgumentoTrabalho));
    argumentos_consolidacao = (ArgumentoConsolidacao *)malloc(
        (size_t)trabalhadores * sizeof(ArgumentoConsolidacao));

    if (rotulos == NULL || pai == NULL || raiz_vista == NULL ||
        altura == NULL || threads == NULL || argumentos_trabalho == NULL ||
        argumentos_consolidacao == NULL) {
        fprintf(stderr, "Nao foi possivel alocar a memoria paralela.\n");
        goto limpeza;
    }

    if (pthread_mutex_init(&mutex_fila, NULL) != 0) {
        fprintf(stderr, "pthread_mutex_init (fila) falhou.\n");
        goto limpeza;
    }
    mutex_fila_iniciado = 1;
    if (pthread_mutex_init(&mutex_uniao, NULL) != 0) {
        fprintf(stderr, "pthread_mutex_init (uniao) falhou.\n");
        goto limpeza;
    }
    mutex_uniao_iniciado = 1;

    escolhe_grade(trabalhadores, matriz.linhas, matriz.colunas, &blocos_linha,
                  &blocos_coluna);
    contexto.matriz = &matriz;
    contexto.rotulos = rotulos;
    contexto.blocos_linha = blocos_linha;
    contexto.blocos_coluna = blocos_coluna;
    contexto.total_blocos = blocos_linha * blocos_coluna;
    contexto.proximo_bloco = &proximo_bloco;
    contexto.falhou = &falhou;
    contexto.mutex_fila = &mutex_fila;

    inicio = agora_segundos();

    /* Fase 1: rotulacao local, distribuida por fila dinamica de blocos. */
    criadas = 0;
    for (indice = 0; indice < trabalhadores; ++indice) {
        argumentos_trabalho[indice].contexto = &contexto;
        argumentos_trabalho[indice].identificador = indice;
        status = pthread_create(&threads[indice], NULL, trabalhador_principal,
                                &argumentos_trabalho[indice]);
        if (status != 0) {
            fprintf(stderr, "pthread_create (trabalhador) falhou: %s\n",
                    strerror(status));
            break;
        }
        criadas = criadas + 1;
    }
    for (indice = 0; indice < criadas; ++indice) {
        status = pthread_join(threads[indice], NULL);
        if (status != 0) {
            fprintf(stderr, "pthread_join (trabalhador) falhou: %s\n",
                    strerror(status));
            goto limpeza;
        }
    }
    if (criadas != trabalhadores || falhou) {
        fprintf(stderr, "Falha ao rotular os blocos da matriz.\n");
        goto limpeza;
    }

    /* Fase 2: consolidacao concorrente das fronteiras. */
    for (indice = 0; indice <= total; ++indice) {
        pai[indice] = indice;
    }
    criadas = 0;
    for (indice = 0; indice < trabalhadores; ++indice) {
        argumentos_consolidacao[indice].matriz = &matriz;
        argumentos_consolidacao[indice].rotulos = rotulos;
        argumentos_consolidacao[indice].pai = pai;
        argumentos_consolidacao[indice].altura = altura;
        argumentos_consolidacao[indice].mutex_uniao = &mutex_uniao;
        argumentos_consolidacao[indice].identificador = indice;
        argumentos_consolidacao[indice].trabalhadores = trabalhadores;
        argumentos_consolidacao[indice].blocos_linha = blocos_linha;
        argumentos_consolidacao[indice].blocos_coluna = blocos_coluna;
        status = pthread_create(&threads[indice], NULL,
                                consolidador_principal,
                                &argumentos_consolidacao[indice]);
        if (status != 0) {
            fprintf(stderr, "pthread_create (consolidador) falhou: %s\n",
                    strerror(status));
            break;
        }
        criadas = criadas + 1;
    }
    for (indice = 0; indice < criadas; ++indice) {
        status = pthread_join(threads[indice], NULL);
        if (status != 0) {
            fprintf(stderr, "pthread_join (consolidador) falhou: %s\n",
                    strerror(status));
            goto limpeza;
        }
    }
    if (criadas != trabalhadores) {
        fprintf(stderr, "Falha ao criar as threads de consolidacao.\n");
        goto limpeza;
    }

    /* Contagem final: numero de raizes distintas entre os rotulos usados. */
    for (indice = 0; indice < total; ++indice) {
        int rotulo;
        int raiz;
        rotulo = rotulos[indice];
        if (rotulo != 0) {
            raiz = uf_encontra(pai, rotulo);
            if (!raiz_vista[raiz]) {
                raiz_vista[raiz] = 1;
                objetos = objetos + 1;
            }
        }
    }

    fim = agora_segundos();
    printf("objetos: %d\n", objetos);
    printf("threads: %d\n", trabalhadores);
    printf("blocos: %d x %d\n", blocos_linha, blocos_coluna);
    printf("tempo: %.6f\n", fim - inicio);
    resultado = 0;

limpeza:
    if (mutex_uniao_iniciado) {
        pthread_mutex_destroy(&mutex_uniao);
    }
    if (mutex_fila_iniciado) {
        pthread_mutex_destroy(&mutex_fila);
    }
    free(argumentos_consolidacao);
    free(argumentos_trabalho);
    free(threads);
    free(altura);
    free(raiz_vista);
    free(pai);
    free(rotulos);
    matriz_libera(&matriz);
    return resultado;
}
