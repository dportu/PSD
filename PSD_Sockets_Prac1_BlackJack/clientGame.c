#include "clientGame.h"

unsigned int readBet (){

	int isValid, bet=0;
	tString enteredMove;
 
		// While player does not enter a correct bet...
		do{

			// Init...
			bzero (enteredMove, STRING_LENGTH);
			isValid = TRUE;

			printf ("Enter a value:");
			fgets(enteredMove, STRING_LENGTH-1, stdin);
			enteredMove[strlen(enteredMove)-1] = 0;

			// Check if each character is a digit
			for (int i=0; i<strlen(enteredMove) && isValid; i++)
				if (!isdigit(enteredMove[i]))
					isValid = FALSE;

			// Entered move is not a number
			if (!isValid)
				printf ("Entered value is not correct. It must be a number greater than 0\n");
			else
				bet = atoi (enteredMove);

		}while (!isValid);

		printf ("\n");

	return ((unsigned int) bet);
}

unsigned int readOption (){

	unsigned int bet;

		do{		
			printf ("What is your move? Press %d to hit a card and %d to stand\n", TURN_PLAY_HIT, TURN_PLAY_STAND);
			bet = readBet();
			if ((bet != TURN_PLAY_HIT) && (bet != TURN_PLAY_STAND))
				printf ("Wrong option!\n");			
		} while ((bet != TURN_PLAY_HIT) && (bet != TURN_PLAY_STAND));

	return bet;
}

int main(int argc, char *argv[]){

	int socketfd;						/** Socket descriptor */
	unsigned int port;					/** Port number (server) */
	struct sockaddr_in server_address;	/** Server address structure */
	char* serverIP;						/** Server IP */
	unsigned int endOfGame;				/** Flag to control the end of the game */
	tString playerName;					/** Name of the player */
	//unsigned int code;					/** Code */
	int activePlayer = 0;

	// Check arguments!
	if (argc != 3){
		fprintf(stderr,"ERROR wrong number of arguments\n");
		fprintf(stderr,"Usage:\n$>%s serverIP port\n", argv[0]);
		exit(0);
	}

	// Get the server address
	serverIP = argv[1];

	// Get the port
	port = atoi(argv[2]);

	// Create socket
	socketfd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

	// Check if the socket has been successfully created
	if (socketfd < 0)
		showError("ERROR while creating the socket");
	
	// Fill server address structure
	memset(&server_address, 0, sizeof(server_address));
	server_address.sin_family = AF_INET;
	server_address.sin_addr.s_addr = inet_addr(serverIP);
	server_address.sin_port = htons(port);

	// Connect with server
	if (connect(socketfd, (struct sockaddr *) &server_address, sizeof(server_address)) < 0)
		showError("ERROR while establishing connection");

	// Init and read the message
	printf("Enter your name: ");
	memset(playerName, 0, STRING_LENGTH);
	fgets(playerName, STRING_LENGTH-1, stdin);


	// Send message to the server side
	int nameLen = strlen(playerName);
	int byteLength = send(socketfd, &nameLen, sizeof(int), 0);
	// Check the number of bytes sent
	if (byteLength < 0) {
		showError("ERROR while sending size of name");
	}

	// Send message to the server side
	int nameLength = send(socketfd, playerName, strlen(playerName), 0);
	// Check the number of bytes sent
	if (nameLength < 0)
		showError("ERROR while name");

	// Init for reading incoming message
	tString m;
	memset(m, 0, STRING_LENGTH);
	nameLength = recv(socketfd, m, STRING_LENGTH-1, 0);

	// Check bytes read
	if (nameLength < 0)
		showError("ERROR while reading from the socket");

	// Show the returned message
	printf("%s\n",m);

	//unsigned int c = 0;
	activePlayer = receiveCode(socketfd);
	/*if(c != 0) {
		activePlayer = 1;
	}*/
	printf("%d\n", activePlayer);
	if(activePlayer == 100) {
		printf("Active Player\n");
		unsigned int code = 0;
		unsigned int stack = 0;
		//memset(code, 0, sizeof(code));
		code = receiveCode(socketfd);
		printf("%i ", code);
	}
	else if(activePlayer == 101) {
		printf("Inactive player\n");
	}


	// Close socket
	close(socketfd);
}
