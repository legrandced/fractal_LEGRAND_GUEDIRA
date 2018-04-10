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

// STRUCTURES------------------------------------------------

// STRUCTURE CONTENANT LES NOMS DES FRACTALES TRIES
typedef struct f_name_node{
  const char *f_name;
  struct f_name_node *next;
} f_name_node_t;

// NOEUD CONTENANT UNE FRACTALE ET LA SUIVANTE
typedef struct fractal_node{
  fractal_t f;
  struct fractal_node *next;
} fractal_node_t;

// LISTE CHAINEE DE FRACTALES DE LONGUEUR len
typedef struct fractal_buffer{
  fractal_node_t *first;

  int in;                 // longueur de la liste chainee
  int out;                // MAX_BUFFER_LEN - in
  sem_t full;             // keep track of the number of full spots
  sem_t empty;            // keep track of the number of empty spots
  pthread_mutex_t mutex;  // enforce mutual exclusion to shared data

} fractal_buffer_t;

//------------------------------------------------------------

// VARIABLES GLOBALES----------------------------------------

fractal_buffer_t fb_shared_uncomputed;
fractal_buffer_t fb_shared_computed;
fractal_node_t fn_highest_mean; // 8=================================D--------------------------------
int d_arg = 0; // = 1 si '-d' est présent en argument, 0 sinon
int stdin_arg = 0; // = 1 si '-' est présent en argument, 0 sinon
int end_read = 1;
int end_compute = 1;

f_name_node_t *first;

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

void write_in_file(char *filename, fractal_t *f){
  if (f == NULL || filename == NULL){
    fprintf(stderr, "write_in_file : f || filename NULL");
    return;
  }

  int fo, fc, fw;

  if (fo = open(filename, O_RDWR | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR) < 0){
    fprintf(stderr, "write_in_file : open()");
    return;
  }

  if (fw = write(fo, (void*) f, sizeof(fractal_t)) < 0){
    fprintf(stderr, "write_in_file : write()");
    return;
  }
  else if (fw == 0){
    fprintf(stderr, "write_in_file : nothing to write in file");
    return;
  }

  if (fc = close(fo) < 0){
    fprintf(stderr, "write_in_file : close()");
    return;
  }
}

//------------------------------------------------------------

// FONCTIONS SUR LA LISTE DES NOMS DES FRACTALES--------------

int compare(const char* a, const char* b) {
  if (strcmp(a, b) == 0) return 0;
  else if (strcmp(a, b) < 0) return -1;
  else return 1;
}

int insert_name(const char* name, int (*cmp)(const char*, const char*)){

  // size == 0
  if(first->f_name == NULL) {
    strcpy((char *) first->f_name, name);
    return 1;
  }

  // size == 1 || to be placed as first
  if(first->next == NULL || cmp(first->f_name, name) > 0) {
    f_name_node_t *elem = first;

    if(cmp(elem->f_name, name) < 0) { // place after
      f_name_node_t *new = (f_name_node_t *) malloc(sizeof(f_name_node_t));
      if(new == NULL) return -1;

      new->next = NULL;
      new->f_name = name;

      elem->next = new;
      return 1;
    } else if(cmp(elem->f_name, name) == 0) { // do not place
      return 0;
    } else { // place before
      f_name_node_t *new = (f_name_node_t *) malloc(sizeof(f_name_node_t));
      if(new == NULL) return -1;

      new->next = elem;
      new->f_name = name;

      first = new;
      return 1;
    }
  }

  // size > 1
  f_name_node_t *runner = first;
  while(cmp(runner->next->f_name, name) < 0) {
    if(runner->next->next == NULL) { // place after
      f_name_node_t *new = (f_name_node_t *) malloc(sizeof(f_name_node_t));
      if(new == NULL) return -1;

      new->next = NULL;
      new->f_name = name;

      runner->next->next = new;
      return 1;
    }
    runner = runner->next;
  }
  if(cmp(runner->next->f_name, name) == 0) { // do not place
    return 0;
  } else { // place before
    f_name_node_t *new = (f_name_node_t *) malloc(sizeof(f_name_node_t));
    if(new == NULL) return -1;

    new->f_name = name;
    new->next = runner->next;

    runner->next = new;
    return 1;
  }
}

//------------------------------------------------------------

// THREADS----------------------------------------------------

void *read_stdin_thread(void *arg){

  printf("Write fractals with the following format :\n   'name' 'width' 'height' 'a' 'b'\nPress ENTER to confirm.\nType 'q' to exit the standard input.\n");

  //INITIALIZING FRACTAL DATA'S
  char *name = (char *) malloc(sizeof(char)*64);
  int width, height;
  double a, b;
  const char *const_name;

  //WORD BUFFER
  char *wordBuffer = (char *)malloc(sizeof(char) * 64);
  if (wordBuffer == NULL) {
    fprintf(stderr, "read_file_thread : wordBuffer NULL");
    return NULL;
  }

  char line[128];
  char *ch;
  int word = 0, i = 0;

  //Lis ligne par ligne l'entrée standard
  while(1){
    fgets(line, 128, stdin);
    ch = line;
    if(ch[0] == 'q' && ch[1] == '\0') break;
    while(ch[0] != '\0'){
      do{
        wordBuffer[i]=ch[0];
        ch++;
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
      ch++;
    }
    word = 0;
    const_name = name;
    fractal_t *f = fractal_new(const_name, width, height, a, b);

    int insert;
    if ((insert = insert_name(const_name, &compare)) == 1) push(&fb_shared_uncomputed, f);
    else if (insert == 0) fprintf(stderr, "read_file_thread : insert_name() :\n     DOUBLON IGNORE : %s", const_name);
    else{
      fprintf(stderr, "read_file_thread : insert_name()");
      return NULL;
    }
  }

  free(name);
  free(wordBuffer);

  char* str = "FIN DU THREAD DE LECTURE DE L'ENTREE STANDARD";
  pthread_exit((void*)str);
}

void *read_file_thread(void *arg){
  //TEST FILE
  char *filename = (char *) arg;
  if (filename == NULL){
    fprintf(stderr, "read_file_thread : filename NULL");
    return NULL;
  }

  //OPERATIONS ON FILE
  int fo, fr, fc;

  if ((fo = open(filename, O_RDONLY)) < 0){
    fprintf(stderr, "read_file_thread : open()");
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
    fprintf(stderr, "read_file_thread : wordBuffer NULL");
    return NULL;
  }
  char ch[1];

  if ((fr = read(fo, (void *) &ch, sizeof(char))) < 0){
    fprintf(stderr, "read_file_thread : read()");
    return NULL;
  }

  int word = 0, i = 0;

  while(fr > 0){

    //Ignore les lignes de commentaires et les lignes vides consécutives
    while(ch[0] == '#' || ch[0] == '\n'){
      //Ignore les commentaires dans le fichier lu
      if(ch[0] == '#'){
        while(ch[0] != '\n'){
          if ((fr = read(fo, (void *) &ch, sizeof(char))) < 0){
            fprintf(stderr, "read_file_thread : read()");
            return NULL;
          }
        }
        if ((fr = read(fo, (void *) &ch, sizeof(char))) < 0){
          fprintf(stderr, "read_file_thread : read()");
          return NULL;
        }
      }

      //Ignore les lignes vides
      if(ch[0] == '\n'){
        if ((fr = read(fo, (void *) &ch, sizeof(char))) < 0){
          fprintf(stderr, "read_file_thread : read()");
          return NULL;
        }
      }
    }

    //Lis ligne par ligne le fichier
    while(ch[0] != '\n'){
      do{
        wordBuffer[i]=ch[0];
        if ((fr = read(fo, (void *) &ch, sizeof(char))) < 0){
          fprintf(stderr, "read_file_thread : read()");
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
        fprintf(stderr, "read_file_thread : read()");
        return NULL;
      }
    }
    word = 0;
    const_name = name;
    fractal_t *f = fractal_new(const_name, width, height, a, b);

    int insert;
    if ((insert = insert_name(const_name, &compare)) == 1) push(&fb_shared_uncomputed, f);
    else if (insert == 0) fprintf(stderr, "read_file_thread : insert_name() :\n     DOUBLON IGNORE : %s", const_name);
    else{
      fprintf(stderr, "read_file_thread : insert_name()");
      return NULL;
    }
  }

  fc = close(fo);
  if (fc  < 0){
    free(name);
    free(wordBuffer);
    fprintf(stderr, "read_file_thread : close()");
    return NULL;
  }
  free(name);
  free(wordBuffer);

  char* str = "FIN DU THREAD DE LECTURE DU FICHIER";
  pthread_exit((void*)str);
}

void *compute_thread(void *arg){
  char *filename_out = (char *) arg;
  if (filename_out == NULL){
    fprintf(stderr, "final_thread : filename_out NULL");
    return NULL;
  }

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

    char *src = (char *) f.name;
    if (d_arg) write_bitmap_sdl(&f, strcat(src, ".bmp")); // condition : d_arg == 1
    push(&fb_shared_computed, &f);
  }

  char* str = "FIN DU THREAD DE CALCUL";
  pthread_exit((void*)str);
}

void *final_thread(void *arg){
  char *filename_out = (char *) arg;
  if (filename_out == NULL){
    fprintf(stderr, "final_thread : filename_out NULL");
    return NULL;
  }

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
    if (!d_arg) write_bitmap_sdl(&(current.f), strcat(src, ".bmp")); // condition : d_arg == 0
    write_in_file(filename_out, &(current.f));
  }

  char* str = "FIN DU THREAD FINAL";
  pthread_exit((void*)str);
}

//------------------------------------------------------------

// MAIN-------------------------------------------------------

int main(int argc, char **argv){
  int i;
  int n = argc - 2; //number of files
  int m = n; //number of compute_threads

  // lecture des option
  if(strcmp(argv[1],"-d")==0){
    n --;
    d_arg = 1;
    if(strcmp(argv[2],"--maxthreads")==0){
      m = atoi(argv[3]);
      n -= 2;
    }
  }
  else if(strcmp(argv[1],"--maxthreads")==0){
    m = atoi(argv[2]);
    n -= 2;
  }
  if(strcmp(argv[argc-2],"-")==0){
    stdin_arg = 1;
  }

  char *filename_out = argv[argc-1];
  printf("LES REPONSES SERONT ECRITES DANS : %s\n", filename_out);

  char *filenames[n];

  printf("LECTURE DES FICHIERS :\n");
  for(i = 0; i<n-stdin_arg; i++) {
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

  pthread_t read_stdin_pthread;
  pthread_t read_file_threads[n];
  pthread_t compute_threads[n];
  pthread_t final_pthread;

  if (stdin_arg) pthread_create(&read_stdin_pthread, NULL, &read_stdin_thread, NULL);

  for(i = 0; i < n; i++){
    pthread_create(&read_file_threads[i], NULL, &read_file_thread, (void *) (filenames[i]));
  }

  for(i = 0; i < m; i++){
    pthread_create(&compute_threads[i], NULL, &compute_thread, (void *) (filename_out));
  }

  pthread_create(&final_pthread, NULL, &final_thread, (void *) (filename_out));

  // Fin des threads de lecture
  char * str;

  for (i = 0; i < n; i++)
  {
    //attend la fin du thread
    pthread_join(read_file_threads[i], (void**)&str);
    printf("%s\n", str);
  }

  if (stdin_arg){
    pthread_join(read_stdin_pthread, (void**)&str);
    printf("%s\n", str);
  }

  end_read = 0;

  // Fin des threads de calcul
  for (i = 0; i < m; i++)
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

//------------------------------------------------------------
