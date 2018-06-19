/*
 * Licencia: ver fichero LICENSE en directorio base
 * Propósito: vista y controlador del efecto
 */

#include "efecto.h"
#include <stdio.h>
#include <time.h>

#define NOMBRE_VENTANA "Efecto mosaico"
#define TAM_BUFER_LECTURA 16

const char* mensajesError[] = {
    "\0",
    "Error: no se ha podido reservar memoria necesaria para la ejecución de la aplicación.\0",
    "Error: no se han podido cargar las imágenes. Comprueba que la ruta de acceso es válida y son RGB TrueColor.\0"
    "Error: no se ha podido crear algún hilo de ejecución para el efecto.\0"
};
static void pedirOpciones();

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
            pedirOpciones();
            
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
                printf("Tiempo de CPU por hilo: %.3f s\n", tiempoCPU / p.nHilos);
                printf("Tiempo real de usuario: %.3f s", tFin.tv_sec - tInicio.tv_sec + (tFin.tv_nsec - tInicio.tv_nsec) / 1000000000.0);

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

static void pedirOpciones() {
    char bufLectura[TAM_BUFER_LECTURA];
    int usarSIMD = 0;
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
        p.nHilos = (unsigned int) n > UINT_MAX ? UINT_MAX : n;
    } while (p.nHilos < 0 || p.nHilos > UINT_MAX - 1 || (hilos = malloc(sizeof(hilo) * p.nHilos), !hilos));
    
    // Mejora: comprobar si la CPU soporta la extensión SIMD usada con __get_cpuid,
    // ver https://en.wikipedia.org/wiki/CPUID
    do {
        printf("¿Usar SIMD? (S/N): ");
        fgets(bufLectura, TAM_BUFER_LECTURA, stdin);
        if (!strstr(bufLectura, "\n")) {
            do {} while (getchar() != '\n');
        }
        usarSIMD = bufLectura[0] == 'S' || bufLectura[0] == 's';
    } while (!usarSIMD && bufLectura[0] != 'N' && bufLectura[0] != 'n');
    
    concretarEstrategia(usarSIMD);
}