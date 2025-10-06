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

void rondaDeApuestas(int socketfd) {
	int code = receiveCode(socketfd);
	if (code == TURN_BET) {
		unsigned int stack = receiveCode(socketfd);
		printf("Stack: %i\n", stack);
		unsigned int bet;

		do {
			bet = readBet(); // pide por consola
			sendCode(bet, socketfd);
		} while (receiveCode(socketfd) == TURN_BET);

		printf("Apuesta aceptada.\n");
	}
	else if (code == TURN_BET_OK) {
		printf("Esperando al otro jugador...\n");
	}
}


unsigned int jugarRonda(int socketfd) {
	unsigned int receivedCode = receiveCode(socketfd);
	unsigned int active = (receivedCode == TURN_PLAY);
	printf("Received code: %i\n", receivedCode);
	unsigned int canPlay = TRUE;
	
	unsigned int points = receiveCode(socketfd);
	printf("Points: %i\n", points);
	tDeck activePlayerDeck;
	receiveDeck(&activePlayerDeck, socketfd);
	printf("Deck received");
	if(active) {
		
		unsigned int play = readOption(); //preparamos la primera play
		printf("play: %i\n", play);
		sendCode(play, socketfd); //enviamos la primera play

		while(play == TURN_PLAY_HIT && canPlay) {
			canPlay = (receiveCode(socketfd) == TURN_PLAY);
			points = receiveCode(socketfd);
			receiveDeck(&activePlayerDeck, socketfd);

			if(active && canPlay) {
				play = readOption();
				sendCode(play, socketfd);
			}
		}
	}
	else {
		while(canPlay) {
			unsigned int code = receiveCode(socketfd);
			if(code != TURN_PLAY_RIVAL_DONE){
				canPlay = (code == TURN_PLAY);
				points = receiveCode(socketfd);
				receiveDeck(&activePlayerDeck, socketfd);
			}
			
		}
		
	}
	
	return FALSE; //ACTUALIZAR A GAME WIN O GAME LOSE
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
