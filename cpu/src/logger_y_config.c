#include "../include/cpu.h"

// Para las conexiones
char* puerto_memoria() {
    return config_get_string_value(cpu_config, "PUERTO_MEMORIA");
}
char* ip_memoria() {
    return config_get_string_value(cpu_config, "IP_MEMORIA");
}
char* puerto_kernel_dispatch() {
    return config_get_string_value(cpu_config, "PUERTO_KERNEL_DISPATCH");
}
char* puerto_kernel_interrupt() {
    return config_get_string_value(cpu_config, "PUERTO_KERNEL_INTERRUPT");
}   
char* ip_kernel() {
    return config_get_string_value(cpu_config, "IP_KERNEL");
}

// Para otras cositas 
int entradas_tlb() {
    return config_get_int_value(cpu_config, "ENTRADAS_TLB");
}
char* reemplazo_tlb() {
    return config_get_string_value(cpu_config, "REEMPLAZO_TLB");
}
int entradas_cache() {
    return config_get_int_value(cpu_config, "ENTRADAS_CACHE");
}
char* reemplazo_cache() {
    return config_get_string_value(cpu_config, "REEMPLAZO_CACHE");
}
char* retardo_cache() {
    return config_get_string_value(cpu_config, "RETARDO_CACHE");
}
char* log_level() {
    return config_get_string_value(cpu_config, "LOG_LEVEL");
}


t_log* inicializar_logger(char *nombre) {
    // Hace el nombre de la CPU con el .log
    size_t longitud = strlen(nombre) + strlen(".log") + 1;
    char* nombre_con_extension = malloc(longitud);
    if (nombre_con_extension == NULL) {
        printf("Error al asignar memoria para el nombre del logger.\n");
        exit(EXIT_FAILURE);
    }

    strcpy(nombre_con_extension, nombre);       // Copio el nombre base
    strcat(nombre_con_extension, ".log");       // Le agrego la extensión

    // Obtengo el nivel de log
    char* nivel_log = log_level();
    if (nivel_log == NULL) {
        printf("Error: LOG_LEVEL no definido en la configuración.\n");
        free(nombre_con_extension);
        exit(EXIT_FAILURE);
    }

    t_log* nuevo_logger = log_create(nombre_con_extension, "LOGGER CPU", 1, log_level_from_string(nivel_log));

    if (nuevo_logger == NULL) {
        free(nombre_con_extension);
        exit(EXIT_FAILURE);
    }

    free(nombre_con_extension); // Libero la memoria asignada
    return nuevo_logger;
}

void inicializar_configCPU() {

    cpu_config = config_create("cpu.config");

    if(cpu_config == NULL){
        perror("Fallo en CPU config: no se pudo crear. ");
        exit(EXIT_FAILURE);
    }
}

