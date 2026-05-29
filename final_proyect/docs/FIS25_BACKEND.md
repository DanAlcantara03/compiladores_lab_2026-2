# Backend FIS-25

## Fuente de la especificacion

La salida se basa en el `Manual Académico FIS-25.pdf` incluido en el repositorio y en las paginas indicadas por `Pages_to_use.txt`. FIS-25 define una maquina virtual secuencial con variables declaradas por `VAR`, operaciones de tres direcciones, saltos con etiquetas, consola, entrada y una pantalla de 64x64 pixeles.

## Pipeline del compilador

1. `lexer.l` tokeniza el lenguaje Summarize e inserta `INDENT`/`DEDENT`.
2. `parser.y` valida la gramatica con Bison.
3. `symtab.c` valida alcances, tipos, llamadas y retornos.
4. `ast.c` construye el AST decorado con tipos.
5. `codegen.c` recorre el AST y emite codigo FIS-25.

Ese pipeline es lo que permite que una instruccion resumida como `wh n != 1:` se convierta en varias instrucciones FIS-25: etiqueta de inicio, comparacion, salto condicional, cuerpo del ciclo y salto de regreso.

## Mapeo de instrucciones

| Summarize | FIS-25 |
| --- | --- |
| Declaracion | `VAR <nombre>` |
| Asignacion | `ASSIGN <valor> <dest>` |
| `a + b` | `ADD a b dest` |
| `a - b` | `SUB a b dest` |
| `a * b` | `MUL a b dest` |
| `a / b` | `DIV a b dest` |
| `a % b` | `MOD a b dest` |
| Comparaciones | `EQ`, `NEQ`, `GT`, `GTE`, `LT`, `LTE` |
| `if` / `wh` | `IFFALSE <cond> GOTO <label>` |
| Saltos | `LABEL`, `GOTO` |
| Funcion | `PARAM`, `PARAM_GET`, `GOSUB`, `RETURN` |
| `print(x)` | `PRINT x` |
| `input()` | `INPUT dest` |
| `pixel(x,y,c)` | `PIXEL x y c` |
| `key(k)` | `KEY k dest` |

## Estrategia de memoria

FIS-25 usa variables globales. El backend declara al inicio:

- variables del programa fuente;
- parametros de funciones;
- temporales internos `__tmp_N`;
- variables de retorno `__ret_nombreFuncion`.

Los prefijos `__tmp_`, `__ret_`, `__fn_` y `__L` quedan reservados para el compilador.

## Control de flujo

Un `while`:

```text
wh x > 0:
    x = x - 1
```

se emite con esta forma:

```text
LABEL __while_0
GT x 0 __tmp_0
IFFALSE __tmp_0 GOTO __endwhile_1
SUB x 1 __tmp_1
ASSIGN __tmp_1 x
GOTO __while_0
LABEL __endwhile_1
```

Un `for i in a..b` se emite como ciclo con limite final exclusivo:

```text
ASSIGN a i
LABEL __for_0
LT i b __tmp_0
IFFALSE __tmp_0 GOTO __endfor_1
...
ADD i 1 i
GOTO __for_0
LABEL __endfor_1
```

## Funciones

Cada funcion se traduce a una etiqueta:

```text
LABEL __fn_sum
PARAM_GET a
PARAM_GET b
ADD a b __tmp_0
ASSIGN __tmp_0 __ret_sum
RETURN
```

Una llamada emite argumentos con `PARAM`, salta con `GOSUB` y copia la variable de retorno:

```text
PARAM 2
PARAM 3
GOSUB __fn_sum
ASSIGN __ret_sum __tmp_1
```

## Optimizaciones

El backend aplica optimizaciones conservadoras:

- plegado de constantes en aritmetica, comparaciones y logica;
- asignacion directa de literales sin temporales;
- traduccion de `&`, `|` y `!` a operaciones FIS-25 simples;
- declaracion unica de variables y temporales.

Se puede comparar contra la version sin optimizar:

```bash
./summc --no-opt < examples/collatz.summ > collatz_no_opt.fis
./summc < examples/collatz.summ > collatz_opt.fis
```

## Limitaciones conocidas

- FIS-25 documenta `pointer`, pero Summarize no implementa arreglos ni memoria dinamica.
- La salida FIS-25 usa un espacio de variables plano; por claridad y seguridad, los programas de torneo deben evitar shadowing de nombres.
- `input()` devuelve `int`.
- No hay generador aleatorio nativo; el ejemplo de Pi usa un LCG determinista.
