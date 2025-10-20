# Práctica 2 de laboratorio - API de Procesos

realizado por:
* Victor Restrepo CC 1017270327
* Guillermo Uribe CC 1037643854

## Objetivos
* Familiarizarse con el entorno de programación de linux y sus llamadas al sistema.
* Aprender cómo los procesos son creados, destruidos y gestionados mediante fork, exec y wait.
* Implementar un intérprete de línea de comandos funcional con comandos integrados, redirección y ejecución paralela.

# NOTA Importante
Todo el código desarrollado se encuentra en el archivo cli.c. Compilar con `gcc -o wish cli.c` . Adicionalmente en este mismo repo se encuentra el programa compilado (wish en la raiz) para linux de 64 bits.

## Descripción del Programa

El programa es un shell básico llamado **wish** que funciona como intérprete de línea de comandos. Tiene dos modos de funcionamiento:

1. **Modo Interactivo**: Muestra el prompt `wish> ` y espera que el usuario digite comandos hasta que escriba `exit`
2. **Modo Batch**: Lee y ejecuta comandos desde un archivo de texto sin mostrar el prompt

El shell busca y ejecuta comandos externos en el PATH, además se le programaron tres comandos integrados especiales: `exit`, `cd` y `path`.

## Características Técnicas Implementadas

### 1. Comandos Integrados (Built-in)
Se programaron 3 comandos que el shell ejecuta directamente:
- **`exit`**: Cierra el shell (no recibe argumentos)
- **`cd <directorio>`**: Cambia el directorio actual usando la syscall `chdir()`
- **`path [dir1] [dir2] ...`**: Configura las rutas donde buscar los ejecutables

### 2. Gestión del PATH
- El PATH inicial solo tiene `/bin`
- Para buscar ejecutables usa `access()` con el flag `X_OK`
- Recorre cada directorio del PATH en orden hasta encontrar el comando

### 3. Redirección de Salida
- Se usa con el formato: `comando [args] > archivo`
- Redirige tanto stdout como stderr al archivo
- Usa las syscalls `open()` y `dup2()` para implementarlo

### 4. Ejecución Paralela
- Se usa el operador `&` para correr varios comandos al tiempo
- Ejemplo: `cmd1 & cmd2 & cmd3`
- Usa `fork()` para crear los procesos hijos y `wait()` para esperar que terminen todos

### 5. Gestión de Memoria Dinámica
- Los arrays empiezan con capacidad de 2 elementos
- Cuando se llenan, se duplica la capacidad usando `realloc()`
- Toda la memoria se libera correctamente al finalizar

## Algoritmo de Funcionamiento

El programa funciona así:

1. **Inicialización:**
   - Valida que no haya más de 1 argumento
   - Define si es modo interactivo o batch
   - Inicializa las estructuras de datos y configura el PATH con /bin

2. **Bucle Principal:**
   - Si es interactivo, muestra el prompt `wish> `
   - Lee la línea con `fgets()`
   - Separa los comandos por `&` y luego los argumentos por espacios
   - Ejecuta cada comando:
     * Si es built-in (exit, cd, path): lo ejecuta directamente
     * Si es externo: hace fork() + busca en PATH + execv()
   - Hace wait() por cada proceso hijo que se creó
   - Verifica si toca salir (porque se digitó exit)

3. **Finalización:**
   - Libera toda la memoria que se reservó
   - Cierra los archivos que estén abiertos

## Estructuras de Datos Utilizadas

### Estructura de Ruta (PATH)
```c
typedef struct {
  char **entradas;      // Los directorios del PATH
  int tamaño;          // Cuántos directorios hay
  int capacidad;        // Capacidad total del array
} arreglo_rutas_t;
```

### Estructura de Comando
```c
typedef struct {
  char **argumentos;    // Los argumentos del comando
  int tamaño;          // Cantidad de argumentos
  int capacidad;        // Capacidad del array
} comando_t;
```

### Estructura de Lista de Comandos
```c
typedef struct {
  comando_t **comandos; // Lista con los comandos paralelos
  int tamaño;          // Cuántos comandos hay
  int capacidad;        // Capacidad del array
} lista_comandos_t;
```

## Funciones Principales Usadas

### Llamadas al Sistema (syscalls)
- **`fork()`**: Crea un proceso hijo para correr comandos externos
- **`execv()`**: Ejecuta el comando en el proceso hijo
- **`wait()`**: Espera a que los procesos hijos terminen
- **`chdir()`**: Cambia de directorio (comando cd)
- **`access()`**: Verifica si un archivo existe y es ejecutable
- **`open()` / `dup2()`**: Para implementar la redirección de salida

### Funciones de Biblioteca
- **`fgets()`**: Lee la entrada del usuario o del archivo
- **`strtok_r()`**: Separa los comandos por `&` y los argumentos por espacios
- **`malloc()` / `realloc()` / `free()`**: Manejo de memoria dinámica
- **`strcmp()` / `strlen()` / `strcpy()`**: Para trabajar con strings
- **`snprintf()`**: Arma las rutas completas de los ejecutables

## Ejemplos de Uso

### Comandos Básicos
```bash
wish> ls -la
wish> cd /tmp
wish> path /bin /usr/bin
```

### Comandos Paralelos
```bash
wish> echo "Tarea 1" & echo "Tarea 2" & echo "Tarea 3"
```

### Redirección
```bash
wish> ls -la > salida.txt
```

### Modo Batch
```bash
$ ./wish comandos.txt
```

## Conclusiones

El proyecto se pudo completar cumpliendo con los objetivos que se plantearon al inicio:

1. **Comprensión de Procesos**: Se aprendió a usar fork, exec y wait que son muy importantes para manejar procesos en Linux
2. **Llamadas al Sistema**: Se ganó experiencia práctica usando las syscalls más comunes del sistema operativo
3. **Arquitectura de Shells**: Se entendió mejor como funcionan los shells de verdad por dentro
4. **Gestión de Memoria**: Se implementaron estructuras dinámicas que crecen según se necesite
5. **Manejo de Errores**: Se validaron bien las entradas y se manejaron los errores que pueden pasar