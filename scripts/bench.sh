#!/usr/bin/env bash
#
# Experimento de desempenho: gera (ou reaproveita) uma matriz grande, mede a
# versao sequencial e a paralela com varias quantidades de threads e calcula a
# aceleracao S = Tsequencial / Tparalelo. Usa o minimo e a mediana de REP
# execucoes para reduzir o ruido de medicao.
#
# Uso: scripts/bench.sh [linhas] [colunas] [densidade] [repeticoes] [threads]
set -eu

cd "$(dirname "$0")/.."

LINHAS=${1:-3000}
COLUNAS=${2:-3000}
DENSIDADE=${3:-40}
REP=${4:-5}
THREADS=${5:-"1 2 4 8"}

mkdir -p results
ARQUIVO="results/grande_${LINHAS}x${COLUNAS}_d${DENSIDADE}.txt"

if [ ! -f "$ARQUIVO" ]; then
    echo "Gerando matriz $LINHAS x $COLUNAS (densidade ${DENSIDADE}%)..."
    ./bin/gerar "$LINHAS" "$COLUNAS" "$DENSIDADE" 20261002 "$ARQUIVO"
fi

temporario=$(mktemp)
trap 'rm -f "$temporario"' EXIT

coleta_seq() {
    : > "$temporario"
    for ((r = 0; r < REP; r++)); do
        ./bin/sequencial "$ARQUIVO" | awk '/^tempo:/{print $2}' >> "$temporario"
    done
}

coleta_par() {
    local th=$1
    : > "$temporario"
    for ((r = 0; r < REP; r++)); do
        ./bin/paralelo "$ARQUIVO" "$th" | awk '/^tempo:/{print $2}' >> "$temporario"
    done
}

minimo() {
    sort -n "$temporario" | head -1
}

mediana() {
    sort -n "$temporario" | awk '{a[NR]=$1} END{
        if (NR == 0) {print "nan"}
        else if (NR % 2) {print a[(NR+1)/2]}
        else {printf "%.6f\n", (a[NR/2] + a[NR/2+1]) / 2}
    }'
}

objetos_seq=$(./bin/sequencial "$ARQUIVO" | awk '/^objetos:/{print $2}')
coleta_seq
seq_min=$(minimo)
seq_med=$(mediana)
echo "Sequencial: objetos=$objetos_seq min=${seq_min}s mediana=${seq_med}s"

for th in $THREADS; do
    objetos_par=$(./bin/paralelo "$ARQUIVO" "$th" | awk '/^objetos:/{print $2}')
    coleta_par "$th"
    par_min=$(minimo)
    par_med=$(mediana)
    speedup=$(awk -v a="$seq_min" -v b="$par_min" \
        'BEGIN{if (b > 0) printf "%.2f", a/b; else print "nan"}')
    if [ "$objetos_par" != "$objetos_seq" ]; then
        echo "AVISO: contagem divergente no paralelo t=$th ($objetos_par != $objetos_seq)"
    fi
    echo "Paralelo t=$th: objetos=$objetos_par min=${par_min}s mediana=${par_med}s speedup(min)=$speedup"
done
