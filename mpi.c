#include <mpi.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#define MAX 999999

//definimos los tipos de comunicación
#define COM_TYPE 0 // Para enviar el tipo de proceso
#define COM_DATA_1 1 // Para enviar el tamaño de los PA
#define COM_DATA_2 2 // Para enviar los ids de los PA
#define COM_DATA_3 3 // Para enviar el número random
#define DISPONIBILIDAD 4
#define JUEGO_ADIVINANZA 5
#define FINALIZAR 6

// Definir los tipos de procesos
#define PES_RANK 0 // Asumimos que el PES tiene el rango 0
#define PG_TYPE 1
#define PA_TYPE 2
#define PI_TYPE 3

void PES_Code(int world_size, int num_pg, int num_pa);
void PG_Code(int my_rank, int world_size);
void PA_Code(int my_rank);
void PI_Code(int my_rank);

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
        MPI_Send(&num_pa, 1, MPI_INT, i, COM_DATA_1, MPI_COMM_WORLD);
    }

    // Enviamos a los PG los ids de los PA
    for(int i = 1; i <= num_pg; i++) {
        MPI_Send(array_pa_ids, num_pa, MPI_INT, i, COM_DATA_2, MPI_COMM_WORLD); // Corrección aplicada
    }

	//numero random para cada PG en el rango MAX
	int random;
	for(int i = 1; i <= num_pg; i++) {
		random = rand() % MAX;
		MPI_Send(&random, 1, MPI_INT, i, COM_DATA_3, MPI_COMM_WORLD);
	}

    free(array_pa_ids); // No olvides liberar la memoria
}


void PG_Code(int my_rank, int world_size) {
	
	bool fin = false;
	bool juego = false;
	//recibimos el tamaño de los PA
	int num_pa;
	MPI_Recv(&num_pa, 1, MPI_INT, PES_RANK, COM_DATA_1, MPI_COMM_WORLD, MPI_STATUS_IGNORE);

	//recibimos los ids de los PA
	int *array_pa_ids = (int *)malloc(num_pa * sizeof(int));
	MPI_Recv(array_pa_ids, num_pa, MPI_INT, PES_RANK, COM_DATA_2, MPI_COMM_WORLD, MPI_STATUS_IGNORE);

	//recibimos el número random
	int random;
	MPI_Recv(&random, 1, MPI_INT, PES_RANK, COM_DATA_3, MPI_COMM_WORLD, MPI_STATUS_IGNORE);

	int consulta = 1;
	MPI_Request send_requests[num_pa];

	for(int i = 0; i < num_pa; i++) {
		MPI_Isend(&my_rank, 1, MPI_INT, array_pa_ids[i], DISPONIBILIDAD, MPI_COMM_WORLD, &send_requests[i]);
	}

	int respuestas[num_pa];
	MPI_Request recv_requests[num_pa];

	for(int i = 0; i < num_pa; i++) {
		MPI_Irecv(&respuestas[i], 1, MPI_INT, array_pa_ids[i], DISPONIBILIDAD, MPI_COMM_WORLD, &recv_requests[i]);
	}

	int index;
	MPI_Status status;
	MPI_Waitany(num_pa, recv_requests, &index, &status);

	if(index != MPI_UNDEFINED) {
		printf("El PG %d está vinculado con el PA %d\n", my_rank, respuestas[index]);
	}

}

void PA_Code(int my_rank) {
	
	bool disponible = true;
	int msg;
	MPI_Status status;

	while(disponible) {
		MPI_Probe(MPI_ANY_SOURCE, MPI_ANY_TAG, MPI_COMM_WORLD, &status);

		switch (status.MPI_TAG)
		{
		case DISPONIBILIDAD:
			if (disponible) {
				MPI_Send(&my_rank, 1, MPI_INT, status.MPI_SOURCE, DISPONIBILIDAD, MPI_COMM_WORLD);
				disponible = false;

				//imprimimos el mensaje
				printf("PA %d está vinculado con PG %d\n", my_rank, status.MPI_SOURCE);
			}
			break;
		
		default:
			break;
		}
	}

}

void PI_Code(int my_rank) {
	//print my rank and type
	printf("PI %d\n", my_rank);
}

