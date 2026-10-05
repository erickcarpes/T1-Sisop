# Contagem de objetos em matriz binaria (conectividade 8)

Trabalho pratico de **Sistemas Operacionais (PUCRS)**: contagem de objetos em
uma imagem binaria representada por uma matriz, usando **conectividade 8**, com
uma versao **sequencial** de referencia e uma versao **paralela** com Pthreads.

- **Aluno(s):** Augusto Ely Missiaggia e Erick Marcondes de Mattos Carpes
  (Erick: GitHub [@erickcarpes](https://github.com/erickcarpes))
- **Disciplina:** Sistemas Operacionais - PUCRS
- **Linguagem:** ANSI C (C89/C90), com APIs POSIX (`pthread`).
- **Plataforma:** Linux/macOS.

## 1. Descricao

Uma matriz binaria (`0` = fundo, `1` = primeiro plano) e lida de um arquivo de
texto. Duas celulas `1` pertencem ao mesmo objeto quando estao conectadas por
lado **ou por diagonal** (conectividade 8). O programa percorre a matriz com
*flood fill* e reporta a quantidade de objetos.

O trabalho entrega duas implementacoes funcionalmente equivalentes:

- `src/sequencial.c` - um unico fluxo de controle, referencia de correcao e de
  desempenho.
- `src/paralelo.c` - multiplas threads POSIX que dividem o trabalho e
  consolidam objetos que atravessam as fronteiras.

## 2. Estrutura do repositorio

```text
README.md
Makefile
src/comum.h            tipos e utilitarios compartilhados (matriz, pilha, relogio)
src/comum.c
src/sequencial.c       versao sequencial
src/paralelo.c         versao paralela (Pthreads)
src/gerar.c            gerador de matrizes para os testes de desempenho
scripts/testes.sh      valida os 5 exemplos obrigatorios
scripts/bench.sh       experimento de desempenho
tests/exemplo1..5.txt  matrizes obrigatorias
results/               resultados e matrizes geradas
```

## 3. Compilacao

```sh
make
```

O `Makefile` usa `cc -std=c89 -Wall -Wextra -pedantic` (e `-pthread` na versao
paralela), compilando **sem erros e sem avisos**. Os executaveis ficam em
`bin/`: `bin/sequencial`, `bin/paralelo` e `bin/gerar`.

Para limpar:

```sh
make clean
```

## 4. Execucao

Formato do arquivo de matriz: a primeira linha tem `linhas colunas`; em seguida
vem `linhas * colunas` valores `0` ou `1`.

```text
5 5
1 1 0 0 0
1 1 0 0 0
0 0 0 1 0
0 0 0 1 0
1 0 0 0 0
```

Versao sequencial:

```sh
./bin/sequencial tests/exemplo1.txt
```

Versao paralela (`<threads>` = numero de threads, no minimo 1):

```sh
./bin/paralelo tests/exemplo1.txt 4
```

Ambas imprimem `objetos: N`. A paralela tambem imprime a configuracao da grade
de blocos e o tempo medido:

```text
objetos: 5
threads: 4
blocos: 4 x 4
tempo: 0.000867
```

Validacao dos cinco exemplos obrigatorios (sequencial e paralelo com varias
quantidades de threads):

```sh
make test
```

## 5. Versao sequencial

A matriz e percorrida linha a linha. Ao encontrar uma celula `1` ainda nao
visitada, inicia-se um novo objeto e faz-se *flood fill* com uma **pilha
explicita** de posicoes `(linha, coluna)` (sem recursao). Cada celula `1` e
marcada como visitada antes de ser empilhada, e os oito vizinhos de cada celula
sao verificados. O vetor `visitado` diferencia celulas visitadas das nao
visitadas.

## 6. Versao paralela

### 6.1 Decomposicao

A matriz e dividida em uma **grade de blocos retangulares** `blocos_linha x
blocos_coluna`. O numero de blocos e escolhido como aproximadamente **quatro
vezes o numero de threads** (por exemplo, 4x4 para 4 threads), ou seja, **as
partes podem ser mais numerosas que as unidades de execucao**. Essa
granularidade fina melhora o balanceamento quando os objetos tem tamanhos muito
diferentes.

### 6.2 Fase 1: rotulacao local (paralela)

Cada thread retira blocos de uma **fila dinamica** (um contador `proximo_bloco`
protegido por mutex) e executa *flood fill* **restrito ao bloco**. Cada
componente local recebe um rotulo **unico global**: o indice da celula de origem
`linha * colunas + coluna + 1`. Como os blocos sao disjuntos, cada thread
escreve apenas em suas proprias celulas, portanto **essa fase nao possui
corrida**.

### 6.3 Fase 2: consolidacao (concorrente, com mutex)

Um objeto pode ocupar dois ou mais blocos; somar contagens locais nao basta. As
fronteiras entre blocos sao varridas e rotulos equivalentes sao unificados com
um **Union-Find** (com uniao por altura e compressao de caminho). Cada operacao
de uniao e protegida por um **mutex** compartilhado.

- **Fronteiras horizontais** (entre faixas de linhas): comparam-se a ultima
  linha do bloco de cima com a primeira linha do bloco de baixo, em todas as
  colunas e com deslocamento `-1, 0, +1`. Isso cobre conexoes **verticais e
  diagonais** entre faixas.
- **Fronteiras verticais** (entre colunas de blocos): comparam-se a ultima
  coluna da esquerda com a primeira coluna da direita, em todas as linhas e com
  deslocamento `-1, 0, +1`. Isso cobre conexoes **horizontais e diagonais**
  entre blocos lado a lado.

O **encontro de quatro blocos** e tratado naturalmente pela varredura
horizontal: um par diagonal `(r, c)` e `(r+1, c+1)` em blocos diferentes e
comparado na coluna `c` com deslocamento `+1`, independentemente de a que bloco
cada celula pertence. A varredura cobre, portanto, conexoes por aresta e por
canto em todas as direcoes.

### 6.4 Contagem final

Apos o `pthread_join` das threads de consolidacao, a thread principal conta o
numero de **raizes distintas** do Union-Find entre os rotulos usados. O
resultado e **deterministico** e **identico ao da versao sequencial**,
independentemente da ordem das unioes e da quantidade de threads.

### 6.5 Por que o resultado e correto

Toda conexao 8 entre duas celulas `1` esta em uma destas situacoes:

1. as duas celulas estao no mesmo bloco - ja foram unidas pelo *flood fill*
   local;
2. estao em blocos diferentes porque mudam de faixa de linhas - a varredura
   horizontal as une;
3. estao em blocos diferentes porque mudam de coluna de blocos, na mesma faixa
   de linhas - a varredura vertical as une (incluindo o caso diagonal
   `(r, c)`/`(r+1, c+1)`).

Como a relacao de conectividade e transitiva, o Union-Find fecha as cadeias que
atravessam tres ou mais blocos.

### 6.6 Partes paralelas e sequenciais

- **Paralelo:** o *flood fill* por bloco (fase 1) e as varreduras de fronteira
  (fase 2).
- **Sequencial:** a contagem final das raizes do Union-Find (percorre a matriz
  uma vez apos o join).

## 7. Tratamento de erros

Os retornos de `malloc`/`calloc`, `fopen`/`fscanf`, `pthread_create`,
`pthread_join`, `pthread_mutex_init`/`destroy` sao verificados. Em caso de falha,
os recursos alocados sao liberados antes de retornar. A leitura valida as
dimensoes e aceita apenas valores `0` ou `1`.

## 8. Matrizes obrigatorias e resultados

| Ex. | Dimensoes | Esperado | Sequencial | Paralelo |
|-----|-----------|----------|------------|----------|
| 1   | 5 x 5     | 3        | 3          | 3        |
| 2   | 6 x 8     | 4        | 4          | 4        |
| 3   | 8 x 8     | 5        | 5          | 5        |
| 4   | 9 x 12    | 6        | 6          | 6        |
| 5   | 12 x 12   | 7        | 7          | 7        |

A versao paralela produziu os valores esperados para 1, 2, 3, 4 e 8 threads em
todas as cinco matrizes (`make test`). Alem dos exemplos obrigatorios, foi feito
um teste diferencial aleatorio (280 matrizes de varios tamanhos e densidades,
comparando a contagem sequencial com a paralela em 9 configuracoes de threads)
sem nenhuma divergencia.

## 9. Desempenho

Matriz grande aleatoria gerada por `bin/gerar`. Em cada configuracao foram
executadas 5 repeticoes; a tabela mostra o **minimo** e a **mediana** dos
tempos, e a aceleracao `S = Tsequencial / Tparalelo` sobre o minimo.

### 9.1 Matriz 3000 x 3000 (densidade 40%, 142432 objetos)

| Threads | Tempo min (s) | Mediana (s) | Aceleracao |
|---------|---------------|-------------|------------|
| 1 (seq) | 0.690         | 0.704       | 1.00       |
| 1 (par) | 0.804         | 0.862       | 0.86       |
| 2       | 0.527         | 0.556       | 1.31       |
| 4       | 0.393         | 0.396       | 1.76       |
| 8       | 0.326         | 0.353       | 2.12       |
| 12      | 0.284         | 0.304       | 2.43       |

### 9.2 Matriz 5000 x 5000 (densidade 40%, 394952 objetos)

| Threads | Tempo min (s) | Mediana (s) | Aceleracao |
|---------|---------------|-------------|------------|
| 1 (seq) | 1.933         | 2.016       | 1.00       |
| 1 (par) | 2.399         | 2.456       | 0.81       |
| 2       | 1.495         | 1.541       | 1.29       |
| 4       | 1.124         | 1.134       | 1.72       |
| 8       | 0.832         | 0.895       | 2.32       |
| 12      | 0.755         | 0.774       | 2.56       |

Para reproduzir:

```sh
make
./scripts/bench.sh 5000 5000 40 5 "1 2 4 8 12"
```

### 9.3 Analise

- A versao paralela com **1 thread e mais lenta** que a sequencial (0,81x a
  0,86x). Isso e esperado: ela aloca os vetores de rotulos, pai e altura, faz a
  rotulacao por blocos e ainda executa a varredura de fronteiras. O custo de
  setup e de sincronizacao nao se paga com uma unica unidade.
- O ganho cresce com a quantidade de threads, chegando a **~2,4x-2,6x** em 12
  threads. O ganho nao e linear por varios motivos:
  - a tarefa e **limitada por banda de memoria**: o *flood fill* faz muitas
    leituras e escritas na matriz e nos vetores de rotulos;
  - o computador usado tem **12 threads logicas** (2 nucleos de desempenho + 8
    eficientes em um laptop), com **throttling** de energia; um teste puramente
    CPU-bound no mesmo ambiente saturou em cerca de 4x-6x, bem abaixo de 12x;
  - a **consolidacao** e uma parcela serial (lei de Amdahl), embora restrita as
    fronteiras e barata.
- Matrizes pequenas (os exemplos obrigatorios) **nao compensam** o paralelismo:
  o tempo de criacao das threads e da consolidacao domina o tempo total
  (milissegundos), o que ilustra por que o trabalho paralelo precisa de volume.

## 10. Referencias

- Enunciado do trabalho pratico de Processos e Threads - PUCRS.
- POSIX Threads Programming (LLNL).
- Editor de tabelas C: <https://filipomor.com/editor-tabelas-c>.
