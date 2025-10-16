#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <limits.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <fcntl.h>

// Definición de constantes para tamaño inicial de arreglos y comandos
#define ARR_LENGTH 2
#define MAX_LENGTH_CM 1024

// Estructura para almacenar las rutas del PATH
typedef struct
{
  char **entries;
  int size; // Esto es para almacenar la antidad de rutas actuales
  int capacity;
} path_array_t;

// Estructura para almacenar un comando y sus argumentos
typedef struct
{
  char **args;
  int size; // Número actual de argumentos
  int capacity;
} cmd_t;

// Estructura para manejar una lista de comandos
typedef struct
{
  cmd_t **commands;
  int size;
  int capacity;
} cmd_list_t;

// Variables globales para la configuración de la shell
static path_array_t path_arr;
static cmd_list_t cmd_list;
static char *input_line;
static char *file_path;
static int exit_code;
static int batch_mode;

