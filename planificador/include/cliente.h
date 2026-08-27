#ifndef CLIENTE_H_
#define CLIENTE_H_

/*
 * Lógica de conexión saliente del Planificador hacia otros
 * módulos servidor (Placa y Storage). El Planificador actúa
 * como cliente TCP en estos dos casos.
 */

// Crea un socket, se conecta a ip:puerto y realiza el handshake
// de identificación (HANDSHAKE_PLANIFICADOR) contra el módulo remoto,
// identificado por nombre_modulo_destino solo para logging.
//
// Devuelve el file descriptor del socket ya conectado y con
// handshake validado, o -1 si falló la conexión o el handshake.
int conectar_a_modulo(char *ip, int puerto, char *nombre_modulo_destino);

#endif /* CLIENTE_H_ */
