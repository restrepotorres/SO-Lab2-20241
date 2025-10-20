#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <limits.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <fcntl.h>

#define LONGITUD_INICIAL_ARREGLO 2
#define LONGITUD_MAXIMA_COMANDO 1024

typedef struct
{
  char **entradas;
  int tamanio; 
  int capacidad;
} arreglo_rutas_t;

typedef struct
{
  char **argumentos;
  int tamanio; 
  int capacidad;
} comando_t;

typedef struct
{
  comando_t **comandos;
  int tamanio;
  int capacidad;
} lista_comandos_t;

static arreglo_rutas_t arreglo_rutas;
static lista_comandos_t lista_comandos;
static char *linea_entrada;
static char *ruta_archivo;
static int codigo_salida;
static int modo_batch;
static const char mensaje_error[30] = "An error has occurred\n";

static void imprimir_error(void)
{
  write(STDERR_FILENO, mensaje_error, strlen(mensaje_error));
}

static void inicializar_comando(comando_t *c)
{
  c->tamanio = 0;
  c->capacidad = LONGITUD_INICIAL_ARREGLO;
  c->argumentos = calloc(c->capacidad, sizeof(char *));
}

static void inicializar_arreglo_rutas(arreglo_rutas_t *arr)
{
  arr->tamanio = 0;
  arr->capacidad = LONGITUD_INICIAL_ARREGLO;
  arr->entradas = calloc(arr->capacidad, sizeof(char *));
}

static void inicializar_lista_comandos(lista_comandos_t *cl)
{
  cl->tamanio = 0;
  cl->capacidad = LONGITUD_INICIAL_ARREGLO;
  cl->comandos = calloc(cl->capacidad, sizeof(comando_t *));
}

static void verificar_longitud_comando(comando_t *c)
{
  if (c->tamanio == c->capacidad)
  {
    c->capacidad *= 2;
    c->argumentos = realloc(c->argumentos, c->capacidad * sizeof(char *));
  }
}

static void verificar_capacidad_rutas(arreglo_rutas_t *arr)
{
  if (arr->tamanio == arr->capacidad)
  {
    arr->capacidad *= 2;
    arr->entradas = realloc(arr->entradas, arr->capacidad * sizeof(char *));
  }
}

static void asegurar_capacidad_lista_comandos(lista_comandos_t *cl)
{
  if (cl->tamanio == cl->capacidad)
  {
    cl->capacidad *= 2;
    cl->comandos = realloc(cl->comandos, cl->capacidad * sizeof(comando_t *));
  }
}

static int verificar_acceso_archivo_ejecutable(char *nombre_archivo, char **ruta_salida)
{
  if (access(nombre_archivo, X_OK) == 0)
  {
    strcpy(*ruta_salida, nombre_archivo);
    return EXIT_SUCCESS;
  }
  for (int i = 0; i < arreglo_rutas.tamanio; i++)
  {
    snprintf(*ruta_salida, LONGITUD_MAXIMA_COMANDO, "%s/%s", arreglo_rutas.entradas[i], nombre_archivo);
    if (access(*ruta_salida, X_OK) == 0)
    {
      return EXIT_SUCCESS;
    }
  }
  return EXIT_FAILURE;
}

static int ejecutar_comando_externo(comando_t *c)
{
  int indice_redireccion = -1;
  for (int i = 0; i < c->tamanio; i++)
  {
    if (strcmp(c->argumentos[i], ">") == 0)
    {
      indice_redireccion = i;
      if (indice_redireccion == 0 || (c->tamanio - (indice_redireccion + 1)) != 1) 
      {
        imprimir_error();
        return EXIT_FAILURE;
      }
      c->argumentos[indice_redireccion] = NULL; 
      break;
    }
  }
  if (verificar_acceso_archivo_ejecutable(c->argumentos[0], &ruta_archivo) == EXIT_SUCCESS)
  {
    pid_t hijo = fork();
    if (hijo == -1) 
    {
      imprimir_error();
      codigo_salida = EXIT_FAILURE;
      return EXIT_FAILURE;
    }
    if (hijo == 0) 
    {
      if (indice_redireccion != -1) 
      {
        int fd = open(c->argumentos[indice_redireccion + 1], O_WRONLY | O_CREAT | O_TRUNC, 0666);
        if (fd == -1)
        {
          imprimir_error();
          exit(EXIT_FAILURE);
        }
        dup2(fd, STDOUT_FILENO);
        dup2(fd, STDERR_FILENO);
        close(fd);
      }
      execv(ruta_archivo, c->argumentos); 
      imprimir_error();
      exit(EXIT_FAILURE); 
    }
    return 1; // Retorna 1 para indicar que se creó un proceso hijo
  }
  else
  {
    imprimir_error();
  }
  return EXIT_FAILURE;
}

static int ejecutar_comando(comando_t *c)
{
  if (c->tamanio == 0 || c->argumentos[0] == NULL)
  {
    return EXIT_FAILURE; 
  }
  if (strcmp(c->argumentos[0], "exit") == 0)
  {
    if (c->tamanio > 1)
    {
      imprimir_error();
      return EXIT_FAILURE;
    }
    return EXIT_SUCCESS; 
  }
  if (strcmp(c->argumentos[0], "cd") == 0) 
  {
    if (c->tamanio != 2)
    {
      imprimir_error();
      return EXIT_FAILURE;
    }
    if (chdir(c->argumentos[1]) == -1) 
    {
      imprimir_error();
      return EXIT_FAILURE;
    }
    return EXIT_FAILURE;
  }
  if (strcmp(c->argumentos[0], "path") == 0) 
  {
    for (int i = 0; i < arreglo_rutas.tamanio; i++)
    {
      free(arreglo_rutas.entradas[i]); 
    }
    arreglo_rutas.tamanio = 0;
    for (int i = 1; i < c->tamanio; i++)
    {
      verificar_capacidad_rutas(&arreglo_rutas); 
      arreglo_rutas.entradas[arreglo_rutas.tamanio] = malloc((strlen(c->argumentos[i]) + 1) * sizeof(char));
      strcpy(arreglo_rutas.entradas[arreglo_rutas.tamanio++], c->argumentos[i]);
    }
    return EXIT_FAILURE;
  }
  return ejecutar_comando_externo(c); 
}

static void iniciar_shell(FILE *entrada)
{
  while (1)
  {
    if (!modo_batch)
    {
      printf("wish> ");
      fflush(stdout);
    }
    if (fgets(linea_entrada, LONGITUD_MAXIMA_COMANDO, entrada) == NULL || feof(entrada))
    {
      if (modo_batch)
      {
        break;
      }
      imprimir_error();
      codigo_salida = EXIT_FAILURE;
      break;
    }
    lista_comandos.tamanio = 0; 
    char *puntero_contexto_separador = NULL;
    char *linea_comando_simple = strtok_r(linea_entrada, "&", &puntero_contexto_separador);
    while (linea_comando_simple != NULL)
    {
      asegurar_capacidad_lista_comandos(&lista_comandos);
      comando_t *comando_individual = lista_comandos.comandos[lista_comandos.tamanio];
      if (comando_individual == NULL)
      {
        comando_individual = malloc(sizeof(comando_t));
        inicializar_comando(comando_individual);
        lista_comandos.comandos[lista_comandos.tamanio] = comando_individual;
      }
      comando_individual->tamanio = 0;
      char *puntero_token = NULL;
      char *token = strtok_r(linea_comando_simple, " \t\n", &puntero_token);
      while (token != NULL)
      {
        verificar_longitud_comando(comando_individual);
        comando_individual->argumentos[comando_individual->tamanio] = malloc((strlen(token) + 1) * sizeof(char));
        strcpy(comando_individual->argumentos[comando_individual->tamanio++], token);
        token = strtok_r(NULL, " \t\n", &puntero_token);
      }
      verificar_longitud_comando(comando_individual);
      comando_individual->argumentos[comando_individual->tamanio] = NULL;
      linea_comando_simple = strtok_r(NULL, "&", &puntero_contexto_separador);
      lista_comandos.tamanio++;
    }
    
    int contador_hijos = 0;
    int debe_salir = 0;
    
    for (int i = 0; i < lista_comandos.tamanio; i++)
    {
      int resultado = ejecutar_comando(lista_comandos.comandos[i]);
      if (resultado == EXIT_SUCCESS) // Es exit
      {
        debe_salir = 1;
        break;
      }
      else if (resultado == 1) // Se creó un proceso hijo
      {
        contador_hijos++;
      }
    }
    
    // Esperar solo por los procesos hijos creados
    for (int i = 0; i < contador_hijos; i++)
    {
      wait(NULL);
    }
    
    if (debe_salir)
    {
      break;
    }
  }
}

int main(int argc, char *argv[])
{
  // Validar número de argumentos
  if (argc > 2)
  {
    imprimir_error();
    exit(1);
  }
  
  modo_batch = argc == 2;
  FILE *entrada = stdin;
  
  if (modo_batch)
  {
    entrada = fopen(argv[1], "r");
    if (entrada == NULL)
    {
      imprimir_error();
      exit(1);
    }
  }
  
  inicializar_arreglo_rutas(&arreglo_rutas);
  
  // Inicializar path con /bin
  arreglo_rutas.entradas[0] = malloc(5 * sizeof(char));
  strcpy(arreglo_rutas.entradas[0], "/bin");
  arreglo_rutas.tamanio = 1;
  
  inicializar_lista_comandos(&lista_comandos);
  linea_entrada = malloc(LONGITUD_MAXIMA_COMANDO * sizeof(char));
  ruta_archivo = malloc(LONGITUD_MAXIMA_COMANDO * sizeof(char));
  
  iniciar_shell(entrada);
  
  // Liberar memoria
  free(ruta_archivo);
  free(linea_entrada);
  for (int i = 0; i < arreglo_rutas.tamanio; i++)
  {
    free(arreglo_rutas.entradas[i]);
  }
  free(arreglo_rutas.entradas);
  for (int i = 0; i < lista_comandos.capacidad; i++)
  {
    comando_t *c = lista_comandos.comandos[i];
    if (c)
    {
      for (int j = 0; j < c->capacidad; j++)
      {
        if (c->argumentos[j])
        {
          free(c->argumentos[j]);
        }
      }
      free(c->argumentos);
      free(c);
    }
  }
  free(lista_comandos.comandos);
  
  if (entrada != stdin)
  {
    fclose(entrada);
  }
  
  exit(codigo_salida); 
}