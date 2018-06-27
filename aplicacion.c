/*
 * Licencia: ver fichero LICENSE en directorio base
 * Propósito: vista y controlador del efecto
 */

#include "aplicacion.h"
#include "efecto.h"
#include <ctype.h>

#define NOMBRE_VENTANA "Efecto mosaico"
#define TAM_BUFER_LECTURA 16

// Cadenas que el usuario tendrá que introducir para seleccionar la técnica de
// paralelismo de datos correspondiente. Deben de estar en mayúsculas
#define CADENA_SISD "SISD"
#define CADENA_SSE2 "SSE2"
// Importante que la posición de las cadenas en el array coincida con el orden
// nominal de tecnicaParalelDatos, y tenga su mismo número de elementos menos 1
static const char cadenasTecParalel[2][5] = {   // [Número de técnicas (opcional)][Longitud máxima (incluyendo \0)]
    CADENA_SISD "\0",
    CADENA_SSE2 "\0"
};

// Este array debe tener tantas entradas como número de valores que puede tomar el enumerado codigoResultado
const char* mensajesError[] = {
    "\0",   /* NO_ERR */
    "Error: no se ha podido reservar memoria necesaria para la ejecución de la aplicación.\0",  /* ERR_MEM */
    "Error: no se han podido cargar las imágenes. Comprueba que la ruta de acceso es válida y son RGB TrueColor.\0",    /* ERR_IMGS */
    "Error: no se ha podido crear algún hilo de ejecución para el efecto.\0",   /* ERR_HILOS */
    "Error: la CPU no soporta la técnica de paralelismo de datos elegida.\0",   /* ERR_NO_SOPORTADO */
    "Error: se ha violado una aserción de la aplicación en tiempo de ejecución.\0"  /* ERR_ASERCION */
};
static codigoResultado pedirOpciones();
static tecnicaParalelDatos leerTecnicaParalel(char buf[]);
static char* obtenerCadenaTecnicasParalel();

int main(int argc, char** argv) {
    char* rutasImg[2];
    codigoResultado res = NO_ERR;
    struct timespec tCPUInicio, tCPUFin;
    struct timespec tInicio, tFin;
    double tiempoCPU;
    
    if (argc >= 3) {
        // Coger rutas de los parámetros del programa
        rutasImg[ORIGINAL] = argv[1];
        rutasImg[PATRON] = argv[2];
        res = prepararImgs(rutasImg);
        
        if (res == NO_ERR) {
            // Pedir opciones hasta que el usuario introduzca unas posibles
            res = pedirOpciones();
            while (res != NO_ERR) {
                fprintf(stderr, mensajesError[res]);
                fprintf(stderr, "\n");
                res = pedirOpciones();
            }
            
            // Mostrar imágenes de entrada
            cvShowImage("Imagen original", imgs[ORIGINAL]);
            cvShowImage("Imagen patr.", imgs[PATRON]);
            cvWaitKey(1000);

            // Ir creando ventana de resultados
            cvNamedWindow(NOMBRE_VENTANA, CV_WINDOW_AUTOSIZE);
            printf("\nProcesando efecto... Esto puede tardar un momento.\n");

            // Empezar el efecto
            clock_gettime(CLOCK_MONOTONIC, &tInicio);
            clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &tCPUInicio);
            res = iniciar();
            clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &tCPUFin);
            clock_gettime(CLOCK_MONOTONIC, &tFin);

            if (res == NO_ERR) {
                tiempoCPU = tCPUFin.tv_sec - tCPUInicio.tv_sec + (tCPUFin.tv_nsec - tCPUInicio.tv_nsec) / 1000000000.0;
                printf("\n-- INFORMACIÓN DE RENDIMIENTO --\n");
                printf("Tiempo de CPU: %.3f s\n", tiempoCPU);
                printf("Tiempo de CPU por hilo: %.3f s\n", tiempoCPU / nHilos);
                printf("Tiempo real de usuario: %.3f s", (double) (tFin.tv_sec - tInicio.tv_sec + (tFin.tv_nsec - tInicio.tv_nsec) / 1000000000.0));

                // Mostrar resultado, que debería de haber quedado en la imagen original
                cvShowImage(NOMBRE_VENTANA, imgs[ORIGINAL]);
                cvWaitKey(0);
            } else {
                // No se pudieron crear todos los hilos, así que el resultado sería incorrecto.
                // Descartarlo y mostrar un mensaje de error
                fprintf(stderr, mensajesError[res]);
            }

            cvDestroyAllWindows();

            cvReleaseImage(&imgs[ORIGINAL]);
            cvReleaseImage(&imgs[PATRON]);
        } else {
            fprintf(stderr, mensajesError[res]);
        }
    } else {
        fprintf(stderr, "Sintaxis: %s [imagen original] [imagen patrón]\n", argv[0]);
    }
    
    return res;
}

static codigoResultado pedirOpciones() {
    char bufLectura[TAM_BUFER_LECTURA];
    tecnicaParalelDatos t = SIN_DEFINIR;
    long n;
    
    printf("-- OPCIONES --\n");
    
    do {
        printf("¿Cuántos hilos de ejecución? (1-%u): ", UINT_MAX - 1);
        fgets(bufLectura, TAM_BUFER_LECTURA, stdin);
        if (!strstr(bufLectura, "\n")) {
            // Descartar caracteres no leídos del búfer de entrada
            do {} while (getchar() != '\n');
        }
        n = atol(bufLectura);
        nHilos = (unsigned int) n > UINT_MAX ? UINT_MAX : n;
    } while (nHilos < 0 || nHilos > UINT_MAX - 1);
    
    do {
        printf("¿Qué técnica de paralelismo de datos se debe de usar? (%s): ", obtenerCadenaTecnicasParalel());
        fgets(bufLectura, TAM_BUFER_LECTURA, stdin);
        if (!strstr(bufLectura, "\n")) {
            do {} while (getchar() != '\n');
        }
        t = leerTecnicaParalel(bufLectura);
    } while (t == SIN_DEFINIR);
    
    return configurar(t);
}

static tecnicaParalelDatos leerTecnicaParalel(char buf[]) {
    static const size_t nTecnicas = sizeof(cadenasTecParalel) / sizeof(cadenasTecParalel[0]);
    tecnicaParalelDatos toret = SIN_DEFINIR;
    size_t l = strlen(buf) - 1;
    // Array de longitud variable. Si es muy grande (no lo va a ser) podría haber
    // problemas reservando espacio para él en la pila. Más rápido que malloc (heap)
    unsigned char banderaCoinc[nTecnicas];
    
    // Inicializar array de banderas a un valor diferente de 0 para cada bit,
    // para que no se interprete como un unsigned char de valor 0 de
    // ninguna manera. sizeof(unsigned char) siempre es 1, ver sección 6.5.3.4
    // del estándar ISO/IEC 9899:1999
    memset(banderaCoinc, 0xFF, nTecnicas);
    
    // Eliminar \n al final del búfer de entrada
    buf[l] = '\0';

    for (size_t i = 0; i < l && i < sizeof(cadenasTecParalel[0]); ++i) {
        char c = toupper(buf[i]);   // Convertir a forma neutral
        
        // Ver si hasta el momento estamos coincidiendo con alguna cadena.
        // Si dejamos de coincidir con una, ya no es posible que se haya introducido esa
        for (size_t j = 0; j < nTecnicas; ++j) {
            banderaCoinc[j] = c == cadenasTecParalel[j][i] && banderaCoinc[j];
        }
    }
    
    // Actualizar resultado con las banderas calculadas
    // (En caso de equidad, se considerará escogida la técnica en menor posición)
    for (size_t i = nTecnicas; i > 0; --i) {
        size_t n = i - 1;
        if (banderaCoinc[n]) {
            toret = n;
        }
    }
    
    return toret;
}

static char* obtenerCadenaTecnicasParalel() {
    static const size_t nTecnicas = sizeof(cadenasTecParalel) / sizeof(cadenasTecParalel[0]);
    static char* toret = NULL;
    
    if (!toret) {
        toret = calloc(1, (sizeof(cadenasTecParalel[0]) - 1) * nTecnicas + nTecnicas);
    }
    
    if (toret) {
        for (size_t i = 0; i < nTecnicas; ++i) {
            sprintf(toret, "%s%s", toret, cadenasTecParalel[i]);
            if (i < nTecnicas - 1) {
                sprintf(toret, "%s/", toret);
            }
        }
    }
    
    return toret;
}