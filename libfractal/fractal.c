#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "fractal.h"

struct fractal *fractal_new(const char *name, int width, int height, double a, double b)
{
    // memory allocation of the fractal structure
    fractal_t *f = (fractal_t*) malloc(sizeof(fractal_t));
    if (f == NULL){
        fprintf(stderr, "malloc fractal_t failed\n");
        return NULL;
    }

    // memory allocation of value matrix
    int *value = (int *) malloc(sizeof(int)*width*height);
    if (value == NULL){
        fprintf(stderr, "malloc value failed\n");
        return NULL;
    }
    f->value = (int **) malloc(sizeof(int *) * width);
    f->value[0] = value;
    for (int i=1 ; i<width ; i++) f->value[i] =f->value[i-1] + width;

    // memory allocation of name
    const char *const_name = (const char *) malloc(64);

    // assignement
    strcpy((char *) const_name, name);
    f->name = const_name;
    f->width = width;
    f->height = height;
    f->a = a;
    f->b = b;

    return f;
}

void fractal_free(struct fractal *f)
{
    if (f == NULL){
        fprintf(stderr, "fractal_free : fractal undefined\n");
    }
    else{
        free((char *)f->name);
        free(f->value);
        free(f);
    }
}

const char *fractal_get_name(const struct fractal *f)
{
    if (f == NULL){
        fprintf(stderr, "fractal_get_name : fractal undefined\n");
        return NULL;
    }
    else{
        return f->name;
    }
}

int fractal_get_value(const struct fractal *f, int x, int y)
{
    if (f == NULL){
        fprintf(stderr, "fractal_get_value : fractal undefined\n");
        return -1;
    }
    else{
        return f->value[x][y];
    }
}

void fractal_set_value(struct fractal *f, int x, int y, int val)
{
    if (f == NULL){
        fprintf(stderr, "fractal_set_value : fractal undefined\n");
    }
    else{
        f->value[x][y] = val;
    }
}

int fractal_get_width(const struct fractal *f)
{
    if (f == NULL){
        fprintf(stderr, "fractal_get_width : fractal undefined\n");
        return -1;
    }
    else{
        return f->width;
    }
}

int fractal_get_height(const struct fractal *f)
{
    if (f == NULL){
        fprintf(stderr, "fractal_get_height : fractal undefined\n");
        return -1;
    }
    else{
        return f->height;
    }
}

double fractal_get_a(const struct fractal *f)
{
    if (f == NULL){
        fprintf(stderr, "fractal_get_a : fractal undefined\n");
        return -1;
    }
    else{
        return f->a;
    }
}

double fractal_get_b(const struct fractal *f)
{
    if (f == NULL){
        fprintf(stderr, "fractal_get_b : fractal undefined\n");
        return -1;
    }
    else{
        return f->b;
    }
}
