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
    char *response =
        "HTTP/1.1 200 OK\r\n"
        "Content-Type: text/html\r\n\r\n"
        "<html><body style=\"background-color:black; color:white;\">"
        "<h1>¡Servidor en C funcionando!</h1>"
        "</body></html>";


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
    client_socket = accept(server_socket, (struct sockaddr *)&client, &c);
    if (client_socket == INVALID_SOCKET) {
        printf("Accept fallo. Error: %d\n", WSAGetLastError());
        return 1;
    }

    printf("Cliente conectado.\n");

    // Recibir la solicitud HTTP
    char request[2000];
    int recv_size = recv(client_socket, request, 2000, 0);
    if (recv_size == SOCKET_ERROR) {
        printf("Error al recibir datos.\n");
        return 1;
    }

    request[recv_size] = '\0'; //Terminar el string
    printf("Peticion recibida:\n%s\n", request);

    //Enviar la respuesta HTTP
    send(client_socket, response, strlen(response), 0);
}