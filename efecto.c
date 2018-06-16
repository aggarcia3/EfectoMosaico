/*
 * Licencia: ver fichero LICENSE en directorio base
 * Propósito: implementación del caso de uso de realizar un efecto de mosaico. Lógica del modelo de dominio
 */

#include "efecto.h"
#include <limits.h>

IplImage* imgsTemp[2];
paramEfecto p = { 0 };

// Con enteros sin signo el compilador puede hacer más optimizaciones: le estamos
// dando a entender que el valor nunca puede ser negativo, no tiene que manejar
// el caso de bit de signo a 1.
// La conversión entre int y unsigned int, de ser necesaria, es sin pérdida de
// información.
// Las filas y columnas se expresan en unidades de píxeles, no en unidades de
// bloques (es decir, el bloque 1 empieza en LADO_BLOQUE, no en 1).
static void* efectoMosaico(void* arg);
static unsigned int compararBloqueSIMD(int filaBloque1, int colBloque1, IplImage* img1, int filaBloque2, int colBloque2, IplImage* img2);
static unsigned int compararBloqueSISD(int filaBloque1, int colBloque1, IplImage* img1, int filaBloque2, int colBloque2, IplImage* img2);
static void copiarBloqueSIMD(int i, int j, IplImage* imagenOrigen, int k, int l, IplImage* imagenDestino);
static void copiarBloqueSISD(int i, int j, IplImage* imagenOrigen, int k, int l, IplImage* imagenDestino);

static void* efectoMosaico(void* arg) {
    unsigned int idHilo = *((unsigned int*) arg);
    unsigned int difActual, menorDiferencia;
    int k, l;
    
    for (int i = idHilo * LADO_BLOQUE; i < imgs[ORIGINAL]->height; i += p.nHilos * LADO_BLOQUE) {
        for (int j = 0; j < imgs[ORIGINAL]->width; j += LADO_BLOQUE) {
            // Para el bloque (i, j) de la imagen original, hallar el bloque (k, l) más parecido de la imagen patrón
            menorDiferencia = UINT_MAX;
            for (int u = 0; u < imgs[PATRON]->height; u += LADO_BLOQUE) {
                for (int v = 0; v < imgs[PATRON]->width; v += LADO_BLOQUE) {
                    // Si este bloque es más parecido que otro anterior para el bloque (i, j), guardar su referencia
                    difActual = (*p.estCompararBloque)(i, j, imgs[ORIGINAL], u, v, imgs[PATRON]);
                    if (difActual < menorDiferencia) {
                        k = u;
                        l = v;
                        menorDiferencia = difActual;
                    }
                }
            }
            (*p.estCopiarBloque)(k, l, imgs[PATRON], i, j, imgs[ORIGINAL]);
        }
    }
}

static unsigned int compararBloqueSIMD(int filaBloque1, int colBloque1, IplImage* img1, int filaBloque2, int colBloque2, IplImage* img2) {
    // TODO
    return 0;
}

static unsigned int compararBloqueSISD(int filaBloque1, int colBloque1, IplImage* img1, int filaBloque2, int colBloque2, IplImage* img2) {
    int toret = 0;
    
    for (int fila = 0; fila < LADO_BLOQUE; ++fila) {
        unsigned char* cc1 = (unsigned char*) (img1->imageData + (filaBloque1 + fila) * img1->widthStep + 3 * colBloque1);
        unsigned char* cc2 = (unsigned char*) (img2->imageData + (filaBloque2 + fila) * img2->widthStep + 3 * colBloque2);

        for (int columna = 0; columna < LADO_BLOQUE; ++columna) {
            // En GCC, usar abs genera código bastante más eficiente que un if
            // manual, sin diferentes ramas de ejecución posibles, usando máscaras
            toret += abs(*cc1++ - *cc2++);
            toret += abs(*cc1++ - *cc2++);
            toret += abs(*cc1++ - *cc2++);
        }
    }
    
    return toret;
}

static void copiarBloqueSIMD(int i, int j, IplImage* imagenOrigen, int k, int l, IplImage* imagenDestino) {
    // TODO
}

static void copiarBloqueSISD(int i, int j, IplImage* imagenOrigen, int k, int l, IplImage* imagenDestino) {
    for (int fila = 0; fila < LADO_BLOQUE; ++fila) {
        char* filaOrigen = imagenOrigen->imageData + (i + fila) * imagenOrigen->widthStep + 3 * j;
        char* filaDestino = imagenDestino->imageData + (k + fila) * imagenDestino->widthStep + 3 * l;
        
        for (int columna = 0; columna < LADO_BLOQUE; ++columna) {
            // Copiar cada canal
            *filaDestino++ = *filaOrigen++;
            *filaDestino++ = *filaOrigen++;
            *filaDestino++ = *filaOrigen++;
        }
    }
}

codigoResultado prepararImgs(char* rutasImg[]) {
    IplImage* imgMenor;
    codigoResultado toret = NO_ERR;
    
    // Cargar las dos imágenes
    imgs[ORIGINAL] = cvLoadImage(rutasImg[ORIGINAL], CV_LOAD_IMAGE_COLOR);
    imgs[PATRON] = cvLoadImage(rutasImg[PATRON], CV_LOAD_IMAGE_COLOR);
    
    if (imgs[ORIGINAL] && imgs[PATRON]) {
        
        // Comprobar que son imágenes RGB TrueColor
        if (imgs[ORIGINAL]->dataOrder == 0 && imgs[PATRON]->dataOrder == imgs[ORIGINAL]->dataOrder
            && imgs[ORIGINAL]->depth == IPL_DEPTH_8U && imgs[PATRON]->depth == imgs[ORIGINAL]->depth
            && imgs[ORIGINAL]->nChannels == 3 && imgs[PATRON]->nChannels == imgs[ORIGINAL]->nChannels) {
            
            // Armonizar su ancho y alto a un tamaño múltiplo del tamaño de bloque común, si es necesario
            // (Redondea dimensiones a múltiplo más cercano por defecto al tamaño del bloque)
            if (imgs[ORIGINAL]->width != imgs[PATRON]->width || imgs[ORIGINAL]->height != imgs[PATRON]->height
                || imgs[ORIGINAL]->width % LADO_BLOQUE != 0 || imgs[ORIGINAL]->height % LADO_BLOQUE != 0) {
                
                imgMenor = imgs[ORIGINAL]->width * imgs[ORIGINAL]->height > imgs[PATRON]->width * imgs[PATRON]->height ? imgs[PATRON] : imgs[ORIGINAL];
                imgsTemp[ORIGINAL] = cvCreateImage(cvSize((imgMenor->width / LADO_BLOQUE) * LADO_BLOQUE, (imgMenor->height / LADO_BLOQUE) * LADO_BLOQUE), IPL_DEPTH_8U, 3);
                imgsTemp[PATRON] = cvCreateImage(cvSize((imgMenor->width / LADO_BLOQUE) * LADO_BLOQUE, (imgMenor->height / LADO_BLOQUE) * LADO_BLOQUE), IPL_DEPTH_8U, 3);

                if (imgsTemp[ORIGINAL] && imgsTemp[PATRON]) {
                    cvResize(imgs[ORIGINAL], imgsTemp[ORIGINAL], CV_INTER_LINEAR);
                    cvResize(imgs[PATRON], imgsTemp[PATRON], CV_INTER_LINEAR);

                    cvReleaseImage(&imgs[ORIGINAL]);
                    cvReleaseImage(&imgs[PATRON]);
                    imgs[ORIGINAL] = imgsTemp[ORIGINAL];
                    imgs[PATRON] = imgsTemp[PATRON];
                } else {
                    toret = ERR_MEM;
                }
            }
        } else {
            toret = ERR_IMGS;
        }
    } else {
        toret = ERR_IMGS;
    }
    
    return toret;
}

void concretarEstrategia(int usarSIMD) {
    if (usarSIMD) {
        p.estCompararBloque = &compararBloqueSIMD;
        p.estCopiarBloque = &copiarBloqueSIMD;
    } else {
        p.estCompararBloque = &compararBloqueSISD;
        p.estCopiarBloque = &copiarBloqueSISD;
    }
}

codigoResultado iniciar() {
    codigoResultado toret = NO_ERR;
    
    for (unsigned int i = 0; i < p.nHilos; ++i) {
        hilos[i].idHilo = i;
        if (pthread_create(&hilos[i].h, NULL, &efectoMosaico, &hilos[i].idHilo)) {
            toret = ERR_HILO;
        }
    }

    for (unsigned int i = 0; i < p.nHilos; ++i) {
        // Intentamos esperar por todos los hilos, aunque haya ocurrido
        // algún error creando alguno. Los hilos pueden estar usando
        // datos de imagen
        pthread_join(hilos[i].h, NULL);
    }
    
    return toret;
}