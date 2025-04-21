## HTTP_Server

### Integrantes: 
* Alejandro Hinestroza Gómez
* Samuel Gutiérrez Jaramillo
* Nicolás Ruíz Urrea

## 1. Introducción:

En este proyecto se presenta el desarrollo e implementación de un servidor HTTP/1.1 utilizando el lenguaje de programación C, para el estudio y un mejor entendimiento de la arquitectura TCP/IP. El objetivo fue construir un servidor que tuviera la capacidad de atender a distintos clientes al mismo tiempo, entregar recursos web y procesar solicitudes del protocolo HTTP.

Para llevar a cabo la implementación se utilizaron recursos como **sockets**, a través de la **librería Windows Sockets (Winsock)**, permitiendo la comunicación a nivel de red mediante conexiones TCP. Además, se hizo uso de **hilos (threads)** para gestionar múltiples clientes de manera simultanea, asignando a cada uno un espacio de ejecución diferente para permitir varios clientes al mismo tiempo.

El servidor permite interpretar y responder a los métodos HTTP **GET**, **HEAD** y **POST**, siguiendo la especificación del estándar HTTP/1.1. Además, se implementó el manejo adecuado de errores mediante los códigos de respuesta **200 OK**, **400 Bad Request** y **404 Not Found.**.

Por último, se incluyó un sistema de **registro de logs**, que permite llevar un historial de todas las peticiones recibidas y las respuestas enviadas por el servidor, lo que permite el monitoreo de las conexiones y peticiones realizadas en todo momento.

Este proyecto se desarrolló como parte del curso de **Telemática**, y se desplegó usando la infraestructura de AWS.


## 2. Desarrollo/implementación: 
### Librerías: 
**<stdio.h>**  - Para funciones como: `printf()`, `sprintf()`, `fopen()`, `fprintf()`, `fclose()`,

**<stdlib.h>** - Para funciones como: `malloc()`, `free()`, `atoi()`

**<string.h>** - Para funciones como las siguientes: 

- `WSAStartup()` → inicializa Winsock.
- `socket()`, `bind()`, `listen()`, `accept()` → funciones base de servidor TCP.
- `recv()`, `send()` → para leer y enviar datos por sockets.
- `closesocket()` → para cerrar sockets.

**<winsock2.h>** - Nos permite trabajar con Sockets en Windows, y sirve para utilizar funciones como las siguientes: 

- `socket()` → Para establecer canales de red
- `connect()` → Para conectarse a otros servidores
- `bind()` y `listen()` → para aceptar conexiones entrantes
- `accept()` → para recibir nuevos clientes
- `closesocket()` → para cerrar sockets
- `send()` y `recv()` → datos por red

**<time.h>** - Nos permite trabajar con el tiempo y las fechas, tanto para obtenerlas como para formatearlas de formas más legibles.

### Función Logger:
Esta función en términos generales se encarga de escribir y registrar todos los movimientos dentro del servidor, es decir, todos los métodos, mensajes, estados y demás que el servidor recibe, manda o procesa. Ahora tiene tres funciones claves: 

1. **Registrar los eventos del servidor** (errores, peticiones, respuestas, etc.)
2. Mostrar esos eventos por **consola**
3. Guardar dichos eventos en un archivo **.log**.

A la función le entran los siguientes parámetros: 

```c
void logger(const char *tipo, const char *metodo, const char *uri, int codigo_estado, const char *ip_cliente)
```

- `Tipo`: Tipo de mensaje ("ERROR", "REQUEST", "RESPONSE", "INFO")
- `Método`: Método HTTP ("GET", "POST", "HEAD")
- `uri`: Recurso  (ej. "/caso1.html")
- `codigo_estado`: Código HTTP (200, 404, 400)
- `ip_cliente`: IP del cliente que hizo la petición

La función obtiene el tiempo actual en formato ***epoch*** qué un formato que te devuelve un número entero que representa la cantidad de segundos que ha transcurrido desde el 1 de enero de 1970, y utiliza esta fecha por todo un trasfondo histórico de Unix. Formato que nos fue bastante útil, pues luego, a la hora local y lo formatea en una cadena legible, de forma que se pueda leer sin problemas, donde se incluye la fecha y seguidamente la hora actual. 

```c
time_t now = time(NULL);
    struct tm *tm_info = localtime(&now);
    char time_str[26];
    strftime(time_str, 26, "%Y-%m-%d %H:%M:%S", tm_info);
```

Luego, se crea una variable de 4096 caracteres de forma que podamos almacenar el mensaje que se va a escribir en dicha variable. Para al creación de este mensaje utilizamos la variable anteriormente mencionada que almacena la fecha actual y los parámetros de la función, seguidamente utilizamos el operador ternario de C para verificar si algún argumento es nulo (NULL) y en caso de que si deja ese argumento en el mensaje final representado por una cadena “vacía” (”-”).

```c
char mensaje[4096];
    sprintf(mensaje, "[%s] [%s] %s %s - %d - %s", 
            time_str, tipo, metodo ? metodo : "-", 
            uri ? uri : "-", codigo_estado, ip_cliente ? ip_cliente : "-");
```

Posteriormente mostramos el mensaje en la consola y abrimos el documento ***.log*** en formato append (”a”) para escriba al final de línea sin la necesidad de afectar todo lo que antes hubiera escrito. Verifica que formato si se pudo abrir y, en caso de que si, escribe el mensaje en su formato completo y, en caso de que no, lanza un mensaje de error por la terminal.

```c
FILE *log_file = fopen(log_file_path, "a");
    if (log_file != NULL) {
        fprintf(log_file, "%s\n", mensaje);
        fclose(log_file);
    } else {
        printf("ERROR: No se pudo abrir el archivo de log '%s'\n", log_file_path);
    }
```

### Función Manejar Cliente

El manejo de clientes se va a hacer a través de un sistema **Thread Based.** De esa forma cada que un cliente se conecté al servidor y empiece a solicitar recursos se le va a asignar un espacio de memoria dinámica en el cual a ese cliente se le va a dar un tratamiento de memoria exclusivamente por ese hilo. Aspecto que es muy positivo para la concurrencia del servidor puesto que cuando entre otro cliente a solicitar recursos se le asignará otro espacios de memoria diferente y se le atenderá en un hilo distinto, lo que finalmente permite que el servidor funcione con cierta concurrencia de manera paralela atendiendo a varios clientes a la vez y a todos de manera separada.

```c
DWORD WINAPI manejar_cliente(LPVOID cliente_socket_ptr) {
```
Esta función se ejecuta en un hilo diferente por cada cliente. Se le pasa un puntero al socket (la conexión con el navegador)

```c
    SOCKET client_socket = (SOCKET)cliente_socket_ptr;
    printf("Atendiendo cliente en hilo ID: %lu\n", GetCurrentThreadId());
    free(cliente_socket_ptr);  // Libera la memoria reservada
```
Estas líneas obtienen la dirección IP del cliente que se conectó 

```c
    char request[2000];
    int recv_size = recv(client_socket, request, sizeof(request) - 1, 0);
```
Con estas líneas se recibe el mensaje que envió el navegador

```c
    if (recv_size > 0) {
        request[recv_size] = '\0';
        printf("Peticion recibida:\n%s\n", request);
```
Aquí se termina el texto recibido por parte del navegador y se imprime todo el request

```c
        char metodo[8];
        char recurso[1024];
        char version[16];
        if (sscanf(request, "%s %s %s", metodo, recurso, version) != 3) {
```
Se extraen tres partes del mensaje:
- El método (GET, POST, etc.)
- Los recursos los cuales son los HTML
- La versión (HTTP/1.1)

Si no se logra extraer bien, responde con un error 400 que se presenta en texto en la página.


### Función Main

```c
    server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket == INVALID_SOCKET) {
        printf("No se pudo crear el socket. Error: %d\n", WSAGetLastError());
        logger("ERROR", NULL, NULL, WSAGetLastError(), "Server");
        return 1;
    }
```
Aquí se crea el "canal de comunicación" del servidor, usando:
- AF_INET: indica que es red IPv4.
- SOCK_STREAM: para conexión tipo TCP.
- 0: protocolo por defecto.

Si falla, se muestra un error y se detiene el programa.

```c
    server.sin_family = AF_INET;
    server.sin_addr.s_addr = INADDR_ANY;
    server.sin_port = htons(puerto);
```
Aquí se configuran los parámetros de red:
- AF_INET: familia de direcciones IPv4.
- INADDR_ANY: acepta conexiones desde cualquier IP.
- htons(puerto): convierte el puerto a formato de red.

Esto indica dónde va a escuchar el servidor.

```c
    if (bind(server_socket, (struct sockaddr *)&server, sizeof(server)) == SOCKET_ERROR) {
        printf("Bind fallo. Error: %d\n", WSAGetLastError());
        logger("ERROR", NULL, NULL, WSAGetLastError(), "Server");
        return 1;
    }
```
Asocia el socket con la dirección IP y puerto configurados.
Si no puede hacerlo (por ejemplo, si el puerto ya está en uso), imprime error y sale.

```c
    listen(server_socket, 3);
    printf("Esperando conexiones en el puerto 8080...\n");
    logger("INFO", "SERVER", "LISTENING", puerto, "Server");
```
Activa el socket para comenzar a recibir conexiones.
- El 3 indica cuántas conexiones puede mantener en cola.
- Se imprime y se registra en el log que el servidor está escuchando.

```c
    c = sizeof(struct sockaddr_in);
    while ((client_socket = accept(server_socket, (struct sockaddr *)&client, &c)) != INVALID_SOCKET) {
        printf("Cliente conectado.\n");
```
- Guarda el tamaño de la estructura client.
- Entra en un bucle infinito donde acepta conexiones (accept).
- Cuando un cliente se conecta, lo imprime en consola.

```c
        SOCKET *nuevo_cliente = malloc(sizeof(SOCKET));
        if (nuevo_cliente == NULL) {
            printf("Error al asignar memoria para el socket del cliente.\n");
            logger("ERROR", "SERVER", "MEMORY", 0, "Server");
            closesocket(client_socket);
            continue;
        }
```
- Reserva memoria para guardar el socket del cliente (porque se lo va a pasar a un hilo).
- Si no hay memoria disponible, muestra error, lo registra y vuelve a esperar otro cliente.

Para el manejo de los clientes por hilos, necesitamos propiamente crear los hilos, para ello se utilizan dos tipos de variables especiales que nos permitan manejar y acceder a recursos de nuestra máquina de windows que son las que gestionan los hilos en el sistema operativo. Esas son las siguientes: 

- *HANDLE* → Este es un tipo  de puntero que representa *una referencia a un recurso del sistema operativo,* en este caso, del acceso y el manejo de hilos.
- DWORD → "Double Word" = entero sin signo de *32 bits.* Esta es la forma que maneja Windows para gestionar el ID de los hilos.

Posteriormente se utiliza la función **CreateThread()* propia de la librería *winsock2.h.** A dicha función le entra como parámetro los siguientes elementos: 

- *Atributo de seguridad:* Un aspecto donde se definen, por ejemplo, que puede acceder a esta sección, para este caso se pone *NULL* y toma los atributos por defecto.
- *Tamaño del Stack:* Define el *tamaño del stack* del nuevo hilo. El stack es donde se almacenan:
    - Variables locales
    - Llamadas a funciones
    - Dirección de retorno
- *Función que ejecutará el hilo:* Aquí se le pasa la función de **manejar_cliente**
- *Cuando inicial el hilo:* Aquí se pone 0 para que el hilo inicie su ejecución de forma inmediata.
- *ID del hilo:* Aquí se le pasa el ID del hilo, es como su elemento identificador.

Luego con un condicional se verifica si la creación del hilo fue fallida (NULL), y en caso de que lo sea pues lanza un mensaje de error por consola, escribe un mensaje de error en el *.log,* cierra el socket del cliente y libera la memoria que un principio se le fue asignada a ese socket. En caso de que la creación fuese exitosa se cierra el handle, puesto que ya no necesitamos recursos de nuestra máquina de Windows propiamente, debido a que ahora el hilo se va a ejecutar hasta que la función de **manejar_cliente** termine su ejecución.

```c
        HANDLE hilo;
        DWORD id_hilo;
        hilo = CreateThread(
            NULL,                
            0,                   
            manejar_cliente,     
            nuevo_cliente,       
            0,                  
            &id_hilo             
        );
    
        if (hilo == NULL) {
            printf("Error al crear el hilo. Código de error: %ld\n", GetLastError());
            logger("ERROR", "SERVER", "THREAD", GetLastError(), "Server");
            closesocket(client_socket);
            free(nuevo_cliente);
        } else {
            CloseHandle(hilo);  
        }
```


## 3. Conclusiones: 
* Después de investigar e implementar el protocolo HTTP/1.1, hemos logrado una mejor comprensión sobre la gestión de métodos comunes como **GET** & **POST**. Aunque estos métodos son habituales en frameworks como Django, donde la capa de bajo nivel está abstraída, como desarrolladores no solíamos dimensionar la complejidad y cantidad de procesos subyacentes. Este proyecto nos ha permitido entender estos métodos con mayor profundidad.
* El hecho de interactuar con Sockets ha permitido que veamos una imagen más grande sobre todas las funcionalidades que proveen los sistemas operativos. Aspectos como el manejo de la memoria dinámica y el tratamiento de hilos impactaron positivamente en nuestra comprensión, en el sentido de que ahora podemos responder con mayor claridad sobre cómo el computador gestiona y almacena los datos.
* Programar en C representó un desafío significativo, ya que ha sido un lenguaje poco utilizado por nosotros durante la carrera. Al ser un lenguaje de bajo nivel, requiere de mucha atención y nos exigió a nosotros como programadores una constante investigación y comprensión profunda de cada aspecto de lo que se quería implementar. Además, la sintaxis es compleja, y enfrentamos diversos obstáculos, como la importación de librerías o el uso de ***return 0*** para  indicar que ha acabado la ejecución sin errores ni problemas.
* Desplegar el proyecto en AWS fue otro de los retos enfrentados, porque aunque antes habíamos interactuado con plataformas de computación en la nube como **GCP,** el hecho de desplegar el servidor en una máquina virtual supuso que aprendiéramos bastante sobre como funcionaba la plataforma, hasta el momento desconocida para nosotros, y su correcta configuración para garantizar el funcionamiento óptimo del servidor.

## 4. Referencias: 
* Helping Hands. (2025, febrero 13). How to Create Windows Server AWS EC2 Instance ? [Video]. YouTube. https://www.youtube.com/watch?v=aKixxEN0xTw
* Bruins Slot, J.P. (2021, noviembre 29). Making a simple HTTP webserver in C. Jan Pieter Bruins Slot. https://bruinsslot.jp/post/simple-http-webserver-in-c/
* La Chica de Sistemas. (2024, abril 05). HACEMOS UN SERVER HTTP EN C de 200 LÍNEAS (LINUX) [Video]. YouTube. https://www.youtube.com/watch?v=OOugpd0BqT0
