/*
 * Licencia: ver fichero LICENSE en directorio base
 */

#pragma once

#include "aplicacion.h"

#define LADO_BLOQUE 16

IplImage* imgs[2];
unsigned int nHilos;

codigoResultado prepararImgs(char* rutasImg[]);
codigoResultado configurar(tecnicaParalelDatos t);
codigoResultado iniciar();