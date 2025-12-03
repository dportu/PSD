// mpiexec -hostfile machines -np 3 ./bmpFilterDynamic ejemplo1.bmp salida.bmp 50 10

#include "bmpBlackWhite.h"
#include "mpi.h"

/** Show log messages */
#define SHOW_LOG_MESSAGES 1

/** Enable output for filtering information */
#define DEBUG_FILTERING 0

/** Show information of input and output bitmap headers */
#define SHOW_BMP_HEADERS 0

void envioTrabajo(int workerRank, int tag, unsigned int rowsSentToWorker, unsigned int rowSize, unsigned int *rowsLeftToSend, int *currentRow, unsigned char *auxPtr) {
	// Enviamos el numero de columnas que tiene que procesar
	printf("Enviando %d filas a partir de fila %d a worker %d\n", rowsSentToWorker,*currentRow, workerRank);
	MPI_Send(&rowsSentToWorker, 1, MPI_INT, workerRank, tag, MPI_COMM_WORLD); // numero de filas
	MPI_Send(currentRow, 1, MPI_INT, workerRank, tag, MPI_COMM_WORLD); // id de fila
	printf("Enviando a worker %d, %d filas\n", workerRank, rowsSentToWorker);
	MPI_Send(auxPtr, rowsSentToWorker * rowSize, MPI_CHAR, workerRank, tag, MPI_COMM_WORLD); // enviamos a cada worker su parte de la imagen
	printf("%d filas * %d rowsize = %d bytes enviados al worker %d\n", rowsSentToWorker, rowSize, rowsSentToWorker * rowSize, workerRank);

	*rowsLeftToSend -= rowsSentToWorker;
}

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
	unsigned int receivedRows;						/** Total number of received rows */
	unsigned int threshold;							/** Threshold */
	unsigned int currentRow;						/** Current row being processed */
	unsigned int currentPixel;						/** Current pixel being processed */
	unsigned int outputPixel;						/** Output pixel */
	unsigned int readBytes;							/** Number of bytes read from input file */
	unsigned int writeBytes;						/** Number of bytes written to output file */
	unsigned int totalBytes;						/** Total number of bytes to send/receive a message */
	unsigned int numPixels;							/** Number of neighbour pixels (including current pixel) */
	unsigned int currentWorker;						/** Current worker process */
	unsigned int *processIDs;
	tPixelVector vector;							/** Vector of neighbour pixels */
	int imageDimensions[2];							/** Dimensions of input image */
	double timeStart, timeEnd;						/** Time stamps to calculate the filtering time */
	int size, rank, tag;							/** Number of process, rank and tag */
	MPI_Status status;								/** Status information for received messages */

	unsigned int rowsLeftToSend;					// number of rows left to be sent to workers
	unsigned int rowsLeftToReceive;					// number of rows left to be received from the workers


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
	if (argc != 5){

		if (rank == 0)
			printf ("Usage: ./bmpFilterDynamic sourceFile destinationFile threshold numRows\n");

		MPI_Finalize();
		exit(0);
	}

	// Get input arguments...
	sourceFileName = argv[1];
	destinationFileName = argv[2];
	threshold = atoi(argv[3]);
	rowsPerProcess = atoi(argv[4]);

	// Allocate memory for process IDs vector
	processIDs = (unsigned int *) malloc (size*sizeof(unsigned int));

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
		rowSize = (((imgInfoHeaderInput.biBitCount * imgInfoHeaderInput.biWidth) + 31) / 32 ) * 4;

		// Show info before processing
		if (SHOW_LOG_MESSAGES){
			printf ("[MASTER] Applying filter to image %s (%dx%d) with threshold %d. Generating image %s\n", sourceFileName,
					rowSize, imgInfoHeaderInput.biHeight, threshold, destinationFileName);
			printf ("Number of workers:%d -> Each worker calculates (at most) %d rows\n", size-1, rowsPerProcess);
		}

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
		unsigned int num_workers = size - 1;
		printf("malloc del master\n");
		outputBuffer = (unsigned char*) malloc(rowSize * imageDimensions[1]);
		inputBuffer =  (unsigned char*) malloc(rowSize * imageDimensions[1]);
		rowsSentToWorker = rowsPerProcess;
		rowsLeftToReceive = imageDimensions[1];
		rowsLeftToSend = imageDimensions[1];
		read(inputFile, inputBuffer, rowSize * imageDimensions[1]); // inicializamos el inputbuffer con la imagen sin filtrar
		
		printf("imageDimensions[0] (width): %d\n", imageDimensions[0]);
		printf("imageDimensions[1] (height): %d\n", imageDimensions[1]);

		printf("num_workers: %d\n", num_workers);
		printf("rowsPerProcess: %d\n", rowsPerProcess);

		printf("rowSize: %d\n", rowSize);
		printf("outputBuffer addr: %p\n", (void*)outputBuffer);
		printf("inputBuffer addr: %p\n", (void*)inputBuffer);



		//	Broadcast de las dimensiones de la imagen y el rowSize
		MPI_Bcast(imageDimensions, 2, MPI_INT, 0, MPI_COMM_WORLD);
		MPI_Bcast(&rowSize, 1, MPI_INT, 0, MPI_COMM_WORLD);
		MPI_Bcast(&rowsPerProcess, 1, MPI_INT, 0, MPI_COMM_WORLD);


		// 	Envio inicial a los workers
		auxPtr = inputBuffer;
		for(int i = 0; i < num_workers; i++) {
			currentRow = i * rowsSentToWorker;
			auxPtr = &(inputBuffer[currentRow * rowSize]);
			envioTrabajo(i+1, tag, rowsSentToWorker, rowSize, &rowsLeftToSend, &currentRow, auxPtr);
		}

		
		while(rowsLeftToReceive > 0) {
			MPI_Recv(&currentRow, 1, MPI_INT, MPI_ANY_SOURCE, tag, MPI_COMM_WORLD, &status);
			int id = status.MPI_SOURCE;
			int procesedRows = 0;
			MPI_Recv(&procesedRows, 1, MPI_INT, id, tag, MPI_COMM_WORLD, &status);
			auxPtr = &(outputBuffer[currentRow * rowSize]);
			MPI_Recv(auxPtr, procesedRows * rowSize, MPI_CHAR, id, tag, MPI_COMM_WORLD, &status); // recibimos el segmento del worker
			rowsLeftToReceive -= procesedRows;
			if(rowsLeftToSend > 0) {
				// en caso de que el numero de filas que quede por enviar sea menor que el rowsPerProcess, enviamos solo las que queden
				if(rowsLeftToSend < rowsPerProcess) { 
					rowsSentToWorker = rowsLeftToSend;
				}

				// colocamos el puntero en la fila que toca enviar ahora
				currentRow = imageDimensions[1] - rowsLeftToSend; 
				auxPtr = &(inputBuffer[currentRow * rowSize]);
				envioTrabajo(id, tag, rowsSentToWorker, rowSize, &rowsLeftToSend, &currentRow, auxPtr);
			}
			else {
				rowsSentToWorker = 0; 
				MPI_Send(&rowsSentToWorker, 1, MPI_INT, id, tag, MPI_COMM_WORLD); //comunicamos al worker que no hay mas trabajo
			}
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
	else {
		// RECIBE LAS DIMENSIONES DE LA IMAGEN, ROWSIZE Y ROWS PER PROCESS
		MPI_Bcast(imageDimensions, 2, MPI_INT, 0, MPI_COMM_WORLD);
		MPI_Bcast(&rowSize, 1, MPI_INT, 0, MPI_COMM_WORLD);
		MPI_Bcast(&rowsPerProcess, 1, MPI_INT, 0, MPI_COMM_WORLD);
		printf("Worker %d ha recibido dimensiones: %dx%d\n",rank, imageDimensions[0],imageDimensions[1]);
		printf("Worker %d ha recibido rowSize: %d\n",rank, rowSize);
		int rowsToProcess = 0;



		// RECIBIMOS EL ID DE LA PRIMERA FILA DEL SEGMENTO A PROCESAR Y EL NUMERO DE FILAS A PROCESAR

		
		
		
		unsigned char *workerInputBuffer =  (unsigned char*) malloc(rowsPerProcess * rowSize);
		unsigned char *workerOutputBuffer = (unsigned char*) malloc(rowsPerProcess * rowSize);
		printf("after malloc del worker %d\n", rank);
		MPI_Recv(&rowsToProcess, 1, MPI_INT, 0, tag, MPI_COMM_WORLD, &status);
		while(rowsToProcess != 0) { // rowsToProcess = 0 : finalizacion del worker
			MPI_Recv(&currentRow, 1, MPI_INT, 0, tag, MPI_COMM_WORLD, &status);
			printf("worker %d ha recibido %d filas a partir de la fila %d\n", rank, rowsToProcess, currentRow);

			// si nos toca hacer menos filas de rowsPerProcess, hacemos resize de los buffers
			if(rowsToProcess < rowsPerProcess) {
				free(workerInputBuffer);
				free(workerOutputBuffer);
				workerInputBuffer =  (unsigned char*) malloc(rowsToProcess * rowSize);
				workerOutputBuffer = (unsigned char*) malloc(rowsToProcess * rowSize);
			}


			printf("worker %d recibiendo trabajo\n", rank);
			MPI_Recv(workerInputBuffer, rowsToProcess * rowSize, MPI_CHAR, 0, tag, MPI_COMM_WORLD, &status);

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
			
			// Enviamos el id de la primera fila de las filas procesadas
			MPI_Send(&currentRow, 1, MPI_INT, 0, tag, MPI_COMM_WORLD);
			// enviamos el numero de filas procesadas
			MPI_Send(&rowsToProcess, 1, MPI_INT, 0, tag, MPI_COMM_WORLD);
			// enviamos las rows procesadas 
			MPI_Send(workerOutputBuffer, rowsToProcess * rowSize, MPI_CHAR, 0, tag, MPI_COMM_WORLD);


			// recibimos el nuevo numero de filas a procesar de nuevo
			printf("Worker %d recibiendo numero de filas a procesar\n", rank);
			MPI_Recv(&rowsToProcess, 1, MPI_INT, 0, tag, MPI_COMM_WORLD, &status);
			printf("Worker %d ahora tiene que procesar %d filas\n", rank, rowsToProcess);
		}
		
	}

	// Finish MPI environment
	MPI_Finalize();


}
/*
				printf("Enviando %d filas a partir de fila %d a worker %d\n", rowsSentToWorker,currentRow, currentRow);
				MPI_Send(&rowsSentToWorker, 1, MPI_INT, id, tag, MPI_COMM_WORLD); // numero de filas
				MPI_Send(&currentRow, 1, MPI_INT, id, tag, MPI_COMM_WORLD); // id de fila
				printf("Enviando a worker %d, %d filas\n", id, rowsSentToWorker);
				MPI_Send(auxPtr, rowsSentToWorker * rowSize, MPI_CHAR, id, tag, MPI_COMM_WORLD); // enviamos a cada worker su parte de la imagen
				printf("%d filas * %d rowsize = %d bytes enviados al worker %d\n", rowsSentToWorker, rowSize, rowsSentToWorker * rowSize, id);

				rowsLeftToSend -= rowsSentToWorker;
				*/