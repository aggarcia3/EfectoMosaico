/*
 * Licencia: ver fichero LICENSE en directorio base
 */

#pragma once

#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <opencv/cv.h>
#include <opencv/highgui.h>

typedef enum {
    NO_ERR = EXIT_SUCCESS,
    ERR_MEM,
    ERR_IMGS,
    ERR_HILO,
    ERR_NO_SOPORTADO,
    ERR_ASERCION
} codigoResultado;

typedef enum {
    ORIGINAL = 0,
    PATRON
} rolImagen;

typedef enum {
    SIN_DEFINIR = -1,
    SISD,
    SSE2
} tecnicaParalelDatos;