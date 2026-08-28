#include "utils.h"

/* ============================================================
 *  8. PRIMITIVAS DE SOCKET (envio/recepcion garantizada)
 * ============================================================
 *  TCP no garantiza que send()/recv() muevan todos los bytes
 *  pedidos de una sola vez. Estas funciones resuelven ese
 *  problema con un loop, evitando el clasico bug de "me llego
 *  el paquete cortado".
 * ============================================================ */

int enviar_bytes(int socket, void* buffer, size_t tamanio) {
    size_t enviados = 0;
    char* ptr = (char*) buffer;

    while (enviados < tamanio) {
        ssize_t resultado = send(socket, ptr + enviados, tamanio - enviados, MSG_NOSIGNAL);
        if (resultado <= 0) {
            return -1; // socket cerrado o error
        }
        enviados += (size_t) resultado;
    }
    return 0;
}

int recibir_bytes(int socket, void* buffer, size_t tamanio) {
    size_t recibidos = 0;
    char* ptr = (char*) buffer;

    while (recibidos < tamanio) {
        ssize_t resultado = recv(socket, ptr + recibidos, tamanio - recibidos, 0);
        if (resultado <= 0) {
            // 0 -> el peer cerro prolijamente; <0 -> error de socket
            return -1;
        }
        recibidos += (size_t) resultado;
    }
    return 0;
}

/* ============================================================
 *  3. SERVIDOR
 * ============================================================ */

int iniciar_servidor(char* puerto, t_log* logger) {
    struct addrinfo hints, *servinfo, *p;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;

    if (getaddrinfo(NULL, puerto, &hints, &servinfo) != 0) {
        log_error(logger, "iniciar_servidor: getaddrinfo() fallo para puerto %s", puerto);
        return -1;
    }

    int socket_servidor = -1;
    for (p = servinfo; p != NULL; p = p->ai_next) {
        socket_servidor = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
        if (socket_servidor == -1) {
            continue;
        }

        int yes = 1;
        if (setsockopt(socket_servidor, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) == -1) {
            log_warning(logger, "iniciar_servidor: no se pudo setear SO_REUSEADDR");
        }

        if (bind(socket_servidor, p->ai_addr, p->ai_addrlen) == -1) {
            close(socket_servidor);
            socket_servidor = -1;
            continue;
        }

        break; // bind exitoso
    }

    freeaddrinfo(servinfo);

    if (socket_servidor == -1) {
        log_error(logger, "iniciar_servidor: no se pudo bindear el socket en puerto %s", puerto);
        return -1;
    }

    if (listen(socket_servidor, SOMAXCONN) == -1) {
        log_error(logger, "iniciar_servidor: listen() fallo en puerto %s", puerto);
        close(socket_servidor);
        return -1;
    }

    log_info(logger, "Servidor escuchando en el puerto %s (fd=%d)", puerto, socket_servidor);
    return socket_servidor;
}

int esperar_cliente(int socket_servidor, t_log* logger) {
    struct sockaddr_storage direccion_cliente;
    socklen_t tamanio_direccion = sizeof(direccion_cliente);

    int socket_cliente = accept(socket_servidor, (struct sockaddr*) &direccion_cliente, &tamanio_direccion);
    if (socket_cliente == -1) {
        log_error(logger, "esperar_cliente: accept() fallo (errno=%d)", errno);
        return -1;
    }

    log_trace(logger, "esperar_cliente: nueva conexion aceptada (fd=%d)", socket_cliente);
    return socket_cliente;
}

/* ============================================================
 *  4. CLIENTE
 * ============================================================ */

int crear_conexion(char* ip, char* puerto, t_log* logger) {
    struct addrinfo hints, *servinfo, *p;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    if (getaddrinfo(ip, puerto, &hints, &servinfo) != 0) {
        log_error(logger, "crear_conexion: getaddrinfo() fallo para %s:%s", ip, puerto);
        return -1;
    }

    int socket_cliente = -1;
    for (p = servinfo; p != NULL; p = p->ai_next) {
        socket_cliente = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
        if (socket_cliente == -1) {
            continue;
        }

        if (connect(socket_cliente, p->ai_addr, p->ai_addrlen) == -1) {
            close(socket_cliente);
            socket_cliente = -1;
            continue;
        }

        break; // connect exitoso
    }

    freeaddrinfo(servinfo);

    if (socket_cliente == -1) {
        log_error(logger, "crear_conexion: no se pudo conectar a %s:%s", ip, puerto);
        return -1;
    }

    log_trace(logger, "crear_conexion: conectado a %s:%s (fd=%d)", ip, puerto, socket_cliente);
    return socket_cliente;
}

void liberar_conexion(int socket_cliente) {
    if (socket_cliente < 0) {
        return;
    }
    shutdown(socket_cliente, SHUT_RDWR);
    close(socket_cliente);
}

/* ============================================================
 *  5. PAQUETES
 * ============================================================ */

t_paquete* crear_paquete(op_code codigo) {
    t_paquete* paquete = malloc(sizeof(t_paquete));

    paquete->codigo_operacion = codigo;
    paquete->buffer = malloc(sizeof(t_buffer));
    paquete->buffer->size = 0;
    paquete->buffer->stream = NULL;

    return paquete;
}

void agregar_a_paquete(t_paquete* paquete, void* datos, int tamanio) {
    paquete->buffer->stream = realloc(
        paquete->buffer->stream,
        paquete->buffer->size + tamanio + sizeof(int)
    );

    memcpy(paquete->buffer->stream + paquete->buffer->size, &tamanio, sizeof(int));
    memcpy(paquete->buffer->stream + paquete->buffer->size + sizeof(int), datos, tamanio);

    paquete->buffer->size += tamanio + sizeof(int);
}

void eliminar_buffer(t_buffer* buffer) {
    if (buffer == NULL) {
        return;
    }
    if (buffer->stream != NULL) {
        free(buffer->stream);
    }
    free(buffer);
}

void eliminar_paquete(t_paquete* paquete) {
    if (paquete == NULL) {
        return;
    }
    eliminar_buffer(paquete->buffer);
    free(paquete);
}

/* ============================================================
 *  6. ENVIO / RECEPCION
 * ============================================================ */

void enviar_paquete(t_paquete* paquete, int socket_cliente) {
    uint32_t tamanio_stream = paquete->buffer->size;
    uint32_t tamanio_total = sizeof(op_code) + sizeof(uint32_t) + tamanio_stream;

    void* a_enviar = malloc(tamanio_total);
    uint32_t offset = 0;

    memcpy(a_enviar + offset, &(paquete->codigo_operacion), sizeof(op_code));
    offset += sizeof(op_code);

    memcpy(a_enviar + offset, &tamanio_stream, sizeof(uint32_t));
    offset += sizeof(uint32_t);

    if (tamanio_stream > 0) {
        memcpy(a_enviar + offset, paquete->buffer->stream, tamanio_stream);
    }

    if (enviar_bytes(socket_cliente, a_enviar, tamanio_total) == -1) {
        // El llamador puede detectar la caida en el proximo recv();
        // aca solo evitamos crashear el proceso.
    }

    free(a_enviar);
}

void enviar_mensaje(char* mensaje, op_code codigo, int socket_cliente) {
    t_paquete* paquete = crear_paquete(codigo);
    agregar_a_paquete(paquete, mensaje, strlen(mensaje) + 1);
    enviar_paquete(paquete, socket_cliente);
    eliminar_paquete(paquete);
}

int recibir_operacion(int socket_cliente) {
    op_code codigo;
    if (recibir_bytes(socket_cliente, &codigo, sizeof(op_code)) == -1) {
        return -1; // conexion cerrada o error de socket
    }
    return codigo;
}

void* recibir_buffer(uint32_t* size, int socket_cliente) {
    if (recibir_bytes(socket_cliente, size, sizeof(uint32_t)) == -1) {
        return NULL;
    }

    if (*size == 0) {
        return NULL; // buffer vacio (ej. handshakes), nada que reservar
    }

    void* buffer = malloc(*size);
    if (recibir_bytes(socket_cliente, buffer, *size) == -1) {
        free(buffer);
        return NULL;
    }

    return buffer;
}

char* recibir_mensaje_texto(int socket_cliente) {
    uint32_t size;
    void* buffer = recibir_buffer(&size, socket_cliente);
    if (buffer == NULL) {
        return NULL;
    }

    // El buffer trae el formato [int tamanio_string][string].
    // Saltamos el header de 'agregar_a_paquete' para quedarnos con el string.
    uint32_t tamanio_string = size - sizeof(int);
    char* mensaje = malloc(tamanio_string);
    memcpy(mensaje, buffer + sizeof(int), tamanio_string);

    free(buffer);
    return mensaje;
}

/* ============================================================
 *  7. HANDSHAKE
 * ============================================================ */

char* nombre_modulo(op_code identificacion) {
    switch (identificacion) {
        case HANDSHAKE_CORE:         return "CORE";
        case HANDSHAKE_PLANIFICADOR: return "PLANIFICADOR";
        case HANDSHAKE_PLACA:        return "PLACA";
        case HANDSHAKE_STORAGE:      return "STORAGE";
        default:                     return "DESCONOCIDO";
    }
}

int realizar_handshake(int socket_cliente, op_code identificacion_propia, t_log* logger) {
    t_paquete* paquete = crear_paquete(identificacion_propia);
    enviar_paquete(paquete, socket_cliente);
    eliminar_paquete(paquete);

    int respuesta = recibir_operacion(socket_cliente);

    if (respuesta == -1) {
        log_error(logger, "realizar_handshake: se perdio la conexion esperando confirmacion");
        return -1;
    }

    if (respuesta != HANDSHAKE_OK) {
        log_error(logger, "realizar_handshake: el servidor rechazo la identificacion como %s",
                   nombre_modulo(identificacion_propia));
        return -1;
    }

    log_info(logger, "Handshake exitoso. Identificado como %s", nombre_modulo(identificacion_propia));
    return 0;
}

int recibir_handshake(int socket_cliente, op_code identificacion_esperada, t_log* logger) {
    int codigo_recibido = recibir_operacion(socket_cliente);

    if (codigo_recibido == -1) {
        log_error(logger, "recibir_handshake: se perdio la conexion durante el handshake");
        return -1;
    }

    // El paquete de handshake no trae payload util, pero hay que
    // consumir el [size] (y el stream si lo hubiera) para no desalinear
    // la lectura del proximo mensaje en este mismo socket.
    uint32_t size;
    void* buffer = recibir_buffer(&size, socket_cliente);
    if (buffer != NULL) {
        free(buffer);
    }

    if (codigo_recibido != (int) identificacion_esperada) {
        log_warning(logger, "recibir_handshake: se esperaba %s y llego el codigo %d",
                     nombre_modulo(identificacion_esperada), codigo_recibido);

        op_code error = HANDSHAKE_ERROR;
        enviar_bytes(socket_cliente, &error, sizeof(op_code));
        return -1;
    }

    op_code ok = HANDSHAKE_OK;
    if (enviar_bytes(socket_cliente, &ok, sizeof(op_code)) == -1) {
        log_error(logger, "recibir_handshake: no se pudo confirmar el handshake, conexion caida");
        return -1;
    }

    log_info(logger, "Handshake recibido correctamente de %s", nombre_modulo(codigo_recibido));
    return 0;
}
