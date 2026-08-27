#include "../include/planificador_config.h"

#include <commons/config.h>
#include <commons/log.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Logger global (definido en main.c). Se usa acá para poder loguear
// errores de configuración incluso antes de que el resto del sistema
// esté levantado.
extern t_log *logger;

// Lista de claves obligatorias que debe tener el archivo de config.
// Se valida antes de intentar leer cualquier valor, para poder dar
// un único mensaje de error claro en vez de fallar a mitad de camino.
static char *CLAVES_OBLIGATORIAS[] = {
    "PUERTO_ESCUCHA",
    "IP_PLACA",
    "PUERTO_PLACA",
    "IP_STORAGE",
    "PUERTO_STORAGE",
    "LOG_LEVEL",
    "ALGORITMO_PLANIFICACION",
    "RR_QUANTUM",
    "ESTIMACION_INICIAL",
    "HRRN_ALFA",
    "GRADO_MULTIPROGRAMACION",
    "PATH_DATOS",
    "RETARDO_LOADER",
    "PATH_REPORTES",
    NULL};

static int config_tiene_todas_las_claves(t_config *config)
{
    for (int i = 0; CLAVES_OBLIGATORIAS[i] != NULL; i++)
    {
        if (!config_has_property(config, CLAVES_OBLIGATORIAS[i]))
        {
            if (logger != NULL)
            {
                log_error(logger, "Falta la clave obligatoria '%s' en el archivo de configuración",
                        CLAVES_OBLIGATORIAS[i]);
            }
            else
            {
                fprintf(stderr, "Falta la clave obligatoria '%s' en el archivo de configuración\n",
                        CLAVES_OBLIGATORIAS[i]);
            }
            return 0;
        }
    }
    return 1;
}

t_config_planificador *cargar_config_planificador(char *path_archivo)
{
    if (path_archivo == NULL)
    {
        fprintf(stderr, "Path de archivo de configuración inválido (NULL)\n");
        return NULL;
    }

    t_config *config = config_create(path_archivo);
    if (config == NULL)
    {
        // config_create de commons ya suele loguear el motivo por consola,
        // igual dejamos constancia explícita del path que falló.
        fprintf(stderr, "No se pudo abrir/parsear el archivo de configuración: %s\n", path_archivo);
        return NULL;
    }

    if (!config_tiene_todas_las_claves(config))
    {
        config_destroy(config);
        return NULL;
    }

    t_config_planificador *cfg = malloc(sizeof(t_config_planificador));
    if (cfg == NULL)
    {
        if (logger != NULL)
        {
            log_error(logger, "No se pudo reservar memoria para la configuración del planificador");
        }
        config_destroy(config);
        return NULL;
    }

    cfg->puerto_escucha = config_get_int_value(config, "PUERTO_ESCUCHA");

    cfg->ip_placa = strdup(config_get_string_value(config, "IP_PLACA"));
    cfg->puerto_placa = config_get_int_value(config, "PUERTO_PLACA");

    cfg->ip_storage = strdup(config_get_string_value(config, "IP_STORAGE"));
    cfg->puerto_storage = config_get_int_value(config, "PUERTO_STORAGE");

    cfg->log_level = strdup(config_get_string_value(config, "LOG_LEVEL"));

    cfg->algoritmo_planificacion = strdup(config_get_string_value(config, "ALGORITMO_PLANIFICACION"));
    cfg->rr_quantum = config_get_int_value(config, "RR_QUANTUM");
    cfg->estimacion_inicial = config_get_int_value(config, "ESTIMACION_INICIAL");
    // so-commons-library no tiene config_get_double_value: se parsea
    // el string manualmente con atof (HRRN_ALFA es un valor entre 0 y 1).
    cfg->hrrn_alfa = atof(config_get_string_value(config, "HRRN_ALFA"));
    cfg->grado_multiprogramacion = config_get_int_value(config, "GRADO_MULTIPROGRAMACION");
    cfg->path_datos = strdup(config_get_string_value(config, "PATH_DATOS"));
    cfg->retardo_loader = config_get_int_value(config, "RETARDO_LOADER");
    cfg->path_reportes = strdup(config_get_string_value(config, "PATH_REPORTES"));

    config_destroy(config);

    // Chequeo defensivo: si algún strdup falló (malloc sin memoria),
    // liberamos todo y devolvemos NULL en vez de arrastrar punteros NULL.
    if (cfg->ip_placa == NULL || cfg->ip_storage == NULL || cfg->log_level == NULL ||
        cfg->algoritmo_planificacion == NULL || cfg->path_datos == NULL || cfg->path_reportes == NULL)
    {
        if (logger != NULL)
        {
            log_error(logger, "No se pudo reservar memoria para copiar valores de configuración");
        }
        liberar_config_planificador(cfg);
        return NULL;
    }

    return cfg;
}

void liberar_config_planificador(t_config_planificador *config)
{
    if (config == NULL)
    {
        return;
    }

    free(config->ip_placa);
    free(config->ip_storage);
    free(config->log_level);
    free(config->algoritmo_planificacion);
    free(config->path_datos);
    free(config->path_reportes);
    free(config);
}
