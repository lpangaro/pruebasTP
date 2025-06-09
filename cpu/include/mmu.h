#ifndef MMU_H_
#define MMU_H_
#include "cpu.h"
#include "math.h"
#include <stdio.h>
#include <stdlib.h>
#include <utils/utils.h>

/* TLB y MEMORIA*/
typedef struct {
    int numero_pagina;
    int marco;
    time_t tstamp_creado;
    time_t tstamp_ultima_vez_usado;
} t_entrada_TLB; //cada cuadradito

void iniciar_TLB(void);
t_entrada_TLB* buscar_en_TLB(int numero_pagina);
int traducir_dir_logica(int direccion_logica, t_log* logger);
void actualizar_TLB(t_entrada_TLB* registro_tlb_nuevo);
void reemplazar_TLB_LRU(t_entrada_TLB* registro_tlb_nuevo);
int verificar_reemplazo_TLB(void);
int existe_entrada_con_marco(t_entrada_TLB* registro_tlb_nuevo);
int solicitar_direcciones_memoria(int vec[], int cantidad_niveles, t_log* cpu_logger, int nro_pagina);
void reemplazar_TLB_FIFO(t_entrada_TLB* registro_tlb_nuevo);

#endif