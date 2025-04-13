#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <winsock2.h>

#pragma comment(lib, "ws2_32.lib") // Enlazar la libreria de windows

// Función para manejar los clientes

DWORD WINAPI manejar_cliente(LPVOID cliente_socket_ptr) {
    SOCKET client_socket = *(SOCKET*)cliente_socket_ptr;
    printf("Atendiendo cliente en hilo ID: %lu\n", GetCurrentThreadId());
    free(cliente_socket_ptr);  // Liberar memoria asignada por malloc

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
                char *bad_request = 
                    "HTTP/1.1 400 Bad Request\r\n"
                    "Content-Type: text/html\r\n\r\n"
                    "<html><body><h1>400 Peticion incorrecta</h1></body></html>";
                send(client_socket, bad_request, strlen(bad_request), 0);
                closesocket(client_socket);
                return 0;
            }

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
                char *post_response = 
                    "HTTP/1.1 200 OK\r\n"
                    "Content-Type: text/html\r\n\r\n"
                    "<html><body><h1>POST recibido correctamente</h1></body></html>";
                send(client_socket, post_response, strlen(post_response), 0);
            } 
            else {
                // Metodo no soportado → 400
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

int main() {
    WSADATA wsa;
    SOCKET server_socket, client_socket;
    struct sockaddr_in server, client;
    int c;

    printf("Iniciando winsock...\n");
    if (WSAStartup(MAKEWORD(2,2), &wsa) != 0) {
        printf("Fallo al iniciar. Error: %d\n", WSAGetLastError());
        return 1;
    }

    // Crear socket
    server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket == INVALID_SOCKET) {
        printf("No se pudo crear el socket. Error: %d\n", WSAGetLastError());
        return 1;
    }

    // Prepara la estructura sockaddr_in
    server.sin_family = AF_INET;
    server.sin_addr.s_addr = INADDR_ANY;
    server.sin_port = htons(8080);

    // Enlazar
    if (bind(server_socket, (struct sockaddr *)&server, sizeof(server)) == SOCKET_ERROR) {
        printf("Bind fallo. Error: %d\n", WSAGetLastError());
        return 1;
    }

    listen(server_socket, 3);
    printf("Esperando conexiones en el puerto 8080...\n");

    c = sizeof(struct sockaddr_in);
    while ((client_socket = accept(server_socket, (struct sockaddr *)&client, &c)) != INVALID_SOCKET) {
        printf("Cliente conectado.\n");

        // Crear un nuevo espacio de memoria para el socket del cliente
        SOCKET *nuevo_cliente = malloc(sizeof(SOCKET));
        if (nuevo_cliente == NULL) {
            printf("Error al asignar memoria para el socket del cliente.\n");
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
            closesocket(client_socket);
            free(nuevo_cliente);
        } else {
            CloseHandle(hilo);  // El hilo se ejecuta de forma independiente, cerrar el handle aquí.
        }
    }

    WSACleanup();
    return 0;
}
