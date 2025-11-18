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
			rowSize = (((imgInfoHeaderInput.biBitCount * imgInfoHeaderInput.biWidth) + 31) / 32 ) * 4;
		
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
			rowsPerProcess = imageDimensions[1] / num_workers; // TODO: estamos asumimiendo que es divisible
			outputBuffer = (unsigned char*) malloc (rowSize * imageDimensions[1]);
			inputBuffer = (unsigned char*) malloc(rowSize * imageDimensions[1]);
			read(inputFile, inputBuffer, rowSize * imageDimensions[1]); // inicializamos el inputbuffer con la imagen sin filtrar
			
			//	Broadcast de las dimensiones de la imagen
			MPI_Bcast(&imageDimensions, 2, MPI_INT, 0, MPI_COMM_WORLD);

			
			
			
			// 		ENVIO A WORKERS
			for(int i = 0; i < num_workers; i++) {
				auxPtr = &inputBuffer[i * rowsPerProcess]; // poner donde toque
				MPI_Send(auxPtr, rowSize * rowsPerProcess, MPI_CHAR, i + 1, tag, MPI_COMM_WORLD); // enviamos a cada worker su parte de la imagen
			} 
			

			//		RECEPCION DE WORKERS
			for(int i = 0; i < num_workers; i++){
				int id;
				MPI_Recv(&id, 1, MPI_CHAR , MPI_ANY_SOURCE, tag, MPI_COMM_WORLD, &status); //recv del id de cualquier worker
				auxPtr = &outputBuffer[id * rowsPerProcess];//colocamos el puntero del outputbuffer en el segmento del worker
				MPI_Recv(auxPtr, rowsPerProcess, rowSize * rowsPerProcess, id, tag, MPI_COMM_WORLD, &status); // recibimos el segmento del worker
			}

			//	Escritura del outputbuffer en el output file
			write(outputFile, outputBuffer, imageDimensions[1] * rowSize);




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
			// MPI_Send(&imageDimensions, rowSize * rowsPerProcess, MPI_CHAR, i + 1, tag, MPI_COMM_WORLD);

			// RECIBE LAS 
			
			
			
		}

		// Finish MPI environment
		MPI_Finalize();
}
