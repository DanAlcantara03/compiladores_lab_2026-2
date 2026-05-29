#!/usr/bin/env sh
set -eu

echo "==> Limpiando archivos generados anteriores"
make clean
rm -rf build-cmake generated

echo "==> Generando compilador con Make"
make

echo "==> Generando build de CMake"
cmake -S . -B build-cmake
cmake --build build-cmake

echo "==> Generando archivos FIS-25 de examples/*.summ"
mkdir -p generated
for source in examples/*.summ; do
    name="$(basename "$source" .summ)"
    output="generated/$name.fis"
    ./summc -o "$output" < "$source"
    echo "    $output"
done

echo "==> Ejecutando pruebas"
make check

echo "==> Listo"
echo "Generado: ./summc, build/, build-cmake/ y generated/*.fis"
