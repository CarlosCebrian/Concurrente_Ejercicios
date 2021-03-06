/* Ejercicio 1 Practica 2 --- Programacion Concurrente y Tiempo Real

   Carlos Jesus Cebrian Sanchez ---- 48686394V */

#define _POSIX_SOURCE
#define _BSD_SOURCE

#include <stdio.h>
#include <signal.h> /* signal(), kill() */
#include <string.h> /* strerror(), strtok() */
#include <errno.h> /* errno */
#include <unistd.h> /* getpid(), _exit(), fork(), sleep(),close(),ftruncate() */
#include <sys/types.h> /* getpid(), kill(), ftruncate() */
#include <stdlib.h> /* EXIT_FAILURE, EXIT_SUCCESS, free(), atoi(), malloc() */
#include <sys/mman.h> /* shm_unlink(), shm_open(), mmap() */
#include <sys/stat.h> /* shm_unlink(), shm_open() */
#include <fcntl.h> /* shm_unlink(), shm_open()*/
#include <math.h> /* ceil() */

#include <definitions.h>
#include <semaphoreI.h>

/* VARIABLES GLOBALES */
int g_nProcesses;
struct TProcess_t *g_process_table;

/* FUNCIONES */
void install_signal_handler();
void signal_handler(int sig);
void terminate_processes();
void free_resources();
void parse_argv(int argc, char *argv[], char **encoded_data, int *n_process_decoder, int *n_decodes_per_process);
void init_process_table(int n_process_decoder,int n_symbol_decoder);
void create_shm_segments(int *shm_data,int *shm_task,int *shm_symbol,struct TData_t **p_data,struct TTask_t **p_task,char *encoded_data,int *n_input_data);
void create_sems(sem_t **p_sem_task_ready,sem_t **p_sem_task_read,sem_t **p_sem_task_processed);
void create_processes_by_class(enum ProcessClass_t class,int n_processes,int index_process_table);
void get_str_process_info(enum ProcessClass_t class,char **path,char **str_process_class);
pid_t create_single_process(const char *path, const char *class, const char *argv);
void notify_tasks(sem_t *sem_task_ready,sem_t *sem_task_read,struct TTask_t *task,int n_decodes_per_process,int *n_tasks,int n_input_data);
void wait_task_termination(sem_t *sem_task_processed,int n_tasks);
void print_result(struct TData_t *data, int n_input_data);
void close_shared_memory_segments(int shm_data,int shm_task,int shm_symbol);

/* METODO PRINCIPAL */
int main(int argc,char *argv[]){
  char *encoded_data; /* Cadena introducida por linea de argumentos */
  int n_process_decoder; /* Numero de procesos Decodificador */
  int n_decodes_per_process; /* Numero de decodificaciones por proceso */
  int n_input_data; /* Numero de caracteres que contiene la cadena */
  int n_tasks;
  
  /* Segmentos de memoria compartida */
  int shm_data; /* Memoria compartida de datos */
  int shm_task; /* Memoria compartida de tareas */
  int shm_symbol; /* Memoria compartida de simbolos */

  /* Estructuras de Datos y Tareas */
  struct TData_t *data;
  struct TTask_t *task;

  /* Semaforos */
  sem_t *sem_task_ready; /* Indica que la tarea esta preparada */
  sem_t *sem_task_read; /* Indica que la tarea ha sido leida */
  sem_t *sem_task_processed; /* Indica que la tarea ha sido procesada */

  /* Instalador de manejo de señales */
  install_signal_handler();

  /* Control de la linea de argumentos */
  parse_argv(argc,argv,&encoded_data,&n_process_decoder,&n_decodes_per_process);
  
  /* Iniciamos la tabla de procesos */
  init_process_table(n_process_decoder,NUM_SYMBOL_DECODERS);

  /* Creamos los segmentos de memoria compartida */
  create_shm_segments(&shm_data,&shm_task,&shm_symbol,&data,&task,encoded_data,&n_input_data);

  /* Creamos e inicializamos los semaforos */
  create_sems(&sem_task_ready,&sem_task_read,&sem_task_processed);

  /* Creamos los procesos DECODER y SYMBOL_DECODER */
  create_processes_by_class(DECODER,n_process_decoder,0);
  create_processes_by_class(SYMBOL_DECODER,NUM_SYMBOL_DECODERS,n_process_decoder);

  /* Crear las tareas */
  notify_tasks(sem_task_ready,sem_task_read,task,n_decodes_per_process,&n_tasks,n_input_data);

  /* Esperar a la finalizacion de las tareas */
  wait_task_termination(sem_task_processed,n_tasks);

  /* Escribir resultados */
  print_result(data,n_input_data);

  /* Terminacion del programa */
  close_shared_memory_segments(shm_data,shm_task,shm_symbol);
  terminate_processes();
  free_resources();

  return EXIT_SUCCESS;
}

/* INSTALADOR DEL MANEJADOR DE SEÑALES */
void install_signal_handler(){
  if(signal(SIGINT,signal_handler) == SIG_ERR){ /* Control de errores */
    fprintf(stderr,"[MANAGER %d] Signal handler's install error: %s\n",getpid(), strerror(errno));
    _exit(EXIT_FAILURE);
  }
}

/* MANEJADOR DE SEÑALES */
void signal_handler(int sig){
  printf("[MANAGER %d] Program Termination (CTRL + C)\n",getpid());
  terminate_processes();
  free_resources();
  _exit(EXIT_SUCCESS);
}

/* TERMINADOR DE PROCESOS */
void terminate_processes(){
  int i;
  printf("\n----- [MANAGER %d] Terminating running child processes -----\n",getpid());
  for(i = 0 ; i < g_nProcesses ; i++){ /* Terminar todos los procesos */
    if(g_process_table[i].pid != 0){ /* El proceso hijo vivo */
      printf("[MANAGER %d] Terminating %s process [%d]...\n",getpid(),g_process_table[i].str_process_class,g_process_table[i].pid);
      if(kill(g_process_table[i].pid,SIGINT) == -1){ /* Control de errores */
	fprintf(stderr,"[MANAGER %d] Kill error : %s\n",getpid(),strerror(errno));
      }
    }
  }
}

/* LIBERADOR DE RECURSOS */
void free_resources(){
  /* Liberar tabla de procesos */
  free(g_process_table);

  /* Eliminar semaforos */
  remove_semaphore(SEM_TASK_READY);
  remove_semaphore(SEM_TASK_READ);
  remove_semaphore(SEM_TASK_PROCESSED);
  remove_semaphore(SEM_MUTEX);
  remove_semaphore(SEM_SYMBOL_READY);
  remove_semaphore(SEM_SYMBOL_DECODED);

  /* Liberar segmentos de memoria compartida */
  shm_unlink(SHM_TASK);
  shm_unlink(SHM_DATA);
  shm_unlink(SHM_SYMBOL);
}

/* PROCESADOR DE ARGUMENTOS */
void parse_argv(int argc, char *argv[], char **encoded_data, int *n_process_decoder, int *n_decodes_per_process){
  if(argc != 4){ /* Control de errores */
    fprintf(stderr,"[MANAGER %d] Argument error, use: manager <String> <nºDecoder> <nºtask_per_process>\n",getpid());
    _exit(EXIT_FAILURE);
  }

  *encoded_data = argv[1]; /* String de datos codificados */
  *n_process_decoder = atoi(argv[2]); /* Numero de procesos Decodificador */
  *n_decodes_per_process = atoi(argv[3]); /* Numero de tareas por proceso */
}

/* INICIADOR DE LA TABLA DE PROCESOS */
void init_process_table(int n_process_decoder,int n_symbol_decoder){
  int i;

  g_nProcesses = n_process_decoder + n_symbol_decoder;
  
  /* Asignamos memoria dinamica */
  g_process_table = malloc(g_nProcesses * sizeof(struct TProcess_t));

  /* Inicializamos tabla */
  for(i = 0 ; i < g_nProcesses; i++){
    g_process_table[i].pid = 0;
  }
}

/* CREADOR DE SEGMENTOS DE MEMORIA COMPARTIDA */
void create_shm_segments(int *shm_data,int *shm_task,int *shm_symbol,struct TData_t **p_data,struct TTask_t **p_task,char *encoded_data,int *n_input_data){
  int i = 0;
  char *encoded_character;

  /* Crear e inicializar segmentos de memoria compartida 
     shm_segment = shm_open(name,flags,perms);
     ftruncate(shm_segment, size);
     mmap(NULL,size,prots,flags,shm_fd,0);
  */
  *shm_data = shm_open(SHM_DATA, O_CREAT | O_RDWR , 0644);
  ftruncate(*shm_data,sizeof(struct TData_t));
  *p_data = mmap(NULL,sizeof(struct TData_t),PROT_READ | PROT_WRITE, MAP_SHARED, *shm_data,0);

  *shm_task = shm_open(SHM_TASK, O_CREAT | O_RDWR , 0644);
  ftruncate(*shm_task,sizeof(struct TTask_t));
  *p_task = mmap(NULL,sizeof(struct TTask_t),PROT_READ | PROT_WRITE, MAP_SHARED, *shm_task,0);

  *shm_symbol = shm_open(SHM_SYMBOL, O_CREAT | O_RDWR, 0644);
  ftruncate(*shm_symbol,sizeof(char));

  /* Cargar los datos codificados */
  (*p_data)->vector[i++] = atoi(strtok(encoded_data,SEPARATOR));
  while((encoded_character = strtok(NULL,SEPARATOR)) != NULL){
    (*p_data)->vector[i++] = atoi(encoded_character);
  }
  *n_input_data = i;
}

/* CREADOR E INICIALIZADOR DE SEMAFOROS */
void create_sems(sem_t **p_sem_task_ready,sem_t **p_sem_task_read,sem_t **p_sem_task_processed){
  
  /* Creacion e inicializacion de los semaforos que utiliza el proceso manager*/
  *p_sem_task_ready = create_semaphore(SEM_TASK_READY,0);
  *p_sem_task_read = create_semaphore(SEM_TASK_READ,0);
  *p_sem_task_processed = create_semaphore(SEM_TASK_PROCESSED,0);

  /* Creacion de los semaforos que utilizan otros procesos */
  create_semaphore(SEM_MUTEX,1);
  create_semaphore(SEM_SYMBOL_READY,0);
  create_semaphore(SEM_SYMBOL_DECODED,0);
}

/* CREADOR DE PROCESOS */
void create_processes_by_class(enum ProcessClass_t class,int n_processes,int index_process_table){
  char *path = NULL; /* Path al programa DECODER O SYMBOL_DECODER */
  char *str_process_class = NULL; /* Descriptor del programa */
  int i;
  pid_t pid;

  get_str_process_info(class,&path,&str_process_class);

  for(i = index_process_table; i < (index_process_table + n_processes);i++){
    pid = create_single_process(path,str_process_class,NULL);

    g_process_table[i].class = class;
    g_process_table[i].pid = pid;
    g_process_table[i].str_process_class = str_process_class;
  }
  
  printf("[MANAGER %d] %d %s processes created.\n",getpid(),n_processes,str_process_class);
  sleep(1);
}

/* MANEJADOR DE INFORMACION DE LOS PROCESOS */
void get_str_process_info(enum ProcessClass_t class,char **path,char **str_process_class){
  switch(class){
  case DECODER:
    *path = DECODER_PATH;
    *str_process_class = DECODER_CLASS;
    break;
  case SYMBOL_DECODER:
    *path = SYMBOL_DECODER_PATH;
    *str_process_class = SYMBOL_DECODER_CLASS;
    break;
  }
} 

/* EJECUTADOR DE PROCESOS */
pid_t create_single_process(const char *path, const char *class, const char *argv){
  pid_t pid;

  switch(pid = fork()){
  case -1:
    fprintf(stderr,"[MANAGER %d] Forking Error: %s\n",getpid(),strerror(errno));
    terminate_processes();
    free_resources();
    _exit(EXIT_FAILURE);
  case 0:
    if(execl(path,class,argv,NULL) == -1){
      fprintf(stderr,"[%s %d] Exec Error: %s\n",class,getpid(),strerror(errno));
      _exit(EXIT_FAILURE);
    }
  }  
  return pid;
}

/* NOTIFICADOR DE TAREAS */
void notify_tasks(sem_t *sem_task_ready,sem_t *sem_task_read,struct TTask_t *task,int n_decodes_per_process,int *n_tasks,int n_input_data){
  int current_task = 0;

  *n_tasks = ceil(n_input_data / (float)n_decodes_per_process);

  while(current_task < *n_tasks){
    task->begin = current_task * n_decodes_per_process;
    task->end = task->begin + n_decodes_per_process - 1;
    
    if(task->end > (n_input_data - 1)){
      task->end = n_input_data -1;
    }
    current_task++;

    signal_semaphore(sem_task_ready);
    wait_semaphore(sem_task_read);
  }
}

/* ESPERA DE TERMINACION DE TAREAS */
void wait_task_termination(sem_t *sem_task_processed,int n_tasks){
  int n_tasks_processed = 0;
  
  while(n_tasks_processed < n_tasks){
    wait_semaphore(sem_task_processed);
    n_tasks_processed++;
  }
}

/* MOSTRAR RESULTADOS */
void print_result(struct TData_t *data, int n_input_data){
  int i;

  printf("\n ----- [MANAGER %d Printing result ----- \n",getpid());
  printf("Decoded result: ");
  for(i = 0 ; i < n_input_data ; i++){
    printf("%c",data->vector[i]);
  }
  printf("\n");
}

void close_shared_memory_segments(int shm_data,int shm_task,int shm_symbol){
  close(shm_data);
  close(shm_task);
  close(shm_symbol);
}
