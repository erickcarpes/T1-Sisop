# Roteiro do video de apresentacao (max. 10 min) - T1-Sisop

**Integrantes:** Augusto Ely Missiaggia e Erick Marcondes de Mattos Carpes
**Trabalho:** contagem de objetos em matriz binaria com conectividade 8
(versao sequencial de referencia e versao paralela com Pthreads).
**Formato:** gravacao de tela (terminal + slides) com os dois integrantes
narrando. Duracao alvo: **9 a 10 minutos**.

## Regras do video

- Duracao **maxima de 10 minutos** (encerre antes de 10:00).
- **Os dois** integrantes aparecem e falam; ambos sabem explicar qualquer parte.
- Publicar como **nao listado** no YouTube (ou privado compartilhado com a conta
  do professor, se indicada). O link precisa ficar acessivel durante toda a
  avaliacao.
- Deixe o terminal aberto na raiz do projeto e rode `make` **antes** de gravar:
  a demonstracao nao pode compilar na hora.
- Ambiente silencioso; teste o audio nos primeiros 30 segundos.
- Mantenha a tabela de resultados e a frase-chave a mao.

## Preparacao (antes de gravar)

- [ ] `make clean && make` sem erros e sem avisos.
- [ ] `make test` terminando com `TODOS OS TESTES PASSARAM`.
- [ ] `tests/exemplo1.txt` e `tests/exemplo3.txt` acessiveis.
- [ ] Slides/graficos abertos e numeros de desempenho visiveis.
- [ ] Fechar notificacoes e abas pessoais.
- [ ] Roteiro impresso ou em uma segunda tela.
- [ ] Cronometro iniciado junto com a gravacao.

## Distribuicao do tempo

| Inicio | Fim   | Bloco                                   | Quem fala     | Na tela |
|--------|-------|-----------------------------------------|---------------|---------|
| 0:00   | 0:45  | 1. Problema e estrategia                | Augusto       | Slide do problema |
| 0:45   | 2:30  | 2. Versao sequencial                    | Erick         | `src/sequencial.c` |
| 2:30   | 4:15  | 3. Decomposicao e sincronizacao         | Augusto       | `src/paralelo.c` |
| 4:15   | 6:00  | 4. Consolidacao e exemplo rastreavel    | Erick         | Exemplo 3 / rotulos |
| 6:00   | 7:45  | 5. Demonstracao executavel              | os dois       | Terminal |
| 7:45   | 9:15  | 6. Testes e desempenho                  | Augusto       | Tabela/grafico |
| 9:15   | 9:50  | 7. Conclusoes                           | os dois       | Slide final |

---

## Bloco 1 - Problema e estrategia (0:00-0:45, Augusto)

> "O problema e contar objetos numa matriz binaria. Usamos **conectividade 8**:
> duas celulas com valor 1 sao o mesmo objeto se estiverem conectadas por lado
> **ou por diagonal**. A versao sequencial faz *flood fill* e serve de
> referencia de correcao e de desempenho.
>
> O ponto dificil aparece no paralelo: um objeto pode **atravessar a divisao**
> entre os trabalhadores, entao **somar contagens locais nao basta**. Nossa
> estrategia tem duas fases: **rotular localmente** cada bloco em paralelo e
> depois **unificar os rotulos das fronteiras** com Union-Find."

Frase-chave para memorizar: *"flood fill local + Union-Find nas fronteiras"*.

## Bloco 2 - Versao sequencial (0:45-2:30, Erick)

Mostre o `src/sequencial.c` na tela e cubra:

- A matriz e percorrida **linha a linha** com dois lacos.
- Ao encontrar um `1` ainda nao visitado, **comeca um novo objeto** e faz-se o
  *flood fill* (`explora_componente`, linha 12).
- O *flood fill* usa uma **pilha explicita** de posicoes `(linha, coluna)` e
  testa os **8 vizinhos** (`linha_delta`/`coluna_delta`, linhas 18-19).
- A celula e marcada como visitada **antes** de empilhar (linhas 41-44): isso
  garante que cada celula entre na pilha uma unica vez, evitando trabalho
  duplicado e loop infinito.
- **Sem recursao**, de proposito, para evitar estouro de pilha em matrizes
  grandes.
- O vetor `visitado` diferencia celulas ja processadas.
- Complexidade: **O(N)** no tempo e no espaco, com N = linhas x colunas.

Demo (mostre a saida):

```sh
./bin/sequencial tests/exemplo1.txt
```

> "A primeira matriz da `objetos: 3`, exatamente o esperado."

Resultados obrigatorios (cite de memoria):

| Ex. | Dimensoes | Esperado |
|-----|-----------|----------|
| 1   | 5 x 5     | 3        |
| 2   | 6 x 8     | 4        |
| 3   | 8 x 8     | 5        |
| 4   | 9 x 12    | 6        |
| 5   | 12 x 12   | 7        |

## Bloco 3 - Decomposicao e sincronizacao (2:30-4:15, Augusto)

Mostre o `src/paralelo.c` e cubra:

- **Divisao:** grade de blocos retangulares `blocos_linha x blocos_coluna`.
  Escolhemos cerca de **4 blocos por thread** (`escolhe_grade`, linha 343), ou
  seja, **mais partes que unidades** de execucao. Isso melhora o balanceamento
  quando os objetos tem tamanhos muito diferentes.
- **Limites do bloco:** divisao inteira proporcional
  (`linha_inicio = bloco_linha * linhas / blocos_linha`, linha 126); as faixas
  contiguas diferem em no maximo 1 linha, e a sobra e espalhada.
- **Fila dinamica:** os blocos ficam numa fila; cada thread pega o proximo
  bloco incrementando `proximo_bloco` **sob o `mutex_fila`** (`trabalhador_principal`,
  linhas 191-199). Se ha mais blocos que threads, a fila distribui conforme cada
  thread termina; se ha menos blocos que threads, algumas terminam sem trabalho.
- **Fase 1 (paralela):** cada thread faz *flood fill* **so dentro do seu bloco**
  (`rotula_bloco`). Cada componente local ganha um rotulo **unico global** =
  `linha * colunas + coluna + 1` (o `+1` porque `0` significa "sem rotulo").
- **Ausencia de corrida na fase 1:** os blocos sao **disjuntos**, cada thread
  escreve apenas nas proprias celulas; nao ha dado compartilhado sendo alterado.
- **Fase 2 (concorrente):** a consolidacao usa um **Union-Find compartilhado**, e
  cada uniao e protegida pelo `mutex_uniao` (linhas 258 e 308).
- **Barreira entre fases:** o `pthread_join` da fase 1 (linha 488) garante que
  nenhuma thread consolida antes de toda a rotulagem terminar.
- **Parte sequencial:** a contagem final das raizes, depois do `pthread_join` da
  fase 2 (linha 540).

## Bloco 4 - Consolidacao e exemplo rastreavel (4:15-6:00, Erick)

Explique as fronteiras (pode desenhar no quadro ou mostrar o slide):

- **Fronteiras horizontais** (entre faixas de linhas): comparamos a **ultima
  linha** da faixa de cima com a **primeira linha** da faixa de baixo, em todas
  as colunas e com deslocamento de coluna `-1, 0, +1`. Cobre conexoes
  **verticais e diagonais** (`consolida_horizontais`, linha 218).
- **Fronteiras verticais** (entre colunas de blocos): comparamos a **ultima
  coluna** da esquerda com a **primeira coluna** da direita, em todas as linhas
  e com deslocamento de linha `-1, 0, +1`. Cobre conexoes **horizontais e
  diagonais** (`consolida_verticais`, linha 268).
- **Encontro de quatro blocos:** o par diagonal `(r, c)` e `(r+1, c+1)`, em
  blocos diferentes, e capturado pela varredura horizontal na coluna `c` com
  deslocamento `+1`.
- **Paralelismo:** as varreduras rodam em paralelo (uma thread por coluna na
  horizontal, uma por linha na vertical); so a uniao e serializada pelo mutex.
- **Resultado:** o numero de objetos e o numero de **raizes distintas** do
  Union-Find entre os rotulos usados. E **deterministico** e igual ao
  sequencial, independente da ordem das unioes e da quantidade de threads.

Exemplo rastreavel (matriz `tests/exemplo3.txt`, 8 x 8, 4 threads -> grade 4 x 4):
o objeto central ocupa uma celula em cada um dos quatro blocos que se encontram
no centro. Antes da consolidacao ele tem **quatro rotulos** (28, 29, 36, 37);
as varreduras unem os quatro, e o objeto passa a contar **uma unica vez** — o
total continua **5**, igual ao sequencial. Use isso como a prova visual de que a
soma de contagens locais estaria errada.

## Bloco 5 - Demonstracao executavel (6:00-7:45, os dois)

Ensaie a sequencia antes; grave a tela do terminal.

```sh
make
./bin/sequencial tests/exemplo1.txt
./bin/paralelo  tests/exemplo1.txt 4
make test
./bin/paralelo  tests/exemplo3.txt 4
```

> "A versao paralela imprime tambem a grade de blocos e o tempo. O `make test`
> roda os cinco exemplos obrigatorios com 1, 2, 3, 4 e 8 threads e termina com
> `TODOS OS TESTES PASSARAM`. No exemplo 3 o objeto central atravessa os quatro
> blocos e mesmo assim o resultado e `objetos: 5`, identico ao sequencial."

Mostre, se sobrar tempo, o exemplo rastreavel completo (matriz 6 x 6 com 2
threads): fase 1 gera os rotulos `1, 11, 18, 27, 29`; a consolidacao une
`11+18` (horizontal) e `27+29` (vertical), fechando **3 objetos**.

## Bloco 6 - Testes e desempenho (7:45-9:15, Augusto)

Testes:

- As 5 matrizes obrigatorias rodam nas duas versoes com **1, 2, 3, 4 e 8
  threads** - todas iguais (`make test`).
- **Teste extra:** comparacao aleatoria de **280 matrizes** (tamanhos e
  densidades variados) em **9 configuracoes** de threads, sem divergencia.

Desempenho - matriz **5000 x 5000**, densidade 40% (5 repeticoes, melhor tempo):

| Threads | Tempo (s) | Aceleracao | Eficiencia |
|---------|-----------|------------|------------|
| 1 (seq) | 1.933     | 1.00       | 1.00       |
| 2       | 1.495     | 1.29       | 0.65       |
| 4       | 1.124     | 1.72       | 0.43       |
| 8       | 0.832     | 2.32       | 0.29       |
| 12      | 0.755     | **2.56**   | 0.21       |

Explique os dois fatos que o professor costuma cobrar:

- **Por que 1 thread e mais lenta** que o sequencial (~0,81x): a versao paralela
  aloca os vetores de rotulos/pai/altura, faz a rotulacao por blocos e ainda
  varre as fronteiras; com uma unidade, esse *overhead* nao se paga.
- **Por que o ganho nao e linear (2,56x em 12 threads):**
  1. a tarefa e **limitada por banda de memoria** (*flood fill* le e escreve
     muito);
  2. a maquina de teste e um **laptop com 12 threads logicas e throttling**; um
     teste puramente CPU-bound no mesmo ambiente saturou em ~4x-6x, nao 12x;
  3. a **consolidacao** e uma parcela serial (lei de Amdahl), embora barata.

## Bloco 7 - Conclusoes (9:15-9:50, os dois)

- A versao paralela produz **exatamente** o mesmo resultado da sequencial,
  incluindo objetos que cruzam 2, 3 ou 4 blocos, e **sem condicao de corrida**
  (mutex no Union-Find, blocos disjuntos na rotulagem).
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
6. **Como evitam deadlock?** Cada thread adquire **um mutex por vez** e nunca
   mantem dois bloqueios ao mesmo tempo; nao ha espera circular.
7. **O que permanece sequencial e por que?** A contagem final das raizes; ela
   percorre a matriz uma vez depois do `pthread_join` (barreira), e paralelizar
   traria pouco ganho.
8. **Por que nao recursao?** Em matrizes grandes a recursao pode estourar a
   pilha; usamos pilha explicita, com memoria controlada.
9. **Qual a complexidade?** **O(N)** para rotular e **O(N)** para consolidar
   (varredura de fronteiras), com N = linhas x colunas.

## Checklist antes de gravar

- [ ] `make clean && make` sem erros/avisos.
- [ ] `make test` imprimindo `TODOS OS TESTES PASSARAM`.
- [ ] Terminal aberto na raiz do projeto; fontes aumentadas para leitura.
- [ ] Cada um consegue explicar as fases 1 e 2 (nao dividir por "meu pedaco").
- [ ] Saber de cor: estrategia em uma frase e os numeros de aceleracao.
- [ ] Audio testado; notificacoes silenciadas.

## Depois de gravar

- [ ] Conferir duracao (<= 10:00) e que os dois integrantes aparecem/falam.
- [ ] Publicar no YouTube como **nao listado** (ou privado compartilhado).
- [ ] Copiar o link para a secao **13. Video de apresentacao** do `README.md`.
- [ ] Abrir o link em uma **janela anonima** para confirmar o acesso.
- [ ] Marcar os itens de 13.1 e do checklist "Repositorio e apresentacao".
