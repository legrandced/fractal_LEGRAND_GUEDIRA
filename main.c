#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <string.h>
#include <pthread.h>
#include <semaphore.h>
#include <errno.h>
#include "libfractal/fractal.h"

#define MAX_BUFFER_LEN 16 // Taille maximale de la structure chainee

//NOEUD CONTENANT UNE FRACTALE ET LA SUIVANTE
typedef struct fractal_node{
  fractal_t f;
  struct fractal_node *next;
} fractal_node_t;

//LISTE CHAINEE DE FRACTALES DE LONGUEUR len
typedef struct fractal_buffer{
  fractal_node_t *first;

  int in;                 /* longueur de la liste chainee */
  int out;                /* MAX_BUFFER_LEN - in */
  sem_t full;             /* keep track of the number of full spots */
  sem_t empty;            /* keep track of the number of empty spots */
  pthread_mutex_t mutex;  /* enforce mutual exclusion to shared data */

} fractal_buffer_t;

// VARIABLES GLOBALES----------------------------------------
fractal_buffer_t fb_shared_uncomputed;
fractal_buffer_t fb_shared_computed;
fractal_node_t fn_highest_mean;
int end_read = 1;
int end_compute = 1;

//------------------------------------------------------------

// FONCTIONS SUR BUFFER---------------------------------------

void push(fractal_buffer_t *fractal_buffer, fractal_t *f){
  fractal_node_t *new = (fractal_node_t*) malloc(sizeof(fractal_node_t));
  if (new == NULL){
    fprintf(stderr, "push : new NULL");
    return;
  }

  new->f = *f;
  sem_wait(&fractal_buffer->empty);
  new->next = fractal_buffer->first;
  fractal_buffer->first = new;

  //section critique
  pthread_mutex_lock(&fractal_buffer->mutex);
  new->next = fractal_buffer->first;
  fractal_buffer->first = new;
  fractal_buffer->in = (fractal_buffer->in+1)%MAX_BUFFER_LEN;
  pthread_mutex_unlock(&fractal_buffer->mutex);

  sem_post(&fractal_buffer->full);
}

fractal_t pop(fractal_buffer_t *fractal_buffer){
  fractal_node_t *removed = fractal_buffer->first;
  sem_wait(&fractal_buffer->full);

  //section critique
  pthread_mutex_lock(&fractal_buffer->mutex);
  fractal_t f = fractal_buffer->first->f;
  fractal_buffer->first = fractal_buffer->first->next;
  fractal_buffer->out = (fractal_buffer->out+1)%MAX_BUFFER_LEN;
  pthread_mutex_unlock(&fractal_buffer->mutex);

  sem_post(&fractal_buffer->empty);
  free(removed);
  return f;
}

//------------------------------------------------------------

// FONCTIONS SUR LES NOEUDS-----------------------------------

void insert(fractal_node_t *fn, fractal_t f){
  fractal_node_t *new;
  new->f = f;
  new->next = fn;
  fn = new;
}

void clear(fractal_node_t *fractal_node){
  fractal_node_t *t;
  while (fractal_node != NULL){
    t = fractal_node;
    fractal_node = fractal_node->next;
    free(t);
  }
}

//------------------------------------------------------------

void *read_file_thread(void *arg){
  //TEST FILE
  char *filename = (char *) arg;
  if (filename == NULL){
    fprintf(stderr, "read_file : filename NULL");
    return NULL;
  }

  //OPERATIONS ON FILE
  int fo, fr, fc;

  if ((fo = open(filename, O_RDONLY)) < 0){
    fprintf(stderr, "read_file : open()");
    return NULL;
  }

  //INITIALIZING FRACTAL DATA'S
  char *name = (char *) malloc(sizeof(char)*64);
  int width, height;
  double a, b;
  const char *const_name;

  //WORD BUFFER
  char *wordBuffer = (char *)malloc(sizeof(char) * 64);
  if (wordBuffer == NULL) {
    fprintf(stderr, "read_file : wordBuffer NULL");
    return NULL;
  }
  char ch[1];

  if ((fr = read(fo, (void *) &ch, sizeof(char))) < 0){
    fprintf(stderr, "read_file : read()");
    return NULL;
  }

  int word = 0, i = 0;

  while(fr > 0){

    //Ignore les commentaires dans le fichier lu
    if(ch[0] == '#'){
      while(ch[0] != '\n'){
        if ((fr = read(fo, (void *) &ch, sizeof(char))) < 0){
          fprintf(stderr, "read_file : read()");
          return NULL;
        }
      }
      if ((fr = read(fo, (void *) &ch, sizeof(char))) < 0){
        fprintf(stderr, "read_file : read()");
        return NULL;
      }
    }

    //Lis ligne par ligne le fichier
    while(ch[0] != '\n'){
      do{
        wordBuffer[i]=ch[0];
        if ((fr = read(fo, (void *) &ch, sizeof(char))) < 0){
          fprintf(stderr, "read_file : read()");
          return NULL;
        }
        i++;
      }while(ch[0] != ' ');
      wordBuffer[i] = '\0';
      switch (word) {
        case 0: strcpy(name, wordBuffer); break;
        case 1: width = atoi(wordBuffer); break;
        case 2: height = atoi (wordBuffer); break;
        case 3: a = atof(wordBuffer); break;
        case 4: b = atof(wordBuffer); break;
      }
      i=0;
      word++;
      if ((fr = read(fo, (void *) &ch, sizeof(char))) < 0){
        fprintf(stderr, "read_file : read()");
        return NULL;
      }
    }
    word = 0;

    const_name = name;
    fractal_t *f = fractal_new(const_name, width, height, a, b);
    push(&fb_shared_uncomputed, f);
  }

  fc = close(fo);
  if (fc  < 0){
    free(name);
    free(wordBuffer);
    fprintf(stderr, "read_file : close");
    return NULL;
  }
  free(name);
  free(wordBuffer);

  char* str = "FIN DU THREAD DE LECTURE DU FICHIER";
  pthread_exit((void*)str);
}


void *compute_thread()
{
  while(end_read)
  {
    fractal_t f = pop(&fb_shared_uncomputed);

    // Calcule les value[][] des fractales et les additionne en vue de calculer la moyenne
    for (int i = 0 ; i < fractal_get_width(&f) ; i++){
      for (int j = 0 ; j < fractal_get_height(&f) ; j++){
        fractal_set_value(&f, i, j, fractal_compute_value(&f, i, j));
        f.mean += (double) fractal_get_value(&f, i, j);
      }
    }
    push(&fb_shared_computed, &f);
  }

  char* str = "FIN DU THREAD DE CALCUL";
  pthread_exit((void*)str);
}

void *final_thread()
{
  while(end_compute){
    fractal_t f = pop(&fb_shared_computed);
    if(f.mean > fn_highest_mean.f.mean){
      clear(&fn_highest_mean);
      fn_highest_mean.f = f;
    }
    else if (f.mean == fn_highest_mean.f.mean){
      insert(&fn_highest_mean, f);
    }
  }

  fractal_node_t current = fn_highest_mean;
  char *src;
  while (current.next != NULL){
    src = (char *) current.f.name;
    write_bitmap_sdl(&(current.f), strcat(src, ".bmp"));
  }

  char* str = "FIN DU THREAD FINAL";
  pthread_exit((void*)str);
}


int main(int argc, char **argv)
{
  int i;
  int n = argc - 2; //number of files
  int m = n; //number of compute_threads

  // lecture des option
  if(strcmp(argv[1],"-d")==0){
    n --;
    if(strcmp(argv[2],"--maxthreads")==0){
      m = atoi(argv[3]);
      n -= 2;
    }
  }
  else if(strcmp(argv[1],"--maxthreads")==0){
    m = atoi(argv[2]);
    n -= 2;
  }

  char *filename_out = argv[argc-1];
  printf("LES REPONSES SERONT ECRITES DANS : %s\n", filename_out);

  char *filenames[n];

  printf("LECTURE DES FICHIERS :\n");
  for(i = 0; i<n; i++) {
    filenames[i] = argv[argc-n-1+i];
    printf("%s\n", filenames[i]);
  }

  // Initialisation des sémaphores et du mutex
  sem_init(&fb_shared_uncomputed.full, 0, 0);
  sem_init(&fb_shared_uncomputed.empty, 0, MAX_BUFFER_LEN);
  pthread_mutex_init(&fb_shared_uncomputed.mutex, NULL);

  sem_init(&fb_shared_computed.full, 0, 0);
  sem_init(&fb_shared_computed.empty, 0, MAX_BUFFER_LEN);
  pthread_mutex_init(&fb_shared_computed.mutex, NULL);

  printf("LANCEMENT DES THREADS\n");

  pthread_t read_file_threads[n];
  pthread_t compute_threads[n];
  pthread_t final_pthread;

  for(i = 0; i < n; i++){
    pthread_create(&read_file_threads[i], NULL, &read_file_thread, (void *) (filenames[i]));
  }

  for(i = 0; i < n; i++){
    pthread_create(&compute_threads[i], NULL, &compute_thread, NULL);
  }

  pthread_create(&final_pthread, NULL, &final_thread, NULL);

  // Fin des threads de lecture
  char * str;
  for (i = 0; i < n; i++)
  {
    //attend la fin du thread
    pthread_join(read_file_threads[i], (void**)&str);
    printf("%s\n", str);
  }
  end_read = 0;

  // Fin des threads de calcul
  for (i = 0; i < n; i++)
  {
    //attend la fin du thread
    pthread_join(compute_threads[i], (void**)&str);
    printf("%s\n", str);
  }

  end_compute = 0;

  // Fin du thread final
  pthread_join(final_pthread, (void**)&str);
  printf("%s\n", str);

}
