#include "game.h"

void showError(const char *msg){
	perror(msg);
	exit(0);
}

void showCode (unsigned int code){

	tString string;
	
		// Reset
		memset (string, 0, STRING_LENGTH);

		switch(code){ 

			case TURN_BET:
				strcpy (string, "TURN_BET");
				break;

			case TURN_BET_OK:
				strcpy (string, "TURN_BET_OK");
				break;

			case TURN_PLAY:
				strcpy (string, "TURN_PLAY");
				break;

			case TURN_PLAY_HIT:
				strcpy (string, "TURN_PLAY_HIT");
				break;

			case TURN_PLAY_STAND:
				strcpy (string, "TURN_PLAY_STAND");
				break;

			case TURN_PLAY_OUT:
				strcpy (string, "TURN_PLAY_OUT");
				break;

			case TURN_PLAY_WAIT:
				strcpy (string, "TURN_PLAY_WAIT");
				break;

			case TURN_PLAY_RIVAL_DONE:
				strcpy (string, "TURN_PLAY_RIVAL_DONE");
				break;

			case TURN_GAME_WIN:
				strcpy (string, "TURN_GAME_WIN");
				break;

			case TURN_GAME_LOSE:
				strcpy (string, "TURN_GAME_LOSE");
				break;

			default:
				strcpy (string, "UNKNOWN CODE");
		    	break;
		}

	printf ("%s\n", string);		
}

char suitToChar (unsigned int number){

	char suit;

		if ((number/SUIT_SIZE) == 0)
			suit = 'c';
		else if ((number/SUIT_SIZE) == 1)
			suit = 's';
		else if ((number/SUIT_SIZE) == 2)
			suit = 'd';
		else
			suit = 'h';

	return suit;
}

char cardNumberToChar (unsigned int number){

	// init
	char numberChar = ' ';

		if ((number%SUIT_SIZE) == 0)
			numberChar = 'A';
		else if ((number%SUIT_SIZE) == 1)
			numberChar = '2';
		else if ((number%SUIT_SIZE) == 2)
			numberChar = '3';
		else if ((number%SUIT_SIZE) == 3)
			numberChar = '4';
		else if ((number%SUIT_SIZE) == 4)
			numberChar = '5';
		else if ((number%SUIT_SIZE) == 5)
			numberChar = '6';
		else if ((number%SUIT_SIZE) == 6)
			numberChar = '7';
		else if ((number%SUIT_SIZE) == 7)
			numberChar = '8';
		else if ((number%SUIT_SIZE) == 8)
			numberChar = '9';
		else if ((number%SUIT_SIZE) == 9)
			numberChar = 'T';
		else if ((number%SUIT_SIZE) == 10)
			numberChar = 'J';
		else if ((number%SUIT_SIZE) == 11)
			numberChar = 'Q';
		else if ((number%SUIT_SIZE) == 12)
			numberChar = 'K';

	return numberChar;
}

void printDeck (tDeck* deck){

	// Print info for player 1
	printf ("%d cards -> ", deck->numCards);

	for (int i=0; i<deck->numCards; i++)
		printf("%c%c ", cardNumberToChar (deck->cards[i]), suitToChar (deck->cards[i]));

	printf("\n");
}

void printFancyDeck (tDeck* deck){
	// Print info for player 1
	printf ("%d cards\n", deck->numCards);

	// Print the first line
	for (int currentCard=0; currentCard<deck->numCards; currentCard++)
		printf ("  ___ ");

	printf ("\n");

	// Print the second line
	for (int currentCard=0; currentCard<deck->numCards; currentCard++)
		printf (" |%c  |", cardNumberToChar (deck->cards[currentCard]));

	printf ("\n");

	// Print the third line
	for (int currentCard=0; currentCard<deck->numCards; currentCard++){
		if (suitToChar (deck->cards[currentCard]) == 'c')
			printf (" | \u2663 |");
		else if (suitToChar (deck->cards[currentCard]) == 'd')
			printf (" | \u25C6 |");
		else if (suitToChar (deck->cards[currentCard]) == 's')
			printf (" | \u2660 |");
		else if (suitToChar (deck->cards[currentCard]) == 'h')
			printf (" | \u2665 |");
	}
		//printf (" | %c |", suitToChar (deck->cards[currentCard]));

	printf ("\n");

	// Print the fourth line
	for (int currentCard=0; currentCard<deck->numCards; currentCard++)
		printf (" |__%c|", cardNumberToChar (deck->cards[currentCard]));

	printf ("\n\n");
}

unsigned int min (unsigned int a, unsigned int b){
	return (a<b?a:b);
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