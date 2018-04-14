#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <math.h>
#include <fcntl.h>
#include <errno.h>
#include <CUnit/CUnit.h>
#include "CUnit/Basic.h"

void write_in_file(char *filename, char *str){
    if (str == NULL || filename == NULL){
        fprintf(stderr, "write_in_file : f || filename NULL\n");
        return;
    }

    int fo, fc, fw;

    if ((fo = open(filename, O_RDWR | O_CREAT | O_APPEND, S_IRUSR | S_IWUSR)) < 0){
        fprintf(stderr, "write_in_file : open()\n");
        return;
    }

    // Write in the filename
    if ((fw = write(fo, str, strlen(str))) < 0){ fprintf(stderr, "write_in_file : write() str\n"); return;}
    else if (fw == 0){ fprintf(stderr, "write_in_file : write() str : nothing to write in file\n"); return;}

    if ((fc = close(fo)) < 0){
        fprintf(stderr, "write_in_file : close()\n");
        return;
    }
}

char *read_from_file(char *filename){
  if (filename == NULL){
      fprintf(stderr, "write_in_file : f || filename NULL\n");
      return NULL;
  }

  int fo, fc, fr;

  if ((fo = open(filename, O_RDWR | O_CREAT | O_APPEND, S_IRUSR | S_IWUSR)) < 0){
      fprintf(stderr, "write_in_file : open()\n");
      return NULL;
  }

  char ch[1];
  char *str = (char *) malloc(256);
  // Read from the file
  if ((fr = read(fo, (void *) &ch, sizeof(char))) < 0){
      fprintf(stderr, "read_file_thread : read()\n");
      return NULL;
  }
  while (fr > 0) {
    strcat(str, ch);

    if ((fr = read(fo, (void *) &ch, sizeof(char))) < 0){
        fprintf(stderr, "read_file_thread : read()\n");
        return NULL;
    }
  }

  if ((fc = close(fo)) < 0){
      fprintf(stderr, "write_in_file : close()\n");
      return NULL;
  }

  return str;
}

int setup(void){
  // Suppression des fichiers avant de les créer
  char *filename = (char *) malloc(64);
  for(int k = 0; k < 156; k++){
    if (k < 6) sprintf(filename, "fichier%d.txt", k);
    else if (k == 6) sprintf(filename, "fichierOut.txt");
    else sprintf(filename, "fract%d.bmp", k);
    if(access(filename, F_OK ) != -1){
      char *str = (char *) malloc(64);
      strcpy(str, "rm ");
      strcat(str, filename);
      system(str);
      free(str);
    }
  }

  int i, width = 500, height = 500;
  double a = 0.0, b = 0.0, theta = 0.0, r = 0.0;

  for (int j = 0; j < 3; j++){
    char *filename = (char *) malloc(64);
    sprintf(filename, "fichier%d.txt", j);

    for (i = 0; i < 50 ; i++){
      char *str = (char *) malloc(128);
      sprintf(str, "fract%d %d %d %f %f\n", 50*j+i, width, height, a, b);

      write_in_file(filename, str);

      theta += M_PI/8;
      r += 0.015;
      a = r*cos(theta);
      b = r*sin(theta);
    }
  }

  write_in_file("fichier3.txt", "fractA 500 500 -0.75 0.03\n");
  write_in_file("fichier3.txt", "fractA 500 500 -0.7 0.3\n");
  write_in_file("fichier4.txt", "fractB 500 500 -0.75 0.03\n");
  write_in_file("fichier4.txt", "fractC 500 500 -0.75 0.03\n");

  return 0;
}

void run_test_2(void){
  int result = system("../main fichier3.txt fichier4.txt fichierOut.txt");
  CU_ASSERT_NOT_EQUAL(-1, result);

  int eq;
  char *str = read_from_file("fichierOut.txt");
  if (strstr(str, "fractA") != NULL) eq = 1; else eq = -1;
  CU_ASSERT_EQUAL(1, eq);

  if (strstr(str, "fractB") != NULL) eq = 1; else eq = -1;
  CU_ASSERT_EQUAL(1, eq);

  if (strstr(str, "fractC") != NULL) eq = 1; else eq = -1;
  CU_ASSERT_EQUAL(1, eq);
}

int main(){
  CU_pSuite pSuite = NULL;

  if (CUE_SUCCESS != CU_initialize_registry())
  return CU_get_error();

  pSuite = CU_add_suite("Test_2", setup, NULL);
  if (NULL == pSuite) {
    CU_cleanup_registry();
    return CU_get_error();
  }

  if ((NULL == CU_add_test(pSuite, "run_test_2", run_test_2)))
  {
    CU_cleanup_registry();
    return CU_get_error();
  }

  CU_basic_set_mode(CU_BRM_VERBOSE);
  CU_basic_run_tests();
  CU_basic_show_failures(CU_get_failure_list());
  CU_cleanup_registry();
  printf("\n");
  return CU_get_error();
}
