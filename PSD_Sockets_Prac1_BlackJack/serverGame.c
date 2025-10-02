#include "serverGame.h"
#include <pthread.h>

tPlayer getNextPlayer (tPlayer currentPlayer){

	tPlayer next;

		if (currentPlayer == player1)
			next = player2;
		else
			next = player1;

	return next;
}

void initDeck (tDeck *deck){

	deck->numCards = DECK_SIZE; 

	for (int i=0; i<DECK_SIZE; i++){
		deck->cards[i] = i;
	}
}

void clearDeck (tDeck *deck){

	// Set number of cards
	deck->numCards = 0;

	for (int i=0; i<DECK_SIZE; i++){
		deck->cards[i] = UNSET_CARD;
	}
}

void printSession (tSession *session){

		printf ("\n ------ Session state ------\n");

		// Player 1
		printf ("%s [bet:%d; %d chips] Deck:", session->player1Name, session->player1Bet, session->player1Stack);
		printDeck (&(session->player1Deck));

		// Player 2
		printf ("%s [bet:%d; %d chips] Deck:", session->player2Name, session->player2Bet, session->player2Stack);
		printDeck (&(session->player2Deck));

		// Current game deck
		if (DEBUG_PRINT_GAMEDECK){
			printf ("Game deck: ");
			printDeck (&(session->gameDeck));
		}
}

void initSession (tSession *session){

	clearDeck (&(session->player1Deck));
	session->player1Bet = 0;
	session->player1Stack = INITIAL_STACK;

	clearDeck (&(session->player2Deck));
	session->player2Bet = 0;
	session->player2Stack = INITIAL_STACK;

	initDeck (&(session->gameDeck));
}

unsigned int calculatePoints (tDeck *deck){

	unsigned int points;

		// Init...
		points = 0;

		for (int i=0; i<deck->numCards; i++){

			if (deck->cards[i] % SUIT_SIZE < 9)
				points += (deck->cards[i] % SUIT_SIZE) + 1;
			else
				points += FIGURE_VALUE;
		}

	return points;
}

unsigned int getRandomCard (tDeck* deck){

	unsigned int card, cardIndex, i;

		// Get a random card
		cardIndex = rand() % deck->numCards;
		card = deck->cards[cardIndex];

		// Remove the gap
		for (i=cardIndex; i<deck->numCards-1; i++)
			deck->cards[i] = deck->cards[i+1];

		// Update the number of cards in the deck
		deck->numCards--;
		deck->cards[deck->numCards] = UNSET_CARD;

	return card;
}

void createClient(int socketfd, unsigned int clientLength, struct sockaddr_in playerAddress, int socketPlayer, tSession *session, int player) {
	// Check accept result
	if (socketPlayer < 0)
		showError("ERROR while accepting");	  

	// Init and read message
	tString message;

	receiveMessage(message, socketPlayer);

	// Show message
	printf("Name player %i: %s\n", player, message);

	// Guardar en session
	if (player == 2) {
		strcpy(session->player2Name, message);
	}
	else {
		strcpy(session->player1Name, message);
	}

	// Get the message length
	memset (message, 0, STRING_LENGTH);
	strcpy (message, "Name received!");
	int messageLength = send(socketPlayer, message, strlen(message), 0);

	// Check bytes sent
	if (messageLength < 0)
		showError("ERROR while writing to socket");
}

void setActivePlayer(int player, int socket1, int socket2) {
	if(player == 1) {
		sendCode(ACTIVE_PLAYER, socket1);
		sendCode(INACTIVE_PLAYER, socket2);
	}
	else {
		sendCode(ACTIVE_PLAYER, socket2);
		sendCode(INACTIVE_PLAYER, socket1);
	}
}


int main(int argc, char *argv[]){

	int socketfd;						/** Socket descriptor */
	struct sockaddr_in serverAddress;	/** Server address structure */
	unsigned int port;					/** Listening port */
	struct sockaddr_in player1Address;	/** Client address structure for player 1 */
	struct sockaddr_in player2Address;	/** Client address structure for player 2 */
	int socketPlayer1;					/** Socket descriptor for player 1 */
	int socketPlayer2;					/** Socket descriptor for player 2 */
	unsigned int clientLength;			/** Length of client structure */
	tThreadArgs *threadArgs; 			/** Thread parameters */
	pthread_t threadID;					/** Thread ID */
	tSession session; 					/** Session de la partida */
	int activePlayer;

		// Seed
		srand(time(0));

		// Check arguments
		if (argc != 2) {
			fprintf(stderr,"ERROR wrong number of arguments\n");
			fprintf(stderr,"Usage:\n$>%s port\n", argv[0]);
			exit(1);
		}


	// Create the socket
	socketfd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

	// Check
	if (socketfd < 0)
	showError("ERROR while opening socket");

	// Init server structure
	memset(&serverAddress, 0, sizeof(serverAddress));

	// Get listening port
	port = atoi(argv[1]);

	// Fill server structure
	serverAddress.sin_family = AF_INET;
	serverAddress.sin_addr.s_addr = htonl(INADDR_ANY);
	serverAddress.sin_port = htons(port);

	// Bind
	if (bind(socketfd, (struct sockaddr *) &serverAddress, sizeof(serverAddress)) < 0)
		showError("ERROR while binding");

	// Listen
	listen(socketfd, 10);

	// Get length of client structure
	clientLength = sizeof(player1Address);

	// Accept!
	socketPlayer1 = accept(socketfd, (struct sockaddr *) &player1Address, &clientLength);
	socketPlayer2 = accept(socketfd, (struct sockaddr *) &player2Address, &clientLength);
	

	// Creamos a los clientes (Se piden los nombres)
	createClient(socketfd, clientLength, player1Address, socketPlayer1, &session, 1);
	createClient(socketfd, clientLength, player2Address, socketPlayer2, &session, 2);

	initSession(&session);

	setActivePlayer(1, socketPlayer1, socketPlayer2);

	// ToDo: Hacer que empiece el jugador A, y vaya cambiandose en cada turno
	
	while (1) {
		
		sendCode(TURN_BET, socketPlayer1);
		sendCode(session.player1Stack, socketfd);
	}
	
	// Close sockets
	close(socketPlayer1);
	close(socketPlayer2);
	close(socketfd);
}
