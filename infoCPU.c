/*
 * Licencia: ver fichero LICENSE en directorio base
 * Propósito: fachada de la interfaz proporcionada por intrínsecos del
 * compilador para que la aplicación, situada a un mayor nivel de abstracción,
 * pueda obtener información de la CPU. Proporciona indirección y bajo
 * acomplamiento
 */

#include "infoCPU.h"

#ifdef __GNUC__
#include <cpuid.h>

int soportaSSE2() {
#ifdef __x86_64__
    // La especificación x86-64 engloba a SSE2
    // https://en.wikipedia.org/wiki/X86-64
    return 1;
#else
    unsigned int eax, ebx, ecx, edx;
    return __get_cpuid(1, &eax, &ebx, &ecx, &edx) && (edx & bit_SSE2 != 0);
#endif
}

#else
#error "No se soporta el conjunto de herramientas de compilación usado."
#endif