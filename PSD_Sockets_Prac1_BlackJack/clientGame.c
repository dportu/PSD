#include "clientGame.h"

unsigned int readBet (){

	int isValid, bet=0;
	tString enteredMove;
 
		// While player does not enter a correct bet...
		do{

			// Init...
			bzero (enteredMove, STRING_LENGTH);
			isValid = TRUE;

			printf ("Enter a value: ");
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

void sendDeck(tDeck *playerDeck, int playerSocket){
	// Send message to the server side
	unsigned int numCards = playerDeck->numCards;
	// Check the number of bytes sent
	if (send(playerSocket, &numCards, sizeof(int), 0) < 0) {
		showError("ERROR while writing to the socket 1");
	}

	if(playerDeck->numCards != 0) {
		// Send message to the server side
		unsigned int size = numCards * sizeof(int);
		int nameLength = send(playerSocket, playerDeck->cards, size, 0);
		// Check the number of bytes sent
		if (nameLength < 0)
			showError("ERROR while writing to the socket 2");
	}
	// printFancyDeck(playerDeck);
}

void receiveDeck(tDeck *deck, int socket){ 
	unsigned int numCards;

	// Check read bytes
	if (recv(socket, &numCards, sizeof(int), 0) < 0) { //recibimos la longitud en bytes de la deck
		showError("ERROR while reading name length");
	}

	deck->numCards = numCards;
	//printf("number of cards in the received deck: %i\n", numCards);

	if(numCards != 0) {
		memset(deck->cards, 0, DECK_SIZE * sizeof(int));
		// Check read bytes
		if (recv(socket, deck->cards, numCards * sizeof(int), 0) < 0)
			showError("ERROR while reading from socket");
	}
	else {
		memset(deck->cards, 0, DECK_SIZE * sizeof(int));
	}
}

void sendCode(unsigned int code, int socketfd) {
	unsigned int c = code;
	// Send message to the server side
	if (send(socketfd, &c, sizeof(int), 0) < 0) {
		showError("ERROR while sending code");
	}
}

unsigned int receiveCode(int socketC) {
	unsigned int code = 0;
	// Check read bytes
	if (recv(socketC, &code, sizeof(int), 0) < 0) {
		showError("ERROR while reading from socket");
	}
	
	//showCode(code);
	return code;
}

unsigned int receiveInt(int socketC) {
	unsigned int code = 0;

	// Check read bytes
	if (recv(socketC, &code, sizeof(int), 0) < 0) {
		showError("ERROR while reading from socket");
	}
	
	//printf("Received int: %i\n", code);
	return code;
}

void sendMessage(tString message, int socketfd) {
	// Send message to the server side
	int nameLen = strlen(message);
	// Check the number of bytes sent
	if (send(socketfd, &nameLen, sizeof(int), 0) < 0) {
		showError("ERROR while writing to the socket 1");
	}

	// Send message to the server side
	if (send(socketfd, message, nameLen, 0) < 0)
		showError("ERROR while writing to the socket 2");
}

void receiveMessage(tString message, int socketC) {
	int bytes;

	//recibimos la longitud en bytes del nombre del cliente
	if (recv(socketC, &bytes, sizeof(int), 0) < 0)
		showError("ERROR while reading name length");

	memset(message, 0, STRING_LENGTH);

	// Check read bytes
	if (recv(socketC, message, bytes, 0) < 0) //recibimos el nombre
		showError("ERROR while reading from socket");

}

void rondaDeApuestas(int socketfd) {
	int code = receiveCode(socketfd);
	if (code == TURN_BET) {
		unsigned int stack = receiveInt(socketfd);
		printf(" -- Betting Round Begins --\n\n");
		printf("Available Stack: %i\n", stack);
		unsigned int bet;

		do {
			bet = readBet(); // pide por consola
			sendCode(bet, socketfd);
		} while (receiveCode(socketfd) == TURN_BET);

		printf("Bet Accepted.\n");
	}
	else if (code == TURN_BET_OK) {
		printf("Waiting for the other player...\n");
	}
}


unsigned int jugarRonda(int socketfd) {
	for(int k =0; k< 2; k++) {
		printf(" --- Game Start --- \n");
		unsigned int receivedCode = receiveCode(socketfd);
		unsigned int active = (receivedCode == TURN_PLAY);

		unsigned int canPlay = TRUE;
		
		unsigned int points = receiveInt(socketfd);
		printf("Points: %i\n", points);
		tDeck activePlayerDeck;
		receiveDeck(&activePlayerDeck, socketfd);
		printFancyDeck(&activePlayerDeck);

		if(active) {
			unsigned int play = readOption(); //preparamos la primera play
			sendCode(play, socketfd); //enviamos la primera play

			while(canPlay) {
				printf("Sent Play: ");
				showCode(play);
				printf("\n------\n\n\n");
				
				unsigned int receivedCode = receiveCode(socketfd);
				canPlay = (receivedCode == TURN_PLAY || receivedCode == TURN_PLAY_OUT);
				if(canPlay) {
					points = receiveInt(socketfd);
					printf("Points: %u\n", points);
					receiveDeck(&activePlayerDeck, socketfd);
					printFancyDeck(&activePlayerDeck);
					if(receivedCode == TURN_PLAY) { //
						play = readOption();
						sendCode(play, socketfd);
					}
					else if(receivedCode == TURN_PLAY_OUT) {
						printf("-- YOU ARE OUT --\n\n");
					}
				}
			}
		}
		else {
			while(canPlay) {
				printf(" -- YOU ARE CURRENTLY SPECTATING -- \n\n");
				unsigned int code = receiveCode(socketfd);
				if(code != TURN_PLAY_RIVAL_DONE){
					canPlay = (code == TURN_PLAY_WAIT || code == TURN_PLAY_OUT);
					points = receiveInt(socketfd);
					printf("Points: %u\n", points);
					receiveDeck(&activePlayerDeck, socketfd);
					printFancyDeck(&activePlayerDeck);

					if(code == TURN_PLAY_OUT) {
						printf("-- OPPONENT IS OUT --\n\n");
					}
				}
				else {
					canPlay = FALSE;
				}
				
			}
			
		}
	}

	unsigned int result = receiveCode(socketfd);
	if (result == TURN_GAME_LOSE) {
		printf("Oh no! You lost :(\n");
	}
	else if (result == TURN_GAME_WIN) {
		printf("Hell yeah! You won :)\n");
	}

	return result != TURN_BET; //RECIBIMOS SI LA PARTIDA ACABA O SIGUE
}

int main(int argc, char *argv[]){

	int socketfd;						/** Socket descriptor */
	unsigned int port;					/** Port number (server) */
	struct sockaddr_in server_address;	/** Server address structure */
	char* serverIP;						/** Server IP */
	unsigned int endOfGame = FALSE;				/** Flag to control the end of the game */
	tString playerName;					/** Name of the player */
	//unsigned int code;					/** Code */

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

	sendMessage(playerName, socketfd);

	// Init for reading incoming message
	tString m;
	receiveMessage(m, socketfd);

	// Show the returned message
	printf("%s\n",m);
	
	while (!endOfGame) {
		rondaDeApuestas(socketfd);
		endOfGame = jugarRonda(socketfd);
	}

	// Close socket
	close(socketfd);
}
