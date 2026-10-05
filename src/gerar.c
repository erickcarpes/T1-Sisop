#include <stdio.h>
#include <stdlib.h>

/*
 * Gera uma matriz binaria aleatoria no mesmo formato lido pelos programas:
 * a primeira linha contem "linhas colunas" e em seguida vem os valores.
 *
 * Uso:
 *   gerar <linhas> <colunas> <densidade> <semente> [arquivo]
 *
 * densidade e um inteiro de 0 a 100 representando a porcentagem de celulas 1.
 * Sem o arquivo de saida, a matriz e escrita na saida padrao.
 */
int main(int argc, char **argv)
{
    int linhas;
    int colunas;
    int densidade;
    int semente;
    long total;
    long indice;
    FILE *saida;

    if (argc < 5 || argc > 6) {
        fprintf(stderr,
                "Uso: %s <linhas> <colunas> <densidade> <semente> [arquivo]\n",
                argv[0]);
        return 1;
    }

    linhas = atoi(argv[1]);
    colunas = atoi(argv[2]);
    densidade = atoi(argv[3]);
    semente = atoi(argv[4]);
    if (linhas <= 0 || colunas <= 0 || densidade < 0 || densidade > 100) {
        fprintf(stderr, "Parametros invalidos.\n");
        return 1;
    }

    if (argc == 6) {
        saida = fopen(argv[5], "w");
        if (saida == NULL) {
            fprintf(stderr, "Nao foi possivel abrir %s para escrita.\n",
                    argv[5]);
            return 1;
        }
    } else {
        saida = stdout;
    }

    srand((unsigned int)semente);
    fprintf(saida, "%d %d\n", linhas, colunas);
    total = (long)linhas * (long)colunas;
    for (indice = 0; indice < total; ++indice) {
        int valor;
        valor = (rand() % 100) < densidade ? 1 : 0;
        fprintf(saida, "%d", valor);
        if ((indice + 1) % colunas == 0) {
            fprintf(saida, "\n");
        } else {
            fprintf(saida, " ");
        }
    }

    if (saida != stdout) {
        fclose(saida);
    }
    return 0;
}
