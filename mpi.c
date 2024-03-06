#include <mpi.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>

#define MAX 100

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
#define FINALIZAR 8
/*INICIO definicion de tipos de comunicacion*/

/*INICIO definicion de estados*/
#define JUGANDO 11
/*FIN definicion de estados*/

/*INICIO funciones del proceso PES*/
void PES_Code(int world_size, int num_pg, int num_pa);
/*FIN funciones del proceso PES*/


/*INICIO funciones del proceso PG*/
void PG_Code(int my_rank, int world_size);
int solicitarAsignacion(int my_rank, int num_pa, int *pa_ids);
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

	//numero random para cada PG en el rango MAX
	int random;
	for(int i = 1; i <= num_pg; i++) {
		random = rand() % MAX;
		MPI_Send(&random, 1, MPI_INT, i, CONF_DATA_3, MPI_COMM_WORLD);
	}

    free(array_pa_ids); // No olvides liberar la memoria
}


void PG_Code(int my_rank, int world_size) {
    
    int estado;
    int proc_adv;
	int num_pa;
    int numero;

	//recibimos el tamaño de los PA
	MPI_Recv(&num_pa, 1, MPI_INT, PES_RANK, CONF_DATA_1, MPI_COMM_WORLD, MPI_STATUS_IGNORE);

	//recibimos los ids de los PA
	int *pa_ids = (int *)malloc(num_pa * sizeof(int));
	MPI_Recv(pa_ids, num_pa, MPI_INT, PES_RANK, CONF_DATA_2, MPI_COMM_WORLD, MPI_STATUS_IGNORE);

	//recibimos el número numero_adivinar
	int numero_adivinar;
	MPI_Recv(&numero_adivinar, 1, MPI_INT, PES_RANK, CONF_DATA_3, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
	printf("PG %d ha recibido el número numero_adivinar %d\n", my_rank, numero_adivinar);	

    proc_adv = solicitarAsignacion(my_rank, num_pa, pa_ids);
    printf("PG %d ha sido emparejado con PA %d####\n", my_rank, proc_adv);
    estado = JUGANDO;


    
    while(estado != FINALIZAR){
        switch (estado)
        {
        case JUGANDO:
        
            while(estado == JUGANDO)
            {
                MPI_Recv(&numero, 1, MPI_INT, proc_adv, RESPUESTA_ADIVINANZA, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
                if(numero == numero_adivinar){
                    char respuesta = '=';
                    MPI_Send(&respuesta, 1, MPI_CHAR, proc_adv, RESPUESTA_ADIVINANZA, MPI_COMM_WORLD);
                    estado = FINALIZAR;
                    //printf("PG %d ha adivinado el número %d\n", my_rank, numero);
                    return;
                }else if(numero < numero_adivinar){
                    char respuesta = '+';
                    MPI_Send(&respuesta, 1, MPI_CHAR, proc_adv, RESPUESTA_ADIVINANZA, MPI_COMM_WORLD);
                }else if(numero > numero_adivinar){
                    char respuesta = '-';
                    MPI_Send(&respuesta, 1, MPI_CHAR, proc_adv, RESPUESTA_ADIVINANZA, MPI_COMM_WORLD);
                }
            }
            break;
        
        default:
            break;
        }
    }
    
}

int solicitarAsignacion(int my_rank, int num_pa, int *pa_ids) {
    int asignado = 0; // Flag para controlar si se ha asignado
    int respuesta; // Almacena la respuesta de los PA
    int proc_adv = -1; // Identificador del proceso asignado, inicializado a -1 para indicar que no hay ninguno asignado todavía

    while(!asignado) {
        for(int i = 0; i < num_pa; i++) {
            // Envía solicitud de disponibilidad a cada PA
            MPI_Send(&my_rank, 1, MPI_INT, pa_ids[i], DISP_REQ, MPI_COMM_WORLD);
            // Espera la respuesta de cada PA
            MPI_Recv(&respuesta, 1, MPI_INT, pa_ids[i], DISP_RES, MPI_COMM_WORLD, MPI_STATUS_IGNORE);

            // Verifica la respuesta
            if(respuesta == 0) { // Si hay disponibilidad
                asignado = 1; // Marca como asignado
                proc_adv = pa_ids[i]; // Guarda el identificador del PA asignado

                //cancelar all pending requests

                

                break; // Sale del bucle for ya que se ha asignado
            }
        }
    }

    return proc_adv; // Devuelve el identificador del proceso asignado
}


void PA_Code(int my_rank) {
    int disponible = 0;
    int fin = 1;
    MPI_Status status;
    int proc_ges = -1;
    int limite_inferior = 0;
    int limite_superior = MAX;
    char respuesta;
    int intento, numIntentos;
    int x = 50;

    while(fin != 0)
    {
        MPI_Probe(MPI_ANY_SOURCE, MPI_ANY_TAG,MPI_COMM_WORLD, &status);
        printf("@@@@");
        switch(status.MPI_TAG)
        {
            case DISP_REQ:
                if(disponible == 0)
                {
                    printf("@@@@");
                    MPI_Recv(&proc_ges,1,MPI_INT,status.MPI_SOURCE, DISP_REQ, MPI_COMM_WORLD,&status);
                    printf("????");
                    //enviar un 0 para indicar que esta disponible
                    MPI_Send(&disponible,1,MPI_INT,proc_ges,DISP_RES,MPI_COMM_WORLD);

                    //imprimir con quien nos emparejamos
                    //printf("PA %d ha sido emparejado con PG %d\n", my_rank, proc_ges);

                    //enviamos el primer intento
                    MPI_Send(&x, 1, MPI_INT, proc_ges, RESPUESTA_ADIVINANZA, MPI_COMM_WORLD);

                    //cambiar el estado a ocupado
                    disponible = 1;
                    numIntentos = 1;

                }else{
                    //enviar un 1 para indicar que esta ocupado
                    MPI_Send(&disponible,1,MPI_INT,status.MPI_SOURCE,DISP_RES,MPI_COMM_WORLD);
                }
            break;
            case RESPUESTA_ADIVINANZA: 
                
                intento = (limite_inferior + limite_superior) / 2;               
                while (limite_inferior <= limite_superior)
                {
                    

                    //recibir la respuesta
                    MPI_Recv(&respuesta, 1, MPI_CHAR, status.MPI_SOURCE, RESPUESTA_ADIVINANZA, MPI_COMM_WORLD, &status);
                    if (respuesta == '=') {
                        printf("PA %d ha adivinado el número %d en %d intentos\n", my_rank, intento, numIntentos);
                       return;
                    } else if (respuesta == '+') {
                        limite_inferior = intento + 1;
                    } else if(respuesta == '-'){
                        limite_superior = intento - 1;
                    }

                    intento = (limite_inferior + limite_superior) / 2;

                    //enviar el intento
                    MPI_Send(&intento, 1, MPI_INT, status.MPI_SOURCE, RESPUESTA_ADIVINANZA, MPI_COMM_WORLD);
                    numIntentos++;
                    
                }

            break;
        }

        
    }   

}


void PI_Code(int my_rank) {
	//print my rank and type
	//printf("PI %d\n", my_rank);
}


