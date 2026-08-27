#include "../include/cliente.h"
#include "../include/protocolo.h"

#include <arpa/inet.h>
#include <commons/log.h>
#include <netdb.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

extern t_log *logger;

int conectar_a_modulo(char *ip, int puerto, char *nombre_modulo_destino)
{
    char puerto_str[16];
    snprintf(puerto_str, sizeof(puerto_str), "%d", puerto);

    struct addrinfo hints, *servinfo;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    int estado_getaddr = getaddrinfo(ip, puerto_str, &hints, &servinfo);
    if (estado_getaddr != 0)
    {
        log_error(logger, "No se pudo resolver dirección de %s (%s:%d): %s",
                nombre_modulo_destino, ip, puerto, gai_strerror(estado_getaddr));
        return -1;
    }

    int socket_cliente = socket(servinfo->ai_family, servinfo->ai_socktype, servinfo->ai_protocol);
    if (socket_cliente == -1)
    {
        log_error(logger, "No se pudo crear el socket para conectar a %s", nombre_modulo_destino);
        freeaddrinfo(servinfo);
        return -1;
    }

    if (connect(socket_cliente, servinfo->ai_addr, servinfo->ai_addrlen) == -1)
    {
        log_error(logger, "No se pudo conectar a %s (%s:%d). Verifique que el módulo esté levantado.",
                nombre_modulo_destino, ip, puerto);
        freeaddrinfo(servinfo);
        close(socket_cliente);
        return -1;
    }

    freeaddrinfo(servinfo);

    log_info(logger, "Conexión establecida con %s (%s:%d)", nombre_modulo_destino, ip, puerto);

    // El planificador es siempre quien se identifica como
    // HANDSHAKE_PLANIFICADOR frente a Placa/Storage.
    if (realizar_handshake_cliente(socket_cliente, HANDSHAKE_PLANIFICADOR) == -1)
    {
        log_error(logger, "Falló el handshake con %s", nombre_modulo_destino);
        close(socket_cliente);
        return -1;
    }

    log_info(logger, "Handshake exitoso con %s", nombre_modulo_destino);

    return socket_cliente;
}
