# Summarize / SummC para FIS-25

Compilador final de laboratorio para el lenguaje **Summarize**, basado en la practica 7 y extendido con generacion de codigo intermedio para la maquina virtual FIS-25.

El proposito de Summarize es que el programador escriba instrucciones cortas, entendibles y rapidas de producir, mientras el compilador expande esas ideas a las instrucciones detalladas de FIS-25. En vez de escribir manualmente etiquetas, saltos, temporales y operaciones de tres direcciones, se escribe codigo resumido como `wh`, `fn`, `ret`, `T`, `F`, `print` e `input`.

El compilador realiza analisis lexico, sintactico y semantico con Flex/Bison, construye un AST decorado con tipos y emite instrucciones FIS-25 listas para cargar en el simulador del manual academico.

## Requisitos

- `gcc`
- `make`
- `flex`
- `bison`
- opcional: `cmake`

## Compilar

```bash
make
```

Tambien se puede usar CMake:

```bash
cmake -S . -B build-cmake
cmake --build build-cmake
```

## Generar todo con Docker

Si no quieres instalar dependencias en tu maquina, usa Docker:

```bash
docker compose run --rm summc
```

Ese comando limpia y regenera:

- `summc`
- `build/`
- `build-cmake/`
- `generated/*.fis` con todos los ejemplos de `examples/*.summ`

Alternativa sin Docker Compose:

```bash
docker build -t summc-builder .
docker run --rm -u "$(id -u):$(id -g)" -v "$PWD:/workspace" summc-builder
```

## Uso

El compilador lee Summarize desde `stdin` y escribe FIS-25 en `stdout`:

```bash
./summc < examples/collatz.summ > collatz.fis
```

Guardar directamente a archivo:

```bash
./summc -o collatz.fis < examples/collatz.summ
```

Opciones:

```text
--check          solo valida lexico/sintaxis/semantica
--ast            imprime el AST en lugar del codigo, salvo que tambien se use -o
--dot ast.dot    exporta el AST en formato DOT
--no-opt         desactiva optimizaciones simples
-o output.fis    guarda el codigo FIS-25 generado
```

## Lenguaje

Summarize es imperativo y procedural, con bloques por indentacion y palabras clave breves. Ejemplo minimo:

```text
int n = input()
int steps = 0

wh n > 1:
    if n % 2 == 0:
        n = n / 2
    else:
        n = 3 * n + 1
    steps = steps + 1

print(steps)
```

Tipos soportados: `int`, `float`, `bool`, `str` y `var` para inferencia local.

Funciones nativas: `print(x)`, `input()`, `pixel(x, y, c)` y `key(k)`.

La especificacion completa del lenguaje esta en [docs/LENGUAJE_SUMMARIZE.md](docs/LENGUAJE_SUMMARIZE.md). La estrategia de traduccion a FIS-25 esta en [docs/FIS25_BACKEND.md](docs/FIS25_BACKEND.md).

## Ejemplos de torneo

La carpeta `examples/` incluye programas para los algoritmos mencionados en la entrega:

- `collatz.summ`
- `nth_prime.summ`
- `pi_monte_carlo.summ`
- `kaprekar.summ`
- `extended_euclid.summ`

Compilar todos:

```bash
for f in examples/*.summ; do ./summc -o "${f%.summ}.fis" < "$f"; done
```

## Pruebas

```bash
make check
```

Las pruebas validan entrada/salida, control de flujo, funciones, graficos nativos y un error semantico.

## Relacion con FIS-25

Se usaron como base el `Manual Académico FIS-25.pdf`, `Pages_to_use.txt` y la captura de especificaciones de la entrega. El backend emite instrucciones documentadas por FIS-25: `VAR`, `ASSIGN`, `ADD`, `SUB`, `MUL`, `DIV`, `MOD`, comparaciones, `LABEL`, `GOTO`, `IFFALSE`, `PARAM`, `PARAM_GET`, `GOSUB`, `RETURN`, `PRINT`, `INPUT`, `PIXEL` y `KEY`.
