#ifndef PLANIFICADOR_CONFIG_H_
#define PLANIFICADOR_CONFIG_H_

/*
 * Estructura con todos los parámetros de configuración del
 * Planificador, tal como los pide el enunciado del TP.
 *
 * Se guardan como copias propias (strdup) y no como punteros al
 * t_config interno, para poder liberar el t_config apenas se
 * termina de leer y no acoplar el resto del programa a commons/config.
 */
typedef struct
{
    int puerto_escucha;

    char *ip_placa;
    int puerto_placa;

    char *ip_storage;
    int puerto_storage;

    char *log_level;

    char *algoritmo_planificacion;
    int rr_quantum;
    int estimacion_inicial;
    double hrrn_alfa;
    int grado_multiprogramacion;
    char *path_datos;
    int retardo_loader;
    char *path_reportes;
} t_config_planificador;

// Carga y valida la configuración desde el archivo indicado por
// path_archivo. Si algo falla (archivo inexistente, clave faltante,
// malloc, etc) loguea el error y devuelve NULL.
t_config_planificador *cargar_config_planificador(char *path_archivo);

// Libera toda la memoria asociada a la configuración cargada.
// config puede ser NULL.
void liberar_config_planificador(t_config_planificador *config);

#endif /* PLANIFICADOR_CONFIG_H_ */
