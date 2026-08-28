#ifndef UTILS_H_
#define UTILS_H_

/* Necesario para exponer struct addrinfo / getaddrinfo bajo -std=c11
 * estricto (estas son extensiones POSIX, no del estandar C puro). */
#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#endif

/* ============================================================
 *  utils.h
 *
 *  Biblioteca compartida de red y protocolo para EntrenadOS.
 *  Uso: Planificador, Core, Placa y Storage.
 *
 *  Esta biblioteca NO contiene logica de negocio de ningun
 *  modulo puntual. Solo ofrece:
 *    - Primitivas de servidor / cliente sobre sockets TCP.
 *    - Definicion del protocolo (op_code) y estructuras de
 *      paquete/buffer.
 *    - Serializacion / deserializacion generica.
 *    - Helpers de Handshake reutilizables.
 * ============================================================ */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <unistd.h>
#include <errno.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <commons/log.h>

/* ============================================================
 *  1. PROTOCOLO DE COMUNICACION (OPCODES)
 * ============================================================ */

/**
 * op_code: identifica el tipo de mensaje que viaja por el socket.
 *
 * - HANDSHAKE_*        -> Identificacion de modulo al conectarse.
 * - HANDSHAKE_OK/ERROR -> Respuesta del que recibe el handshake.
 * - MENSAJE            -> Mensaje de texto de prueba (para probar
 *                          el envio/recepcion de paquetes en CC1).
 * - OK / ERROR         -> Respuestas genericas de una operacion.
 * - DESCONEXION        -> Aviso de cierre prolijo de conexion.
 *
 * IMPORTANTE: este enum es la "fuente de verdad" del protocolo.
 * Los codigos especificos de cada Checkpoint (ej. planificacion,
 * pedidos de memoria, instrucciones de CPU, etc.) NO van aca:
 * se agregan en el protocolo propio de cada modulo en CC2+,
 * pudiendo definir su propio enum que arranque despues del ultimo
 * valor de este, o encapsulando este enum dentro del payload.
 */
typedef enum {
    // --- Identificacion de modulos (Handshake) ---
    HANDSHAKE_CORE = 0,
    HANDSHAKE_PLANIFICADOR,
    HANDSHAKE_PLACA,
    HANDSHAKE_STORAGE,

    // --- Respuestas de control del handshake ---
    HANDSHAKE_OK,
    HANDSHAKE_ERROR,

    // --- Mensajes de prueba / genericos ---
    MENSAJE,

    // --- Respuestas genericas de operacion ---
    OK,
    ERROR,

    // --- Fin de conexion prolija ---
    DESCONEXION
} op_code;

/* ============================================================
 *  2. ESTRUCTURAS DE RED Y PAQUETES
 * ============================================================ */

/**
 * t_buffer: payload crudo de un paquete, ya serializado.
 * 'stream' es un blob de bytes con el formato:
 *   [ int tamanio_campo_1 | datos_campo_1 | int tamanio_campo_2 | datos_campo_2 | ... ]
 * generado por sucesivos llamados a agregar_a_paquete().
 */
typedef struct {
    void* stream;
    uint32_t size;
} t_buffer;

/**
 * t_paquete: unidad logica de envio. Se arma en memoria con
 * crear_paquete() + agregar_a_paquete() y se serializa a la red
 * con enviar_paquete().
 */
typedef struct {
    op_code codigo_operacion;
    t_buffer* buffer;
} t_paquete;

/* ============================================================
 *  3. FUNCIONES DE SERVIDOR (usadas por Planificador, Placa, Storage)
 * ============================================================ */

/**
 * Crea un socket pasivo, aplica SO_REUSEADDR, hace bind() y listen().
 * Devuelve el fd del socket servidor, o -1 ante error (logueado).
 */
int iniciar_servidor(char* puerto, t_log* logger);

/**
 * Bloquea en accept() esperando una nueva conexion entrante.
 * Devuelve el fd del socket cliente aceptado, o -1 ante error.
 * Pensada para ser llamada dentro de un while(1) en el hilo
 * "aceptador", delegando cada conexion a un hilo nuevo (pthread_create
 * + pthread_detach) desde el modulo que la usa.
 */
int esperar_cliente(int socket_servidor, t_log* logger);

/* ============================================================
 *  4. FUNCIONES DE CLIENTE (usadas por Core y Planificador)
 * ============================================================ */

/**
 * Crea el socket y realiza connect() contra ip:puerto.
 * Devuelve el fd conectado, o -1 ante error (logueado).
 */
int crear_conexion(char* ip, char* puerto, t_log* logger);

/**
 * Cierra el socket de forma segura (shutdown + close).
 * Seguro de llamar con fd < 0 (no hace nada).
 */
void liberar_conexion(int socket_cliente);

/* ============================================================
 *  5. PAQUETES: creacion, serializacion y destruccion
 * ============================================================ */

/** Crea un paquete vacio con el op_code indicado. */
t_paquete* crear_paquete(op_code codigo);

/**
 * Agrega un campo al paquete. Internamente antepone el tamanio
 * del campo (int) para permitir su posterior deserializacion
 * ordenada del lado receptor.
 */
void agregar_a_paquete(t_paquete* paquete, void* datos, int tamanio);

/** Libera un t_buffer (y su stream interno si no es NULL). */
void eliminar_buffer(t_buffer* buffer);

/** Libera un t_paquete completo (buffer incluido). */
void eliminar_paquete(t_paquete* paquete);

/* ============================================================
 *  6. ENVIO / RECEPCION DE PAQUETES
 * ============================================================ */

/**
 * Serializa [op_code][tamanio_buffer][stream] y lo envia por el
 * socket, manejando internamente los envios parciales.
 */
void enviar_paquete(t_paquete* paquete, int socket_cliente);

/**
 * Helper para enviar un string simple sin tener que armar el
 * paquete manualmente. Util para pruebas de CC1 (ping/pong de texto).
 */
void enviar_mensaje(char* mensaje, op_code codigo, int socket_cliente);

/**
 * Lee (bloqueante) el op_code inicial de un paquete entrante.
 * Devuelve -1 si el peer cerro la conexion o hubo error de socket
 * (se debe interpretar como desconexion, no como op_code invalido).
 */
int recibir_operacion(int socket_cliente);

/**
 * Lee el tamanio del payload y luego el payload completo.
 * *size queda seteado con el tamanio leido. Devuelve el puntero
 * al buffer (a liberar por el llamador) o NULL ante error/desconexion.
 */
void* recibir_buffer(uint32_t* size, int socket_cliente);

/**
 * Helper inverso de enviar_mensaje(): recibe un buffer con un unico
 * campo string (agregado via agregar_a_paquete) y devuelve el string
 * ya extraido (a liberar por el llamador). Devuelve NULL ante error.
 */
char* recibir_mensaje_texto(int socket_cliente);

/* ============================================================
 *  7. HANDSHAKE
 * ============================================================ */

/**
 * Lado CLIENTE del handshake: envia el op_code de identificacion
 * propia (ej. HANDSHAKE_CORE) y espera HANDSHAKE_OK como confirmacion.
 * Devuelve 0 si el handshake fue aceptado, -1 en caso contrario.
 */
int realizar_handshake(int socket_cliente, op_code identificacion_propia, t_log* logger);

/**
 * Lado SERVIDOR del handshake: recibe la identificacion entrante y
 * la compara contra la esperada. Responde HANDSHAKE_OK o
 * HANDSHAKE_ERROR segun corresponda.
 * Devuelve 0 si coincide con lo esperado, -1 en caso contrario
 * (incluyendo desconexion durante el proceso).
 */
int recibir_handshake(int socket_cliente, op_code identificacion_esperada, t_log* logger);

/** Traduce un op_code de handshake a su nombre legible (para logs). */
char* nombre_modulo(op_code identificacion);

/* ============================================================
 *  8. PRIMITIVAS DE SOCKET (envio/recepcion garantizada)
 * ============================================================ */

/** send() en loop hasta enviar 'tamanio' bytes. 0 si OK, -1 si error. */
int enviar_bytes(int socket, void* buffer, size_t tamanio);

/** recv() en loop hasta recibir 'tamanio' bytes. 0 si OK, -1 si error/EOF. */
int recibir_bytes(int socket, void* buffer, size_t tamanio);

#endif /* UTILS_H_ */
