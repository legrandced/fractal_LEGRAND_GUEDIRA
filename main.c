#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <errno.h>
#include "fractal.h"

void *read_file(void *param){
  //TEST FILE
  char *filename = (char *) param;
  if (filename == NULL){
    fprintf(stderr, "read_file : filename NULL");
    return -1;
  }

  //OPERATIONS ON FILE
  int fo, fr, fc;

  if ((fo = open(filename, O_RDONLY)) < 0){
    fprintf(stderr, "read_file : open()");
    return -1;
  }

  //INITIALIZING FRACTAL DATA'S
  char *name = (char *) malloc(sizeof(char)*64);
  int width, height;
  double a, b;

  //WORD BUFFER
  char *wordBuffer = (char *)malloc(sizeof(char) * 64);
  if (lineBuffer == NULL) {
    //error
  }
  char ch;

  if ((fr = read(fo, (void *) &ch, sizeof(char))) < 0){
    fprintf(stderr, "read_file : read()");
    return -1;
  }

  int word = 0, i = 0;
  //int line = 0;
  while(fr == 0){
    while(ch != '\n'){
      do{
        wordBuffer[i]=ch;
        if ((fr = read(fo, (void *) &ch, sizeof(char))) < 0){
          fprintf(stderr, "read_file : read()");
          return -1;
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
        return -1;
      }
    }
    word = 0;
    //line++;
  }
  const char *const_name = name;
  fractal_t *f = fractal_new(const_name, width, height, a, b);

  return (void *) f;
  }

  fc = close(fo);
  if (fc  < 0){
    free(name);
    free(wordBuffer);
    fprintf(stderr, "read_file : close");
    return -1;
  }
  free(name);
  free(wordBuffer);
  return 0;
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
  p_thread *read_file_threads = (p_thread *) malloc(sizeof(p_thread)*n);
  for(i = 0; i<n; i++){
    p_thread_create(&read_file_threads[i], NULL, &read_file, (void *) &(filenames[i]));
  }


    return 0;
}
