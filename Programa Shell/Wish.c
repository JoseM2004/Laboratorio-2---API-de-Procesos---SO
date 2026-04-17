#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <ctype.h>

int TAMANIO_BUFFER = 512;

char MENSAJE_ERROR[128] = "An error has occurred\n";

int modoBatch = 0; // Indica si el shell corre en modo batch (1) o interactivo (0)
int rutaCambiada = 0; // Indica si el usuario cambió el search path con 'route'
char *ruta; //Puntero al directorio de búsqueda actual (un solo directorio)
int REDIRIGIDO = 0; //Bandera: stdout/stderr fueron redirigidos en esta iteración
int rutaVacia = 0; // Bandera: el path está vacío (route sin argumentos)
char multiRuta[512][512]; // Arreglo de directorios cuando 'route' recibe múltiples rutas
int cantidadMultiRuta = 0; //Cantidad de directorios en multiRuta


int soloEspacios(char *buffer) {
    int hayContenido = 0;
    for (int i = 0; i < (int)strlen(buffer); i++) {
        if (isspace(buffer[i]) == 0) { /* isspace() retorna 0 si NO es espacio */
            hayContenido = 1;
            break;
        }
    }
    return hayContenido;
}


void imprimirError() {
    write(STDERR_FILENO, MENSAJE_ERROR, strlen(MENSAJE_ERROR));
    exit(1);
}


void imprimirPrompt() {
    write(STDOUT_FILENO, "wish> ", strlen("wish> "));
}


int nuevoProceso(char *argumentos[]) {
    int pid = fork();

    if (pid < 0) {
        // fork() falló — error fatal
        imprimirError();
        exit(1);
    }
    else if (pid == 0 && rutaVacia != 1) {
        //Código del proceso HIJO 

        if (rutaCambiada == 0) {
            // Caso 1: path por defecto — buscar primero en /bin/
            ruta = strdup("/bin/");
            ruta = strcat(ruta, argumentos[0]);
            if (access(ruta, X_OK) != 0) {
                // No está en /bin/, intentar /usr/bin/
                ruta = strdup("/usr/bin/");
                ruta = strcat(ruta, argumentos[0]);
                if (access(ruta, X_OK) != 0) {
                    // No encontrado en ninguno → error recuperable
                    write(STDERR_FILENO, MENSAJE_ERROR, strlen(MENSAJE_ERROR));
                    exit(0);
                }
            }
        }
        else if (rutaCambiada == 1 && cantidadMultiRuta == 0) {
            // Caso 2: 'route' fijó exactamente un directorio
            ruta = strcat(ruta, argumentos[0]);
            if (access(ruta, X_OK) != 0) {
                write(STDERR_FILENO, MENSAJE_ERROR, strlen(MENSAJE_ERROR));
                exit(0);
            }
        }
        else {
            // Caso 3: 'route' fijó múltiples directorios 
            for (int x = 0; x < cantidadMultiRuta; x++) {
                strcat(multiRuta[x], argumentos[0]);
                if (access(multiRuta[x], X_OK) == 0) {
                    // Encontrado en multiRuta[x]
                    strcpy(ruta, multiRuta[x]);
                    break;
                }
            }
        }

        /* Ejecutar el binario encontrado.
           Si execv retorna, significa que falló. */
        if (execv(ruta, argumentos) == -1) {
            imprimirError();
            exit(0);
        }
    }
    else {
        /* ── Código del proceso PADRE ──
           No espera aquí; el wait se hace en el llamador
           para poder lanzar comandos paralelos primero. */
        int estadoRetorno = 0;
        (void)estadoRetorno; // evitar warning de variable no usada
    }

    return pid; // el padre retorna el PID del hijo
}


int preProcesar(char *buffer) {
    int copiaStdout = 0; // descriptor de respaldo de stdout 
    int pid = 0;


    // Bloque A: Detección de redirección
    if (strstr(buffer, ">") != NULL) {

        int contadorPiezas = 0;
        char *piezas[sizeof(char) * 512];
        piezas[0] = strtok(strdup(buffer), " \n\t>");
        while (piezas[contadorPiezas] != NULL) {
            contadorPiezas++;
            piezas[contadorPiezas] = strtok(NULL, " \n\t>");
        }
        if (contadorPiezas == 1) { // solo comando, sin archivo destino
            write(STDERR_FILENO, MENSAJE_ERROR, strlen(MENSAJE_ERROR));
            exit(0);
        }

        // Separar buffer en: parteComando > archivoSalida
        int i = 0;
        char *partes[sizeof(buffer)];
        partes[0] = strtok(buffer, "\n\t>");
        while (partes[i] != NULL) {
            i++;
            partes[i] = strtok(NULL, " \n\t>");
        }
        if (i > 2) { // más de un archivo destino → error
            write(STDERR_FILENO, MENSAJE_ERROR, strlen(MENSAJE_ERROR));
            exit(0);
        }

        // Extraer el nombre del archivo destino (partes[1]) 
        int x = 0;
        char *tokensArchivo[sizeof(partes[1])];
        tokensArchivo[0] = strtok(partes[1], " \n\t");
        while (tokensArchivo[x] != NULL) {
            x++;
            tokensArchivo[x] = strtok(NULL, " \n\t");
        }

        char *archivoSalida = strdup(tokensArchivo[0]);

        // Guardar copia del descriptor de stdout para restaurarlo después
        copiaStdout = dup(1);

        // Abrir el archivo destino (crear si no existe, truncar si existe)
        int fdSalida = open(archivoSalida, O_WRONLY | O_CREAT | O_TRUNC, 0666);
        int fdError  = open(archivoSalida, O_WRONLY | O_CREAT | O_TRUNC, 0666);

        fflush(stdout);

        // Redirigir stdout Y stderr al archivo (requisito del PDF)
        dup2(fdSalida, STDOUT_FILENO);
        dup2(fdSalida, STDERR_FILENO);
        close(fdSalida);
        REDIRIGIDO = 1;

        // Verificar que la apertura del archivo fue exitosa
        if (fdSalida == -1 || fdError == -1 || x > 1 || i > 2) {
            write(STDERR_FILENO, MENSAJE_ERROR, strlen(MENSAJE_ERROR));
            exit(0);
        }

        // Dejar en buffer SOLO la parte del comando
        partes[i + 1] = NULL;
        tokensArchivo[x + 1] = NULL;
        strcpy(buffer, partes[0]);
    }

    // Bloque B: Parseo del comando y sus argumentos
    if (buffer[0] != '\0' && buffer[0] != '\n') {

        char *comando[sizeof(buffer)];
        comando[0] = strtok(buffer, " \t\n");
        int p = 0;
        while (comando[p] != NULL) {
            p++;
            comando[p] = strtok(NULL, " \n\t");
        }
        comando[p + 1] = NULL;
        
        // B.1: Comando integrado 'chd'
        if (strcmp(comando[0], "chd") == 0) {
            
            if (p == 2) {
                if (chdir(comando[1]) != 0) {
                    /* chdir() falló (directorio no existe, sin permisos...) */
                    write(STDERR_FILENO, MENSAJE_ERROR, strlen(MENSAJE_ERROR));
                }
            }
            else {
                // 0 argumentos o más de 1 → error
                write(STDERR_FILENO, MENSAJE_ERROR, strlen(MENSAJE_ERROR));
            }
        }

        // B.2: Comando integrado 'route'
        else if (strcmp(comando[0], "route") == 0) {
            rutaCambiada = 1; // marcar que el usuario modificó el path 

            if (p == 2) {
                // 'route /un/directorio' — exactamente un directorio
                rutaVacia = 0;
                cantidadMultiRuta = 0;
                ruta = strdup(comando[1]);
                // Asegurar que termina en '/' para concatenar el ejecutable
                if (ruta[strlen(ruta) - 1] != '/')
                    strcat(ruta, "/");
            }
            else if (p == 1) {
                // 'route' sin argumentos → path vacío.
                // No se podrá ejecutar ningún comando externo.
                rutaVacia = 1;
                cantidadMultiRuta = 0;
            }
            else {
                // 'route /bin /usr/bin ...' — múltiples directorios
                rutaVacia = 0;
                cantidadMultiRuta = 0;
                for (int i = 1; i < p; i++) {
                    char *temp = strdup(comando[i]);
                    if (temp[strlen(temp) - 1] != '/')
                        strcat(temp, "/");
                    strcpy(multiRuta[i - 1], temp);
                    cantidadMultiRuta++;
                }
            }
        }

        // Bloque B.3: Comando integrado 'exit'
        else if (strcmp(comando[0], "exit") == 0) {
            if (p == 1) {
                // 'exit' sin argumentos → terminar el shell correctamente
                exit(0);
            }
            else {
                // Pasar argumentos a exit es un error (el shell continúa)
                write(STDERR_FILENO, MENSAJE_ERROR, strlen(MENSAJE_ERROR));
            }
        }

        // Bloque B.4: Comando externo
        else {
            if (rutaVacia == 1) {
                // Path vacío: imposible buscar el ejecutable
                write(STDERR_FILENO, MENSAJE_ERROR, strlen(MENSAJE_ERROR));
            }
            else {
                // Lanzar el proceso hijo con fork+execv
                pid = nuevoProceso(comando);
            }
        }
    }

    //Bloque C: Restaurar stdout/stderr si se redirigieron
    if (REDIRIGIDO == 1) {
        dup2(copiaStdout, 1); // restaurar stdout al descriptor original
        close(copiaStdout);
    }

    return pid;
}


int main(int argc, char *argv[]) {
    FILE *archivo = NULL;
    ruta = (char *)malloc(TAMANIO_BUFFER);
    char buffer[512];

    // Determinar el modo de ejecución
    if (argc == 1) {
        // Modo interactivo: la fuente de comandos es stdin
        archivo = stdin;
        imprimirPrompt();
    }
    else if (argc == 2) {
        // Modo batch: abrir el archivo de comandos
        char *nombreArchivo = strdup(argv[1]);
        archivo = fopen(nombreArchivo, "r");
        if (archivo == NULL) {
            // No se pudo abrir el archivo → error fatal
            imprimirError();
        }
        modoBatch = 1;
    }
    else {
        // Más de un argumento → error fatal
        imprimirError();
    }

    // Bucle principal de lectura de comandos
    while (fgets(buffer, TAMANIO_BUFFER, archivo)) {
        // Reiniciar la bandera de redirección en cada iteración
        REDIRIGIDO = 0;

        // Ignorar líneas que solo contienen espacios/tabs/newlines
        if (soloEspacios(buffer) == 0) {
            continue;
        }

        // Caso: comandos paralelos (contiene '&')
        if (strstr(buffer, "&") != NULL) {

            // Separar la línea en sub-comandos usando '&' como delimitador 
            int j = 0;
            char *subComandos[sizeof(buffer)];
            subComandos[0] = strtok(buffer, "\n\t&");
            while (subComandos[j] != NULL) {
                j++;
                subComandos[j] = strtok(NULL, "\n\t&");
            }
            subComandos[j + 1] = NULL;

            // Arreglo para guardar los PIDs de todos los hijos
            int pids[j];

            // FASE 1: Lanzar TODOS los sub-comandos sin esperar a ninguno.
            // Esto los hace correr verdaderamente en paralelo.
            for (int i = 0; i < j; i++) {
                pids[i] = preProcesar(subComandos[i]);
            }

            // FASE 2: Esperar a que TODOS los hijos terminen con waitpid().
            // CORRECCIÓN: el original tenía i++ en lugar de x++,
            // causando un loop infinito.
            for (int x = 0; x < j; x++) {
                int estadoRetorno = 0;
                waitpid(pids[x], &estadoRetorno, 0);
                if (estadoRetorno == 1) {
                    imprimirError();
                }
            }
        }

        // Caso: comando único (sin '&')
        else {
            int pid = preProcesar(buffer);
            // Esperar al hijo antes de mostrar el prompt de nuevo
            if (pid > 0) {
                waitpid(pid, NULL, 0);
            }
        }

        // Mostrar el prompt solo en modo interactivo
        if (argc == 1) {
            imprimirPrompt();
        }
    }

    // fgets retornó NULL → se alcanzó EOF → salir limpiamente
    exit(0);
}