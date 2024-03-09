#include <mpi.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>

#define MAX 999999
#define N_NUMEROS 20

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
/*INICIO definicion de tipos de comunicacion*/

/*INICIO definicion de estados*/
#define JUGANDO 11
#define INTERMEDIO 12
#define FINALIZAR 13
/*FIN definicion de estados*/



/*Struct con float para el tiempo, numero de intentos totales, numero de send, de recv y de probes*/
MPI_Datatype Estadisticas_mpi;

typedef struct {
    float tiempo;
    int intentos;
    int send;
    int recv;
    int probes;
    int tipo;
    int numero;
} Estadisticas;
/*FIN struct con float para el tiempo, numero de intentos totales, numero de send, de recv y de probes*/


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

	int num_pg = atoi(argv[1]);
	int num_pa = atoi(argv[2]);

    MPI_Init(&argc, &argv);

    int world_size;
    MPI_Comm_size(MPI_COMM_WORLD, &world_size); // Obtener el número de procesos

    int my_rank;
    MPI_Comm_rank(MPI_COMM_WORLD, &my_rank);

    // Definir un tipo de dato MPI correspondiente al struct Estadisticas
    MPI_Datatype types[7] = {MPI_FLOAT, MPI_INT, MPI_INT, MPI_INT, MPI_INT, MPI_INT, MPI_INT};
    int block_lengths[7] = {1, 1, 1, 1, 1, 1, 1};
    MPI_Aint offsets[7];
    offsets[0] = offsetof(Estadisticas, tiempo);
    offsets[1] = offsetof(Estadisticas, intentos);
    offsets[2] = offsetof(Estadisticas, send);
    offsets[3] = offsetof(Estadisticas, recv);
    offsets[4] = offsetof(Estadisticas, probes);
    offsets[5] = offsetof(Estadisticas, tipo);
    offsets[6] = offsetof(Estadisticas, numero);
    MPI_Type_create_struct(7, block_lengths, offsets, types, &Estadisticas_mpi);
    MPI_Type_commit(&Estadisticas_mpi);

    // Inicializar la semilla para la generación de números aleatorios
    srand(time(NULL) + my_rank);

    if (my_rank == PES_RANK) { // Si el proceso es el PES
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

    //array de estadisticas del tamano de N_NUMEROS
    Estadisticas estadisticas[N_NUMEROS];
    int num_pi = world_size - (num_pa + num_pg);
    
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
        //imprimir estadisticas
        estadisticas[adivinados] = received_stats;
        adivinados++;

        if(adivinados == N_NUMEROS){
            instruccion = 0;
            for(int i = 1; i <= num_pg; i++) {
                MPI_Send(&instruccion, 1, MPI_INT, i, INSTRUCCION, MPI_COMM_WORLD);
            }
            break;
        }

        //enviar instruccion
        instruccion = 1;
        MPI_Send(&instruccion, 1, MPI_INT, status.MPI_SOURCE, INSTRUCCION, MPI_COMM_WORLD);

        //enviar el siguiente numero
        random = rand() % MAX;
        MPI_Send(&random, 1, MPI_INT, status.MPI_SOURCE, CONF_DATA_3, MPI_COMM_WORLD);
        
    }

    //

    free(array_pa_ids); // No olvides liberar la memoria
    return;
}


void PG_Code(int my_rank, int world_size) {
    
    int estado;
    int proc_adv;
	int num_pa;
    int numero;
    int instruccion;

    // estadiaticas
    Estadisticas stats;
    stats.tiempo = 0;
    stats.intentos = 0;
    stats.send = 0;
    stats.recv = 0;
    stats.probes = 0;
    stats.tipo = PG_TYPE;
    stats.numero = 0;

	//recibimos el tamaño de los PA
	MPI_Recv(&num_pa, 1, MPI_INT, PES_RANK, CONF_DATA_1, MPI_COMM_WORLD, MPI_STATUS_IGNORE);

	//recibimos los ids de los PA
	int *pa_ids = (int *)malloc(num_pa * sizeof(int));
	MPI_Recv(pa_ids, num_pa, MPI_INT, PES_RANK, CONF_DATA_2, MPI_COMM_WORLD, MPI_STATUS_IGNORE);

	//recibimos el número numero_adivinar
	int numero_adivinar;
	MPI_Recv(&numero_adivinar, 1, MPI_INT, PES_RANK, CONF_DATA_3, MPI_COMM_WORLD, MPI_STATUS_IGNORE);

    //solicitar asignacion, y ahora le pasamos los stats
    proc_adv = solicitarAsignacion(my_rank, num_pa, pa_ids, &stats);
   
    estado = JUGANDO;


    
    while(estado != FINALIZAR){
        switch (estado)
        {
        case JUGANDO:
        
            while(estado == JUGANDO)
            {
                MPI_Recv(&numero, 1, MPI_INT, proc_adv, RESPUESTA_ADIVINANZA, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
                stats.recv++;
                if(numero == numero_adivinar){
                    char respuesta = '=';
                    MPI_Send(&respuesta, 1, MPI_CHAR, proc_adv, RESPUESTA_ADIVINANZA, MPI_COMM_WORLD);
                    stats.send++;
                    estado = INTERMEDIO;
                    

                }else if(numero < numero_adivinar){
                    char respuesta = '+';
                    MPI_Send(&respuesta, 1, MPI_CHAR, proc_adv, RESPUESTA_ADIVINANZA, MPI_COMM_WORLD);
                    stats.intentos++;
                    stats.send++;
                }else if(numero > numero_adivinar){
                    char respuesta = '-';
                    MPI_Send(&respuesta, 1, MPI_CHAR, proc_adv, RESPUESTA_ADIVINANZA, MPI_COMM_WORLD);
                    stats.intentos++;
                    stats.send++;
                }
            }
            break;
        case INTERMEDIO:
            //enviar estadisticas
            stats.tipo = PG_TYPE;
            stats.numero = numero_adivinar;
            MPI_Send(&stats, 1, Estadisticas_mpi, PES_RANK, ESTADISTICAS, MPI_COMM_WORLD);
            stats.send++;

            //recibir instruccion
            MPI_Recv(&instruccion, 1, MPI_INT, PES_RANK, INSTRUCCION, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
            stats.recv++;

            if(instruccion == 1){
                estado = JUGANDO;
                //resetear las estadisticas
                stats.tiempo = 0;
                stats.intentos = 0;
                stats.send = 0;
                stats.recv = 0;
                stats.probes = 0;
                stats.tipo = PG_TYPE;
                stats.numero = 0;

                //recibir el número numero_adivinar
                MPI_Recv(&numero_adivinar, 1, MPI_INT, PES_RANK, CONF_DATA_3, MPI_COMM_WORLD, MPI_STATUS_IGNORE);

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
            stats->send++;

            // Espera la respuesta de cada PA
            MPI_Recv(&respuesta, 1, MPI_INT, pa_ids[i], DISP_RES, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
            stats->recv++;

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
    stats.tiempo = 0;
    stats.intentos = 0;
    stats.send = 0;
    stats.recv = 0;
    stats.probes = 0;
    stats.tipo = PA_TYPE;
    

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

                    //enviar el intento
                    MPI_Send(&intento, 1, MPI_INT, status.MPI_SOURCE, RESPUESTA_ADIVINANZA, MPI_COMM_WORLD);
                    stats.send++;
                    
                    break;
                    
                }
             break;
            case INSTRUCCION:
                MPI_Recv(&fin, 1, MPI_INT, status.MPI_SOURCE, INSTRUCCION, MPI_COMM_WORLD, &status);
                if(fin == 0){
                    //recibimos
                    return;
                }
            break;
        }

        
    }   

}


void PI_Code(int my_rank) {
	//print my rank and type
	//printf("PI %d\n", my_rank);
}


