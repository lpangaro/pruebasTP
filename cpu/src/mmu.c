#include "mmu.h"

//-------------TLB------------------

/**
* @fn     void iniciar_TLB(void)
* @brief  Inicializa la lista de entradas de la TLB, reservando memoria para cada entrada y configurando sus valores iniciales. Deja todas las entradas listas para ser utilizadas por el sistema de traducción de direcciones, con valores por defecto que indican que están vacías.
* @param  Ninguno
* @return Ninguno
*/
void iniciar_TLB(void){
    lista_tlb = list_create();

    for(int i = 0; i < entradas_tlb(); i++){
        t_entrada_TLB* registro_tlb = malloc(sizeof(t_entrada_TLB)); //realloc
        if (!registro_tlb) {
            perror("No se pudo reservar memoria para la TLB");
            exit(EXIT_FAILURE);
        } 
        registro_tlb->numero_pagina = -1;
        registro_tlb->marco = -1;
        registro_tlb->time_creado = time(NULL);
        registro_tlb->time_usado = time(NULL);

        list_add(lista_tlb, registro_tlb);
    }
}

/**
* @fn     t_entrada_TLB* buscar_en_TLB(int numero_pagina)
* @brief  Busca una entrada en la TLB que corresponda al número de página solicitado. Recorre la lista de entradas y retorna un puntero a la entrada si la encuentra, o NULL si no existe.
* @param  numero_pagina Número de página a buscar en la TLB.
* @return Puntero a la entrada encontrada o NULL si no existe.
*/
t_entrada_TLB* buscar_en_TLB(int numero_pagina){
    t_entrada_TLB* registro_tlb;
    for(int i = 0; i < list_size(lista_tlb); i++){
        registro_tlb = list_get(lista_tlb, i);
        if(registro_tlb->numero_pagina == numero_pagina)
            return registro_tlb;
    }
    return NULL; // No se encontro la pagina en la t_entrada_TLB
}

/**
* @fn     int traducir_dir_logica(int direccion_logica, t_log* logger)
* @brief  Traduce una dirección lógica a una dirección física utilizando la MMU. Calcula el número de página y el desplazamiento, obtiene el marco correspondiente y construye la dirección física final. Registra información relevante en el logger.
* @param  direccion_logica Dirección lógica a traducir.
* @param  logger Logger para imprimir información de depuración.
* @return Dirección física resultante.
*/
int traducir_dir_logica(int direccion_logica, t_log* logger) {

    //Tiene que estar fuera de la funcion para que cache verifique si posee el numero de pagina
    int direccion_fisica;
    int nro_pagina = direccion_logica / tam_pagina;
    desplazamiento = direccion_logica % tam_pagina;

    int vec[cantidad_niveles];
    for (int X = 1; X <= cantidad_niveles; X++) {
        int divisor = (int)pow(entradas_tabla, cantidad_niveles - X); //indices de tabla de paginas
        vec[X-1] = (nro_pagina / divisor) % entradas_tabla;
    }

    log_info(logger, "Direccion logica: %d", direccion_logica);
    log_info(logger, "Numero de pagina: %d | Desplazamiento: %d", nro_pagina, desplazamiento);

    int marco = obtener_marco(nro_pagina, vec); //obtiene el marco, ya sea desde la tlb o desde memoria
    direccion_fisica = marco * tam_pagina + desplazamiento;

    return direccion_fisica;
}

/**
* @fn     int obtener_marco(int nro_pagina, int vec[])
* @brief  Obtiene el marco correspondiente a una página. Primero busca en la TLB; si no está, consulta a memoria y actualiza la TLB con la nueva entrada. Devuelve el número de marco obtenido.
* @param  nro_pagina Número de página a buscar.
* @param  vec Vector de índices de tablas de páginas.
* @return Número de marco correspondiente.
*/
int obtener_marco (int nro_pagina, int vec[]) {
    t_entrada_TLB* entrada_tlb_aux = buscar_en_TLB(nro_pagina);
    int marco;

    if(entrada_tlb_aux != NULL){ //Existe la pagina en t_entrada_TLB
        entrada_tlb_aux->time_usado = time(NULL);

        marco = entrada_tlb_aux->marco;
    }
    else { //No esta en la t_entrada_TLB
        entrada_tlb_aux = malloc(sizeof(t_entrada_TLB));
        marco = buscar_marco_en_memoria(vec, cpu_logger, nro_pagina);

        entrada_tlb_aux->marco = marco;
        entrada_tlb_aux->numero_pagina = nro_pagina;
        entrada_tlb_aux->time_creado = time(NULL);
        entrada_tlb_aux->time_usado =time(NULL);
        actualizar_TLB(entrada_tlb_aux);    
    }
    return marco;
}

/**
* @fn     void actualizar_TLB(t_entrada_TLB* registro_tlb_nuevo)
* @brief  Actualiza la TLB con una nueva entrada. Si ya existe una entrada con el mismo marco, la reemplaza. Si no hay lugar, aplica el algoritmo de reemplazo configurado (FIFO o LRU). Si hay lugar vacío, inserta la nueva entrada.
* @param  registro_tlb_nuevo Nueva entrada de TLB a insertar.
* @return Ninguno
*/
void actualizar_TLB(t_entrada_TLB* registro_tlb_nuevo){
    registro_tlb_nuevo->time_usado = time(NULL);

    int indice = existe_entrada_con_marco(registro_tlb_nuevo);
    if(indice != -1){
        list_replace_and_destroy_element(lista_tlb, indice, registro_tlb_nuevo, (void*)free);
    }
    else{
        indice = verificar_reemplazo_TLB();
        if(indice == -1){ // No hay lugares vacios
            if(strcmp(reemplazo_tlb(), "FIFO") == 0){
                reemplazar_TLB_FIFO(registro_tlb_nuevo);
            }
            else if(strcmp(reemplazo_tlb(), "LRU") == 0){
                reemplazar_TLB_LRU(registro_tlb_nuevo);
            }
        }
        else{ // Hay lugares vacios 
            list_replace_and_destroy_element(lista_tlb, indice, registro_tlb_nuevo, (void*)free);
        }
    }
}

/**
* @fn     void reemplazar_TLB_FIFO(t_entrada_TLB* registro_tlb_nuevo)
* @brief  Reemplaza la entrada más antigua de la TLB utilizando el algoritmo FIFO. Busca la entrada con menor timestamp de creación y la reemplaza por la nueva entrada.
* @param  registro_tlb_nuevo Nueva entrada de TLB a insertar.
* @return Ninguno
*/
void reemplazar_TLB_FIFO(t_entrada_TLB* registro_tlb_nuevo){
    int i, indice_registro_tlb_mas_viejo = 0;

    t_entrada_TLB* entrada_tlb_aux;

    t_entrada_TLB* registro_tlb_mas_viejo = list_get(lista_tlb, 0);

    for(i = 0; i < list_size(lista_tlb); i++){

        entrada_tlb_aux = list_get(lista_tlb, i);

        if(difftime(registro_tlb_mas_viejo -> time_creado, entrada_tlb_aux -> time_creado) > 0){
            indice_registro_tlb_mas_viejo = i;
            registro_tlb_mas_viejo = entrada_tlb_aux;
        }
    }

    list_replace_and_destroy_element(lista_tlb, indice_registro_tlb_mas_viejo, registro_tlb_nuevo, (void*)free);
}

/**
* @fn     void reemplazar_TLB_LRU(t_entrada_TLB* registro_tlb_nuevo)
* @brief  Reemplaza la entrada menos recientemente usada de la TLB utilizando el algoritmo LRU. Busca la entrada con menor timestamp de último uso y la reemplaza por la nueva entrada.
* @param  registro_tlb_nuevo Nueva entrada de TLB a insertar.
* @return Ninguno
*/
void reemplazar_TLB_LRU(t_entrada_TLB* registro_tlb_nuevo){
    int indice_mas_viejo_tlb = 0;

    t_entrada_TLB* entrada_tlb_aux;

    t_entrada_TLB* tlb_mas_viejo = list_get(lista_tlb, 0);

    for(int i = 0; i < list_size(lista_tlb); i++){

        entrada_tlb_aux = list_get(lista_tlb, i);

        if(difftime(tlb_mas_viejo -> time_usado, entrada_tlb_aux -> time_usado) > 0){
            indice_mas_viejo_tlb = i;
            tlb_mas_viejo = entrada_tlb_aux;
        }
    }
    
    list_replace_and_destroy_element(lista_tlb, indice_mas_viejo_tlb, registro_tlb_nuevo, (void*)free);
}

/**
* @fn     int verificar_reemplazo_TLB(void)
* @brief  Busca un espacio vacío en la TLB. Recorre la lista de entradas y retorna el índice de la primera entrada vacía encontrada, o -1 si no hay lugar disponible.
* @param  Ninguno
* @return Índice del espacio vacío o -1 si no hay lugar.
*/
int verificar_reemplazo_TLB(void){
    t_entrada_TLB* entrada_tlb_aux;

    for(int i = 0; i < list_size(lista_tlb); i++){
        entrada_tlb_aux = list_get(lista_tlb, i);

        if(entrada_tlb_aux->numero_pagina == -1)
            return i; //Retorna el indice del registro t_entrada_TLB vacio
    }

    return -1; // Retorna -1 si no hay registros t_entrada_TLB vacios
}

/**
* @fn     int existe_entrada_con_marco(t_entrada_TLB* registro_tlb_nuevo)
* @brief  Verifica si ya existe una entrada en la TLB con el mismo marco que la nueva entrada. Si la encuentra, retorna el índice; si no, retorna -1.
* @param  registro_tlb_nuevo Entrada de TLB a comparar.
* @return Índice de la entrada encontrada o -1 si no existe.
*/
int existe_entrada_con_marco(t_entrada_TLB* registro_tlb_nuevo){
    int i;
    t_entrada_TLB * entrada_tlb_aux;
    for(i = 0; i < list_size(lista_tlb); i++){
        entrada_tlb_aux = list_get(lista_tlb, i);

        if(entrada_tlb_aux->marco == registro_tlb_nuevo->marco)
            return i;
    }
    return -1;
}

//------------------ MMU ------------------

/**
* @fn     int buscar_marco_en_memoria(int vec[], t_log* cpu_logger, int nro_pagina)
* @brief  Solicita a memoria el marco correspondiente a una página. Envía una petición a memoria con los datos necesarios y espera la respuesta. Devuelve el número de marco recibido o -1 en caso de error.
* @param  vec Vector de índices de tablas de páginas.
* @param  cpu_logger Logger para imprimir información.
* @param  nro_pagina Número de página.
* @return Número de marco recibido o -1 en caso de error.
*/
int buscar_marco_en_memoria(int vec[], t_log* cpu_logger, int nro_pagina) { //MMU
    int marco;
    t_buffer* buffer_peticion = crear_buffer();
    cargar_int_al_buffer(buffer_peticion, pid);
    cargar_int_al_buffer(buffer_peticion, nro_pagina);

    for(int j=0 ; j<cantidad_niveles ; j++){
        cargar_int_al_buffer(buffer_peticion, vec[j]); //indices de tabla de paginas
    }
    
    t_paquete* paquete = crear_paquete(CPU_M_ACCESO_TABLA_PAGINAS, buffer_peticion);
    enviar_paquete(paquete, socket_memoria);

    if(recibir_operacion(socket_memoria) == M_CPU_RESPUESTA_DIRECCION_FISICA) {
        t_buffer* buffer = recibir_buffer(socket_memoria);
        marco = extraer_int_del_buffer(buffer);    
        eliminar_buffer(buffer);
    } 
    else {
        log_debug(cpu_logger, "Memoria me contestó otra cosa");
        marco = -1; // Si no se pudo obtener el marco, retornar un valor de error
    }
    return marco;
}

//------------------ CACHE ------------------

/**
* @fn     void inicializar_cache(void)
* @brief  Inicializa la caché de páginas, reservando memoria para cada entrada y configurando sus valores iniciales. Deja todas las entradas listas para ser utilizadas por el sistema de caché de páginas.
* @param  Ninguno
* @return Ninguno
*/
void inicializar_cache() {
    lista_cache = list_create();
    for (int i = 0; i < entradas_cache(); i++) {
        t_entrada_cache* cache = malloc(sizeof(t_entrada_cache));
        cache->marco = -1;
        cache->numero_pagina = -1;
        cache->contenido = NULL;
        cache->bit_uso = false;
        cache->bit_modificado = false;
        cache->presente = false;
        list_add(lista_cache, cache); // Agregar a la lista de caché
    }
}

/**
* @fn     bool cache_habilitada(void)
* @brief  Indica si la caché está habilitada y tiene entradas disponibles. Devuelve true si la caché está habilitada, false en caso contrario.
* @param  Ninguno
* @return true si la caché está habilitada, false en caso contrario.
*/
bool cache_habilitada() {
    return (entradas_cache() > 0);
}

/**
* @fn     char* obtener_contenido_memoria(int marco, int nro_pagina, t_log* cpu_logger)
* @brief  Solicita a memoria el contenido de una página. Envía una petición a memoria y retorna el contenido recibido como un puntero a char, o NULL en caso de error.
* @param  marco Número de marco.
* @param  nro_pagina Número de página.
* @param  cpu_logger Logger para imprimir información.
* @return Puntero al contenido recibido o NULL en caso de error.
*/
char* obtener_contenido_memoria(int marco, int nro_pagina, t_log* cpu_logger) {
    t_buffer* buffer_peticion = crear_buffer();
    //TODO Verificar que a memoria le sirva el nro_pagina
    cargar_int_al_buffer(buffer_peticion, nro_pagina); //no se si necesito pasar el nro_pagina xq quiza 
    cargar_int_al_buffer(buffer_peticion, marco);

    t_paquete* paquete = crear_paquete(CPU_M_SOLICITAR_PAGINA, buffer_peticion);
    enviar_paquete(paquete, socket_memoria);

    if (recibir_operacion(socket_memoria) == M_CPU_RECIBIR_CONTENIDO) {
        t_buffer* buffer = recibir_buffer(socket_memoria);
        int marco_recibido = extraer_int_del_buffer(buffer);
        if(marco_recibido != marco) {
            log_error(cpu_logger, "Error: El marco recibido no coincide con el solicitado");
            eliminar_buffer(buffer); // Error al recibir el marco
        }
        //TODO Verificar que el char* este bien por que puede romper aca
        char* contenido = extraer_string_del_buffer(buffer); 
        printf("Contenido de la página %d recibido desde memoria: %s\n", nro_pagina, contenido);
        eliminar_buffer(buffer);
        return contenido;
    } 
    else {
        log_error(cpu_logger, "Error al obtener la página desde memoria");
    }
    return NULL; // Error al obtener la página
}

/**
* @fn     void cargar_contenido_cache(t_log* cpu_logger, int direccion_logica, int operacion, char* origen)
* @brief  Carga el contenido de una página en la caché, leyendo o escribiendo según la operación. Si la página está en caché, la utiliza directamente; si no, la carga desde memoria y la almacena en la caché. Permite operaciones de lectura y escritura.
* @param  cpu_logger Logger para imprimir información.
* @param  direccion_logica Dirección lógica de la operación.
* @param  operacion Tipo de operación (READ o WRITE).
* @param  origen Contenido a escribir en caso de WRITE.
* @return Ninguno
*/
void cargar_contenido_cache(t_log* cpu_logger, int direccion_logica, int operacion, char* origen) { 
    int nro_pagina = direccion_logica / tam_pagina;
    
    t_entrada_cache* entrada_cache = buscar_en_cache(nro_pagina);
    if (entrada_cache != NULL) { // HIT en cache
        entrada_cache -> bit_uso = true; // Para CLOCK/CLOCK-M
        log_info(cpu_logger,"Cache HIT: Leyendo contenido de la página %d desde la caché\n", nro_pagina);
        
    }
    else { //MISS CHACHE - no esta en la cahe, vamos a buscar la informacion en memmoria
        int vec[cantidad_niveles]; //obtengo el vector de niveles para luego obtener el marco
        for (int X = 1; X <= cantidad_niveles; X++) {
            int divisor = (int)pow(entradas_tabla, cantidad_niveles - X); //indices de tabla de paginas
            vec[X-1] = (nro_pagina / divisor) % entradas_tabla;
        }     
        entrada_cache -> marco = obtener_marco(nro_pagina, vec); //obtiene el marco, ya sea desde la tlb o desde memoria
        entrada_cache -> numero_pagina = nro_pagina;
        entrada_cache -> bit_uso = true;
        entrada_cache -> presente = true;
        entrada_cache -> contenido = obtener_contenido_memoria(entrada_cache->marco, nro_pagina, cpu_logger); // Asignar contenido de la pagina a la entrada de cache
        //ver si es necesario un free de contenido (lo que devuelve la funcion)

    }
    //Leer o escribir
    if (operacion == READ) {
        log_debug(cpu_logger, "Contenido leido desde cache %s", entrada_cache -> contenido); // Imprimir el contenido de la página
    } 
    else if (operacion == WRITE) {
        entrada_cache -> bit_modificado = true;
        memcpy(entrada_cache -> contenido, origen , tam_pagina); 
        log_debug(cpu_logger, "Contenido escrito en cache: %s \n", entrada_cache -> contenido);
        
    }
    
    actualizar_entrada_cache(entrada_cache);
}

/**
* @fn     t_entrada_cache* buscar_en_cache(int nro_pagina)
* @brief  Busca una entrada en la caché por número de página. Recorre la lista de entradas y retorna un puntero a la entrada si la encuentra y está presente, o NULL si no existe.
* @param  nro_pagina Número de página a buscar en la caché.
* @return Puntero a la entrada encontrada o NULL si no existe.
*/
t_entrada_cache* buscar_en_cache(int nro_pagina) {
    for (int i = 0; i < list_size(lista_cache); i++) {
        t_entrada_cache* entrada = list_get(lista_cache, i);
        if (entrada->presente && entrada->numero_pagina == nro_pagina) {
            return entrada;
        }
    }
    return NULL;  // No encontrado
}

/**
* @fn     void actualizar_entrada_cache(t_entrada_cache* entrada_cache_aux)
* @brief  Actualiza la caché con una nueva entrada. Si hay lugar disponible, la inserta directamente; si no, aplica el algoritmo de reemplazo configurado (CLOCK o CLOCK-M) para decidir qué entrada reemplazar.
* @param  entrada_cache_aux Nueva entrada de caché a insertar.
* @return Ninguno
*/
void actualizar_entrada_cache(t_entrada_cache* entrada_cache_aux){

    int indice_reemplazo_cache = encontrar_vacio(); // quizas no es necesario, probar de sacarlo, los algoritmos de clock y clock-m por como esta inicializada la lista funcionarian sin esto
    if(indice_reemplazo_cache == ESTA_LLENA){ //Siendo -1 que no hay lugares vacios
        if(strcmp(reemplazo_cache(), "CLOCK") == 0){ // aca los llamamos SOLO si la lista esta llena 
            reemplazar_cache_CLOCK(entrada_cache_aux);
        }
        else if(strcmp(reemplazo_cache(), "CLOCK-M") == 0){
            reemplazar_cache_CLOCK_M(entrada_cache_aux);
        }
    }
    else{ // Hay lugares vacios 
        list_replace_and_destroy_element(lista_cache, indice_reemplazo_cache, entrada_cache_aux, (void*)free);
    }
}

/**
* @fn     int encontrar_vacio(void)
* @brief  Busca un espacio vacío en la caché. Recorre la lista de entradas y retorna el índice de la primera entrada vacía encontrada, o ESTA_LLENA si no hay lugar disponible.
* @param  Ninguno
* @return Índice del espacio vacío o ESTA_LLENA si no hay lugar.
*/
int encontrar_vacio(void){
    t_entrada_cache* entrada_cache_aux;
    for(int i = 0; i < list_size(lista_cache); i++){
        entrada_cache_aux = list_get(lista_cache, i);

        if(entrada_cache_aux->numero_pagina == -1)
            return i; //Retorna el indice del registro t_entrada_cache vacio
    }

    return ESTA_LLENA; // Retorna -1 si no hay registros t_entrada_cache vacios
}

//Algoritmos de reemplazo de cache
int clock_pointer = 0;  // Para algoritmo CLOCK

/**
* @fn     void avanzar_puntero(void)
* @brief  Avanza el puntero del algoritmo CLOCK/CLOCK-M a la siguiente posición de la caché, de manera circular.
* @param  Ninguno
* @return Ninguno
*/
void avanzar_puntero() {
    clock_pointer = (clock_pointer + 1) % entradas_cache();
}

/**
* @fn     void reemplazar_cache_CLOCK(t_entrada_cache* nueva_entrada)
* @brief  Reemplaza una entrada en la caché utilizando el algoritmo CLOCK. Busca una entrada con bit de uso en 0 para reemplazarla; si el bit de uso está en 1, lo pone en 0 y avanza el puntero. Si la entrada a reemplazar está modificada, la escribe en memoria antes de reemplazarla.
* @param  nueva_entrada Nueva entrada de caché a insertar.
* @return Ninguno
*/
void reemplazar_cache_CLOCK(t_entrada_cache* nueva_entrada) {
    while (true) {
        t_entrada_cache* actual = list_get(lista_cache, clock_pointer);

        if (!actual->bit_uso) {
             if (actual->bit_modificado) {
                //TODO: mandar a memoria
                t_buffer* buffer_escritura = crear_buffer();
                cargar_int_al_buffer(buffer_escritura, actual->numero_pagina); // numero de pagina
                cargar_string_al_buffer(buffer_escritura, actual->contenido); // contenido a escribir
                t_paquete* paquete = crear_paquete(CPU_M_ESCRIBIR_MEMORIA, buffer_escritura);
                enviar_paquete(paquete, socket_memoria);
            }

            list_replace_and_destroy_element(lista_cache, clock_pointer, nueva_entrada, (void*)free);
            avanzar_puntero();  // Mover a la próxima posición
            break;
        } 
        else {
            actual->bit_uso = false;
            avanzar_puntero();
        }
    }
}

/**
* @fn     void reemplazar_cache_CLOCK_M(t_entrada_cache* nueva_entrada)
* @brief  Reemplaza una entrada en la caché utilizando el algoritmo CLOCK-M. Busca primero una entrada con bit de uso y modificado en 0; si no la encuentra, realiza un segundo ciclo considerando solo el bit de uso. Si la entrada a reemplazar está modificada, la escribe en memoria antes de reemplazarla.
* @param  nueva_entrada Nueva entrada de caché a insertar.
* @return Ninguno
*/
void reemplazar_cache_CLOCK_M(t_entrada_cache* nueva_entrada) {
    int entradas = entradas_cache();
    bool reemplazo_realizado = false;

    while (!reemplazo_realizado) {
        // (U=0, M=0)
        for (int i = 0; i < entradas; i++) {
            t_entrada_cache* actual = list_get(lista_cache, clock_pointer);

            if (!actual->bit_uso && !actual->bit_modificado) {
                list_replace_and_destroy_element(lista_cache, clock_pointer, nueva_entrada, (void*)free);
                avanzar_puntero();
                reemplazo_realizado = true;
                return;
            }

            avanzar_puntero();
        }

        //(U=0, M=1)
        for (int i = 0; i < entradas; i++) {
            t_entrada_cache* actual = list_get(lista_cache, clock_pointer);

            if (!actual->bit_uso && actual->bit_modificado) {
                //escribir en memroia el contenido de la pagina
                t_buffer* buffer_escritura = crear_buffer();
                cargar_int_al_buffer(buffer_escritura, actual->numero_pagina); // numero de pagina
                cargar_string_al_buffer(buffer_escritura, actual->contenido); // contenido a escribir
                t_paquete* paquete = crear_paquete(CPU_M_ESCRIBIR_MEMORIA, buffer_escritura);
                enviar_paquete(paquete, socket_memoria);

                list_replace_and_destroy_element(lista_cache, clock_pointer, nueva_entrada, (void*)free);
                avanzar_puntero();
                reemplazo_realizado = true;
                return;
            }

            actual->bit_uso = false;
            avanzar_puntero();
        }
    }
}





