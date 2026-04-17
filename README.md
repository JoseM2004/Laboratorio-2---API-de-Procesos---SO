#  Wish Shell — Wisconsin Shell

> Laboratorio de Sistemas Operativos · Práctica No. 2 — API de Procesos  
> Facultad de Ingeniería · Ingeniería de Sistemas · Universidad de Antioquia

---

## a) Integrantes

| Nombre completo | Correo institucional | N° Documento |
|---|---|---|
| Miguel Angel Agudelo Vera | miguel.agudelo2@udea.edu.co | CC 1039678287 |
| Jose Miguel Monsalve Marín | jmiguel.monsalve@udea.edu.co | CC 1038062019 |

---

## b) Documentación de funciones

### `soloEspacios(char *buffer)`

**Descripción:** Determina si una cadena de texto contiene únicamente caracteres de espacio en blanco (espacios, tabulaciones, saltos de línea) o si tiene al menos un carácter con contenido.

**Parámetros:**
- `buffer` — puntero a la cadena de caracteres a evaluar.

**Retorna:** `1` si la cadena tiene al menos un carácter no-espacio (hay contenido); `0` si la cadena está vacía o es solo espacios en blanco.

**Uso interno:** Se llama en el bucle principal antes de procesar cada línea leída, para ignorar líneas en blanco o que solo contengan espacios/tabs.

---

### `imprimirError()`

**Descripción:** Escribe el mensaje de error estándar `"An error has occurred\n"` en la salida de error estándar (`stderr`) mediante la llamada al sistema `write()`, y termina el proceso con `exit(1)`.

**Parámetros:** Ninguno.

**Retorna:** No retorna (termina el proceso).

**Uso interno:** Se invoca en errores fatales, como cuando `fork()` falla o cuando el ejecutable hallado no puede ser lanzado por `execv()`.

---

### `imprimirPrompt()`

**Descripción:** Escribe la cadena `"wish> "` en la salida estándar (`stdout`) mediante la llamada al sistema `write()`, para mostrar el prompt del shell al usuario.

**Parámetros:** Ninguno.

**Retorna:** `void`.

**Uso interno:** Se llama al inicio del modo interactivo y al final de cada iteración del bucle principal cuando el shell corre en modo interactivo (`argc == 1`).

---

### `nuevoProceso(char *argumentos[])`

**Descripción:** Crea un proceso hijo mediante `fork()` y ejecuta en él el binario indicado por `argumentos[0]` usando `execv()`. El padre retorna inmediatamente el PID del hijo, sin esperar a que este termine (el `waitpid()` se realiza en el llamador para permitir paralelismo).

**Parámetros:**
- `argumentos[]` — arreglo de cadenas terminado en `NULL` que representa el comando y sus argumentos; `argumentos[0]` es el nombre del ejecutable.

**Retorna:** El PID del proceso hijo (entero positivo). En caso de error fatal de `fork()`, termina el proceso con `exit(1)`.

**Lógica de búsqueda del ejecutable (en el hijo):**

| Caso | Condición | Comportamiento |
|---|---|---|
| Path por defecto | `rutaCambiada == 0` | Busca primero en `/bin/`, luego en `/usr/bin/` |
| Un solo directorio custom | `rutaCambiada == 1` y `cantidadMultiRuta == 0` | Concatena `ruta` con `argumentos[0]` |
| Múltiples directorios | `cantidadMultiRuta > 0` | Recorre `multiRuta[]` hasta encontrar el ejecutable |

Si `rutaVacia == 1`, el hijo termina inmediatamente sin ejecutar nada.

---

### `preProcesar(char *buffer)`

**Descripción:** Función central de procesamiento de cada línea de comando. Realiza en orden: (A) detección y configuración de redirección de salida, (B) parseo del comando y despacho según su tipo (built-in o externo), y (C) restauración de `stdout`/`stderr` si fueron redirigidos.

**Parámetros:**
- `buffer` — cadena con el comando completo leído desde stdin o el archivo batch. La función modifica este buffer internamente con `strtok`.

**Retorna:** El PID del proceso hijo lanzado, o `0` si el comando era built-in (no genera proceso hijo).

**Bloques internos:**

**Bloque A — Redirección:**
Detecta si el buffer contiene `>`. Valida que haya exactamente un archivo destino y que el comando no sea solo `>` sin archivo. Abre el archivo con `O_WRONLY | O_CREAT | O_TRUNC` y redirige tanto `stdout` como `stderr` al archivo usando `dup2()`. Guarda una copia de `stdout` con `dup()` para restaurarla en el Bloque C.

**Bloque B — Parseo y despacho:**
Tokeniza el buffer con `strtok` usando como delimitadores espacios, tabs y saltos de línea. Identifica el comando (`comando[0]`) y lo clasifica:

- **`chd`** (built-in): Llama a `chdir()` con el único argumento esperado. Reporta error si hay 0 o más de 1 argumento.
- **`route`** (built-in): Actualiza la variable global `ruta` o el arreglo `multiRuta[]` según la cantidad de argumentos. Sin argumentos activa `rutaVacia = 1`.
- **`exit`** (built-in): Invoca `exit(0)` si no recibe argumentos; reporta error en caso contrario.
- **Comando externo**: Llama a `nuevoProceso()` con el arreglo de tokens como argumentos.

**Bloque C — Restauración:**
Si `REDIRIGIDO == 1`, restaura el descriptor original de `stdout` usando `dup2(copiaStdout, 1)` y cierra la copia.

---

### `main(int argc, char *argv[])`

**Descripción:** Punto de entrada del shell. Determina el modo de ejecución (interactivo o batch), abre la fuente de entrada correspondiente, y ejecuta el bucle principal de lectura-procesamiento de comandos.

**Parámetros:**
- `argc` — número de argumentos de línea de comandos.
- `argv[]` — arreglo de argumentos; `argv[1]` es el nombre del archivo batch (si aplica).

**Flujo principal:**

1. Si `argc == 1`: modo interactivo, fuente = `stdin`, imprime el prompt inicial.
2. Si `argc == 2`: modo batch, abre el archivo `argv[1]` con `fopen()`. Si falla, termina con error.
3. Si `argc > 2`: error fatal, termina con `exit(1)`.
4. Bucle `while(fgets(...))`:
   - Reinicia la bandera `REDIRIGIDO = 0`.
   - Ignora líneas vacías con `soloEspacios()`.
   - Si la línea contiene `&`: separa los sub-comandos, los lanza todos con `preProcesar()` (fase 1), luego espera a todos con `waitpid()` en orden (fase 2).
   - Si no contiene `&`: llama a `preProcesar()` y hace `waitpid()` del hijo resultante.
   - En modo interactivo, imprime el prompt al final de cada iteración.
5. Al alcanzar EOF: termina limpiamente con `exit(0)`.

---

## c) Problemas presentados y soluciones



## d) Pruebas de funcionalidad

> A continuación se listan los diferentes pruebas de sistema. 
### Compilación

```bash
gcc Wish.c -o wish
```

---

### Prueba 1 — Modo interactivo básico

```bash
./wish
```
 Aparece el prompt `wish> `. El shell queda esperando comandos. Salir con `exit`.
 
 <img width="689" height="73" alt="image" src="https://github.com/user-attachments/assets/2872adfa-1372-4700-888b-05826cd58276" />


---

### Prueba 2 — Ejecución de comando externo simple

```bash
./wish
# Dentro del shell:
ls
```
 El shell lista el contenido del directorio actual, igual que ejecutar `ls` directamente.

 <img width="717" height="118" alt="image" src="https://github.com/user-attachments/assets/c38fe7bc-c43d-4c52-b526-0c7c4007f32e" />

---

### Prueba 3 — Comando externo con argumentos

```bash
./wish
# Dentro del shell:
ls -la /tmp
```
 Lista detallada del directorio `/tmp` con permisos, tamaños y fechas.

 <img width="1464" height="525" alt="image" src="https://github.com/user-attachments/assets/84b5ac13-db95-4879-8174-69836bdac9a3" />

---

### Prueba 4 — Built-in `chd` (cambio de directorio)

```bash
./wish
# Dentro del shell:
chd /tmp
ls
```
 El directorio de trabajo cambia a `/tmp` y `ls` muestra su contenido.

 <img width="991" height="312" alt="image" src="https://github.com/user-attachments/assets/1379c30a-9f42-4235-bd4d-906af58cf2e3" />

---

### Prueba 5 — Built-in `chd` con error (directorio inexistente)

```bash
./wish
# Dentro del shell:
chd /directorio_que_no_existe
```
 Se imprime `An error has occurred` en stderr. El shell continúa.

 <img width="699" height="127" alt="image" src="https://github.com/user-attachments/assets/c6313146-a770-4b52-93ae-e13670cae08b" />

---

### Prueba 6 — Built-in `route` con un solo directorio

```bash
./wish
# Dentro del shell:
route /usr/bin
ls
```
 El comando `ls` sigue funcionando porque se buscó en `/usr/bin/ls`.

 <img width="686" height="132" alt="image" src="https://github.com/user-attachments/assets/c9014048-011e-47a8-9b17-fe8bdfc07a5a" />

---

### Prueba 7 — Built-in `route` con múltiples directorios

```bash
./wish
# Dentro del shell:
route /bin /usr/bin
ls -la
echo hola
```
 Ambos comandos funcionan correctamente usando las rutas especificadas.

 <img width="691" height="261" alt="image" src="https://github.com/user-attachments/assets/fbb4dd4f-3e94-472a-99c4-2d1c0008ccaf" />

---

### Prueba 8 — Built-in `route` vacío (path vacío)

```bash
./wish
# Dentro del shell:
route
ls
```
 El shell imprime `An error has occurred` porque no hay ningún directorio de búsqueda. Los built-ins (`exit`, `chd`, `route`) siguen funcionando.

 <img width="693" height="146" alt="image" src="https://github.com/user-attachments/assets/0b7b144d-ba90-4475-9e79-f5cae65ceeea" />

---

### Prueba 9 — Built-in `exit` sin argumentos

```bash
./wish
# Dentro del shell:
exit
```
 El shell termina limpiamente y regresa al prompt del sistema operativo.

 <img width="694" height="90" alt="image" src="https://github.com/user-attachments/assets/665c7e32-e084-44a1-90ff-a775a8089c99" />

---

### Prueba 10 — Built-in `exit` con argumentos (error)

```bash
./wish
# Dentro del shell:
exit argumento_de_mas
```
 Se imprime `An error has occurred`. El shell **no** termina y sigue esperando comandos.

 <img width="686" height="126" alt="image" src="https://github.com/user-attachments/assets/56b8f8bb-33ab-46cb-ad4b-c4b865f3f91f" />

---

### Prueba 11 — Redirección de salida estándar

```bash
./wish
# Dentro del shell:
ls -la /tmp > salida.txt
# Luego verificar fuera del shell:
cat salida.txt
```
 El archivo `salida.txt` contiene el listado de `/tmp`. No aparece nada en pantalla durante la ejecución del comando.

 <img width="1462" height="504" alt="image" src="https://github.com/user-attachments/assets/aabd9621-232b-431c-a377-7639d4930383" />

---

### Prueba 12 — Redirección sobrescribe archivo existente

```bash
./wish
# Dentro del shell:
echo hola > archivo.txt
ls > archivo.txt
# Verificar:
cat archivo.txt
```
 `archivo.txt` contiene únicamente la salida de `ls`, no la palabra "hola". El archivo fue truncado.

 <img width="699" height="244" alt="image" src="https://github.com/user-attachments/assets/42a27bd4-6de1-43d4-a48b-b07a60f44c98" />

---

### Prueba 13 — Redirección con error (sin archivo destino)

```bash
./wish
# Dentro del shell:
ls >
```
 Se imprime `An error has occurred`. El shell continúa.

 <img width="695" height="108" alt="image" src="https://github.com/user-attachments/assets/5d5af4cb-c84f-40ed-a84a-3e1ffce8fd99" />

---

### Prueba 14 — Comandos paralelos con `&`

```bash
./wish
# Dentro del shell:
ls /tmp & ls /var & ls /etc
```
 Los tres listados aparecen (posiblemente intercalados) y el shell devuelve el control solo cuando los tres terminan. El orden de salida puede variar entre ejecuciones.

 <img width="1391" height="919" alt="image" src="https://github.com/user-attachments/assets/5c64624b-ae57-4213-9abe-fe702aab8e63" />

---

### Prueba 15 — Modo batch (archivo de comandos)

```bash
# Crear el archivo batch:
echo -e "ls /tmp\nls /var\nexit" > batch.txt

# Ejecutar en modo batch:
./wish batch.txt
```
 El shell ejecuta `ls /tmp` y `ls /var` sin mostrar prompt, luego termina. No se imprime `wish>` en ningún momento.

 <img width="1142" height="280" alt="image" src="https://github.com/user-attachments/assets/2ba51ab7-7eee-4650-a3ed-25e247229789" />

---

### Prueba 16 — Modo batch con archivo inexistente

```bash
./wish archivo_que_no_existe.txt
```
 Se imprime `An error has occurred` y el proceso termina con código de salida `1`.

 <img width="1002" height="127" alt="image" src="https://github.com/user-attachments/assets/3bd0db12-0507-48db-aa57-e045713234f0" />

---

### Prueba 17 — Más de un argumento al invocar el shell (error)

```bash
./wish archivo1.txt archivo2.txt
```
 Se imprime `An error has occurred` y el proceso termina inmediatamente.

 <img width="1000" height="84" alt="image" src="https://github.com/user-attachments/assets/73108671-a8f0-4149-8c14-4748fb6708f7" />

---

### Prueba 18 — Comandos paralelos con redirección individual

```bash
./wish
# Dentro del shell:
ls /tmp > salida1.txt & ls /var > salida2.txt
# Verificar:
cat salida1.txt
cat salida2.txt
```
 Cada archivo contiene la salida del comando correspondiente. Ambas redirecciones funcionaron en paralelo.
 
 <img width="992" height="598" alt="image" src="https://github.com/user-attachments/assets/75c8c3dc-5d50-42e4-9af7-069ba9f948b7" />

---

### Prueba 19 — Líneas con solo espacios (ignoradas)

```bash
./wish
# Dentro del shell (presionar Enter varias veces o escribir espacios):
   
	
ls
```
 El shell ignora las líneas en blanco o con solo espacios/tabs, y ejecuta `ls` normalmente.

 <img width="908" height="174" alt="image" src="https://github.com/user-attachments/assets/688795cc-d155-4a03-9342-f7e682f042c2" />

---

### Prueba 20 — Comando inexistente

```bash
./wish
# Dentro del shell:
comandoquenoexiste
```
 Se imprime `An error has occurred`. El shell **no** termina y sigue en ejecución.

 <img width="691" height="105" alt="image" src="https://github.com/user-attachments/assets/bf1ee6a9-d337-4635-9b4a-4cbebb06fb0b" />

---

## e) Video de sustentación

> 🎥 [Enlace al video de 10 minutos — YouTube / Drive]  
> *(Añadir el enlace al video donde se sustenta el desarrollo de la práctica)*

---

## f) Manifiesto de transparencia — Uso de IA generativa

Durante el desarrollo de esta práctica se hizo uso de herramientas de IA generativa (Claude de Anthropic) como recurso de apoyo en distintas etapas del proceso. Su utilización estuvo orientada principalmente a fortalecer la comprensión, mejorar la calidad de la documentación y asistir en la identificación de errores, sin sustituir el trabajo de desarrollo realizado por el equipo.

En particular, la IA fue empleada en los siguientes aspectos:

- **Redacción del README:** Se utilizó como apoyo para estructurar y mejorar la claridad del informe, partiendo del análisis del código fuente desarrollado y de los criterios establecidos en el enunciado de la práctica.  
- **Revisión de bugs:** Se consultó la IA como herramienta de orientación para identificar y comprender errores.  
- **Documentación de funciones:** Las descripciones de las funciones fueron elaboradas con asistencia de IA, tomando como base el código fuente y los comentarios previamente desarrollados en el archivo *Wish.c*.

Es importante resaltar que, aunque se contó con el apoyo de estas herramientas, se procuró en todo momento el entendimiento completo de la lógica, el funcionamiento y la implementación de cada componente del programa. La IA fue utilizada como un medio de acompañamiento para el aprendizaje y la mejora del resultado final, mas no como un sustituto del proceso de desarrollo ni de la autoría del trabajo realizado.

---

## Estructura del repositorio

```
/
├── src/
│   └── Wish.c          # Código fuente del shell
├── README.md           # Este informe
└── batch.txt           # Archivo de ejemplo para modo batch (opcional)
```

---

## Compilación y ejecución rápida

```bash
# Compilar
gcc Wish.c -o wish Wish.c

# Modo interactivo
./wish

# Modo batch
./wish batch.txt
```
