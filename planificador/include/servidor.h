#ifndef SERVIDOR_H_
#define SERVIDOR_H_

/*
 * Servidor multihilo del Planificador: escucha en PUERTO_ESCUCHA
 * y acepta conexiones entrantes de múltiples Cores (y, en general,
 * de cualquier módulo que quiera identificarse por handshake).
 *
 * Cada conexión aceptada se atiende en un hilo (pthread) propio,
 * de forma que el aceptar-nuevas-conexiones nunca se bloquea
 * esperando a que un Core termine de mandar mensajes.
 */

// Crea, bindea y pone en escucha un socket TCP en el puerto dado.
// Devuelve el file descriptor del socket servidor, o -1 en error.
int iniciar_servidor(int puerto);

// Loop principal de aceptación de conexiones. Bloquea el hilo que
// la invoca (pensada para correr en el hilo principal de main).
// Por cada conexión aceptada, lanza un hilo que ejecuta
// atender_cliente() y queda desacoplado (pthread_detach).
//
// socket_servidor debe venir de iniciar_servidor().
void escuchar_conexiones(int socket_servidor);

#endif /* SERVIDOR_H_ */
