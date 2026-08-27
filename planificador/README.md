# Planificador — EntrenadOS (Checkpoint 1)

Módulo Planificador: servidor multihilo para Cores + cliente hacia Placa y Storage,
con protocolo de paquetes serializados y handshake de identificación de módulo.

## Estructura

```
planificador/
├── include/
│   ├── planificador_config.h   # struct de config + carga desde archivo
│   ├── protocolo.h             # op_code, t_paquete, serialización, handshake
│   ├── servidor.h              # servidor multihilo (Cores)
│   └── cliente.h               # conexión saliente (Placa/Storage)
├── src/
│   ├── planificador_config.c
│   ├── protocolo.c
│   ├── servidor.c
│   ├── cliente.c
│   └── main.c
├── config/
│   ├── planificador.config     # config de ejemplo
│   └── job_ejemplo.txt         # job de ejemplo (path job inicial)
├── Makefile
└── README.md
```

## Requisitos

- `so-commons-library` instalada (`libcommons`), con headers en
  `/usr/include/commons/` y librería en el linker path (`sudo make install`
  desde el repo de so-commons-library, como se hace habitualmente en la cátedra).
- gcc con soporte pthread.

## Compilar

```bash
make
```

Genera `bin/planificador`.

## Ejecutar

```bash
./bin/planificador <archivo_config> <path_job_inicial>

# Ejemplo:
./bin/planificador config/planificador.config config/job_ejemplo.txt
```

También podés usar `make run`, que compila y corre con los archivos de ejemplo
de `config/`.

**Importante:** para que el Planificador arranque tienen que estar levantados
antes (o al menos aceptando conexiones) los módulos Placa y Storage en las
direcciones/puertos indicados por `IP_PLACA`/`PUERTO_PLACA` e
`IP_STORAGE`/`PUERTO_STORAGE` del config, porque el Planificador se conecta
a ellos como cliente antes de levantar su propio servidor para los Cores.

## Diseño del protocolo (`protocolo.h/.c`)

- **`op_code`**: enum con los tipos de mensaje. Incluye 4 opcodes de handshake
  (uno por módulo), `HANDSHAKE_OK`/`HANDSHAKE_ERROR` como respuesta, `MENSAJE`
  como placeholder genérico de CC1, y `PAQUETE_DESCONEXION` para cierre prolijo.
  Los opcodes de CC2/CC3/CC4 se agregan marcados con `TODO CCx` dentro del enum.
- **`t_paquete`**: `{ op_code, tamanio_buffer, buffer }`. El buffer se arma con
  `agregar_a_paquete()`, que serializa cada campo como `[tamaño][datos]`, así se
  pueden empaquetar múltiples valores heterogéneos (ints, strings, structs) en
  un mismo paquete, y desempaquetarlos después con `extraer_de_buffer()`
  llevando un offset.
- **Envío/recepción**: `enviar_bytes`/`recibir_bytes` (privadas) hacen loops
  sobre `send`/`recv` para garantizar que se transmitan/reciban exactamente
  los bytes esperados, algo indispensable con sockets TCP (los envíos pueden
  ser parciales). `recibir_paquete()` devuelve `NULL` tanto en desconexión
  prolija del peer como en error de socket, así el que llama solo necesita
  chequear `== NULL` para cortar su loop.
- **Handshake**: `realizar_handshake_cliente()` (lo usa `cliente.c`) manda el
  opcode de la propia identidad y espera `HANDSHAKE_OK`. Del lado servidor,
  `realizar_handshake_servidor()` (lo usa `servidor.c`) recibe el opcode,
  valida que sea uno de los 4 válidos, **loguea el mensaje obligatorio**
  `"## Módulo: <NOMBRE_MODULO_CONECTADO>"` y responde `HANDSHAKE_OK`/`ERROR`.

## Servidor multihilo (`servidor.c`)

- `iniciar_servidor()`: crea el socket, setea `SO_REUSEADDR`, hace `bind` +
  `listen` sobre `PUERTO_ESCUCHA`.
- `escuchar_conexiones()`: loop infinito de `accept()`. Por cada conexión
  aceptada crea un `pthread` (`atender_cliente`) y lo desacopla con
  `pthread_detach` (no se necesita `join`: cada hilo libera sus propios
  recursos al terminar, y el hilo principal sigue aceptando conexiones nuevas
  sin bloquearse).
- `atender_cliente()`: hace el handshake, y luego entra en un loop de
  `recibir_paquete()` hasta que el módulo remoto se desconecta (ya sea
  avisando con `PAQUETE_DESCONEXION` o cortando la conexión de forma
  abrupta, ambos casos se manejan y loguean sin colgar el hilo ni crashear
  el proceso).

## Cliente (`cliente.c`)

`conectar_a_modulo(ip, puerto, nombre)`: resuelve la dirección con
`getaddrinfo` (soporta IPv4/IPv6 e incluso `localhost`), conecta, y realiza el
handshake identificándose como `HANDSHAKE_PLANIFICADOR`. Se usa dos veces
desde `main.c`: una para Placa y otra para Storage.

## Manejo de errores implementado

- Archivo de config inexistente o con claves faltantes → error claro y `exit`
  controlado (sin dejar sockets o memoria colgando).
- Fallas de `malloc`/`realloc` en la capa de paquetes → se loguea y se aborta
  de forma controlada (evita corromper memoria intentando seguir con punteros
  inválidos).
- Sockets caídos durante `send`/`recv` → se detectan por el valor de retorno
  y se propagan como error (`-1`) o desconexión (`NULL` en `recibir_paquete`),
  nunca se ignoran silenciosamente.
- `accept()` fallido puntualmente no tira abajo el servidor completo.
- Toda la memoria reservada (config, paquetes, buffers de campos) tiene su
  contraparte de liberación (`liberar_config_planificador`, `eliminar_paquete`).

## Puntos de extensión para próximos checkpoints

Buscá `TODO CC2`, `TODO CC3` y `TODO CC4` en el código (`grep -rn "TODO CC" src include`)
para ubicar exactamente dónde continúa el trabajo de:
- **CC2**: parseo real del job inicial, algoritmos de planificación (FIFO/RR/HRRN).
- **CC3**: paginación (requiere coordinar con Placa vía nuevos op_codes).
- **CC4**: journaling/persistencia (requiere coordinar con Storage).
