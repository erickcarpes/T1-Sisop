# Roteiro de apresentacao (10 min) - T1-Sisop

**Integrantes:** Augusto Ely Missiaggia e Erick Marcondes de Mattos Carpes
**Trabalho:** contagem de objetos em matriz binaria com conectividade 8
(versao sequencial de referencia e versao paralela com Pthreads).

Regras praticas:

- Os **dois** integrantes falam e sabem explicar qualquer parte.
- Deixe o terminal aberto na raiz do projeto e rode `make` **antes** de comecar
  (a demo nao pode compilar na hora).
- Fale devagar e use o ponteiro/terminal; mantenha os numeros da tabela na mao.

## Distribuicao do tempo

| Tempo | Bloco                                   | Fala     |
|-------|-----------------------------------------|----------|
| 1 min | 1. Problema e estrategia                | Augusto  |
| 2 min | 2. Versao sequencial                    | Erick    |
| 2 min | 3. Decomposicao e sincronizacao         | Augusto  |
| 2 min | 4. Consolidacao e demonstracao          | Erick    |
| 2 min | 5. Testes e desempenho                  | Augusto  |
| 1 min | 6. Conclusoes                           | os dois  |

---

## Bloco 1 - Problema e estrategia (1 min, Augusto)

> "O problema e contar objetos numa matriz binaria. Consideramos conectividade
> 8: duas celulas com valor 1 sao o mesmo objeto se estiverem conectadas por
> lado **ou por diagonal**. A versao sequencial faz *flood fill* e serve de
> referencia. O ponto dificil aparece no paralelo: um objeto pode atravessar a
> divisao entre os trabalhadores, entao **somar contagens locais nao basta**.
> Nossa estrategia tem duas fases: **rotular localmente** cada bloco em paralelo
> e depois **unificar os rotulos das fronteiras** com Union-Find."

Frase-chave para memorizar: *"flood fill local + Union-Find nas fronteiras"*.

## Bloco 2 - Versao sequencial (2 min, Erick)

Pontos a cobrir:

- A matriz e percorrida linha a linha com dois lacos.
- Ao encontrar um `1` ainda nao visitado, **comeca um novo objeto** e faz-se o
  *flood fill*.
- O *flood fill* usa uma **pilha explicita** `(linha, coluna)` e testa os **8
  vizinhos**; a celula e marcada como visitada **antes** de empilhar.
- Nao usamos recursao de proposito, para evitar estouro de pilha em matrizes
  grandes.
- O vetor `visitado` diferencia celulas ja processadas.
- Complexidade: **O(N)**, com N = linhas x colunas.

Mostre o comando:

```sh
./bin/sequencial tests/exemplo1.txt
```

> "O resultado e `objetos: 3`, exatamente o esperado para a primeira matriz."

Resultados obrigatorios (cite de memoria):

| Ex. | Dimensoes | Esperado |
|-----|-----------|----------|
| 1   | 5 x 5     | 3        |
| 2   | 6 x 8     | 4        |
| 3   | 8 x 8     | 5        |
| 4   | 9 x 12    | 6        |
| 5   | 12 x 12   | 7        |

## Bloco 3 - Decomposicao e sincronizacao (2 min, Augusto)

Pontos a cobrir:

- **Divisao:** grade de blocos retangulares. Escolhemos cerca de **4 blocos por
  thread**, ou seja, **mais partes que unidades** de execucao. Isso ajuda o
  balanceamento quando os objetos tem tamanhos muito diferentes.
- **Fila dinamica:** os blocos ficam numa fila; cada thread pega o proximo
  bloco pegando um contador `proximo_bloco` **protegido por mutex** (o
  `mutex_fila`).
- **Fase 1 (paralela):** cada thread faz *flood fill* **so dentro do seu
  bloco**. Cada componente local ganha um rotulo **unico global** = indice da
  celula de origem + 1.
- **Ausencia de corrida na fase 1:** os blocos sao **disjuntos**, cada thread
  escreve apenas nas suas celulas; nao ha dado compartilhado sendo alterado.
- **Fase 2 (concorrente):** a consolidacao usa um Union-Find compartilhado,
  e cada uniao e protegida pelo `mutex_uniao`.
- **Parte sequencial:** a contagem final das raizes, depois do `pthread_join`
  (que tambem e a barreira entre as fases).

## Bloco 4 - Consolidacao e demonstracao (2 min, Erick)

Explique as fronteiras (pode desenhar no quadro):

- **Fronteiras horizontais** (entre faixas de linhas): comparamos a **ultima
  linha** do bloco de cima com a **primeira linha** do bloco de baixo, em todas
  as colunas e com deslocamento de coluna `-1, 0, +1`. Cobre conexoes
  **verticais e diagonais**.
- **Fronteiras verticais** (entre blocos lado a lado): comparamos a **ultima
  coluna** da esquerda com a **primeira coluna** da direita, em todas as linhas
  e com deslocamento de linha `-1, 0, +1`. Cobre conexoes **horizontais e
  diagonais**.
- **Encontro de quatro blocos:** o par diagonal `(r, c)` e `(r+1, c+1)`, que
  esta em blocos diferentes, e capturado pela varredura horizontal na coluna
  `c` com deslocamento `+1`.
- **Resultado:** o numero de objetos e o numero de **raizes distintas** do
  Union-Find entre os rotulos usados. E **deterministico** e igual ao
  sequencial, independente da ordem das unioes e da quantidade de threads.

Demonstracao (escolha 1 ou 2, sem enrolar):

```sh
make test
./bin/paralelo tests/exemplo3.txt 4
```

> "No exemplo 3, o objeto central ocupa os quatro blocos e mesmo assim da
> `objetos: 5`, igual ao sequencial. `make test` roda os cinco exemplos com 1,
> 2, 3, 4 e 8 threads e todos batem."

## Bloco 5 - Testes e desempenho (2 min, Augusto)

Testes:

- As 5 matrizes obrigatorias rodam nas duas versoes com **1, 2, 3, 4 e 8
  threads** - todas iguais (`make test`).
- **Teste extra:** comparacao aleatoria de **280 matrizes** (tamanhos e
  densidades variados) em **9 configuracoes** de threads, sem nenhuma
  divergencia.

Desempenho - matriz **5000 x 5000**, densidade 40% (5 repeticoes, melhor tempo):

| Threads | Tempo (s) | Aceleracao |
|---------|-----------|------------|
| 1 (seq) | 1.933     | 1.00       |
| 2       | 1.495     | 1.29       |
| 4       | 1.124     | 1.72       |
| 8       | 0.832     | 2.32       |
| 12      | 0.755     | **2.56**   |

Explique os dois fatos que o professor vai cobrar:

- **Por que 1 thread e mais lenta** que o sequencial (~0,81x): a versao paralela
  aloca os vetores de rotulos/pai/altura, faz a rotulacao por blocos e ainda
  varre as fronteiras; com uma unidade, esse *overhead* nao se paga.
- **Por que o ganho nao e linear (2,56x em 12 threads):**
  1. a tarefa e **limitada por banda de memoria** (*flood fill* le e escreve
     muito);
  2. a maquina de teste e um **laptop com 12 threads logicas e throttling**; um
     teste puramente CPU-bound no mesmo ambiente saturou em ~4x-6x, nao 12x;
  3. a **consolidacao** e uma parcela serial (lei de Amdahl), embora barata.

## Bloco 6 - Conclusoes (1 min, os dois)

- A versao paralela produz **exatamente** o mesmo resultado da sequencial,
  incluindo objetos que cruzam 2, 3 ou 4 blocos, e **sem condicao de corrida**
  (mutex no Union-Find, blocos disjuntos na rotulacao).
- O paralelismo distribui **trabalho real** (flood fill por bloco), nao apenas
  cria threads.
- O ganho e limitado por **memoria e hardware**, nao pela estrategia; num
  servidor com mais nucleos e sem throttling a tendencia e melhorar.
- Licoes: decompor, sincronizar onde ha dado compartilhado e **consolidar
  resultados locais** de forma determinista.

---

## Perguntas provaveis e respostas curtas

1. **Por que conectividade 8 e nao 4?** O enunciado define que vale lado **ou**
   canto; por isso testamos os 8 vizinhos de cada celula.
2. **Como evitam contar o mesmo objeto duas vezes?** Cada componente local tem
   **um** rotulo; a consolidacao **une** rotulos equivalentes e contamos
   **raizes distintas** do Union-Find, nao rotulos.
3. **Como garantem que o resultado e identico ao sequencial?** Toda conexao 8
   ou esta dentro de um bloco (ja unida pelo flood fill) ou cruza uma fronteira
   (unida pela varredura horizontal/vertical). A transitividade do Union-Find
   fecha cadeias em 3+ blocos.
4. **Onde esta a regiao critica?** No Union-Find durante a consolidacao, sempre
   acessado sob o `mutex_uniao`; na fase 1 nao ha dado compartilhado alterado.
5. **Por que a fila dinamica?** Ha mais blocos que threads; a fila distribui o
   proximo pedaco conforme cada thread termina, melhorando o balanceamento.
6. **O que permanece sequencial e por que?** A contagem final das raizes; ela
   percorre a matriz uma vez depois do `pthread_join` (barreira), e paralelizar
   traria pouco ganho.
7. **Por que nao recursao?** Em matrizes grandes a recursao pode estourar a
   pilha; usamos pilha explicita, com memoria controlada.
8. **Qual a complexidade?** **O(N)** para rotular e **O(N)** para consolidar
   (varredura de fronteiras), com N = linhas x colunas.

## Checklist antes de comecar

- [ ] `make clean && make` sem erros/avisos.
- [ ] `make test` imprimindo `TODOS OS TESTES PASSARAM`.
- [ ] Terminal aberto na raiz do projeto.
- [ ] Cada um consegue explicar as fases 1 e 2 (nao dividir por "meu pedaco").
- [ ] Saber de cor: estrategia em uma frase e os numeros de aceleracao.
