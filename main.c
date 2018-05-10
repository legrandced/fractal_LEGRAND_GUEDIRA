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

#define MAX_BUFFER_LEN 16 // Taille maximale des buffers producteurs-consommateur

// STRUCTURES===================================================================

// STRUCTURE CONTENANT UN NOM DE FRACTALES ET LE SUIVANT, TRIES ALPHABETIQUEMENT
typedef struct f_name_node{
    const char *f_name;
    struct f_name_node *next;
} f_name_node_t;

// NOEUD CONTENANT UNE FRACTALE ET LA SUIVANTE----------------------------------
typedef struct fractal_node{
    fractal_t *f;
    struct fractal_node *next;
} fractal_node_t;

// BUFFER DE FRACTALES----------------------------------------------------------
// consiste en une liste chainée protégée par 2 sémaphores et un mutex
typedef struct fractal_buffer{
    fractal_node_t *first;  // first node of the linked list
    sem_t full;             // keep track of the number of full spots
    sem_t empty;            // keep track of the number of empty spots
    pthread_mutex_t mutex;  // enforce mutual exclusion to shared data
} fractal_buffer_t;

// =============================================================================

// VARIABLES GLOBALES===========================================================

fractal_buffer_t fb_shared_uncomputed;
fractal_buffer_t fb_shared_computed;

int d_arg = 0;              // 1 si '-d' est présent en argument, 0 sinon
int stdin_arg = 0;          // 1 si '-' est présent en argument, 0 sinon

int end_read = 0;           // 1 si les threads de lecture sont terminés, 0 sinon
int end_compute = 0;        // 1 si les threads de calcul sont terminés, 0 sinon

// premier noeud de la liste de fractal possédant la plus grande valeur moyenne
fractal_node_t *fn_highest_mean_first;

// premier noeud de la liste de noms de fractales
f_name_node_t *fnn_first;

// =============================================================================

// FONCTIONS SUR LES NOEUDS=====================================================

void print_fnhm(fractal_node_t *first){
    if(first == NULL) printf("List vide\n");
    else{
        printf("List = { ");
        fractal_node_t *n = (fractal_node_t *) malloc(sizeof(fractal_node_t));
        memcpy(n, first, sizeof(fractal_node_t));
        while(n != NULL){
            //IMPRIME LA FRACTALE (TEST)==================================================================================================
            printf("(%s %d %d %f %f) ", n->f->name, n->f->width, n->f->height, n->f->a, n->f->b);
            n = n->next;
        }
        printf("}\n");
    }

}

void print_fnn(f_name_node_t *first){
    if(first == NULL) printf("List Names vide\n");
    else{
        printf("Names = { ");
        f_name_node_t *n = (f_name_node_t *) malloc(sizeof(f_name_node_t));
        memcpy(n, first, sizeof(f_name_node_t));
        while(n != NULL){
            //IMPRIME LE NOM==================================================================================================
            printf("%s ", n->f_name);
            n = n->next;
        }
        printf("}\n");
    }

}

void insert(fractal_node_t **fn, fractal_t *f){
    fractal_node_t *new = (fractal_node_t *) malloc(sizeof(fractal_node_t));
    if (new == NULL){
        fprintf(stderr, "insert : new NULL\n");
        return;
    }
    new->f = f;
    if(*fn == NULL){
        new->next = NULL;
        *fn = new;
    }
    else{
        new->next = *fn;
        *fn = new;
    }
    //print_fnhm(*fn);
}

void clear(fractal_node_t **fn){
    fractal_node_t *removed;
    while (*fn != NULL){
        removed = *fn;
        *fn = (*fn)->next;
        free(removed);
    }
    //print_fnhm(*fn);
}

void write_in_file(char *filename, fractal_t *f){
    if (f == NULL || filename == NULL){
        fprintf(stderr, "write_in_file : f || filename NULL\n");
        return;
    }

    int fo, fc, fw;

    if ((fo = open(filename, O_RDWR | O_CREAT | O_APPEND, S_IRUSR | S_IWUSR)) < 0){
        fprintf(stderr, "write_in_file : open()\n");
        return;
    }

    char *str = (char *) malloc(128);
    sprintf(str, "%s %d %d %f %f\n", f->name, f->width, f->height, f->a, f->b);

    // Write in the filename
    if ((fw = write(fo, str, strlen(str))) < 0){ fprintf(stderr, "write_in_file : write() \\n\n"); return;}
    else if (fw == 0){ fprintf(stderr, "write_in_file : write() \\n : nothing to write in file\n"); return;}

    if ((fc = close(fo)) < 0){
        fprintf(stderr, "write_in_file : close()\n");
        return;
    }
}

// =============================================================================

// FONCTIONS SUR BUFFER=========================================================

void push(fractal_buffer_t *fb, fractal_t *f){
    fractal_node_t *new = (fractal_node_t*) malloc(sizeof(fractal_node_t));
    if (new == NULL){
        fprintf(stderr, "push : new NULL\n");
        return;
    }
    new->f = f;
    if(fb->first == NULL){
        new->next = NULL;
        fb->first = new;
    }
    else{
        new->next = fb->first;
        fb->first = new;
    }
    //print_fnhm(fb->first);
}

fractal_t *pop(fractal_buffer_t *fractal_buffer){
    fractal_node_t *removed = fractal_buffer->first;

    fractal_t *f = fractal_buffer->first->f;
    fractal_buffer->first = fractal_buffer->first->next;

    free(removed);
    return f;
}

// =============================================================================

// FONCTIONS SUR LA LISTE DES NOMS DES FRACTALES================================

int compare(const char* a, const char* b) {
    if (strcmp(a, b) == 0) return 0;
    else if (strcmp(a, b) < 0) return -1;
    else return 1;
}

int insert_name(const char* name, int (*cmp)(const char*, const char*)){
    // size == 0
    if(fnn_first == NULL) {
        f_name_node_t *new = (f_name_node_t *) malloc(sizeof(f_name_node_t));
        if(new == NULL) return -1;
        new->f_name = name;
        new->next = NULL;
        fnn_first = new;
        return 1;
    }

    // size == 1 || to be placed as first
    if(fnn_first->next == NULL || cmp(fnn_first->f_name, name) >= 0) {
        if(cmp(fnn_first->f_name, name) < 0) { // place after
            f_name_node_t *new = (f_name_node_t *) malloc(sizeof(f_name_node_t));
            if(new == NULL) return -1;

            new->next = NULL;
            new->f_name = name;

            fnn_first->next = new;
            return 1;
        } else if(cmp(fnn_first->f_name, name) == 0) { // do not place
            return 0;
        } else { // place before
            f_name_node_t *new = (f_name_node_t *) malloc(sizeof(f_name_node_t));
            if(new == NULL) return -1;

            new->next = fnn_first;
            new->f_name = name;

            fnn_first = new;
            return 1;
        }
    }

    // size > 1
    f_name_node_t *runner = fnn_first;
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

// =============================================================================

// THREADS======================================================================

void *read_stdin_thread(){

    printf("Write fractals with the following format :\n   'name' 'width' 'height' 'a' 'b'\nPress ENTER to confirm.\nType 'q' to exit the standard input.\n");

    //INITIALIZING FRACTAL DATA'S
    char *name = (char *) malloc(sizeof(char)*64);
    int width, height;
    double a, b;
    const char *const_name;

    //WORD BUFFER
    char *wordBuffer = (char *)malloc(sizeof(char) * 64);
    if (wordBuffer == NULL) {
        fprintf(stderr, "read_file_thread : wordBuffer NULL\n");
        return NULL;
    }

    char line[128];
    char *ch;
    int word = 0, i = 0;

    //Lis ligne par ligne l'entrée standard until 'q ' is typed
    while(1){
        fgets(line, 128, stdin);
        if(line[0] == 'q' && line[2] == '\0') break; //quit read_stdin
        ch = line;
        while(ch[0] != '\0'){
            do{
                wordBuffer[i]=ch[0];
                ch++;
                i++;
            }while(ch[0] != ' ' && ch[0] != '\0');

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
        if ((insert = insert_name(const_name, &compare)) == 1){
            /* If there are no empty slots, wait */
            sem_wait(&fb_shared_uncomputed.empty);
            /* If another thread uses the buffer, wait */
            pthread_mutex_lock(&fb_shared_uncomputed.mutex);
            push(&fb_shared_uncomputed, f);
            // Release the buffer
            pthread_mutex_unlock(&fb_shared_uncomputed.mutex);
            /* Increment the number of full slots */
            sem_post(&fb_shared_uncomputed.full);
        }
        else if (insert == 0) fprintf(stderr, "read_file_thread : insert_name() :\n     DOUBLON IGNORE : %s\n", const_name);
        else{
            fprintf(stderr, "read_file_thread : insert_name()\n");
            return NULL;
        }
    }

    free(wordBuffer);

    char* str = "FIN DU THREAD DE LECTURE DE L'ENTREE STANDARD\n";
    pthread_exit((void*)str);
}

void *read_file_thread(void *arg){
    //TEST FILE
    char *filename = (char *) arg;
    if (filename == NULL){
        fprintf(stderr, "read_file_thread : filename NULL\n");
        return NULL;
    }

    //OPERATIONS ON FILE
    int fo, fr, fc;

    if ((fo = open(filename, O_RDONLY)) < 0){
        fprintf(stderr, "read_file_thread : open()\n");
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
        fprintf(stderr, "read_file_thread : wordBuffer NULL\n");
        return NULL;
    }
    char ch[1];

    if ((fr = read(fo, (void *) &ch, sizeof(char))) < 0){
        fprintf(stderr, "read_file_thread : read()\n");
        return NULL;
    }

    int word = 0, i = 0;

    while(fr > 0){ // si fr == 0 => fin du fichier
        //Ignore les lignes de commentaires et les lignes vides consécutives
        while(ch[0] == '#' || ch[0] == '\n'){
            //Ignore les commentaires dans le fichier lu
            if(ch[0] == '#'){
                while(ch[0] != '\n'){
                    if ((fr = read(fo, (void *) &ch, sizeof(char))) < 0){
                        fprintf(stderr, "read_file_thread : read()\n");
                        return NULL;
                    }
                }
                if ((fr = read(fo, (void *) &ch, sizeof(char))) < 0){
                    fprintf(stderr, "read_file_thread : read()\n");
                    return NULL;
                }
            }

            //Ignore les lignes vides
            if(ch[0] == '\n'){
                if ((fr = read(fo, (void *) &ch, sizeof(char))) < 0){
                    fprintf(stderr, "read_file_thread : read()\n");
                    return NULL;
                }
            }
        }

        //Lis ligne par ligne le fichier
        while(fr > 0){
            do{
                wordBuffer[i]=ch[0];
                if ((fr = read(fo, (void *) &ch, sizeof(char))) < 0){
                    fprintf(stderr, "read_file_thread : read()\n");
                    return NULL;
                }
                i++;
            }while(ch[0] != ' ' && ch[0] != '\n');
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
            if(ch[0] == '\n') { //A REPENSER
                if ((fr = read(fo, (void  *) &ch, sizeof(char))) < 0){
                    fprintf(stderr, "read_file_thread : read()\n");
                    return NULL;
                }
                break;
            }
            if ((fr = read(fo, (void  *) &ch, sizeof(char))) < 0){
                fprintf(stderr, "read_file_thread : read()\n");
                return NULL;
            }
        }

        word = 0;
        const_name = name;
        fractal_t *f = fractal_new(const_name, width, height, a, b);

        int insert;
        if ((insert = insert_name(f->name, &compare)) == 1){
            // If there are no empty slots, wait
            sem_wait(&fb_shared_uncomputed.empty);
            // If another thread uses the buffer, wait
            pthread_mutex_lock(&fb_shared_uncomputed.mutex);
            push(&fb_shared_uncomputed, f);
            // Release the buffer
            pthread_mutex_unlock(&fb_shared_uncomputed.mutex);
            // Increment the number of full slots
            sem_post(&fb_shared_uncomputed.full);
        }
        else if (insert == 0) fprintf(stderr, "read_file_thread : insert_name() :\n     DOUBLON IGNORE : %s\n", const_name);
        else{
            fprintf(stderr, "read_file_thread : insert_name()\n");
            return NULL;
        }
        //print_fnn(fnn_first);
    }

    fc = close(fo);
    if (fc  < 0){
        free(wordBuffer);
        fprintf(stderr, "read_file_thread : close()\n");
        return NULL;
    }
    free(wordBuffer);

    char* str = "FIN DU THREAD DE LECTURE DU FICHIER\n";
    pthread_exit((void*)str);
}

void *compute_thread(void *arg){
    char *filename_out = (char *) arg;
    if (filename_out == NULL){
        fprintf(stderr, "final_thread : filename_out NULL\n");
        return NULL;
    }

    while(1)
    {

        //PARTIE CONSOMMATEUR---------------------------------------------------
        // If there are no full slots, wait
        sem_wait(&fb_shared_uncomputed.full);
        if(end_read) if(fb_shared_uncomputed.first == NULL) break; //les producteurs on fini de produire et le buffer est vide
        // If another thread uses the buffer, wait
        pthread_mutex_lock(&fb_shared_uncomputed.mutex);
        fractal_t *f = pop(&fb_shared_uncomputed);
        // Release the buffer
        pthread_mutex_unlock(&fb_shared_uncomputed.mutex);
        // Increment the number of empty slots
        sem_post(&fb_shared_uncomputed.empty);

        //CALCULS---------------------------------------------------------------
        // Calcule les value[][] des fractales et les additionne en vue de
        // calculer la moyenne
        for (int i = 0 ; i < fractal_get_width(f) ; i++){
            for (int j = 0 ; j < fractal_get_height(f) ; j++){
                fractal_compute_value(f, i, j);
                f->mean += (long double) fractal_get_value(f, i, j);
            }
        }
        // Ecriture du fichier bitmap si -d est présent.
        char *bmp_name = (char *) malloc(64);
        strcpy(bmp_name, f->name);
        strcat(bmp_name, ".bmp");
        if (d_arg) write_bitmap_sdl(f, bmp_name); // condition : d_arg == 1


        //PARTIE PRODUCTEUR-----------------------------------------------------
        // If there are no empty slots, wait
        sem_wait(&fb_shared_computed.empty);
        // If another thread uses the buffer, wait
        pthread_mutex_lock(&fb_shared_computed.mutex);
        push(&fb_shared_computed, f);
        // Release the buffer
        pthread_mutex_unlock(&fb_shared_computed.mutex);
        // Increment the number of full slots
        sem_post(&fb_shared_computed.full);
    }

    char* str = "FIN DU THREAD DE CALCUL\n";
    pthread_exit((void*)str);
}

void *final_thread(void *arg){
    char *filename_out = (char *) arg;
    if (filename_out == NULL){
        fprintf(stderr, "final_thread : filename_out NULL\n");
        return NULL;
    }

    while(1){
        // If there are no full slots, wait
        sem_wait(&fb_shared_computed.full);
        if(end_compute) if(fb_shared_computed.first == NULL) break; //les producteurs on fini de produire et le buffer est vide
        // If another thread uses the buffer, wait
        pthread_mutex_lock(&fb_shared_computed.mutex);
        fractal_t *f = pop(&fb_shared_computed);
        // Release the buffer
        pthread_mutex_unlock(&fb_shared_computed.mutex);
        // Increment the number of empty slots
        sem_post(&fb_shared_computed.empty);

        f->mean /= f->width * f->height; // last step of computing mean

        if(f->mean > fn_highest_mean_first->f->mean){
            clear(&fn_highest_mean_first);
            insert(&fn_highest_mean_first, f);
            //print_fnhm(fn_highest_mean_first);

        }
        else if (f->mean == fn_highest_mean_first->f->mean){
            insert(&fn_highest_mean_first, f);
        }
    }

    fractal_node_t *current = fn_highest_mean_first;
    char *bmp_name = (char *) malloc(64);
    while (current != NULL){
        strcpy(bmp_name, current->f->name);
        strcat(bmp_name, ".bmp");
        if (!d_arg) write_bitmap_sdl(current->f, bmp_name); // condition : d_arg == 0
        write_in_file(filename_out, current->f);
        current = current->next;
    }

    char* str = "FIN DU THREAD FINAL\n";
    pthread_exit((void*)str);
}

// =============================================================================

// MAIN=========================================================================

int main(int argc, char **argv){
    int i;
    int n = argc - 2; //number of files aka number of read_file_thread
    int m = n; //number of compute_threads, default value is n

    // LECTURE DE LA LIGNE DE COMMANDE------------------------------------------

    if(strcmp(argv[1],"-d")==0){
        n --;
        m --;
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
        n--;
    }

    char *filename_out = argv[argc-1];
    printf("LES REPONSES SERONT ECRITES DANS : %s\n", filename_out);

    char *filenames[n];

    printf("LECTURE DES FICHIERS :\n");
    for(i = 0; i<n; i++) {
        filenames[i] = argv[argc-n-stdin_arg-1+i];
        printf("%s\n", filenames[i]);
    }

    //--------------------------------------------------------------------------

    //Initialisation de la liste des fractales possédant la plus grande valeur moyenne
    fn_highest_mean_first = (fractal_node_t*) malloc(sizeof(fractal_node_t));
    fn_highest_mean_first->next = NULL;
    fn_highest_mean_first->f = fractal_new("null", 0, 0, 0.0, 0.0);

    // Initialisation de first, des sémaphores et du mutex
    fb_shared_uncomputed.first = NULL;
    sem_init(&fb_shared_uncomputed.full, 0, 0);
    sem_init(&fb_shared_uncomputed.empty, 0, MAX_BUFFER_LEN);
    pthread_mutex_init(&fb_shared_uncomputed.mutex, NULL);
    // Initialisation des sémaphores et du mutex
    fb_shared_computed.first = NULL;
    sem_init(&fb_shared_computed.full, 0, 0);
    sem_init(&fb_shared_computed.empty, 0, MAX_BUFFER_LEN);
    pthread_mutex_init(&fb_shared_computed.mutex, NULL);

    printf("LANCEMENT DES THREADS\n");

    pthread_t read_stdin_pthread;
    pthread_t read_file_threads[n];
    pthread_t compute_threads[m];
    pthread_t final_pthread;

    // THREADS CREATE-----------------------------------------------------------

    if (stdin_arg) pthread_create(&read_stdin_pthread, NULL, &read_stdin_thread, NULL);

    if (n > 0){
        for(i = 0; i < n; i++){
            pthread_create(&read_file_threads[i], NULL, &read_file_thread, (void *) (filenames[i]));
        }
    }

    for(i = 0; i < m; i++){
        pthread_create(&compute_threads[i], NULL, &compute_thread, (void *) (filename_out));
    }

    pthread_create(&final_pthread, NULL, &final_thread, (void *) (filename_out));

    // THREADS JOIN-------------------------------------------------------------

    char * str;

    // Fin des threads de lecture
    for (i = 0; i < n; i++){
        //attend la fin du thread
        pthread_join(read_file_threads[i], (void**)&str);
        printf("%s\n", str);
    }
    if (stdin_arg){
        pthread_join(read_stdin_pthread, (void**)&str);
        printf("%s\n", str);
    }
    printf("LA LECTURE DES FICHIERS/STDIN EST TERMINEE\n");

    end_read = 1;

    //envoyer autant de post qu'il y a de consommateurs, pour qu'ils n'aient plus a attendre inutilement
    for (i = 0; i < m; i++) {
        sem_post(&fb_shared_uncomputed.full);
    }

    // Fin des threads de calcul

    for (i = 0; i < m; i++){
        //attend la fin du thread
        pthread_join(compute_threads[i], (void**)&str);
        printf("%s\n", str);
    }
    printf("LES CALCULS SONT TERMINES\n");
    end_compute = 1;

    //envoyer autant de post qu'il y a de consommateurs, pour qu'ils n'aient plus a attendre inutilement
    sem_post(&fb_shared_computed.full);

    // Fin du thread final
    pthread_join(final_pthread, (void**)&str);
    printf("%s\n", str);

}

// =============================================================================
