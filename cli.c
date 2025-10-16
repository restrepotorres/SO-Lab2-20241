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

// Inicializa un comando con memoria para sus argumentos
static void initialize_command(cmd_t *c)
{
  c->size = 0;
  c->capacity = ARR_LENGTH;
  c->args = calloc(c->capacity, sizeof(char *));
}

// Inicializa el arreglo de rutas del PATH
static void initialize_array_path_for_command(path_array_t *arr)
{
  arr->size = 0;
  arr->capacity = ARR_LENGTH;
  arr->entries = calloc(arr->capacity, sizeof(char *));
}

// Inicializa la lista de comandos
static void initialize_command_list(cmd_list_t *cl)
{
  cl->size = 0;
  cl->capacity = ARR_LENGTH;
  cl->commands = calloc(cl->capacity, sizeof(cmd_t *));
}

// Verifica si es necesario redimensionar el arreglo de argumentos de un comando
static void check_length_of_command(cmd_t *c)
{
  if (c->size == c->capacity)
  {
    c->capacity *= 2;
    c->args = realloc(c->args, c->capacity * sizeof(char *));
  }
}

// Verifica si es necesario redimensionar el arreglo de rutas del PATH
static void check_path_capacity(path_array_t *arr)
{
  if (arr->size == arr->capacity)
  {
    arr->capacity *= 2;
    arr->entries = realloc(arr->entries, arr->capacity * sizeof(char *));
  }
}

// Verifica si es necesario redimensionar la lista de comandos
static void ensure_cmd_list_capacity(cmd_list_t *cl)
{
  if (cl->size == cl->capacity)
  {
    cl->capacity *= 2;
    cl->commands = realloc(cl->commands, cl->capacity * sizeof(cmd_t *));
  }
}

// Verifica si un archivo es ejecutable o busca su ruta en el PATH
static int check_file_access_for_executable_file(char *filename, char **out_fp)
{
  if (access(filename, X_OK) == 0)
  {
    strcpy(*out_fp, filename);
    return EXIT_SUCCESS;
  }
  for (int i = 0; i < path_arr.size; i++)
  {
    snprintf(*out_fp, MAX_LENGTH_CM, "%s/%s", path_arr.entries[i], filename);
    if (access(*out_fp, X_OK) == 0)
    {
      return EXIT_SUCCESS;
    }
  }
  return EXIT_FAILURE;
}

// Corre un comando externo en un proceso hijo, con manejo de redirección de salida
static int run_external_command(cmd_t *c)
{
  int ri = -1;
  for (int i = 0; i < c->size; i++)
  {
    if (strcmp(c->args[i], ">") == 0)
    {
      ri = i;
      if (ri == 0 || (c->size - (ri + 1)) != 1) // Validación de redirección
      {
        fprintf(stderr, "An error has occurred\n");
        return EXIT_FAILURE;
      }
      c->args[ri] = NULL; // Marca el final de los argumentos
      break;
    }
  }
  if (check_file_access_for_executable_file(c->args[0], &file_path) == EXIT_SUCCESS)
  {
    pid_t child = fork();
    if (child == -1) // Error al crear el proceso hijo
    {
      fprintf(stderr, "error: %s\n", strerror(errno));
      exit_code = EXIT_FAILURE;
      return EXIT_SUCCESS;
    }
    if (child == 0) // Código del proceso hijo
    {
      if (ri != -1) // Configuración de redirección si aplica
      {
        int fd = open(c->args[ri + 1], O_WRONLY | O_CREAT | O_TRUNC | O_SYNC, 0666);
        if (fd == -1)
        {
          fprintf(stderr, "error: %s\n", strerror(errno));
          exit(EXIT_FAILURE);
        }
        dup2(fd, STDOUT_FILENO); // Redirige la salida estándar
        dup2(fd, STDERR_FILENO); // Redirige errores
        close(fd);
      }
      execv(file_path, c->args); // Ejecuta el comando
      fprintf(stderr, "error: %s\n", strerror(errno));
      exit(EXIT_FAILURE); // Finaliza el hijo si falla execv
    }
  }
  else
  {
    fprintf(stderr, batch_mode ? "An error has occurred\n" : "error:\n\tcommand '%s' not found\n", c->args[0]);
  }
  return EXIT_FAILURE;
}

// Ejecuta un comando según su tipo (interno o externo)
// Aquí es donde se manejan los comandos mencioandos en el enunciado
// Y se ejecutan los comandos externos en caso de no ser uno de estos.
