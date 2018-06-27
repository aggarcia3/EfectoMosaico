/*
 * Licencia: ver fichero LICENSE en directorio base
 * Propósito: implementación del caso de uso de realizar un efecto de mosaico
 */

#include "efecto.h"
#include "infoCPU.h"
#include <pthread.h>
#include <limits.h>
#include <emmintrin.h>

typedef struct {
    void (*copiarBloque) (int i, int j, IplImage* imagenOrigen, int k, int l, IplImage* imagenDestino);
    unsigned int (*compararBloque) (int filaBloque1, int colBloque1, IplImage* img1, int filaBloque2, int colBloque2, IplImage* img2);
} estrategiaEfecto;
typedef struct {
    pthread_t h;
    unsigned int idHilo;
} hilo;

static IplImage* imgsTemp[2];
static estrategiaEfecto est;
static hilo* hilos;

// Con enteros sin signo el compilador puede hacer más optimizaciones: le estamos
// dando a entender que el valor nunca puede ser negativo, no tiene que manejar
// el caso de bit de signo a 1.
// La conversión entre int y unsigned int, de ser necesaria, es sin pérdida de
// información.
// Las filas y columnas se expresan en unidades de píxeles, no en unidades de
// bloques (es decir, el bloque 1 empieza en LADO_BLOQUE, no en 1).
static void* efectoMosaico(void* arg);
static unsigned int compararBloqueSSE2(int filaBloque1, int colBloque1, IplImage* img1, int filaBloque2, int colBloque2, IplImage* img2);
static unsigned int compararBloqueSISD(int filaBloque1, int colBloque1, IplImage* img1, int filaBloque2, int colBloque2, IplImage* img2);
static void copiarBloqueSSE2(int i, int j, IplImage* imagenOrigen, int k, int l, IplImage* imagenDestino);
static void copiarBloqueSISD(int i, int j, IplImage* imagenOrigen, int k, int l, IplImage* imagenDestino);

static void* efectoMosaico(void* arg) {
    unsigned int idHilo = *((unsigned int*) arg);
    unsigned int difActual, menorDiferencia;
    int k, l;
    
    for (int i = idHilo * LADO_BLOQUE; i < imgs[ORIGINAL]->height; i += nHilos * LADO_BLOQUE) {
        for (int j = 0; j < imgs[ORIGINAL]->width; j += LADO_BLOQUE) {
            // Para el bloque (i, j) de la imagen original, hallar el bloque (k, l) más parecido de la imagen patrón
            menorDiferencia = UINT_MAX;
            for (int u = 0; u < imgs[PATRON]->height; u += LADO_BLOQUE) {
                for (int v = 0; v < imgs[PATRON]->width; v += LADO_BLOQUE) {
                    // Si este bloque es más parecido que otro anterior para el bloque (i, j), guardar su referencia
                    difActual = (*est.compararBloque)(i, j, imgs[ORIGINAL], u, v, imgs[PATRON]);
                    if (difActual < menorDiferencia) {
                        k = u;
                        l = v;
                        menorDiferencia = difActual;
                    }
                }
            }
            (*est.copiarBloque)(k, l, imgs[PATRON], i, j, imgs[ORIGINAL]);
        }
    }
}

static unsigned int compararBloqueSSE2(int filaBloque1, int colBloque1, IplImage* img1, int filaBloque2, int colBloque2, IplImage* img2) {
    int toret = 0;

    for (int fila = 0; fila < LADO_BLOQUE; ++fila) {
        __m128i* fila1 = (__m128i*) (img1->imageData + (filaBloque1 + fila) * img1->widthStep + 3 * colBloque1);
        __m128i* fila2 = (__m128i*) (img2->imageData + (filaBloque2 + fila) * img2->widthStep + 3 * colBloque2);

        // En 3 __m128i caben 16 píxeles exactos
        for (int columna = 0; columna < LADO_BLOQUE / 16 * 3; ++columna) {
            __m128i a = _mm_sad_epu8(_mm_load_si128(fila1++), _mm_load_si128(fila2++)); // HI PAD LO
            __m128i b = _mm_srli_si128(a, 8);
            toret += _mm_cvtsi128_si32(_mm_add_epi32(a, b));
        }
    }

    return toret;
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

static void copiarBloqueSSE2(int i, int j, IplImage* imagenOrigen, int k, int l, IplImage* imagenDestino) {
    for (int fila = 0; fila < LADO_BLOQUE; ++fila) {
        __m128i* filaOrigen = (__m128i*) (imagenOrigen->imageData + (i + fila) * imagenOrigen->widthStep + 3 * j);
        __m128i* filaDestino = (__m128i*) (imagenDestino->imageData + (k + fila) * imagenDestino->widthStep + 3 * l);

        // En 3 __m128i caben 16 píxeles exactos de 3 componentes de color
        for (int columna = 0; columna < LADO_BLOQUE / 16 * 3; ++columna) {
            _mm_store_si128(filaDestino++, _mm_load_si128(filaOrigen++));
        }
    }
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
        
        // Comprobar que son imágenes RGB TrueColor de tamaño mayor o igual que LADO_BLOQUExLADO_BLOQUE
        if (imgs[ORIGINAL]->dataOrder == 0 && imgs[PATRON]->dataOrder == imgs[ORIGINAL]->dataOrder
            && imgs[ORIGINAL]->depth == IPL_DEPTH_8U && imgs[PATRON]->depth == imgs[ORIGINAL]->depth
            && imgs[ORIGINAL]->nChannels == 3 && imgs[PATRON]->nChannels == imgs[ORIGINAL]->nChannels
            && imgs[ORIGINAL]->width >= LADO_BLOQUE && imgs[ORIGINAL]->height >= LADO_BLOQUE
            && imgs[PATRON]->width >= LADO_BLOQUE && imgs[PATRON]->height >= LADO_BLOQUE) {
            
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
    
    // Liberar recursos si hubo un error
    if (toret != NO_ERR) {
        if (imgs[ORIGINAL]) {
            cvReleaseImage(&imgs[ORIGINAL]);
        }
        if (imgs[PATRON]) {
            cvReleaseImage(&imgs[PATRON]);
        }
    }
    
    return toret;
}

codigoResultado configurar(tecnicaParalelDatos t) {
    codigoResultado toret = NO_ERR;
    
    hilos = malloc(sizeof(hilo) * nHilos);
    
    if (hilos) {
        switch (t) {
            case SISD: {
                est.compararBloque = &compararBloqueSISD;
                est.copiarBloque = &copiarBloqueSISD;
                break;
            }
            case SSE2: {
                if (soportaSSE2()) {
                    est.compararBloque = &compararBloqueSSE2;
                    est.copiarBloque = &copiarBloqueSSE2;
                } else {
                    toret = ERR_NO_SOPORTADO;
                }
                break;
            }
            default: {
                toret = ERR_ASERCION;
            }
        }
    } else {
        toret = ERR_MEM;
    }
    
    if (toret != NO_ERR && hilos) {
        free(hilos);
    }
    
    return toret;
}

codigoResultado iniciar() {
    codigoResultado toret = NO_ERR;
    
    for (unsigned int i = 0; i < nHilos; ++i) {
        hilos[i].idHilo = i;
        if (pthread_create(&hilos[i].h, NULL, &efectoMosaico, &hilos[i].idHilo)) {
            toret = ERR_HILO;
        }
    }

    for (unsigned int i = 0; i < nHilos; ++i) {
        // Intentamos esperar por todos los hilos, aunque haya ocurrido
        // algún error creando alguno. Los hilos pueden estar usando
        // datos de imagen
        pthread_join(hilos[i].h, NULL);
    }
    
    return toret;
}