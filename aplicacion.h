/*
 * Licencia: ver fichero LICENSE en directorio base
 */

#pragma once

#include <stdlib.h>

typedef enum {
    NO_ERR = EXIT_SUCCESS,
    ERR_MEM,
    ERR_IMGS,
    ERR_HILO
} codigoResultado;

// Este array debe tener tantas entradas como número de valores que puede tomar el enumerado codigoResultado
extern const char* mensajesError[];