//	mpiexec -hostfile machines -np 3 ./bmpFilterStatic ejemplo1.bmp salida.bmp 50

#include "bmpBlackWhite.h"
#include <time.h>
#include "mpi.h"

/** Show log messages */
#define SHOW_LOG_MESSAGES 1

/** Enable output for filtering information */
#define DEBUG_FILTERING 0

/** Show information of input and output bitmap headers */
#define SHOW_BMP_HEADERS 0


int main(int argc, char** argv){

	tBitmapFileHeader imgFileHeaderInput;			/** BMP file header for input image */
	tBitmapInfoHeader imgInfoHeaderInput;			/** BMP info header for input image */
	tBitmapFileHeader imgFileHeaderOutput;			/** BMP file header for output image */
	tBitmapInfoHeader imgInfoHeaderOutput;			/** BMP info header for output image */
	char* sourceFileName;							/** Name of input image file */
	char* destinationFileName;						/** Name of output image file */
	int inputFile, outputFile;						/** File descriptors */
	unsigned char *outputBuffer;					/** Output buffer for filtered pixels */
	unsigned char *inputBuffer;						/** Input buffer to allocate original pixels */
	unsigned char *auxPtr;							/** Auxiliary pointer */
	unsigned int rowSize;							/** Number of pixels per row */
	unsigned int rowsPerProcess;					/** Number of rows to be processed (at most) by each worker */
	unsigned int rowsSentToWorker;					/** Number of rows to be sent to a worker process */
	unsigned int threshold;							/** Threshold */
	unsigned int currentRow;						/** Current row being processed */
	unsigned int currentPixel;						/** Current pixel being processed */
	unsigned int outputPixel;						/** Output pixel */
	unsigned int readBytes;							/** Number of bytes read from input file */
	unsigned int writeBytes;						/** Number of bytes written to output file */
	unsigned int totalBytes;						/** Total number of bytes to send/receive a message */
	unsigned int numPixels;							/** Number of neighbour pixels (including current pixel) */
	unsigned int currentWorker;						/** Current worker process */
	tPixelVector vector;							/** Vector of neighbour pixels */
	int imageDimensions[2];							/** Dimensions of input image */
	double timeStart, timeEnd;						/** Time stamps to calculate the filtering time */
	int size, rank;									/** Number of process and rank*/
	int  tag;										// preguntar: lo usamos para enviarle a cada worker su fila ?
	int num_workers;								//	Number of workers = size - 1
	int leftover;									// numero de workers que tienen que procesar una fila mas por no ser el numero de rows divisible
	MPI_Status status;								/** Status information for received messages */


		// Init
		MPI_Init(&argc, &argv);
		MPI_Comm_size(MPI_COMM_WORLD, &size);
		MPI_Comm_rank(MPI_COMM_WORLD, &rank);
		tag = 1;
		srand(time(NULL));
		

		// Check the number of processes
		if (size<=2){

			if (rank == 0)
				printf ("This program must be launched with (at least) 3 processes\n");

			MPI_Finalize();
			exit(0);
		}

		// Check arguments
		if (argc != 4){

			if (rank == 0)
				printf ("Usage: ./bmpFilterStatic sourceFile destinationFile threshold\n");

			MPI_Finalize();
			exit(0);
		}

		// Get input arguments...
		sourceFileName = argv[1];
		destinationFileName = argv[2];
		threshold = atoi(argv[3]);


		


		// Master process
		if (rank == 0){

			// Process starts
			timeStart = MPI_Wtime();

			// Read headers from input file
			readHeaders (sourceFileName, &imgFileHeaderInput, &imgInfoHeaderInput);
			readHeaders (sourceFileName, &imgFileHeaderOutput, &imgInfoHeaderOutput);

			// Write header to the output file
			writeHeaders (destinationFileName, &imgFileHeaderOutput, &imgInfoHeaderOutput);

			// Calculate row size for input and output images
			rowSize = (((imgInfoHeaderInput.biBitCount * abs(imgInfoHeaderInput.biWidth)) + 31) / 32 ) * 4;
		
			// Show headers...
			if (SHOW_BMP_HEADERS){
				printf ("Source BMP headers:\n");
				printBitmapHeaders (&imgFileHeaderInput, &imgInfoHeaderInput);
				printf ("Destination BMP headers:\n");
				printBitmapHeaders (&imgFileHeaderOutput, &imgInfoHeaderOutput);
			}

			// Open source image
			if((inputFile = open(sourceFileName, O_RDONLY)) < 0){
				printf("ERROR: Source file cannot be opened: %s\n", sourceFileName);
				exit(1);
			}

			// Open target image
			if((outputFile = open(destinationFileName, O_WRONLY | O_APPEND, 0777)) < 0){
				printf("ERROR: Target file cannot be open to append data: %s\n", destinationFileName);
				exit(1);
			}

			// Allocate memory to copy the bytes between the header and the image data
			outputBuffer = (unsigned char*) malloc ((imgFileHeaderInput.bfOffBits-BIMAP_HEADERS_SIZE) * sizeof(unsigned char));

			// Copy bytes between headers and pixels
			lseek (inputFile, BIMAP_HEADERS_SIZE, SEEK_SET);
			read (inputFile, outputBuffer, imgFileHeaderInput.bfOffBits-BIMAP_HEADERS_SIZE);
			write (outputFile, outputBuffer, imgFileHeaderInput.bfOffBits-BIMAP_HEADERS_SIZE);

			free(outputBuffer);

			// INICIALIZACION VARIABLES
			imageDimensions[0] = abs(imgInfoHeaderInput.biWidth);
			imageDimensions[1] = abs(imgInfoHeaderInput.biHeight);
			num_workers = size - 1;
			rowsPerProcess = imageDimensions[1] / num_workers;
			leftover =  imageDimensions[1] % num_workers;
			printf("malloc del master\n");
			outputBuffer = (unsigned char*) malloc(rowSize * imageDimensions[1]);
			inputBuffer =  (unsigned char*) malloc(rowSize * imageDimensions[1]);
			rowsSentToWorker = rowsPerProcess;
			read(inputFile, inputBuffer, rowSize * imageDimensions[1]); // inicializamos el inputbuffer con la imagen sin filtrar
			
			printf("imageDimensions[0] (width): %d\n", imageDimensions[0]);
			printf("imageDimensions[1] (height): %d\n", imageDimensions[1]);

			printf("num_workers: %d\n", num_workers);
			printf("rowsPerProcess: %d\n", rowsPerProcess);
			printf("leftover: %d\n", leftover);

			printf("rowSize: %d\n", rowSize);
			printf("outputBuffer addr: %p\n", (void*)outputBuffer);
			printf("inputBuffer addr: %p\n", (void*)inputBuffer);



			//	Broadcast de las dimensiones de la imagen y el rowSize
			MPI_Bcast(imageDimensions, 2, MPI_INT, 0, MPI_COMM_WORLD);
			MPI_Bcast(&rowSize, 1, MPI_INT, 0, MPI_COMM_WORLD);

			
			
			
			// 		ENVIO A WORKERS
			// TOASK: se podria enviar auxPtr o sentRows, para que el worker pueda luego hacer un send al master directamente de donde tiene que escribir lo que le envia?
			
			//auxPtr = inputBuffer; 
			for(int i = 0; i < num_workers; i++) {
				
				// Enviamos el numero de columnas que tiene que procesar
				if(i == num_workers - 1) {
					rowsSentToWorker += leftover; 
				}
				auxPtr = &(inputBuffer[i * rowsSentToWorker * rowSize]);
				printf("Enviando %d columnas a worker %d\n", rowsSentToWorker, i + 1);
				MPI_Send(&rowsSentToWorker, 1, MPI_INT, i + 1, tag, MPI_COMM_WORLD); 
				printf("Enviando a worker %d, %d filas\n", i + 1, rowsSentToWorker);
				MPI_Send(auxPtr, rowsSentToWorker * rowSize, MPI_CHAR, i + 1, tag, MPI_COMM_WORLD); // enviamos a cada worker su parte de la imagen
				printf("%d filas * %d rowsize = %d bytes enviados al worker %d\n", rowsSentToWorker, rowSize, rowsSentToWorker * rowSize, i + 1);

				//auxPtr += rowsSentToWorker * rowSize;
			}
			

			

			//		RECEPCION DE WORKERS
			for(int i = 0; i < num_workers; i++) {
				int procesedRows = 0;
				MPI_Recv(&procesedRows, 1, MPI_INT , MPI_ANY_SOURCE, tag, MPI_COMM_WORLD, &status); //recv del id de cualquier worker y el numero de rows que ha procesado
				int id = status.MPI_SOURCE; //asumimos que status.MPI_SOURCE es el rank
				printf("Recibidas %d columnas del worker %d\n", procesedRows, id);
				auxPtr = &(outputBuffer[(id - 1)* rowsPerProcess * rowSize]);
				printf("auxPtr apuntando a fila %d \n", (id - 1)* rowsPerProcess);
				MPI_Recv(auxPtr, procesedRows * rowSize, MPI_CHAR, id, tag, MPI_COMM_WORLD, &status); // recibimos el segmento del worker
				rowsSentToWorker = 0; 
				MPI_Send(&rowsSentToWorker, 1, MPI_INT, id, tag, MPI_COMM_WORLD); //comunicamos al worker que ya ha terminado de trabajar
			}





			printf("Master ha recibido todo, escribimos la imagen...\n");
			//	Escritura del outputbuffer en el output file
			if(writeBytes = write(outputFile, outputBuffer, imageDimensions[1] * rowSize * sizeof(char)) != imageDimensions[1] * rowSize * sizeof(char)) {
				printf("what the hell man\n");
			}



			// Close files
			close (inputFile);
			close (outputFile);

			// Process ends
			timeEnd = MPI_Wtime();

			// Show processing time
			printf("Filtering time: %f\n",timeEnd-timeStart);
		}


		// Worker process
		else{
			// RECIBE LAS DIMENSIONES DE LA IMAGEN
			MPI_Bcast(imageDimensions, 2, MPI_INT, 0, MPI_COMM_WORLD);
			MPI_Bcast(&rowSize, 1, MPI_INT, 0, MPI_COMM_WORLD);
			printf("Worker %d ha recibido dimensiones: %dx%d\n",rank, imageDimensions[0],imageDimensions[1]);
			printf("Worker %d ha recibido rowSize: %d\n",rank, rowSize);
			int rowsToProcess = 0;



			// RECIBIMOS GRAIN (rowsToProcess * rowSize)
			MPI_Recv(&rowsToProcess, 1, MPI_INT, 0, tag, MPI_COMM_WORLD, &status);

			
			printf("worker %d ha recibido %d filas\n", rank, rowsToProcess);
			unsigned char *workerInputBuffer =  (unsigned char*) malloc(rowsToProcess * rowSize);
			unsigned char *workerOutputBuffer = (unsigned char*) malloc(rowsToProcess * rowSize);
			printf("after malloc del worker %d\n", rank);

			while(rowsToProcess != 0) { // grain = 0 : finalizacion del worker
				//	Recibimos el trabajo inicial
				printf("worker %d recibiendo trabajo inicial\n", rank);
				MPI_Recv(workerInputBuffer, rowsToProcess * rowSize, MPI_CHAR, 0, tag, MPI_COMM_WORLD, &status);
				printf("worker %d ha recibido trabajo inicial\n", rank);

				//	PROCESADO
				// For each pixel in the current row...
				for (currentPixel = 0; currentPixel < rowsToProcess * rowSize; currentPixel++){
					
					// Current pixel
					numPixels = 0;
					vector[numPixels] = workerInputBuffer[currentPixel];
					numPixels++;
					
					// Not the first pixel
					if (currentPixel > 0){
						vector[numPixels] = workerInputBuffer[currentPixel-1];
						numPixels++;
					}
					
					// Not the last pixel
					if (currentPixel < (imageDimensions[0]-1)){
						vector[numPixels] = workerInputBuffer[currentPixel+1];
						numPixels++;
					}
					
					// Store current pixel value
					workerOutputBuffer[currentPixel] = calculatePixelValue(vector, numPixels, threshold, DEBUG_FILTERING);
				}

				//	ENVIO A MASTER

				// enviamos el numero de filas procesadas
				//printf("Worker %d enviando numero de filas procesadas\n", rank);
				MPI_Send(&rowsToProcess, 1, MPI_INT, 0, tag, MPI_COMM_WORLD);
				// enviamos las rows procesadas 
				//printf("Worker %d enviando filas procesadas\n", rank);
				MPI_Send(workerOutputBuffer, rowsToProcess * rowSize, MPI_CHAR, 0, tag, MPI_COMM_WORLD);


				// recibimos el grain de nuevo
				printf("Worker %d recibiendo numero de filas a procesar\n", rank);
				MPI_Recv(&rowsToProcess, 1, MPI_INT, 0, tag, MPI_COMM_WORLD, &status);
				printf("Worker %d ha recibido %d filas\n", rank, rowsToProcess);
			}
		}

		// Finish MPI environment
		MPI_Finalize();
}
