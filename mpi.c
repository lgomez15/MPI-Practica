#include <mpi.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>
#include <math.h>

#define MAX 999999
#define N_NUMEROS 20
#define PESO_MIM 1000000
#define PESO_MEDIO 10000

/*INICIO definicion de tipos de proceso*/
#define PES_RANK 0 // Asumimos que el PES tiene el rango 0
#define PG_TYPE 1
#define PA_TYPE 2
#define PI_TYPE 3
/*FIN definicion de tipos de proceso*/

/*INICIO definicion de tipos de comunicacion*/
#define COM_TYPE 0 // Para enviar el tipo de proceso
#define CONFIGURE 1 // Para enviar el tamaño de los PA
#define CONF_DATA_1 2 // Para enviar el tamaño de los PA
#define CONF_DATA_2 3 // Para enviar los ids de los PA
#define CONF_DATA_3 4 // Para enviar el número random
#define DISP_REQ 5
#define DISP_RES 6
#define RESPUESTA_ADIVINANZA 7
#define ESTADISTICAS 8
#define INSTRUCCION 9
#define RESPUESTA_INSTRUCCION 111
/*INICIO definicion de tipos de comunicacion*/

/*INICIO definicion de estados*/
#define JUGANDO 11
#define INTERMEDIO 12
#define FINALIZAR 13
/*FIN definicion de estados*/



/*Struct con float para el tiempo, numero de intentos totales, numero de send, de recv y de probes*/
MPI_Datatype Estadisticas_mpi;

typedef struct {
    double tiempo;
    double tiempo_c;
    int intentos;
    int send;
    int recv;
    int probes;
    int tipo;
    int numero;
    int proceso;
    int consultas;
} Estadisticas;

typedef struct {
    int num_proc;
    int tipo;
    double tt;
    double tt_computo;
    double ptc;
    int send;
    int recv;
    int probes;
} Estadisticas_Finales;
/*FIN struct con float para el tiempo, numero de intentos totales, numero de send, de recv y de probes*/

/*INICIO Funciones auxiliares*/
void fuerza_espera(unsigned long peso);
double Wtime(void);
/*FIN Funciones auxiliares*/

/*INICIO funciones del proceso PES*/
void PES_Code(int world_size, int num_pg, int num_pa);
/*FIN funciones del proceso PES*/

/*INICIO funciones del proceso PG*/
void PG_Code(int my_rank, int world_size);
int solicitarAsignacion(int my_rank, int num_pa, int *pa_ids, Estadisticas *stats);
/*FIN funciones del proceso PES*/


/*INICIO funciones del proceso PA*/
void PA_Code(int my_rank);
/*FIN funciones del proceso PA*/

/*INICIO funciones del proceso PI*/
void PI_Code(int my_rank);
/*FIN funciones del proceso PI*/



int main(int argc, char** argv) {

    MPI_Init(&argc, &argv);

    int world_size;
    MPI_Comm_size(MPI_COMM_WORLD, &world_size); // Obtener el número de procesos

    int my_rank;
    MPI_Comm_rank(MPI_COMM_WORLD, &my_rank);

    if(world_size < atoi(argv[1]) + atoi(argv[2]) + 1) {
        if(my_rank == 0) {
            printf("El número de procesos debe ser mayor o igual a la suma de PG y PA más el PES\n");
        }
        MPI_Finalize();
        return 0;
    }


    // Definir un tipo de dato MPI correspondiente al struct Estadisticas
    MPI_Datatype types[10] = {MPI_DOUBLE, MPI_DOUBLE, MPI_INT, MPI_INT, MPI_INT, MPI_INT, MPI_INT, MPI_INT, MPI_INT, MPI_INT};
    int block_lengths[10] = {1, 1, 1, 1, 1, 1, 1, 1, 1, 1};
    MPI_Aint offsets[10];
    offsets[0] = offsetof(Estadisticas, tiempo);
    offsets[1] = offsetof(Estadisticas, tiempo_c);
    offsets[2] = offsetof(Estadisticas, intentos);
    offsets[3] = offsetof(Estadisticas, send);
    offsets[4] = offsetof(Estadisticas, recv);
    offsets[5] = offsetof(Estadisticas, probes);
    offsets[6] = offsetof(Estadisticas, tipo);
    offsets[7] = offsetof(Estadisticas, numero);
    offsets[8] = offsetof(Estadisticas, proceso);
    offsets[9] = offsetof(Estadisticas, consultas);
    MPI_Type_create_struct(10, block_lengths, offsets, types, &Estadisticas_mpi);
    MPI_Type_commit(&Estadisticas_mpi);

    // Inicializar la semilla para la generación de números aleatorios
    srand(time(NULL) + my_rank);

    if (my_rank == PES_RANK) { // Si el proceso es el PES
        
        int num_pg = atoi(argv[1]);
        int num_pa = atoi(argv[2]);

        PES_Code(world_size, num_pg, num_pa);
    } else {
        // Esperar a recibir el tipo de proceso asignado
        int process_type;
        MPI_Recv(&process_type, 1, MPI_INT, PES_RANK, COM_TYPE, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
        
        if (process_type == PG_TYPE) {
            PG_Code(my_rank, world_size);
        } else if (process_type == PA_TYPE) {
            PA_Code(my_rank);
        } else {
			PI_Code(my_rank);
		}
    }

    MPI_Finalize();
    return 0;
}

void PES_Code(int world_size, int num_pg, int num_pa) {

    double t_inicio = Wtime();
    double t_fin;
    double t_total;
    //array de estadisticas del tamano de N_NUMEROS
    Estadisticas estadisticas_pg[N_NUMEROS];
    Estadisticas estadisticas_pa[num_pa];
    Estadisticas estadisticas_pi;
    int num_pi = world_size - (num_pa + num_pg);


    printf("+--------------------------------------+\n");
    printf("| Numero de procesos: %d\n", world_size);
    printf("| Numero de PG: %d\n", num_pg);
    printf("| Numero de PA: %d\n", num_pa);
    printf("| Numero de PI: %d\n", num_pi);
    printf("+--------------------------------------+\n");

    // Imprimir información detallada de cada proceso
    for (int i = 0; i < world_size; i++) {
        printf("| P%d: ", i);
        if (i == 0) {
            printf("PES\n");
        } else if (i <= num_pg && i > 0) {
            printf("PG\n");
        } else if (i <= num_pg + num_pa && i > num_pg) {
            printf("PA\n");
        } else {
            printf("PI\n");
        }
    }
    printf("+--------------------------------------+\n");

    
    // Asignar y enviar roles a PG y PA
    for (int i = 1; i < world_size; i++) {
        int process_type;
        if (i <= num_pg) {
            process_type = PG_TYPE;
        } else if (i <= num_pg + num_pa) {
            process_type = PA_TYPE;
        } else {
            process_type = PI_TYPE;
        }
        MPI_Send(&process_type, 1, MPI_INT, i, COM_TYPE, MPI_COMM_WORLD);
    }
    
    int *array_pa_ids = (int *)malloc(num_pa * sizeof(int)); // Corrección aquí
    for(int i = num_pg + 1; i <= num_pg + num_pa; i++) {
        array_pa_ids[i - num_pg - 1] = i;
    }

    // Enviamos a los PG el tamaño de los PA
    for(int i = 1; i <= num_pg; i++) {
        MPI_Send(&num_pa, 1, MPI_INT, i, CONF_DATA_1, MPI_COMM_WORLD);
    }

    // Enviamos a los PG los ids de los PA
    for(int i = 1; i <= num_pg; i++) {
        MPI_Send(array_pa_ids, num_pa, MPI_INT, i, CONF_DATA_2, MPI_COMM_WORLD); // Corrección aplicada
    }

    int adivinados = 0;
	int random;
        
	for(int i = 1; i <= num_pg; i++) {
		random = rand() % MAX;
		MPI_Send(&random, 1, MPI_INT, i, CONF_DATA_3, MPI_COMM_WORLD);
	}

    Estadisticas received_stats;
    int instruccion;

    while(1){ 
        MPI_Status status;
        MPI_Recv(&received_stats, 1, Estadisticas_mpi, MPI_ANY_SOURCE, ESTADISTICAS, MPI_COMM_WORLD, &status);
        estadisticas_pg[adivinados] = received_stats;
        adivinados++;

        if(adivinados == N_NUMEROS){
            instruccion = 0;
            for(int i = 1; i <= num_pg; i++) {
                MPI_Send(&instruccion, 1, MPI_INT, i, INSTRUCCION, MPI_COMM_WORLD);
                //recibir respuesta instrucción
                MPI_Recv(&instruccion, 1, MPI_INT, i, RESPUESTA_INSTRUCCION, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
            }
            break;
        }else{
            //enviar instruccion
            instruccion = 1;
            MPI_Send(&instruccion, 1, MPI_INT, status.MPI_SOURCE, INSTRUCCION, MPI_COMM_WORLD);

            //esperar confirmacion de instruccion
            MPI_Recv(&instruccion, 1, MPI_INT, status.MPI_SOURCE, RESPUESTA_INSTRUCCION, MPI_COMM_WORLD, MPI_STATUS_IGNORE);

            //enviar el siguiente numero
            random = rand() % MAX;
            MPI_Send(&random, 1, MPI_INT, status.MPI_SOURCE, CONF_DATA_3, MPI_COMM_WORLD);
        }
        
    }

    // enviar instruccion de finalizar a los procesos PA
    instruccion = 0;
    for(int i = num_pg + 1; i <= num_pg + num_pa; i++) {
        MPI_Send(&instruccion, 1, MPI_INT, i, INSTRUCCION, MPI_COMM_WORLD);

        //recibir estadisticas
        MPI_Recv(&received_stats, 1, Estadisticas_mpi, i, ESTADISTICAS, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
        estadisticas_pa[i - num_pg - 1] = received_stats;
    }

    //enviar instruccion de finalizar a los procesos PI
    for(int i = num_pg + num_pa + 1; i < world_size; i++) {
        MPI_Send(&instruccion, 1, MPI_INT, i, INSTRUCCION, MPI_COMM_WORLD);

        //recibir estadisticas
        MPI_Recv(&received_stats, 1, Estadisticas_mpi, i, ESTADISTICAS, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
        estadisticas_pi = received_stats;
    }

    //array de estadisticas totales para todos los procesos
    Estadisticas_Finales estadisticas_finales[world_size -1];

    //llenar el array de estadisticas finales con la info de los PG
    for(int i = 0 ; i < num_pg; i++){
        estadisticas_finales[i].num_proc = i+1;
        estadisticas_finales[i].tipo = PG_TYPE;
        estadisticas_finales[i].tt = 0;
        estadisticas_finales[i].tt_computo = 0;
        estadisticas_finales[i].ptc = 0;
        estadisticas_finales[i].send = 0;
        estadisticas_finales[i].recv = 0;
        estadisticas_finales[i].probes = 0;

        for(int j =0; j < N_NUMEROS; j++)
        {
            if( estadisticas_finales[i].num_proc == estadisticas_pg[j].proceso)
            {
                estadisticas_finales[i].tt = estadisticas_finales[i].tt + estadisticas_pg[j].tiempo;
                estadisticas_finales[i].tt_computo = estadisticas_finales[i].tt_computo + estadisticas_pg[j].tiempo_c;
                estadisticas_finales[i].send = estadisticas_finales[i].send + estadisticas_pg[j].send;
                estadisticas_finales[i].recv = estadisticas_finales[i].recv + estadisticas_pg[j].recv;
                estadisticas_finales[i].probes = estadisticas_finales[i].probes + estadisticas_pg[j].probes;
            }
        }

        estadisticas_finales[i].ptc = (estadisticas_finales[i].tt_computo / estadisticas_finales[i].tt) * 100;
    }

    //llenar el array de estadisticas finales con la info de los PA
    for(int i = num_pg; i < num_pg + num_pa; i++){
        estadisticas_finales[i].num_proc = array_pa_ids[i - num_pg];
        estadisticas_finales[i].tipo = PA_TYPE;
        estadisticas_finales[i].tt = estadisticas_pa[i - num_pg].tiempo;
        estadisticas_finales[i].tt_computo = estadisticas_pa[i - num_pg].tiempo_c;
        estadisticas_finales[i].ptc = (estadisticas_pa[i - num_pg].tiempo_c / estadisticas_pa[i - num_pg].tiempo) * 100;
        estadisticas_finales[i].send = estadisticas_pa[i - num_pg].send;
        estadisticas_finales[i].recv = estadisticas_pa[i - num_pg].recv;
        estadisticas_finales[i].probes = estadisticas_pa[i - num_pg].probes;
    }

    //llenar el array de estadisticas finales con la info de los PI
    for(int i = num_pg + num_pa; i < world_size - 1; i++){
        estadisticas_finales[i].num_proc = i + 1;
        estadisticas_finales[i].tipo = PI_TYPE;
        estadisticas_finales[i].tt = estadisticas_pi.tiempo;
        estadisticas_finales[i].tt_computo = estadisticas_pi.tiempo_c;
        estadisticas_finales[i].ptc = (estadisticas_pi.tiempo_c / estadisticas_pi.tiempo) * 100;
        estadisticas_finales[i].send = estadisticas_pi.send;
        estadisticas_finales[i].recv = estadisticas_pi.recv;
        estadisticas_finales[i].probes = estadisticas_pi.probes;
    }

    t_fin = Wtime();
    t_total = t_fin - t_inicio;

    
    printf("Estadísticas parciales\n");
    printf("%-5s %-17s %-9s %-17s %-9s %-17s\n", "PG", "Consultas_Disp", "Tiempo", "Tiempo_Computo", "Intentos", "Numero_Adivinado");
    for(int i = 0; i < N_NUMEROS; i++){
        printf("%-5d %-17d %-9f %-17f %-9d %-17d\n", estadisticas_pg[i].proceso, estadisticas_pg[i].consultas, estadisticas_pg[i].tiempo, estadisticas_pg[i].tiempo_c, estadisticas_pg[i].intentos, estadisticas_pg[i].numero);
    }

    printf("\nEstadísticas finales\n");
    printf("%-5s %-5s %-9s %-17s %-27s %-5s %-5s %-7s\n", "PG", "tipo", "Tiempo", "Tiempo_Computo", "Porcentaje_Tiempo_Computo", "Send", "Recv", "Probes");
    for(int i = 0; i < world_size -1; i++){
        printf("%-5d %-5d %-9f %-17f %-27f %-5d %-5d %-7d\n", estadisticas_finales[i].num_proc, estadisticas_finales[i].tipo, estadisticas_finales[i].tt, estadisticas_finales[i].tt_computo, estadisticas_finales[i].ptc, estadisticas_finales[i].send, estadisticas_finales[i].recv, estadisticas_finales[i].probes);
    }
    printf("\n+--------------------------------------+\n");
    printf("Tiempo total: %f\n", t_total);
    printf("+--------------------------------------+\n");

    

    free(array_pa_ids); // No olvides liberar la memoria
    return;
}


void PG_Code(int my_rank, int world_size) {
    int estado;
    int proc_adv;
	int num_pa;
    int numero;
    int instruccion;
    double tiempo_total_i = 0.0;
    double tiempo_computo_i = 0.0;
    double tiempo_total_f;
    
    // estadiaticas
    Estadisticas stats;
    stats.proceso = my_rank;
    stats.tipo = PG_TYPE;
    stats.tiempo = 0;
    stats.tiempo_c = 0;
    stats.intentos = 0;
    stats.send = 0;
    stats.recv = 0;
    stats.probes = 0;
    stats.numero = 0;
    stats.consultas = 0;

	//recibimos el tamaño de los PA
	MPI_Recv(&num_pa, 1, MPI_INT, PES_RANK, CONF_DATA_1, MPI_COMM_WORLD, MPI_STATUS_IGNORE);

	//recibimos los ids de los PA
	int *pa_ids = (int *)malloc(num_pa * sizeof(int));
	MPI_Recv(pa_ids, num_pa, MPI_INT, PES_RANK, CONF_DATA_2, MPI_COMM_WORLD, MPI_STATUS_IGNORE);

	//recibimos el número numero_adivinar
	int numero_adivinar;
	MPI_Recv(&numero_adivinar, 1, MPI_INT, PES_RANK, CONF_DATA_3, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
    stats.numero = numero_adivinar;

    //solicitar asignacion, y ahora le pasamos los stats
    proc_adv = solicitarAsignacion(my_rank, num_pa, pa_ids, &stats);
    estado = JUGANDO;

    tiempo_total_i = Wtime();
    
    while(estado != FINALIZAR){
        switch (estado)
        {
        case JUGANDO:
        
            while(estado == JUGANDO)
            {
                MPI_Recv(&numero, 1, MPI_INT, proc_adv, RESPUESTA_ADIVINANZA, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
                stats.recv++;
                tiempo_computo_i = Wtime();
                if(numero == numero_adivinar){
                    fuerza_espera(PESO_MIM);
                    tiempo_total_f = Wtime();
                    stats.tiempo_c = stats.tiempo_c + (Wtime() - tiempo_computo_i);
                    stats.tiempo = tiempo_total_f - tiempo_total_i;
                    char respuesta = '=';
                    MPI_Send(&respuesta, 1, MPI_CHAR, proc_adv, RESPUESTA_ADIVINANZA, MPI_COMM_WORLD);
                    stats.send++;
                    estado = INTERMEDIO;
                }else if(numero < numero_adivinar){
                    fuerza_espera(PESO_MIM);
                    stats.tiempo_c = stats.tiempo_c + (Wtime() - tiempo_computo_i);
                    char respuesta = '+';
                    MPI_Send(&respuesta, 1, MPI_CHAR, proc_adv, RESPUESTA_ADIVINANZA, MPI_COMM_WORLD);
                    stats.intentos++;
                    stats.send++;
                }else if(numero > numero_adivinar){
                    fuerza_espera(PESO_MIM);
                    stats.tiempo_c = stats.tiempo_c + (Wtime() - tiempo_computo_i);
                    char respuesta = '-';
                    MPI_Send(&respuesta, 1, MPI_CHAR, proc_adv, RESPUESTA_ADIVINANZA, MPI_COMM_WORLD);
                    stats.intentos++;
                    stats.send++;
                }
            }
            break;
        case INTERMEDIO:
            //enviar estadisticas

            stats.tiempo = tiempo_total_f - tiempo_total_i;
            MPI_Send(&stats, 1, Estadisticas_mpi, PES_RANK, ESTADISTICAS, MPI_COMM_WORLD);
            //recibir instruccion
            MPI_Recv(&instruccion, 1, MPI_INT, PES_RANK, INSTRUCCION, MPI_COMM_WORLD, MPI_STATUS_IGNORE);

            //confirmar instruccion
            MPI_Send(&instruccion, 1, MPI_INT, PES_RANK, RESPUESTA_INSTRUCCION, MPI_COMM_WORLD);


            if(instruccion == 1){
                estado = JUGANDO;
                //resetear las estadisticas
                stats.tiempo = 0;
                stats.tiempo_c = 0;
                stats.intentos = 0;
                stats.send = 0;
                stats.recv = 0;
                stats.probes = 0;
                stats.numero = 0;
                stats.consultas = 0;

                //recibir el número numero_adivinar
                MPI_Recv(&numero_adivinar, 1, MPI_INT, PES_RANK, CONF_DATA_3, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
                stats.numero = numero_adivinar;

                //solicitar asignacion
                proc_adv = solicitarAsignacion(my_rank, num_pa, pa_ids, &stats);



            }else{
                estado = FINALIZAR;
            }

            break;
            case FINALIZAR:
                return;
            break;        
        default:
            printf("Error en el estado\n");
        break;
        }
    }
    
}

int solicitarAsignacion(int my_rank, int num_pa, int *pa_ids, Estadisticas *stats) {

    int asignado = 0; 
    int respuesta; 
    int proc_adv = -1; 
    MPI_Request request;
    MPI_Status status;

    while(!asignado) {
        for(int i = 0; i < num_pa; i++) {
            // Envía solicitud de disponibilidad a cada PA
            MPI_Send(&my_rank, 1, MPI_INT, pa_ids[i], DISP_REQ, MPI_COMM_WORLD);
            stats->consultas++;

            // Espera la respuesta de cada PA
            MPI_Recv(&respuesta, 1, MPI_INT, pa_ids[i], DISP_RES, MPI_COMM_WORLD, MPI_STATUS_IGNORE);

             if(respuesta == 0) { 
                asignado = 1; 
                proc_adv = pa_ids[i]; 

                break; 
            }
        }
    }

    return proc_adv; 
}


void PA_Code(int my_rank) {

    double tiempo_total_i = 0.0;
    double tiempo_total_f;
    double tiempo_computo_i = 0.0;
    double tiempo_computo_f;
    int instruccion;
    int disponible = 0;
    int fin = 1;
    MPI_Status status;
    int proc_ges = -1;
    int limite_inferior;
    int limite_superior;
    char respuesta;
    int intento;
    int x = MAX / 2;

    Estadisticas stats;
    stats.proceso = my_rank;
    stats.tipo = PA_TYPE;
    stats.tiempo = 0;
    stats.tiempo_c = 0;
    stats.intentos = 0;
    stats.send = 0;
    stats.recv = 0;
    stats.probes = 0;
    stats.numero = 0;
    stats.consultas = 0;

    tiempo_total_i = Wtime();

    while(fin != 0)
    {
        MPI_Probe(MPI_ANY_SOURCE, MPI_ANY_TAG,MPI_COMM_WORLD, &status);
        stats.probes++;
        switch(status.MPI_TAG)
        {
          
            case DISP_REQ:
                MPI_Recv(&proc_ges,1,MPI_INT,status.MPI_SOURCE, DISP_REQ, MPI_COMM_WORLD,&status);
                stats.recv++;
                if(disponible == 0)
                {         
                    //printf("PA %d ha aceptado con PG %d\n", my_rank, proc_ges);
                    //enviar un 0 para indicar que esta disponible
                    MPI_Send(&disponible,1,MPI_INT,proc_ges,DISP_RES,MPI_COMM_WORLD);
                    stats.send++;
                    disponible = 1;
                    limite_inferior = 0;
                    limite_superior = MAX;

                    //enviamos el primer intento
                    MPI_Send(&x, 1, MPI_INT, proc_ges, RESPUESTA_ADIVINANZA, MPI_COMM_WORLD);
                    stats.send++;

                    

                }else{
                    //enviar un 1 para indicar que esta ocupado
                    MPI_Send(&disponible,1,MPI_INT,status.MPI_SOURCE,DISP_RES,MPI_COMM_WORLD);
                    stats.send++;
                }
            break;
            case RESPUESTA_ADIVINANZA: 
                
                intento = (limite_inferior + limite_superior) / 2;      
                while (limite_inferior <= limite_superior)
                {
                    tiempo_computo_i = Wtime();
                    //recibir la respuesta
                    MPI_Recv(&respuesta, 1, MPI_CHAR, status.MPI_SOURCE, RESPUESTA_ADIVINANZA, MPI_COMM_WORLD, &status);
                    stats.recv++;
                    if (respuesta == '=') {
                        //printf("PA %d ha adivinado el número %d en %d intentos\n", my_rank, intento, numIntentos);   
                        disponible = 0;
                        break;
                    } else if (respuesta == '+') {
                        limite_inferior = intento + 1;
                        stats.intentos++;
                    } else if(respuesta == '-'){
                        limite_superior = intento - 1;
                        stats.intentos++;
                    }

                    intento = (limite_inferior + limite_superior) / 2;

                    fuerza_espera(PESO_MEDIO);
                    MPI_Send(&intento, 1, MPI_INT, status.MPI_SOURCE, RESPUESTA_ADIVINANZA, MPI_COMM_WORLD);
                    stats.send++;
                    stats.tiempo_c = stats.tiempo_c + (Wtime() - tiempo_computo_i);

                    break;
                    
                }
             break;
            case INSTRUCCION:
                MPI_Recv(&instruccion, 1, MPI_INT, status.MPI_SOURCE, INSTRUCCION, MPI_COMM_WORLD, &status);
                if(instruccion == 0){
                    //enviar estadisticas
                    tiempo_total_f = Wtime();
                    stats.tiempo = tiempo_total_f - tiempo_total_i;
                    MPI_Send(&stats, 1, Estadisticas_mpi, PES_RANK, ESTADISTICAS, MPI_COMM_WORLD);
                    return;
                }
            break;
        }

        
    }   

}


void PI_Code(int my_rank) {

    double tiempo_total_i = 0.0;
    double tiempo_total_f;

    Estadisticas stats;
    stats.proceso = my_rank;
    stats.tipo = PI_TYPE;
    stats.tiempo = 0;
    stats.tiempo_c = 0;
    stats.intentos = 0;
    stats.send = 0;
    stats.recv = 0;
    stats.probes = 0;
    stats.numero = 0;
    stats.consultas = 0;

    int instruccion;
    MPI_Status status;

    tiempo_total_i = Wtime();

    //esperar instruccion de finalizar
    MPI_Recv(&instruccion, 1, MPI_INT, PES_RANK, INSTRUCCION, MPI_COMM_WORLD, &status);

    tiempo_total_f = Wtime();

    stats.tiempo = tiempo_total_f - tiempo_total_i;

    //enviar estadisticas
    MPI_Send(&stats, 1, Estadisticas_mpi, PES_RANK, ESTADISTICAS, MPI_COMM_WORLD);
    return;

}

void fuerza_espera(unsigned long peso)
{
    for (unsigned long i=1; i<1*peso; i++) sqrt(i);
}

double Wtime(void) {
  struct timeval tv;
  if(gettimeofday(&tv, 0) < 0) {
    perror("oops");
  }
  return (double)tv.tv_sec + (0.000001 * (double)tv.tv_usec);
}
