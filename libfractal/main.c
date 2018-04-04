#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <string.h>
#include <pthread.h>
#include <errno.h>
#include "fractal.h"

#define MAX_BUFFER_LEN 16

//NOEUD CONTENANT UNE FRACTALE ET LA SUIVANTE
typedef struct fractal_node{
  fractal_t f;
  struct fractal_node *next;
} fractal_node_t;

//LISTE CHAINEE DE FRACTALES DE LONGUEUR len
typedef struct fractal_buffer{
  fractal_node_t *first;
  int len;
} fractal_buffer_t;


void push(fractal_buffer_t *buffer, fractal_t *f){
  fractal_node_t *new = (fractal_node_t*) malloc(sizeof(fractal_node_t));
  if (new == NULL){
    fprintf(stderr, "push : new NULL");
    return;
  }
  new->f = *f;
  new->next = buffer->first;
  buffer->first = new;
  buffer->len ++;
}


fractal_t pop(fractal_buffer_t *buffer){
  fractal_node_t *removed = buffer->first;
  fractal_t f = buffer->first->f;
  buffer->first = buffer->first->next;
  free(removed);
  buffer->len --;
  return f;
}


void *read_file(void *param){
  //TEST FILE
  char *filename = (char *) param;
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

  //FRACTAL BUFFER
  fractal_buffer_t *buffer = (fractal_buffer_t*) malloc(sizeof(fractal_buffer_t));

  //WORD BUFFER
  char *wordBuffer = (char *)malloc(sizeof(char) * 64);
  if (wordBuffer == NULL) {
    fprintf(stderr, "read_file : wordBuffer NULL");
    return NULL;
  }
  char ch;

  if ((fr = read(fo, (void *) &ch, sizeof(char))) < 0){
    fprintf(stderr, "read_file : read()");
    return NULL;
  }

  int word = 0, i = 0;
  //int line = 0;
  while(fr == 0){
    while(ch != '\n'){
      do{
        wordBuffer[i]=ch;
        if ((fr = read(fo, (void *) &ch, sizeof(char))) < 0){
          fprintf(stderr, "read_file : read()");
          return NULL;
        }
        i++;
      }while(ch != ' ');
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
    //line++;

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

  const char *const_name = name;
  fractal_t *f = fractal_new(const_name, width, height, a, b);
  return (void *) f;
}



int main(int argc, char **argv)
{
  int i;
  int n = argc - 1; //number of files
  if(argv[1] == "-d") n --;
  char *filenames[n];
  for(i = 0; i<n; i++){
    filenames[i] = argv[argc-n+i];
  }
  pthread_t *read_file_threads = (pthread_t *) malloc(sizeof(pthread_t)*n);
  for(i = 0; i<n; i++){
    pthread_create(&read_file_threads[i], NULL, &read_file, (void *) &(filenames[i]));
  }


    return 0;
}
