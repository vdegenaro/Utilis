#include "../include/cliente.h"
#include "../include/planificador_config.h"
#include "../include/protocolo.h"
#include "../include/servidor.h"

#include <commons/log.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

// Logger global: lo usan protocolo.c, cliente.c y servidor.c también
// (declarado ahí como extern), para no tener que pasarlo como
// parámetro por todo el código.
t_log *logger = NULL;

// Traduce el string LOG_LEVEL del config a t_log_level de commons.
// Ante un valor no reconocido, cae a LOG_LEVEL_INFO por defecto.
static t_log_level parsear_log_level(char *nivel)
{
    if (strcmp(nivel, "TRACE") == 0) return LOG_LEVEL_TRACE;
    if (strcmp(nivel, "DEBUG") == 0) return LOG_LEVEL_DEBUG;
    if (strcmp(nivel, "INFO") == 0) return LOG_LEVEL_INFO;
    if (strcmp(nivel, "WARNING") == 0) return LOG_LEVEL_WARNING;
    if (strcmp(nivel, "ERROR") == 0) return LOG_LEVEL_ERROR;
    return LOG_LEVEL_INFO;
}

int main(int argc, char *argv[])
{
    // ------------------------------------------------------------
    // 1) Validación y parseo de argumentos de consola
    //    ./bin/planificador [Archivo Config] [Path Job Inicial]
    // ------------------------------------------------------------
    if (argc < 3)
    {
        fprintf(stderr, "Uso: %s [Archivo Config] [Path Job Inicial]\n", argv[0]);
        return EXIT_FAILURE;
    }

    char *path_config = argv[1];
    char *path_job_inicial = argv[2];

    // ------------------------------------------------------------
    // 2) Logger provisorio a INFO para poder loguear errores de
    //    config incluso antes de conocer el LOG_LEVEL real.
    // ------------------------------------------------------------
    logger = log_create("planificador.log", "PLANIFICADOR", true, LOG_LEVEL_INFO);
    if (logger == NULL)
    {
        fprintf(stderr, "No se pudo crear el logger del planificador\n");
        return EXIT_FAILURE;
    }

    // ------------------------------------------------------------
    // 3) Carga de configuración
    // ------------------------------------------------------------
    t_config_planificador *config = cargar_config_planificador(path_config);
    if (config == NULL)
    {
        log_error(logger, "No se pudo cargar la configuración desde '%s'. Abortando.", path_config);
        log_destroy(logger);
        return EXIT_FAILURE;
    }

    // Recreamos el logger con el nivel real definido en el config,
    // ya con el archivo LOG_LEVEL parseado correctamente.
    log_destroy(logger);
    logger = log_create("planificador.log", "PLANIFICADOR", true, parsear_log_level(config->log_level));
    if (logger == NULL)
    {
        fprintf(stderr, "No se pudo recrear el logger con LOG_LEVEL='%s'\n", config->log_level);
        liberar_config_planificador(config);
        return EXIT_FAILURE;
    }

    log_info(logger, "Configuración cargada correctamente desde '%s'", path_config);
    log_info(logger, "Job inicial a cargar: '%s'", path_job_inicial);

    // TODO CC2: acá se debería parsear/validar el job inicial
    // (path_job_inicial) y encolarlo en NEW antes de arrancar a
    // planificar. Para CC1 solo se deja registrado el path.

    // ------------------------------------------------------------
    // 4) Conexión como cliente a Placa y Storage
    //    (Requisito CC1: deben conectarse ANTES de aceptar Cores,
    //    para asegurar que el clúster esté completo).
    // ------------------------------------------------------------
    int socket_placa = conectar_a_modulo(config->ip_placa, config->puerto_placa, "PLACA");
    if (socket_placa == -1)
    {
        log_error(logger, "No se pudo establecer conexión con la Placa. Abortando.");
        liberar_config_planificador(config);
        log_destroy(logger);
        return EXIT_FAILURE;
    }

    int socket_storage = conectar_a_modulo(config->ip_storage, config->puerto_storage, "STORAGE");
    if (socket_storage == -1)
    {
        log_error(logger, "No se pudo establecer conexión con el Storage. Abortando.");
        close(socket_placa);
        liberar_config_planificador(config);
        log_destroy(logger);
        return EXIT_FAILURE;
    }

    // TODO CC3/CC4: guardar socket_placa / socket_storage en algún
    // estado global (o pasarlos a un módulo de "conexiones") para
    // que el resto del sistema (paginación, journaling) los use
    // más adelante en vez de descartarlos acá.

    // ------------------------------------------------------------
    // 5) Levantar servidor multihilo para los Cores
    // ------------------------------------------------------------
    int socket_servidor = iniciar_servidor(config->puerto_escucha);
    if (socket_servidor == -1)
    {
        log_error(logger, "No se pudo iniciar el servidor de escucha. Abortando.");
        close(socket_placa);
        close(socket_storage);
        liberar_config_planificador(config);
        log_destroy(logger);
        return EXIT_FAILURE;
    }

    log_info(logger, "Planificador inicializado correctamente. Esperando conexiones de Cores...");

    // Bloquea el hilo principal aceptando conexiones de Cores;
    // cada una se atiende en su propio hilo (ver servidor.c).
    escuchar_conexiones(socket_servidor);

    // ------------------------------------------------------------
    // 6) Liberación de recursos (en la práctica, este punto solo se
    //    alcanza si escuchar_conexiones() retornara, lo cual hoy no
    //    ocurre salvo que se agregue una señal de corte prolijo).
    // ------------------------------------------------------------
    close(socket_servidor);
    close(socket_placa);
    close(socket_storage);
    liberar_config_planificador(config);
    log_destroy(logger);

    return EXIT_SUCCESS;
}
