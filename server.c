#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <winsock2.h>

#pragma comment(lib, "ws2_32.lib") // Enlazar la libreria de windows

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
    // Recibir la solicitud HTTP
    while ((client_socket = accept(server_socket, (struct sockaddr *)&client, &c)) != INVALID_SOCKET) {
        printf("Cliente conectado.\n");

        char request[2000];
        int recv_size = recv(client_socket, request, sizeof(request) - 1, 0);
        if (recv_size > 0) {
            request[recv_size] = '\0';
            printf("Peticion recibida:\n%s\n", request);
            
            // Analizar la peticion y obtener el nombre del recurso
            char archivo_solicitado[1024] = {0};
            sscanf(request, "GET %s HTTP/1.1", archivo_solicitado);

            // Si se pide la raíz "/", cargar index.html
            if (strcmp(archivo_solicitado, "/") == 0) {
                strcpy(archivo_solicitado, "index.html");
            } else {
                // Eliminar el "/" inicial
                memmove(archivo_solicitado, archivo_solicitado + 1, strlen(archivo_solicitado));
            }

            // Si cintiene parametros tipo ?var=1, los ignoramos
            char *params = strchr(archivo_solicitado, '?');
            if (params) *params = '\0';

            // Armar la ruta completa al archivo dentro de "www/"
            char ruta_completa[2048];
            sprintf(ruta_completa, "www/%s", archivo_solicitado);

            FILE *f = fopen(ruta_completa, "rb");
            if (f == NULL) {
                // Archivo no encontrado
                char *not_found = 
                    "HTTP/1.1 404 Not Found\r\n"
                    "Content-Type: text/html\r\n\r\n"
                    "<html><body><h1>404 No encontrado</h1></body></html>";
                send(client_socket, not_found, strlen(not_found), 0);
            } else {
                // Leer el contenido del archivo
                fseek(f, 0, SEEK_END);
                long length = ftell(f);
                rewind(f);
                char *contenido = malloc(length + 1);
                fread(contenido, 1, length, f);
                contenido[length] = '\0';

                // Cabecera HTTP
                char header[1024];
                sprintf(header, "HTTP/1.1 200 0K\r\nContent-Type: text/html\r\n\r\n");

                // Enviar la cabecera y contenido
                send(client_socket, header, strlen(header), 0);
                send(client_socket, contenido, length, 0);

                fclose(f);
                free(contenido); // liberar memoria
            }
            }
            closesocket(client_socket); // cerrar después de enviar la respuesta
        }
        WSACleanup(); // Limpiar winsock
        return 0;
}