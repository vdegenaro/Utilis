#include "../include/servidor.h"
#include "../include/protocolo.h"

#include <arpa/inet.h>
#include <commons/log.h>
#include <netinet/in.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>
#include <stdbool.h>

extern t_log *logger;

// Cantidad de conexiones pendientes que el kernel puede encolar
// antes de que el proceso llame a accept().
#define BACKLOG_LISTEN 50

// Estructura auxiliar para pasarle el fd del cliente al hilo nuevo,
// ya que pthread_create solo acepta un único puntero como argumento.
typedef struct
{
    int socket_cliente;
} t_args_hilo_cliente;

// Función que corre en cada hilo dedicado a un cliente (típicamente
// un Core, aunque el handshake acepta cualquier módulo válido).
// Se encarga de: handshake -> loop de recepción -> desconexión prolija.
static void *atender_cliente(void *arg)
{
    t_args_hilo_cliente *args = (t_args_hilo_cliente *)arg;
    int socket_cliente = args->socket_cliente;
    free(args); // ya copiamos el fd a una variable local, no lo necesitamos más

    // pthread_detach: nadie va a hacer join sobre este hilo, así que
    // liberamos sus recursos automáticamente al terminar.
    pthread_detach(pthread_self());

    op_code identidad = realizar_handshake_servidor(socket_cliente);
    if (identidad == (op_code)-1)
    {
        log_error(logger, "Se descartó una conexión entrante por handshake inválido/fallido");
        close(socket_cliente);
        return NULL;
    }

    char *nombre = nombre_modulo(identidad);

    // --- Loop de recepción de paquetes hasta desconexión ---
    bool conectado = true;
    while (conectado)
    {
        t_paquete *paquete = recibir_paquete(socket_cliente);

        if (paquete == NULL)
        {
            // El peer cerró la conexión de forma abrupta (sin avisar
            // con PAQUETE_DESCONEXION) o hubo un error de socket.
            log_warning(logger, "Se perdió la conexión con %s de forma inesperada", nombre);
            conectado = false;
            break;
        }

        switch (paquete->codigo_operacion)
        {
        case PAQUETE_DESCONEXION:
            log_info(logger, "El módulo %s solicitó desconexión prolija", nombre);
            conectado = false;
            break;

        case MENSAJE:
            // Placeholder de CC1: simplemente confirmamos recepción.
            // TODO CC2: acá se van a procesar mensajes reales de
            // planificación (ej. avisos de fin de ráfaga, etc).
            log_info(logger, "Mensaje recibido de %s (%d bytes de payload)",
                    nombre, paquete->tamanio_buffer);
            break;

        default:
            log_warning(logger, "Opcode desconocido (%d) recibido de %s",
                        paquete->codigo_operacion, nombre);
            break;
        }

        eliminar_paquete(paquete);
    }

    log_info(logger, "Cerrando conexión con %s", nombre);
    close(socket_cliente);
    return NULL;
}

int iniciar_servidor(int puerto)
{
    int socket_servidor = socket(AF_INET, SOCK_STREAM, 0);
    if (socket_servidor == -1)
    {
        log_error(logger, "No se pudo crear el socket de escucha del servidor");
        return -1;
    }

    // Permite reutilizar el puerto inmediatamente después de reiniciar
    // el proceso (evita el típico "Address already in use" en pruebas).
    int reuse = 1;
    if (setsockopt(socket_servidor, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)) == -1)
    {
        log_warning(logger, "No se pudo setear SO_REUSEADDR (se continúa igual)");
    }

    struct sockaddr_in direccion_servidor;
    memset(&direccion_servidor, 0, sizeof(direccion_servidor));
    direccion_servidor.sin_family = AF_INET;
    direccion_servidor.sin_addr.s_addr = INADDR_ANY;
    direccion_servidor.sin_port = htons((uint16_t)puerto);

    if (bind(socket_servidor, (struct sockaddr *)&direccion_servidor, sizeof(direccion_servidor)) == -1)
    {
        log_error(logger, "No se pudo bindear el puerto %d (¿ya está en uso?)", puerto);
        close(socket_servidor);
        return -1;
    }

    if (listen(socket_servidor, BACKLOG_LISTEN) == -1)
    {
        log_error(logger, "No se pudo poner en escucha el socket en el puerto %d", puerto);
        close(socket_servidor);
        return -1;
    }

    log_info(logger, "Planificador escuchando conexiones en el puerto %d", puerto);

    return socket_servidor;
}

void escuchar_conexiones(int socket_servidor)
{
    while (true)
    {
        struct sockaddr_in direccion_cliente;
        socklen_t tamanio_direccion = sizeof(direccion_cliente);

        int socket_cliente = accept(socket_servidor,
                                     (struct sockaddr *)&direccion_cliente,
                                    &tamanio_direccion);

        if (socket_cliente == -1)
        {
            // Un error puntual en accept() no debería tirar abajo todo
            // el servidor: se loguea y se sigue esperando conexiones.
            log_warning(logger, "Falló accept() sobre una conexión entrante, se continúa escuchando");
            continue;
        }

        char ip_cliente[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &(direccion_cliente.sin_addr), ip_cliente, INET_ADDRSTRLEN);
        log_info(logger, "Nueva conexión entrante desde %s:%d",
                ip_cliente, ntohs(direccion_cliente.sin_port));

        t_args_hilo_cliente *args = malloc(sizeof(t_args_hilo_cliente));
        if (args == NULL)
        {
            log_error(logger, "No se pudo reservar memoria para atender una nueva conexión");
            close(socket_cliente);
            continue;
        }
        args->socket_cliente = socket_cliente;

        pthread_t hilo_cliente;
        if (pthread_create(&hilo_cliente, NULL, atender_cliente, args) != 0)
        {
            log_error(logger, "No se pudo crear el hilo para atender una nueva conexión");
            free(args);
            close(socket_cliente);
            continue;
        }
    }
}
