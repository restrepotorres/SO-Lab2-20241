# Práctica 
# Práctica 2 de laboratorio - API de Procesos #

realizado por:
* Victor Restrepo CC 1017270327
* Guillermo Uribe CC 1037643854

> ## Objetivos
> * Aprender a codificar programas usando el lenguaje C a nivel básico e intermedio.
> * Aprender a usar las herramientas básicas para desarrollar aplicaciones en un ambiente de desarrollo linux.

# NOTA Importante
Todo el codigo desarrollado  se encuentra en la ruta ./Laboratorio/Lab1.c , el archivo compilado para linux de 64 bits es ./Laboratorio1/reverse


## Descripción del Programa

El programa desarrollado es un inversor de líneas de archivo que lee un archivo de texto y muestra su contenido con las líneas en orden inverso (de la última a la primera). El programa implementa tres modos de funcionamiento diferentes según los argumentos proporcionados.


1. Si no se pasan argumentos: 
   - Lee texto desde la entrada estándar
   - El usuario ingresa líneas de texto manualmente
   - El usuario termina el ingreso de texto con Ctrl+D
   - Muestra el resultado en pantalla

2. Si se pasa 1 argumento
   - Se lee el argumento como la ruta del texto de entrada
   - Muestra el contenido invertido en la consola

3. Si se pasan 2 argumentos
   - Lee desde un archivo de entrada (argumento 1)
   - Guarda el resultado en un archivo de salida (argumento 2)

## Características Técnicas Implementadas

### Gestión de memoria dinamica
El programa utiliza memoria dinámica para adaptarse a archivos de cualquier tamaño:

- **Array dinámico de punteros**: Almacena referencias a cada línea
- **Crecimiento de memoria reservada**: La capacidad se multiplica por 2 cuando es necesario
- **Asignación exacta**: Cada línea ocupa solo la memoria necesaria

### Algoritmo de Funcionamiento

1. **Inicialización**
   - Se crea un array dinámico con capacidad inicial de 10 líneas
   - Se determina la fuente de entrada según los argumentos

2. **Lectura de Líneas**
   - Se lee línea por línea usando la funcion `fgets()`
   - Si se agota la capacidad, se redimensiona el array usando `realloc()`
   - Cada línea se almacena en memoria dinámica individual
   - Se normaliza el formato agregando `\n` si la última línea no lo tiene

3. **Salida Invertida**
   - Se recorre el array desde el último índice hasta el primero
   - Se escribe a la salida correspondiente (pantalla o archivo)


### Manejo de Errores
El programa incluye validación de los siguientes errores:

- cantidad de argumentos a la entrada
- Validación de apertura de archivos
- Verificación de asignación de memoria
- Mensajes informativos para el usuario

## Estructuras de Datos Utilizadas

### Array Dinámico de Punteros
```c
char **lines = NULL;  // Array principal
int line_count = 0;   // Contador de líneas
int capacity = 10;    // Capacidad actual
```

### Buffer de Lectura
```c
char buffer[1024];    // Buffer temporal para leer los archivos
```

## Funciones usadas

### Gestión de Memoria
- `malloc()`: Asignación inicial de memoria
- `realloc()`: Redimensionamiento dinámico del array
- `free()`: Liberación de memoria

### Entrada/Salida
- `fgets()`: Lectura de líneas desde archivo o stdin
- `fprintf()`: Escritura a archivo o stdout
- `fopen()/fclose()`: Manejo de archivos

### Manipulación de Strings
- `strlen()`: Cálculo de longitud de cadenas
- `strcpy()`: Copia de cadenas


## Conclusiones

El programa desarrollado cumple exitosamente con los objetivos planteados para la practica, demostrando el uso de conceptos fundamentales de programación en C como:

- Gestión dinámica de memoria
- Manejo de archivos y entrada/salida
- Validación de errores
- Manipulación de argumentos de línea de comandos
- Estructuras de datos dinámicas
