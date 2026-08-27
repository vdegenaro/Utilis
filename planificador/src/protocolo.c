#include "../include/protocolo.h"

#include <commons/log.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>
#include <stdio.h>
#include <stdbool.h>

// Logger global del módulo (declarado e inicializado en main.c).
// El protocolo lo usa para loguear errores de red / handshake.
extern t_log *logger;

/* ============================================================
 *  Helpers internos de envío/recepción "a bajo nivel" sobre el
 *  socket. Garantizan que se lean/escriban exactamente los
 *  bytes pedidos, reintentando ante envíos/recepciones
 *  parciales (comportamiento normal de TCP).
 * ============================================================ */

static int enviar_bytes(int socket_cliente, void *datos, int tamanio)
{
    int enviados = 0;
    while (enviados < tamanio)
    {
        int resultado = send(socket_cliente, (char *)datos + enviados,
                            tamanio - enviados, MSG_NOSIGNAL);
        if (resultado <= 0)
        {
            return -1; // socket caído / error de red
        }
        enviados += resultado;
    }
    return 0;
}

static int recibir_bytes(int socket_cliente, void *destino, int tamanio)
{
    int recibidos = 0;
    while (recibidos < tamanio)
    {
        int resultado = recv(socket_cliente, (char *)destino + recibidos,
                            tamanio - recibidos, 0);
        if (resultado == 0)
        {
            return 0; // el peer cerró la conexión prolijamente
        }
        if (resultado < 0)
        {
            return -1; // error de socket
        }
        recibidos += resultado;
    }
    return 1;
}

/* ============================================================
 *  Construcción / destrucción de paquetes
 * ============================================================ */

t_paquete *crear_paquete(op_code codigo)
{
    t_paquete *paquete = malloc(sizeof(t_paquete));
    if (paquete == NULL)
    {
        if (logger != NULL)
        {
            log_error(logger, "No se pudo reservar memoria para un paquete (malloc falló)");
        }
        abort();
    }

    paquete->codigo_operacion = codigo;
    paquete->tamanio_buffer = 0;
    paquete->buffer = NULL;

    return paquete;
}

void agregar_a_paquete(t_paquete *paquete, void *valor, int tamanio)
{
    if (paquete == NULL || valor == NULL || tamanio <= 0)
    {
        return;
    }

    // Cada campo se serializa como: [int tamanio][bytes del valor]
    int tamanio_nuevo = paquete->tamanio_buffer + sizeof(int) + tamanio;

    void *nuevo_buffer = realloc(paquete->buffer, tamanio_nuevo);
    if (nuevo_buffer == NULL)
    {
        if (logger != NULL)
        {
            log_error(logger, "No se pudo reservar memoria al agregar campo a un paquete");
        }
        abort();
    }
    paquete->buffer = nuevo_buffer;

    memcpy((char *)paquete->buffer + paquete->tamanio_buffer, &tamanio, sizeof(int));
    memcpy((char *)paquete->buffer + paquete->tamanio_buffer + sizeof(int), valor, tamanio);

    paquete->tamanio_buffer = tamanio_nuevo;
}

void eliminar_paquete(t_paquete *paquete)
{
    if (paquete == NULL)
    {
        return;
    }
    if (paquete->buffer != NULL)
    {
        free(paquete->buffer);
    }
    free(paquete);
}

void *extraer_de_buffer(void *buffer, int *offset, int tamanio)
{
    void *valor = malloc(tamanio);
    if (valor == NULL)
    {
        if (logger != NULL)
        {
            log_error(logger, "No se pudo reservar memoria al extraer un campo de un paquete");
        }
        abort();
    }
    memcpy(valor, (char *)buffer + *offset, tamanio);
    *offset += tamanio;
    return valor;
}

/* ============================================================
 *  Envío / recepción de paquetes completos
 * ============================================================ */

int enviar_paquete(t_paquete *paquete, int socket_cliente)
{
    if (paquete == NULL)
    {
        return -1;
    }

    // Formato en el "wire": [op_code][tamanio_buffer][buffer...]
    int tamanio_total = sizeof(op_code) + sizeof(int) + paquete->tamanio_buffer;
    void *a_enviar = malloc(tamanio_total);
    if (a_enviar == NULL)
    {
        if (logger != NULL)
        {
            log_error(logger, "No se pudo reservar memoria para serializar un paquete");
        }
        return -1;
    }

    int offset = 0;
    memcpy(a_enviar + offset, &(paquete->codigo_operacion), sizeof(op_code));
    offset += sizeof(op_code);
    memcpy(a_enviar + offset, &(paquete->tamanio_buffer), sizeof(int));
    offset += sizeof(int);
    if (paquete->tamanio_buffer > 0)
    {
        memcpy(a_enviar + offset, paquete->buffer, paquete->tamanio_buffer);
    }

    int resultado = enviar_bytes(socket_cliente, a_enviar, tamanio_total);
    free(a_enviar);

    return resultado;
}

int enviar_opcode(op_code codigo, int socket_cliente)
{
    t_paquete *paquete = crear_paquete(codigo);
    int resultado = enviar_paquete(paquete, socket_cliente);
    eliminar_paquete(paquete);
    return resultado;
}

t_paquete *recibir_paquete(int socket_cliente)
{
    op_code codigo;
    int estado = recibir_bytes(socket_cliente, &codigo, sizeof(op_code));
    if (estado <= 0)
    {
        // 0 = desconexión prolija, -1 = error de socket
        return NULL;
    }

    int tamanio_buffer;
    estado = recibir_bytes(socket_cliente, &tamanio_buffer, sizeof(int));
    if (estado <= 0)
    {
        return NULL;
    }

    t_paquete *paquete = crear_paquete(codigo);
    paquete->tamanio_buffer = tamanio_buffer;

    if (tamanio_buffer > 0)
    {
        paquete->buffer = malloc(tamanio_buffer);
        if (paquete->buffer == NULL)
        {
            if (logger != NULL)
            {
                log_error(logger, "No se pudo reservar memoria para recibir el payload de un paquete");
            }
            eliminar_paquete(paquete);
            return NULL;
        }

        estado = recibir_bytes(socket_cliente, paquete->buffer, tamanio_buffer);
        if (estado <= 0)
        {
            eliminar_paquete(paquete);
            return NULL;
        }
    }

    return paquete;
}

/* ============================================================
 *  Handshake
 * ============================================================ */

char *nombre_modulo(op_code identidad)
{
    switch (identidad)
    {
    case HANDSHAKE_PLANIFICADOR:
        return "PLANIFICADOR";
    case HANDSHAKE_CORE:
        return "CORE";
    case HANDSHAKE_PLACA:
        return "PLACA";
    case HANDSHAKE_STORAGE:
        return "STORAGE";
    default:
        return "DESCONOCIDO";
    }
}

int realizar_handshake_cliente(int socket_servidor, op_code identidad_propia)
{
    if (enviar_opcode(identidad_propia, socket_servidor) == -1)
    {
        if (logger != NULL)
        {
            log_error(logger, "Error al enviar handshake como %s", nombre_modulo(identidad_propia));
        }
        return -1;
    }

    t_paquete *respuesta = recibir_paquete(socket_servidor);
    if (respuesta == NULL)
    {
        if (logger != NULL)
        {
            log_error(logger, "No se recibió respuesta de handshake (conexión caída)");
        }
        return -1;
    }

    int ok = (respuesta->codigo_operacion == HANDSHAKE_OK);
    if (!ok && logger != NULL)
    {
        log_error(logger, "Handshake rechazado por el módulo remoto");
    }

    eliminar_paquete(respuesta);
    return ok ? 0 : -1;
}

op_code realizar_handshake_servidor(int socket_cliente)
{
    t_paquete *paquete = recibir_paquete(socket_cliente);
    if (paquete == NULL)
    {
        if (logger != NULL)
        {
            log_error(logger, "Conexión entrante cerrada antes de completar el handshake");
        }
        return -1;
    }

    op_code identidad = paquete->codigo_operacion;
    eliminar_paquete(paquete);

    bool identidad_valida = identidad == HANDSHAKE_CORE ||
                            identidad == HANDSHAKE_PLACA ||
                            identidad == HANDSHAKE_STORAGE ||
                            identidad == HANDSHAKE_PLANIFICADOR;

    if (!identidad_valida)
    {
        if (logger != NULL)
        {
            log_error(logger, "Handshake inválido recibido (opcode %d)", identidad);
        }
        enviar_opcode(HANDSHAKE_ERROR, socket_cliente);
        return -1;
    }

    // Log obligatorio de CC1
    if (logger != NULL)
    {
        log_info(logger, "## Módulo: %s", nombre_modulo(identidad));
    }

    if (enviar_opcode(HANDSHAKE_OK, socket_cliente) == -1)
    {
        if (logger != NULL)
        {
            log_error(logger, "No se pudo confirmar el handshake al módulo %s", nombre_modulo(identidad));
        }
        return -1;
    }

    return identidad;
}
