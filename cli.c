#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <limits.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <fcntl.h>

// Constantes para el tamaño inicial de los arreglos dinámicos y longitud máxima de comandos
#define LONGITUD_INICIAL_ARREGLO 2
#define LONGITUD_MAXIMA_COMANDO 1024

// Estructura para almacenar el PATH (directorios donde buscar ejecutables)
typedef struct
{
  char **entradas;      // Arreglo de strings con las rutas
  int tamanio;          // Cantidad actual de rutas
  int capacidad;        // Capacidad total del arreglo
} arreglo_rutas_t;

// Estructura para almacenar un comando con sus argumentos
typedef struct
{
  char **argumentos;    // Arreglo de strings (argv)
  int tamanio;          // Número actual de argumentos
  int capacidad;        // Capacidad total del arreglo
} comando_t;

// Estructura para almacenar múltiples comandos (para ejecución paralela con &)
typedef struct
{
  comando_t **comandos; // Arreglo de punteros a comandos
  int tamanio;          // Número actual de comandos
  int capacidad;        // Capacidad total del arreglo
} lista_comandos_t;

// Variables globales de la shell
static arreglo_rutas_t arreglo_rutas;   // PATH de búsqueda de ejecutables
static lista_comandos_t lista_comandos; // Lista de comandos a ejecutar
static char *linea_entrada;             // Buffer para leer entrada del usuario
static char *ruta_archivo;              // Buffer para construir rutas completas
static int codigo_salida;               // Código de salida de la shell
static int modo_batch;                  // Flag: 1 si está en modo batch, 0 si interactivo
static const char mensaje_error[30] = "Ha ocurrido un error\n";

// Imprime el mensaje de error estándar a stderr
static void imprimir_error(void)
{
  write(STDERR_FILENO, mensaje_error, strlen(mensaje_error));
}

// Inicializa un comando con memoria para sus argumentos
static void inicializar_comando(comando_t *c)
{
  c->tamanio = 0;
  c->capacidad = LONGITUD_INICIAL_ARREGLO;
  c->argumentos = calloc(c->capacidad, sizeof(char *));
}

// Inicializa el arreglo de rutas del PATH
static void inicializar_arreglo_rutas(arreglo_rutas_t *arr)
{
  arr->tamanio = 0;
  arr->capacidad = LONGITUD_INICIAL_ARREGLO;
  arr->entradas = calloc(arr->capacidad, sizeof(char *));
}

// Inicializa la lista de comandos
static void inicializar_lista_comandos(lista_comandos_t *cl)
{
  cl->tamanio = 0;
  cl->capacidad = LONGITUD_INICIAL_ARREGLO;
  cl->comandos = calloc(cl->capacidad, sizeof(comando_t *));
}

// Verifica si es necesario redimensionar el arreglo de argumentos de un comando
// Si está lleno, duplica su capacidad
static void verificar_longitud_comando(comando_t *c)
{
  if (c->tamanio == c->capacidad)
  {
    c->capacidad *= 2;
    c->argumentos = realloc(c->argumentos, c->capacidad * sizeof(char *));
  }
}

// Verifica si es necesario redimensionar el arreglo de rutas del PATH
// Si está lleno, duplica su capacidad
static void verificar_capacidad_rutas(arreglo_rutas_t *arr)
{
  if (arr->tamanio == arr->capacidad)
  {
    arr->capacidad *= 2;
    arr->entradas = realloc(arr->entradas, arr->capacidad * sizeof(char *));
  }
}

// Verifica si es necesario redimensionar la lista de comandos
// Si está llena, duplica su capacidad
static void asegurar_capacidad_lista_comandos(lista_comandos_t *cl)
{
  if (cl->tamanio == cl->capacidad)
  {
    cl->capacidad *= 2;
    cl->comandos = realloc(cl->comandos, cl->capacidad * sizeof(comando_t *));
  }
}

// Busca un ejecutable en el PATH
// Primero verifica si el nombre_archivo es ejecutable directamente,
// luego busca en cada directorio del PATH
// Retorna EXIT_SUCCESS si lo encuentra y guarda la ruta completa en ruta_salida
static int verificar_acceso_archivo_ejecutable(char *nombre_archivo, char **ruta_salida)
{
  // Verificar si el archivo es ejecutable tal como está (ruta absoluta o relativa)
  if (access(nombre_archivo, X_OK) == 0)
  {
    strcpy(*ruta_salida, nombre_archivo);
    return EXIT_SUCCESS;
  }
  
  // Buscar el ejecutable en cada directorio del PATH
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

// Ejecuta un comando externo (no built-in)
// Maneja la redirección de salida si existe el operador '>'
// Crea un proceso hijo con fork() y ejecuta el comando con execv()
// Retorna 1 si se creó un proceso hijo exitosamente
static int ejecutar_comando_externo(comando_t *c)
{
  int indice_redireccion = -1;
  
  // Buscar el operador de redirección '>' en los argumentos
  for (int i = 0; i < c->tamanio; i++)
  {
    if (strcmp(c->argumentos[i], ">") == 0)
    {
      indice_redireccion = i;
      
      // Validar formato de redirección: no puede estar al inicio ni tener múltiples archivos
      if (indice_redireccion == 0 || (c->tamanio - (indice_redireccion + 1)) != 1) 
      {
        imprimir_error();
        return EXIT_FAILURE;
      }
      
      // Marcar el final de los argumentos para execv (antes del '>')
      c->argumentos[indice_redireccion] = NULL; 
      break;
    }
  }
  
  // Buscar el ejecutable en el PATH
  if (verificar_acceso_archivo_ejecutable(c->argumentos[0], &ruta_archivo) == EXIT_SUCCESS)
  {
    // Crear proceso hijo
    pid_t hijo = fork();
    if (hijo == -1) 
    {
      imprimir_error();
      codigo_salida = EXIT_FAILURE;
      return EXIT_FAILURE;
    }
    
    // Código del proceso hijo
    if (hijo == 0) 
    {
      // Si hay redirección, configurar stdout y stderr al archivo
      if (indice_redireccion != -1) 
      {
        // Abrir o crear el archivo de salida
        int fd = open(c->argumentos[indice_redireccion + 1], O_WRONLY | O_CREAT | O_TRUNC, 0666);
        if (fd == -1)
        {
          imprimir_error();
          exit(EXIT_FAILURE);
        }
        
        // Redirigir stdout y stderr al archivo
        dup2(fd, STDOUT_FILENO);
        dup2(fd, STDERR_FILENO);
        close(fd);
      }
      
      // Ejecutar el comando (reemplaza el proceso hijo)
      execv(ruta_archivo, c->argumentos); 
      
      // Si execv retorna, hubo un error
      imprimir_error();
      exit(EXIT_FAILURE); 
    }
    
    // El padre retorna 1 para indicar que se creó un hijo
    return 1;
  }
  else
  {
    // No se encontró el ejecutable en el PATH
    imprimir_error();
  }
  return EXIT_FAILURE;
}

// Ejecuta un comando (built-in o externo)
// Identifica si es exit, cd, path o un comando externo
// Retorna EXIT_SUCCESS solo para el comando 'exit' (para salir del shell)
static int ejecutar_comando(comando_t *c)
{
  // Validar que el comando no esté vacío
  if (c->tamanio == 0 || c->argumentos[0] == NULL)
  {
    return EXIT_FAILURE; 
  }
  
  // --- COMANDO BUILT-IN: exit ---
  if (strcmp(c->argumentos[0], "exit") == 0)
  {
    // exit no debe tener argumentos
    if (c->tamanio > 1)
    {
      imprimir_error();
      return EXIT_FAILURE;
    }
    return EXIT_SUCCESS; // Señal para terminar la shell
  }
  
  // --- COMANDO BUILT-IN: cd ---
  if (strcmp(c->argumentos[0], "cd") == 0) 
  {
    // cd debe tener exactamente un argumento (el directorio)
    if (c->tamanio != 2)
    {
      imprimir_error();
      return EXIT_FAILURE;
    }
    
    // Cambiar de directorio usando la syscall chdir
    if (chdir(c->argumentos[1]) == -1) 
    {
      imprimir_error();
      return EXIT_FAILURE;
    }
    return EXIT_FAILURE; // No es exit, continuar
  }
  
  // --- COMANDO BUILT-IN: path ---
  if (strcmp(c->argumentos[0], "path") == 0) 
  {
    // Liberar las rutas actuales del PATH
    for (int i = 0; i < arreglo_rutas.tamanio; i++)
    {
      free(arreglo_rutas.entradas[i]); 
    }
    arreglo_rutas.tamanio = 0;
    
    // Agregar las nuevas rutas especificadas
    for (int i = 1; i < c->tamanio; i++)
    {
      verificar_capacidad_rutas(&arreglo_rutas); 
      arreglo_rutas.entradas[arreglo_rutas.tamanio] = malloc((strlen(c->argumentos[i]) + 1) * sizeof(char));
      strcpy(arreglo_rutas.entradas[arreglo_rutas.tamanio++], c->argumentos[i]);
    }
    return EXIT_FAILURE; // No es exit, continuar
  }
  
  // Si no es built-in, es un comando externo
  return ejecutar_comando_externo(c); 
}

// Bucle principal de la shell
// Lee comandos, los parsea, y los ejecuta
static void iniciar_shell(FILE *entrada)
{
  while (1)
  {
    // Mostrar prompt solo en modo interactivo
    if (!modo_batch)
    {
      printf("wish> ");
      fflush(stdout);
    }
    
    // Leer línea de entrada
    if (fgets(linea_entrada, LONGITUD_MAXIMA_COMANDO, entrada) == NULL || feof(entrada))
    {
      if (modo_batch)
      {
        break; // Fin del archivo batch
      }
      imprimir_error();
      codigo_salida = EXIT_FAILURE;
      break;
    }
    
    // Reiniciar la lista de comandos para esta línea
    lista_comandos.tamanio = 0; 
    
    // --- PARSING: Separar comandos por '&' (para ejecución paralela) ---
    char *puntero_contexto_separador = NULL;
    char *linea_comando_simple = strtok_r(linea_entrada, "&", &puntero_contexto_separador);
    
    while (linea_comando_simple != NULL)
    {
      // Asegurar espacio en la lista de comandos
      asegurar_capacidad_lista_comandos(&lista_comandos);
      
      // Obtener o crear el comando en esta posición
      comando_t *comando_individual = lista_comandos.comandos[lista_comandos.tamanio];
      if (comando_individual == NULL)
      {
        comando_individual = malloc(sizeof(comando_t));
        inicializar_comando(comando_individual);
        lista_comandos.comandos[lista_comandos.tamanio] = comando_individual;
      }
      comando_individual->tamanio = 0;
      
      // --- PARSING: Separar argumentos por espacios, tabs y newlines ---
      char *puntero_token = NULL;
      char *token = strtok_r(linea_comando_simple, " \t\n", &puntero_token);
      
      while (token != NULL)
      {
        // Agregar cada token como argumento
        verificar_longitud_comando(comando_individual);
        comando_individual->argumentos[comando_individual->tamanio] = malloc((strlen(token) + 1) * sizeof(char));
        strcpy(comando_individual->argumentos[comando_individual->tamanio++], token);
        token = strtok_r(NULL, " \t\n", &puntero_token);
      }
      
      // Agregar NULL al final (requerido por execv)
      verificar_longitud_comando(comando_individual);
      comando_individual->argumentos[comando_individual->tamanio] = NULL;
      
      // Pasar al siguiente comando (separado por &)
      linea_comando_simple = strtok_r(NULL, "&", &puntero_contexto_separador);
      lista_comandos.tamanio++;
    }
    
    // --- EJECUCIÓN: Ejecutar todos los comandos (en paralelo si hay &) ---
    int contador_hijos = 0;
    int debe_salir = 0;
    
    for (int i = 0; i < lista_comandos.tamanio; i++)
    {
      int resultado = ejecutar_comando(lista_comandos.comandos[i]);
      
      if (resultado == EXIT_SUCCESS) // El comando fue 'exit'
      {
        debe_salir = 1;
        break; // No ejecutar más comandos
      }
      else if (resultado == 1) // Se creó un proceso hijo (comando externo)
      {
        contador_hijos++;
      }
    }
    
    // --- SINCRONIZACIÓN: Esperar a que todos los procesos hijos terminen ---
    for (int i = 0; i < contador_hijos; i++)
    {
      wait(NULL);
    }
    
    // Si se ejecutó 'exit', salir del bucle
    if (debe_salir)
    {
      break;
    }
  }
}

int main(int argc, char *argv[])
{
  // --- VALIDACIÓN: Verificar número de argumentos ---
  // Solo se permite: ./wish (modo interactivo) o ./wish archivo (modo batch)
  if (argc > 2)
  {
    imprimir_error();
    exit(1);
  }
  
  // Determinar el modo de operación
  modo_batch = argc == 2;
  FILE *entrada = stdin;
  
  // Si es modo batch, abrir el archivo
  if (modo_batch)
  {
    entrada = fopen(argv[1], "r");
    if (entrada == NULL)
    {
      imprimir_error();
      exit(1);
    }
  }
  
  // --- INICIALIZACIÓN: Preparar estructuras de datos ---
  inicializar_arreglo_rutas(&arreglo_rutas);
  
  // Configurar el PATH inicial con /bin
  arreglo_rutas.entradas[0] = malloc(5 * sizeof(char));
  strcpy(arreglo_rutas.entradas[0], "/bin");
  arreglo_rutas.tamanio = 1;
  
  inicializar_lista_comandos(&lista_comandos);
  linea_entrada = malloc(LONGITUD_MAXIMA_COMANDO * sizeof(char));
  ruta_archivo = malloc(LONGITUD_MAXIMA_COMANDO * sizeof(char));
  
  // --- EJECUCIÓN: Iniciar el bucle principal de la shell ---
  iniciar_shell(entrada);
  
  // --- LIMPIEZA: Liberar toda la memoria asignada ---
  free(ruta_archivo);
  free(linea_entrada);
  
  // Liberar las entradas del PATH
  for (int i = 0; i < arreglo_rutas.tamanio; i++)
  {
    free(arreglo_rutas.entradas[i]);
  }
  free(arreglo_rutas.entradas);
  
  // Liberar todos los comandos y sus argumentos
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
  
  // Cerrar archivo si está en modo batch
  if (entrada != stdin)
  {
    fclose(entrada);
  }
  
  // Salir con el código apropiado
  exit(codigo_salida); 
}