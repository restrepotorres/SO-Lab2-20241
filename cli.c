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
static int run_cmd(cmd_t *c)
{
  if (c->size == 0 || c->args[0] == NULL)
  {
    return EXIT_FAILURE; // Comando vacío
  }
  if (strcmp(c->args[0], "exit") == 0) // Maneja el comando "exit"
  {
    if (c->size > 1)
    {
      fprintf(stderr, "An error has occurred\n");
      return EXIT_FAILURE;
    }
    if (!batch_mode)
    {
      printf("Goodbye!\n");
    }
    return EXIT_SUCCESS; // Finaliza la shell
  }
  if (strcmp(c->args[0], "cd") == 0) // Maneja el comando "cd"
  {
    if (c->size != 2)
    {
      fprintf(stderr, "An error has occurred\n");
      return EXIT_FAILURE;
    }
    if (chdir(c->args[1]) == -1) // Cambia de directorio
    {
      fprintf(stderr, "error:\n\tcannot execute command 'cd': %s\n", strerror(errno));
      return EXIT_FAILURE;
    }
    return EXIT_FAILURE;
  }
  if (strcmp(c->args[0], "path") == 0) // Maneja el comando "path"
  {
    for (int i = 0; i < path_arr.size; i++)
    {
      free(path_arr.entries[i]); // Limpia rutas actuales
    }
    path_arr.size = 0;
    for (int i = 1; i < c->size; i++)
    {
      check_path_capacity(&path_arr); // Asegura capacidad del PATH
      path_arr.entries[path_arr.size] = malloc((strlen(c->args[i]) + 1) * sizeof(char));
      strcpy(path_arr.entries[path_arr.size++], c->args[i]);
    }
    return EXIT_FAILURE;
  }
  return run_external_command(c); // Corre un comando externo
}

// Inicia el bucle de la shell
static void start_shell(FILE *in)
{
  while (1)
  {
    if (!batch_mode)
    {
      char *cwd = getcwd(NULL, 0);
      if (cwd)
      {
        printf("%s\n", cwd);
        free(cwd);
      }
      printf("wish>");
    }
    if (fgets(input_line, MAX_LENGTH_CM, in) == NULL || feof(in))
    {
      if (batch_mode)
      {
        break;
      }
      fprintf(stderr, "error: %s", strerror(errno));
      exit_code = EXIT_FAILURE;
      break;
    }
    cmd_list.size = 0; // Reinicia la lista de comandos
    char *scp = NULL;
    char *sc_line = strtok_r(input_line, "&", &scp); // Separa comandos por '&'
    while (sc_line != NULL)
    {
      ensure_cmd_list_capacity(&cmd_list);
      cmd_t *single_c = cmd_list.commands[cmd_list.size];
      if (single_c == NULL)
      {
        single_c = malloc(sizeof(cmd_t));
        initialize_command(single_c);
        cmd_list.commands[cmd_list.size] = single_c;
      }
      single_c->size = 0;
      char *tp = NULL;
      char *tok = strtok_r(sc_line, " \n", &tp);
      while (tok != NULL)
      {
        check_length_of_command(single_c);
        single_c->args[single_c->size] = malloc((strlen(tok) + 1) * sizeof(char));
        strcpy(single_c->args[single_c->size++], tok);
        tok = strtok_r(NULL, " \n", &tp);
      }
      check_length_of_command(single_c);
      single_c->args[single_c->size] = NULL;
      sc_line = strtok_r(NULL, "&", &scp);
      cmd_list.size++;
    }
    for (int i = 0; i < cmd_list.size; i++)
    {
      if (run_cmd(cmd_list.commands[i]) == EXIT_SUCCESS)
      {
        goto FREE_MEM;
      }
    }
  }
FREE_MEM:
  return;
}

int main(int argc, char *argv[])
{
  batch_mode = argc > 1;
  FILE *input = stdin;
  if (batch_mode)
  {
    input = fopen(argv[1], "r");
    if (input == NULL)
    {
      fprintf(stderr, "error: %s\n", strerror(errno));
      exit(EXIT_FAILURE);
    }
  }
  initialize_array_path_for_command(&path_arr);
  path_arr.entries[0] = malloc(5 * sizeof(char));
  strcpy(path_arr.entries[0], "/bin");
  path_arr.size = 1;
  initialize_command_list(&cmd_list);
  input_line = malloc(MAX_LENGTH_CM * sizeof(char));
  file_path = malloc(MAX_LENGTH_CM * sizeof(char));
  start_shell(input);
  free(file_path);
  free(input_line);
  for (int i = 0; i < path_arr.size; i++)
  {
    free(path_arr.entries[i]);
  }
  free(path_arr.entries);
  for (int i = 0; i < cmd_list.capacity; i++)
  {
    cmd_t *c = cmd_list.commands[i];
    if (c)
    {
      for (int j = 0; j < c->capacity; j++)
      {
        if (c->args[j])
        {
          free(c->args[j]);
        }
      }
      free(c->args);
      free(c);
    }
  }
  free(cmd_list.commands);
  fclose(input);
  exit(exit_code); // Termina con el código de salida adecuado
}