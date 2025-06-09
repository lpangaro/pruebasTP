#include "../include/cpu.h"

/**
* @fn    main
* @brief Punto de entrada del programa. Inicializa logs, configuración y conexiones, y luego cierra los recursos.
* @param argc Cantidad de argumentos pasados al programa.
* @param argv Array de argumentos pasados al programa.
* @return Código de salida del programa.
*/
int main(int argc, char* argv[]) {
    if (argc < 2) {
        printf("Falta el identificador de la CPU.\nUso: %s [identificador]\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    char* cpu_id = argv[1];
    
    inicializar_configCPU();
    t_log* logger = inicializar_logger(cpu_id);
    if(cache_habilitada()) {
        inicializar_cache();
    }
	iniciar_TLB();
    conexiones(cpu_id, logger);
    cerrar_cpu(logger);
    log_debug(cpu_logger, "Recursos liberados y CPU cerrada.\n");
	
	return EXIT_SUCCESS;
}

