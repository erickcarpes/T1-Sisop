#!/usr/bin/env bash
#
# Valida as versoes sequencial e paralela contra os valores esperados das cinco
# matrizes obrigatorias, testando varias quantidades de threads.
set -u

cd "$(dirname "$0")/.."

# Indice: 1..5 -> objetos esperados.
esperado=(0 3 4 5 6 7)
falhas=0

for i in 1 2 3 4 5; do
    arquivo="tests/exemplo${i}.txt"

    seq=$(./bin/sequencial "$arquivo" | awk '/^objetos:/{print $2}')
    if [ "$seq" != "${esperado[$i]}" ]; then
        echo "FALHA  seq  exemplo$i: esperado ${esperado[$i]}, obtido $seq"
        falhas=$((falhas + 1))
    else
        echo "OK     seq  exemplo$i = $seq"
    fi

    for t in 1 2 3 4 8; do
        par=$(./bin/paralelo "$arquivo" "$t" | awk '/^objetos:/{print $2}')
        if [ "$par" != "${esperado[$i]}" ]; then
            echo "FALHA  par  exemplo$i threads=$t: esperado ${esperado[$i]}, obtido $par"
            falhas=$((falhas + 1))
        else
            echo "OK     par  exemplo$i threads=$t = $par"
        fi
    done
done

if [ "$falhas" -eq 0 ]; then
    echo "TODOS OS TESTES PASSARAM"
    exit 0
fi
echo "$falhas verificacao(oes) falharam"
exit 1
