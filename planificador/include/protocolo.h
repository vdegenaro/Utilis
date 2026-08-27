#ifndef PROTOCOLO_H_
#define PROTOCOLO_H_

#include <stdint.h>
#include <stdlib.h>

/*
 * ============================================================
 *  op_code: identifica el tipo de mensaje que viaja en cada
 *  paquete entre los distintos módulos de EntrenadOS.
 *
 *  - HANDSHAKE_*: se usan una única vez al establecer la
 *    conexión, para que quien recibe sepa con qué módulo
 *    está hablando.
 *  - HANDSHAKE_OK / HANDSHAKE_ERROR: respuesta del que
 *    recibe el handshake.
 *  - MENSAJE: opcode genérico de prueba para CC1 (ping/pong,
 *    texto plano, etc).
 *  - PAQUETE_DESCONEXION: aviso explícito de cierre prolijo
 *    de la conexión.
 *
 *  Los opcodes de CC2/CC3/CC4 (planificación, paginación,
 *  journaling, etc.) se van a agregar más adelante; se deja
 *  el enum abierto a propósito con un TODO.
 * ============================================================
 */
typedef enum
{
    // --- Handshakes de identificación de módulo ---
    HANDSHAKE_PLANIFICADOR = 0,
    HANDSHAKE_CORE,
    HANDSHAKE_PLACA,
    HANDSHAKE_STORAGE,

    // --- Respuestas de handshake ---
    HANDSHAKE_OK,
    HANDSHAKE_ERROR,

    // --- Mensajería genérica (placeholder para CC1) ---
    MENSAJE,

    // --- Control de conexión ---
    PAQUETE_DESCONEXION

    // TODO CC2: agregar opcodes de planificación de procesos
    // (ej. NUEVO_JOB, DISPATCH_CORE, JOB_FINALIZADO, etc.)
    // TODO CC3: agregar opcodes de memoria/paginación
    // TODO CC4: agregar opcodes de journaling/persistencia
} op_code;

/*
 * t_paquete: unidad de transporte del protocolo.
 *
 * codigo_operacion  -> qué tipo de mensaje es
 * tamanio_buffer    -> cantidad de bytes válidos en buffer
 * buffer            -> payload serializado (puede tener 0 o más
 *                       "campos" empaquetados con agregar_a_paquete)
 */
typedef struct
{
    op_code codigo_operacion;
    int tamanio_buffer;
    void *buffer;
} t_paquete;

/* ---------- Construcción / destrucción de paquetes ---------- */

// Crea un paquete vacío con el opcode indicado. Nunca devuelve NULL
// (aborta el proceso con log de error si malloc falla).
t_paquete *crear_paquete(op_code codigo);

// Agrega un campo (con su tamaño) al final del buffer del paquete.
// Internamente cada campo queda serializado como [tamanio][datos].
// Permite armar paquetes con múltiples valores heterogéneos.
void agregar_a_paquete(t_paquete *paquete, void *valor, int tamanio);

// Libera un paquete y su buffer interno. paquete puede ser NULL.
void eliminar_paquete(t_paquete *paquete);

/* ---------------------- Envío / recepción -------------------- */

// Serializa y envía un paquete completo por el socket dado.
// Devuelve 0 en éxito, -1 en error (socket caído, etc).
int enviar_paquete(t_paquete *paquete, int socket_cliente);

// Envía únicamente un opcode "pelado" (sin payload). Se usa mucho
// para handshakes y respuestas de control (HANDSHAKE_OK, etc).
int enviar_opcode(op_code codigo, int socket_cliente);

// Bloquea esperando el próximo paquete completo en el socket.
// Devuelve un t_paquete recién creado (a liberar con eliminar_paquete)
// o NULL si el peer cerró la conexión o hubo un error de recepción.
t_paquete *recibir_paquete(int socket_cliente);

// Extrae el siguiente campo serializado con agregar_a_paquete desde
// un buffer de payload, y avanza el offset. Devuelve un puntero
// nuevo (a liberar por el llamador) con los `tamanio` bytes leídos.
void *extraer_de_buffer(void *buffer, int *offset, int tamanio);

/* --------------------------- Handshake ------------------------ */

// Envía el opcode de identificación correspondiente al propio módulo
// y espera un HANDSHAKE_OK / HANDSHAKE_ERROR de respuesta.
// Devuelve 0 si el handshake fue exitoso, -1 en caso contrario.
int realizar_handshake_cliente(int socket_servidor, op_code identidad_propia);

// Del lado servidor: recibe el opcode de identificación del peer que
// se acaba de conectar, loguea "## Módulo: <NOMBRE>" y responde
// HANDSHAKE_OK. Devuelve el op_code recibido, o -1 en error.
op_code realizar_handshake_servidor(int socket_cliente);

// Traduce un op_code de handshake a un string legible para logs
// (ej. HANDSHAKE_CORE -> "CORE"). Nunca devuelve NULL.
char *nombre_modulo(op_code identidad);

#endif /* PROTOCOLO_H_ */
