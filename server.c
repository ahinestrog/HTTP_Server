#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <winsock2.h>
#include <time.h>

#pragma comment(lib, "ws2_32.lib") // Enlazar la libreria de windows

// Variable global para la ruta del archivo de log
char log_file_path[1024] = "server.log";

// Función logger 
void logger(const char *tipo, const char *metodo, const char *uri, int codigo_estado, const char *ip_cliente) {
    // Obtener la hora actual (timestamp)
    time_t now = time(NULL);
    struct tm *tm_info = localtime(&now);
    char time_str[26];
    strftime(time_str, 26, "%Y-%m-%d %H:%M:%S", tm_info);
    
    // Crear el mensaje completo
    char mensaje[4096];
    sprintf(mensaje, "[%s] [%s] %s %s - %d - %s", 
            time_str, tipo, metodo ? metodo : "-", 
            uri ? uri : "-", codigo_estado, ip_cliente ? ip_cliente : "-");
    
    // Mostrar en consola
    printf("%s\n", mensaje);
    
    // Abre el archivo de log y escribe el mensaje que se acaba de generar
    FILE *log_file = fopen(log_file_path, "a");
    if (log_file != NULL) {
        fprintf(log_file, "%s\n", mensaje);
        fclose(log_file);
    } else {
        printf("ERROR: No se pudo abrir el archivo de log '%s'\n", log_file_path);
    }
}

// Función para manejar los clientes

DWORD WINAPI manejar_cliente(LPVOID cliente_socket_ptr) {
    SOCKET client_socket = *(SOCKET*)cliente_socket_ptr;
    printf("Atendiendo cliente en hilo ID: %lu\n", GetCurrentThreadId());
    free(cliente_socket_ptr);  // Liberar memoria asignada por malloc

    // Obtener IP del cliente para el log
    struct sockaddr_in client_info;
    int addrlen = sizeof(client_info);
    getpeername(client_socket, (struct sockaddr*)&client_info, &addrlen);
    char client_ip[20];
    strcpy(client_ip, inet_ntoa(client_info.sin_addr));

    char request[2000];
        int recv_size = recv(client_socket, request, sizeof(request) - 1, 0);
        if (recv_size > 0) {
            request[recv_size] = '\0';
            printf("Peticion recibida:\n%s\n", request);

            // Parseo de metodo, recurso y version
            char metodo[8];
            char recurso[1024];
            char version[16];
            if (sscanf(request, "%s %s %s", metodo, recurso, version) != 3) {
                // Si no se pudo parsear correctamente, enviar 400
                logger("ERROR", "UNKNOWN", "UNKNOWN", 400, client_ip);
                char *bad_request = 
                    "HTTP/1.1 400 Bad Request\r\n"
                    "Content-Type: text/html\r\n\r\n"
                    "<html><body><h1>400 Peticion incorrecta</h1></body></html>";
                send(client_socket, bad_request, strlen(bad_request), 0);
                closesocket(client_socket);
                return 0;
            }

            // Registrar la petición recibida
            logger("REQUEST", metodo, recurso, 0, client_ip);

            // *** CAMBIO 2: Procesar los metodos HTTP
            if (strcmp(metodo, "GET") == 0 || strcmp(metodo, "HEAD") == 0) {
                // Si se pide la raíz "/", cargar index.html
                if (strcmp(recurso, "/") == 0) {
                    strcpy(recurso, "index.html");
                } else {
                    // Eliminar el "/" inicial
                    memmove(recurso, recurso + 1, strlen(recurso));
                }

                // Ignorar parámetros tipo ?var=1
                char *params = strchr(recurso, '?');
                if (params) *params = '\0';

                // Armar la ruta completa al archivo
                char ruta_completa[2048];
                sprintf(ruta_completa, "www/%s", recurso);

                FILE *f = fopen(ruta_completa, "rb");
                if (f == NULL) {
                    // Archivo no encontrado → 404
                    logger("RESPONSE", metodo, recurso, 404, client_ip);
                    char *not_found = 
                        "HTTP/1.1 404 Not Found\r\n"
                        "Content-Type: text/html\r\n\r\n"
                        "<html><body><h1>404 No encontrado</h1></body></html>";
                    send(client_socket, not_found, strlen(not_found), 0);
                } else {
                    // Leer contenido del archivo
                    fseek(f, 0, SEEK_END);
                    long length = ftell(f);
                    rewind(f);
                    char *contenido = malloc(length + 1);
                    fread(contenido, 1, length, f);
                    contenido[length] = '\0';

                    // Cabecera HTTP 200 OK
                    char header[1024];
                    sprintf(header, "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\n\r\n");

                    send(client_socket, header, strlen(header), 0);
                    logger("RESPONSE", metodo, recurso, 200, client_ip);

                    // Enviar body solo si es GET
                    if (strcmp(metodo, "GET") == 0) {
                        send(client_socket, contenido, length, 0);
                    }

                    fclose(f);
                    free(contenido);
                }
            } 
            else if (strcmp(metodo, "POST") == 0) {
                // Para pruebas, devolver mensaje simple de recibido
                logger("RESPONSE", metodo, recurso, 200, client_ip);

                char *post_response = 
                    "HTTP/1.1 200 OK\r\n"
                    "Content-Type: text/html\r\n\r\n"
                    "<html><body><h1>POST recibido correctamente</h1></body></html>";
                send(client_socket, post_response, strlen(post_response), 0);
            } 
            else {
                // Metodo no soportado → 400
                logger("RESPONSE", metodo, recurso, 400, client_ip);

                char *bad_request = 
                    "HTTP/1.1 400 Bad Request\r\n"
                    "Content-Type: text/html\r\n\r\n"
                    "<html><body><h1>400 Metodo no soportado</h1></body></html>";
                send(client_socket, bad_request, strlen(bad_request), 0);
            }
        }
        closesocket(client_socket);
        return 0;
}

int main(int argc, char *argv[]) {
    WSADATA wsa;
    SOCKET server_socket, client_socket;
    struct sockaddr_in server, client;
    int c;

    // Verificar los argumentos de línea de comandos
    if (argc < 4) {
        printf("Uso: %s <puerto> <archivo_log> <carpeta_documentos>\n", argv[0]);
        return 1;

    }

    // Cargar el puerto
    int puerto = atoi(argv[1]);
    strcpy(log_file_path, argv[2]);
    char document_root[1024];
    strcpy(document_root, argv[3]);

    logger("INFO", NULL, NULL, 0, "Server");
    printf("Iniciando servidor en puerto %d, log: %s, directorio: %s\n", 
           puerto, log_file_path, document_root);

    printf("Iniciando winsock...\n");
    if (WSAStartup(MAKEWORD(2,2), &wsa) != 0) {
        printf("Fallo al iniciar. Error: %d\n", WSAGetLastError());
        logger("ERROR", NULL, NULL, WSAGetLastError(), "Server");
        return 1;
    }

    // Crear socket
    server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket == INVALID_SOCKET) {
        printf("No se pudo crear el socket. Error: %d\n", WSAGetLastError());
        logger("ERROR", NULL, NULL, WSAGetLastError(), "Server");
        return 1;
    }

    // Prepara la estructura sockaddr_in
    server.sin_family = AF_INET;
    server.sin_addr.s_addr = INADDR_ANY;
    server.sin_port = htons(puerto);

    // Enlazar
    if (bind(server_socket, (struct sockaddr *)&server, sizeof(server)) == SOCKET_ERROR) {
        printf("Bind fallo. Error: %d\n", WSAGetLastError());
        logger("ERROR", NULL, NULL, WSAGetLastError(), "Server");
        return 1;
    }

    listen(server_socket, 3);
    printf("Esperando conexiones en el puerto 8080...\n");
    logger("INFO", "SERVER", "LISTENING", puerto, "Server");

    c = sizeof(struct sockaddr_in);
    while ((client_socket = accept(server_socket, (struct sockaddr *)&client, &c)) != INVALID_SOCKET) {
        printf("Cliente conectado.\n");

        // Crear un nuevo espacio de memoria para el socket del cliente
        SOCKET *nuevo_cliente = malloc(sizeof(SOCKET));
        if (nuevo_cliente == NULL) {
            printf("Error al asignar memoria para el socket del cliente.\n");
            logger("ERROR", "SERVER", "MEMORY", 0, "Server");
            closesocket(client_socket); // En caso de tener un error en la memoria asignada al nuevo cliente, cerrar el socket del cliente.
            continue; // Aceptar otro cliente
        }

        *nuevo_cliente = client_socket;

        // Crear el hilo para manejar cada cliente en un hilo diferente
        HANDLE hilo;
        DWORD id_hilo;
        hilo = CreateThread(
            NULL,                // Atributos de seguridad, NULL por defecto
            0,                   // tamaño de stack por defecto, donde se crearan las posiciones de memoeria dinamicamente
            manejar_cliente,     // función que ejecutará el hilo
            nuevo_cliente,       // El socket del cliente se pasa como parámetro
            0,                   // flags, esto hace que el hilo se ejecute inmediatamente sin delay
            &id_hilo             // ID del hilo
        );
    
        if (hilo == NULL) {
            printf("Error al crear el hilo. Código de error: %ld\n", GetLastError());
            logger("ERROR", "SERVER", "THREAD", GetLastError(), "Server");
            closesocket(client_socket);
            free(nuevo_cliente);
        } else {
            CloseHandle(hilo);  // El hilo se ejecuta de forma independiente, cerrar el handle aquí.
        }
    }

    WSACleanup();
    return 0;
}
