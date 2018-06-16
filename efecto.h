/*
 * Licencia: ver fichero LICENSE en directorio base
 */

#pragma once

#include "aplicacion.h"
#include <pthread.h>
#include <opencv/cv.h>
#include <opencv/highgui.h>

typedef enum {
    ORIGINAL,
    PATRON
} rolImagen;
typedef struct {
    unsigned int nHilos;
    void (*estCopiarBloque) (int i, int j, IplImage* imagenOrigen, int k, int l, IplImage* imagenDestino);
    unsigned int (*estCompararBloque) (int filaBloque1, int colBloque1, IplImage* img1, int filaBloque2, int colBloque2, IplImage* img2);
} paramEfecto;
typedef struct {
    pthread_t h;
    unsigned int idHilo;
} hilo;

#define LADO_BLOQUE 16

IplImage* imgs[2];
extern paramEfecto p;
hilo* hilos;

codigoResultado prepararImgs(char* rutasImg[]);
void concretarEstrategia(int usarSIMD);
codigoResultado iniciar();