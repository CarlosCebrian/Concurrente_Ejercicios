/* Ejercicio 1 Practica 2 --- Programacion Concurrente y Tiempo Real

   Carlos Jesus Cebrian Sanchez ---- 48686394V */

#include <stdio.h>
#include <stdlib.h> /* EXIT_SUCCESS , EXIT_FAILURE */
#include <sys/mman.h> /* shm_open(), mmap() */
#include <sys/stat.h> /* shm_open() */
#include <fcntl.h> /* shm_open()*/
#include <unistd.h> /* getpid() */
#include <sys/types.h> /* getpid() */

#include <definitions.h>
#include <semaphoreI.h>

/* FUNCIONES */
void get_shm_segments(int *shm_data,int *shm_task,int *shm_symbol,struct TData_t **p_data,struct TTask_t **p_task,struct TSymbol_t **symbol);
void get_sems(sem_t **p_sem_task_ready,sem_t **p_sem_task_read,sem_t **p_sem_task_processed,sem_t **p_sem_mutex,sem_t **p_sem_symbol_ready,sem_t **p_sem_symbol_decoded);
void get_and_process_task(sem_t *sem_task_ready,sem_t *sem_task_read,sem_t *sem_mutex,sem_t *sem_symbol_ready,sem_t *sem_symbol_decoded,struct TData_t *data,const struct TTask_t *task,struct TSymbol_t *symbol);
void notify_task_completed(sem_t *sem_task_processed);


int main(int argc,char *argv[]){
  struct TData_t *data; /* Estructura de datos codificados y decodificados */
  struct TTask_t *task; /* Estructura de datos de tareas */
  struct TSymbol_t *symbol; /* Estructura de datos de simbolos */

  int shm_data; /* Descriptor de archivo de la memoria compartida de datos */
  int shm_task; /* Descriptor de archivo de la memoria compartida de tareas */
  int shm_symbol; /* Descriptor de archivo de la memoria compartida de simbolos */
  
  sem_t *sem_task_ready; /* Indica que la tarea esta preparada */
  sem_t *sem_task_read; /* Indica que la tarea ha sido leida */
  sem_t *sem_task_processed; /* Indica que la tarea ha sido procesada */
  sem_t *sem_mutex; /* Acceso de datos en exclusion mutua */
  sem_t *sem_symbol_ready; /* Indica que el simbolo esta preparado */
  sem_t *sem_symbol_decoded; /* Indica que el simbolo ha sido decodificado */

  /* Obtenemos y creamos los segmentos de memoria compartida */
  get_shm_segments(&shm_data,&shm_task,&shm_symbol,&data,&task,&symbol);

  /* Obtenemos los semaforos */
  get_sems(&sem_task_ready,&sem_task_read,&sem_task_processed,&sem_mutex,&sem_symbol_ready,&sem_symbol_decoded);

  /* Procesamiento de tareas */
  while(1){
    get_and_process_task(sem_task_ready,sem_task_read,sem_mutex,sem_symbol_ready,sem_symbol_decoded,data,task,symbol);
    notify_task_completed(sem_task_processed);
  }

  return EXIT_SUCCESS;
}

/* OBTENCION Y CREACION DE SEGMENTOS DE MEMORIA COMPARTIDA */
void get_shm_segments(int *shm_data,int *shm_task,int *shm_symbol,struct TData_t **p_data,struct TTask_t **p_task,struct TSymbol_t **p_symbol){
  
  *shm_data = shm_open(SHM_DATA,O_RDWR,0644);
  *p_data = mmap(NULL,sizeof(struct TData_t),PROT_READ | PROT_WRITE, MAP_SHARED, *shm_data,0);

  *shm_task = shm_open(SHM_TASK,O_RDWR,0644);
  *p_task = mmap(NULL,sizeof(struct TTask_t),PROT_READ | PROT_WRITE, MAP_SHARED, *shm_task,0);

  *shm_symbol = shm_open(SHM_SYMBOL,O_RDWR,0644);
  *p_symbol = mmap(NULL, sizeof(struct TSymbol_t),PROT_READ | PROT_WRITE, MAP_SHARED, *shm_symbol,0);
}

/* OBTENCION DE SEMAFOROS */
void get_sems(sem_t **p_sem_task_ready,sem_t **p_sem_task_read,sem_t **p_sem_task_processed,sem_t **p_sem_mutex,sem_t **p_sem_symbol_ready,sem_t **p_sem_symbol_decoded){
  *p_sem_task_ready = get_semaphore(SEM_TASK_READY);
  *p_sem_task_read = get_semaphore(SEM_TASK_READ);
  *p_sem_task_processed = get_semaphore(SEM_TASK_PROCESSED);
  *p_sem_mutex = get_semaphore(SEM_MUTEX);
  *p_sem_symbol_ready = get_semaphore(SEM_SYMBOL_READY);
  *p_sem_symbol_decoded = get_semaphore(SEM_SYMBOL_DECODED);
}

/* PROCESAMIENTO DE DATOS */
void get_and_process_task(sem_t *sem_task_ready,sem_t *sem_task_read,sem_t *sem_mutex,sem_t *sem_symbol_ready,sem_t *sem_symbol_decoded,struct TData_t *data,const struct TTask_t *task,struct TSymbol_t *symbol){
  int i, task_begin, task_end;

  wait_semaphore(sem_task_ready);
  task_begin = task->begin;
  task_end = task->end;
  signal_semaphore(sem_task_read);
  
  for(i = task->begin; i <= task->end; i++){
    if(data->vector[i] <= 26){
      data->vector[i] = data->vector[i] + 96;
    }
    else{
      if(data->vector[i] <= 52){
      data->vector[i] = data->vector[i] + 38;
      }
      else{
	wait_semaphore(sem_mutex);
	symbol->value = data->vector[i];
	signal_semaphore(sem_symbol_ready);
	wait_semaphore(sem_symbol_decoded);
	data->vector[i] = symbol->value;
	signal_semaphore(sem_mutex);
      }
    }
  }

  printf("[PID %d] Begin %d End %d Decoded Task ",getpid(),task_begin,task_end);
  for(i = task_begin; i < task_end; i++){
    printf("%c",data->vector[i]);
  }
  printf("\n");
}

/* NOTIFICACION DE TAREA FINALIZADA */
void notify_task_completed(sem_t *sem_task_processed){
  signal_semaphore(sem_task_processed);
}
