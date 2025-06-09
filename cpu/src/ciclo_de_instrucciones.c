#include "../include/cpu.h"

int cant_interrupciones;

/**
* @fn     void fetch(t_log* cpu_logger)
* @brief  Realiza el ciclo de instrucción principal: solicita la instrucción correspondiente al PID y PC a memoria, la decodifica y ejecuta según el tipo de operación. Si corresponde, reitera el ciclo para la siguiente instrucción. Gestiona interrupciones y SYSCALLs.
* @param  cpu_logger Logger para imprimir información de depuración y control.
* @return Ninguno
*/
void fetch(t_log* cpu_logger) {

    log_info(cpu_logger, "## PID: %d - FETCH - Program Counter: %d", pid, pc);
    
    /*  Le mando el PID y PC a memoria para que me devuelva la instruccion*/
    t_buffer* buffer = crear_buffer();
    cargar_int_al_buffer(buffer, pc);
    cargar_int_al_buffer(buffer, pid);
    
    t_paquete* paquete = crear_paquete(CPU_M_SOLICITAR_INSTRUCCION, buffer);
    enviar_paquete(paquete, socket_memoria);

    int cod_op = recibir_operacion(socket_memoria);

    if (cod_op == M_CPU_RESPUESTA_INSTRUCCION) {
		
        t_buffer* buffer_respuesta = recibir_buffer(socket_memoria); //Instruccion recibida
        
        /*   ETAPA DECODE   */
        t_instruccion* instruccion = decode(buffer_respuesta);  
        t_operacion operacion = instruccion -> operacion;
        
        eliminar_buffer(buffer_respuesta);
        
        if (operacion == READ || operacion == WRITE) { //necesito traduccion de memoria (es READ o WRITE)
            execute (instruccion, cpu_logger);
            check_interrupt(instruccion, cpu_logger);
            destruir_instruccion(instruccion);
            fetch(cpu_logger);
        }

        else if (!es_syscall(instruccion)) { // es GOTO o NOOP
            execute (instruccion, cpu_logger);
            check_interrupt(instruccion, cpu_logger);
            destruir_instruccion(instruccion);
            fetch(cpu_logger);
        }

        else { // es una SYSCALL
            int fin_del_archivo = enviar_instruccion_a_kernel(instruccion, cpu_logger);
            check_interrupt(instruccion, cpu_logger);
            destruir_instruccion(instruccion);
            if(!fin_del_archivo) { //si la instruccion envaida no es EXIT
                fetch(cpu_logger);
            }
            else {
                log_debug(cpu_logger, "Instruccion recibida EXIT, no quedan mas instrucciones en el archivo\n");
            }
        }

    }
}


/**
* @fn     bool es_syscall(t_instruccion* instruccion)
* @brief  Determina si la instrucción recibida corresponde a una syscall (IO, EXIT, INIT_PROC o DUMP_MEMORY).
* @param  instruccion Puntero a la instrucción a analizar.
* @return true si es una syscall, false en caso contrario.
*/
bool es_syscall(t_instruccion* instruccion) {
    t_operacion operacion = instruccion->operacion;
    return (operacion == IO || operacion == EXIT || operacion == INIT_PROC || operacion == DUMP_MEMORY);
}

/**
* @fn     void execute(t_instruccion* instruccion, t_log* cpu_logger)
* @brief  Ejecuta la instrucción recibida según su tipo (NOOP, READ, WRITE, GOTO). Realiza la traducción de direcciones, acceso a memoria y logging de la operación. Si corresponde, utiliza la caché.
* @param  instruccion Puntero a la instrucción a ejecutar.
* @param  cpu_logger Logger para imprimir información de ejecución.
* @return Ninguno
*/
void execute (t_instruccion* instruccion, t_log* cpu_logger){
    int direccion_fisica;
    switch (instruccion->operacion) {
        case NOOP:  //solo consume el tiempo del ciclo de instruccion
            log_info(cpu_logger, "## PID: %d - Ejecutando: NOOP", pid);
        break;

        case READ:
        {
            char* direccion_logica = instruccion->parametros[0];
            int tamanio = atoi(instruccion->parametros[1]); //atoi convierte un string a int

            // Lectura/Escritura Memoria: “PID: <PID> - Acción: <LEER / ESCRIBIR> - Dirección Física: <DIRECCION_FISICA> - Valor: <VALOR LEIDO / ESCRITO>”.
            log_info(cpu_logger, "## PID: %d - Ejecutando: READ - %s - %d", pid, direccion_logica, tamanio);

            if(cache_habilitada()) {
                // intentar leer desde la cahce directamente
                cargar_contenido_cache(cpu_logger, atoi(direccion_logica), READ, NULL); // Carga el contenido de la cache   
            }
            else {
                direccion_fisica = traducir_dir_logica(atoi(direccion_logica), cpu_logger); //Traduce dirección lógica a física
                
                log_info(cpu_logger, "Dir logica: %s, Dir fisica: %d", direccion_logica, direccion_fisica);

                t_buffer* buffer_rta = crear_buffer();
                cargar_int_al_buffer(buffer_rta, frame);      // número de marco
                cargar_int_al_buffer(buffer_rta, desplazamiento);     // offset dentro de la página
                cargar_int_al_buffer(buffer_rta, tamanio);       // cantidad de bytes a leer
                t_paquete* paquete = crear_paquete(CPU_M_LEER_MEMORIA, buffer_rta);
                enviar_paquete(paquete, socket_memoria);
    
                // Recibo respuesta de Memoria
                if(recibir_operacion(socket_memoria) == M_CPU_VALOR_LEIDO){

                    t_buffer* buffer = recibir_buffer(socket_memoria);
                    char* valor_leido = extraer_string_del_buffer(buffer);
                    log_debug(cpu_logger, "%s", valor_leido);    
                    eliminar_buffer(buffer);

                } else {
                    log_debug(cpu_logger, "Memoria me contestó otra cosa");
                }
            }
        }
        break;

        case WRITE:
            {
            // Lectura/Escritura Memoria: “PID: <PID> - Acción: <LEER / ESCRIBIR> - Dirección Física: <DIRECCION_FISICA> - Valor: <VALOR LEIDO / ESCRITO>”.
            char* direccion = instruccion->parametros[0];
            char* datos = instruccion->parametros[1];

            log_info(cpu_logger, "## PID: %d - Ejecutando: WRITE - %s - %s", pid, direccion, datos);
            if(cache_habilitada()) {
                cargar_contenido_cache(cpu_logger, atoi(direccion), WRITE, datos); // Carga el contenido de la cache   

                // Escribir en la cache
            }
            else {
                direccion_fisica = traducir_dir_logica(atoi(direccion), cpu_logger); // Traduzco la dirección lógica a física
                // Envio a Memoria lo que necesito escribir
                t_buffer* buffer_rta = crear_buffer();
                cargar_int_al_buffer(buffer_rta, frame);      // número de marco
                cargar_int_al_buffer(buffer_rta, desplazamiento);     // offset dentro de la página
                cargar_string_al_buffer(buffer_rta, datos);   // datos a escribir
                t_paquete* paquete = crear_paquete(CPU_M_ESCRIBIR_MEMORIA, buffer_rta);
                enviar_paquete(paquete, socket_memoria);
                
                // Recibo respuesta de Memoria
                if(recibir_operacion(socket_memoria) == M_CPU_CONFIRMACION_ESCRITURA){

                    t_buffer* buffer = recibir_buffer(socket_memoria);
                    char* valor_leido = extraer_string_del_buffer(buffer); 
                    log_debug(cpu_logger, "%s", valor_leido);     
                    eliminar_buffer(buffer);

                } else {
                    log_debug(cpu_logger, "Memoria me contestó otra cosa");
                }
            }
        }

        break;

        case GOTO:{
            int valor = atoi(instruccion->parametros[0]);
            pc = valor; //se actualiza el pc por direccion de memoria
            
            log_info(cpu_logger, "## PID: %d - Ejecutando: GOTO - %d", pid, valor);
            }
        break;

        default:
        break;
    }

}

/**
* @fn     void check_interrupt(t_instruccion* instruccion, t_log* cpu_logger)
* @brief  Verifica si ocurrió una interrupción. Si es así, envía el PID y el PC actualizado al kernel por el canal de interrupciones. Además, incrementa el PC si la instrucción no es GOTO.
* @param  instruccion Puntero a la instrucción actual.
* @param  cpu_logger Logger para imprimir información de control.
* @return Ninguno
*/
void check_interrupt(t_instruccion* instruccion, t_log* cpu_logger){ //TODO: evaluar si el incremento de pc se puede hacer por fuera, asi me evito pasarle la instruccion por parametro
    if (hay_alguna_interrupcion()){
        log_info(cpu_logger, "## LLega interrupcion al puerto interrupt");
        //mandar pid y pc actualizado
        t_buffer* buffer_interrupt = crear_buffer();
        cargar_int_al_buffer(buffer_interrupt, pc);
        cargar_int_al_buffer(buffer_interrupt, pid);

        t_paquete* paquete = crear_paquete(K_CPU_INTERRUPT_PROCESO, buffer_interrupt);

        enviar_paquete(paquete, socket_kernel_interrupt);
    }

    if (instruccion->operacion != GOTO){
        pc++;
    }

}


/**
* @fn     bool hay_alguna_interrupcion(void)
* @brief  Indica si hay una interrupción pendiente en el sistema.
* @param  Ninguno
* @return true si hay interrupción, false en caso contrario.
*/
bool hay_alguna_interrupcion(){
    return interrupt;
}

/**
* @fn     t_instruccion* decode(t_buffer* buffer)
* @brief  Deserializa y decodifica una instrucción recibida en un buffer, extrayendo la operación y sus parámetros.
* @param  buffer Puntero al buffer que contiene la instrucción serializada.
* @return Puntero a la instrucción deserializada.
*/
t_instruccion* decode(t_buffer* buffer) {
    t_instruccion* instruccion_deserializada = malloc(sizeof(t_instruccion));

    instruccion_deserializada->operacion = extraer_int_del_buffer(buffer);
    instruccion_deserializada->cantidad_parametros = extraer_int_del_buffer(buffer);

    instruccion_deserializada->parametros = malloc(instruccion_deserializada->cantidad_parametros * sizeof(char*));
    for (int i = 0; i < instruccion_deserializada->cantidad_parametros; i++) {
        instruccion_deserializada->parametros[i] = extraer_string_del_buffer(buffer);
    }

    return instruccion_deserializada;
}

/**
* @fn     int enviar_instruccion_a_kernel(t_instruccion* instruccion, t_log* cpu_logger)
* @brief  Envía la instrucción correspondiente al kernel según el tipo de operación (INIT_PROC, DUMP_MEMORY, IO, EXIT). Serializa los parámetros necesarios y gestiona la comunicación con el kernel. Devuelve 1 si la instrucción es EXIT, 0 en otros casos, -1 en caso de error.
* @param  instruccion Puntero a la instrucción a enviar.
* @param  cpu_logger Logger para imprimir información de control.
* @return 1 si la instrucción es EXIT, 0 si no es EXIT, -1 en caso de error.
*/
int enviar_instruccion_a_kernel(t_instruccion* instruccion, t_log* cpu_logger) {

    t_buffer* buffer = crear_buffer();
    
    cargar_int_al_buffer(buffer, pid);

    switch (instruccion -> operacion){
        case INIT_PROC:
            // cargar_int_al_buffer(buffer, instruccion -> operacion);
            cargar_string_al_buffer(buffer, instruccion -> parametros[0]); //archivo de instrucciones
            cargar_string_al_buffer(buffer, instruccion -> parametros[1]); //tamaño

            t_paquete* paquete_init_proc = crear_paquete(CPU_K_INIT_PROC, buffer);
            enviar_paquete(paquete_init_proc, socket_kernel_dispatch);
            return 0;
            
        break;
            
        case DUMP_MEMORY:
            // cargar_int_al_buffer(buffer, instruccion -> operacion);
            
            t_paquete* paquete_dump_memory = crear_paquete(CPU_K_DUMP_MEMORY, buffer);
            enviar_paquete(paquete_dump_memory, socket_kernel_dispatch);
            return 0;
        break;
            
        case IO:
            // cargar_int_al_buffer(buffer, instruccion -> operacion);
            cargar_string_al_buffer(buffer, instruccion -> parametros[0]); //Dispositivo
            cargar_string_al_buffer(buffer, instruccion -> parametros[1]); //Tiempo
            cargar_int_al_buffer(buffer, pc+1); //Para salvar contexto
           
            t_paquete* paquete_io = crear_paquete(CPU_K_SOLICITAR_IO, buffer);
            enviar_paquete(paquete_io, socket_kernel_dispatch);
            
            // Dejar de ejecutar ese proceso y empezar a ejecutar otro
            atender_kernel_cpu_dispatch(cpu_logger);
            return 0;
        break;
            
        case EXIT:
            // cargar_int_al_buffer(buffer, instruccion -> operacion);    
            t_paquete* paquete_exit = crear_paquete(CPU_K_EXIT, buffer);
            enviar_paquete(paquete_exit, socket_kernel_dispatch);
            return 1;
        break;

        default:
            eliminar_buffer(buffer);
            return (-1);
    }
}