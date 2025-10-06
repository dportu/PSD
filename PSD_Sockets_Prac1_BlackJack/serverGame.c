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
	sendMessage(message, socketPlayer);

}

void switchActivePlayer(tPlayer *activePlayer, int *activePlayerSocket, tPlayer *inactivePlayer, int *inactivePlayerSocket, int socket1, int socket2) {
	printf("active player before switch: player%i\n", *activePlayer);
	printf("inactive player before switch: player%i\n", *inactivePlayer);
	if(*activePlayerSocket == socket1) {
		*activePlayerSocket = socket2;
		*activePlayer = player2;
		*inactivePlayerSocket = socket1;
		*inactivePlayer = player1;
	}
	else {
		*activePlayerSocket = socket1;
		*activePlayer = player1;
		*inactivePlayerSocket = socket2;
		*inactivePlayer = player2;
	}
	printf("active player after switch: player%i\n", *activePlayer);
	printf("inactive player before switch: player%i\n", *inactivePlayer);
}

int activePLayerStack(tSession session, tPlayer activePlayer) {
	if(activePlayer == player2)  {
		return session.player2Stack;
	}
	else{
		return session.player1Stack;
	}
}

void rondaDeApuestas(tPlayer *activePlayer, int *activePlayerSocket, tPlayer *inactivePlayer, int *inactivePlayerSocket, tSession *session, int socketPlayer1, int socketPlayer2) {
	for(int i =0;i<2;i++) {
		sendCode(TURN_BET, *activePlayerSocket);
		int stack = activePLayerStack(*session, *activePlayer);
		sendCode(stack, *activePlayerSocket); // TODO: añadir una funcion que devuelva la session del activePlayer

		int bet = receiveCode(*activePlayerSocket);
		
		while(bet > stack) {
			sendCode(TURN_BET, *activePlayerSocket);
			bet = receiveCode(*activePlayerSocket);
		}
		printf("Player %i -> bet: %i\n", i, bet);
		sendCode(TURN_BET_OK, *activePlayerSocket);
		if(*activePlayer == player1) {
			session->player1Bet = bet;
		}
		else {
			session->player2Bet = bet;
		}
		
		switchActivePlayer(activePlayer, activePlayerSocket, inactivePlayer, inactivePlayerSocket, socketPlayer1, socketPlayer2);
	}

}

void hit(tSession *session, tPlayer player) {
	if(player == player1) {
		session->player1Deck.cards[session->player1Deck.numCards] = getRandomCard(&session->gameDeck);
		session->player1Deck.numCards++;
	} else {
		session->player2Deck.cards[session->player2Deck.numCards] = getRandomCard(&session->gameDeck);
		session->player2Deck.numCards++;
	}
}
void broadcastCode(unsigned int code, int activePlayerSocket, int inactivePlayerSocket) {
	sendCode(code, activePlayerSocket);
	sendCode(code, inactivePlayerSocket);
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
	int activePlayerSocket;
	tPlayer activePlayer;
	int inactivePlayerSocket;
	tPlayer inactivePlayer;

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

	activePlayerSocket = socketPlayer1;
	activePlayer = player1;
	inactivePlayerSocket = socketPlayer2;
	inactivePlayer = player2;

	printf("Apuestas...\n");
	
	rondaDeApuestas(&activePlayer, &activePlayerSocket, &inactivePlayer, &inactivePlayerSocket, &session, socketPlayer1, socketPlayer2);

	printf("Empieza la ronda!\n");
	while(session.player1Stack > 0 && session.player2Stack > 0) {
		//enviamos turn_play, puntos de jugada actual y deck actual al jugador activo

		
		//TURN BET
		sendCode(TURN_PLAY, activePlayerSocket);
		sendCode(TURN_PLAY_WAIT, inactivePlayerSocket);

		//enviamos puntos
		broadcastCode(calculatePoints(activePlayer == player1 ? &session.player1Deck : &session.player2Deck), activePlayerSocket, inactivePlayerSocket);
		printf("player1 stack: %i\n player2 stack: %i\n", session.player1Stack, session.player2Stack);
		//enviamos el deck
		if(activePlayer == player1) {
			printf("active player1\n");
			sendDeck(session.player1Deck, activePlayerSocket);
			printf("deck sent to active player\n");
			sendDeck(session.player1Deck, inactivePlayerSocket);
		} else {
			printf("active player2\n");
			sendDeck(session.player2Deck, activePlayerSocket);
			sendDeck(session.player2Deck, inactivePlayerSocket);
		}

		printf("Deck sent\n");
		unsigned int code = receiveCode(activePlayerSocket); //recibimos el stand o hit
		printf("received play: %i\n", code);

		int canPlay = TRUE;
		while (code == TURN_PLAY_HIT && canPlay) {
			hit(&session, activePlayer);
			unsigned int points = calculatePoints(activePlayer == player1 ? &session.player1Deck : &session.player2Deck);
			if(points > 21) {
				sendCode(TURN_PLAY_OUT, socketfd);
				canPlay = FALSE;
			}
			else {
				sendCode(TURN_PLAY, socketfd);
			}
			sendCode(points, socketfd);
			if(activePlayer == player1) {
				sendDeck(session.player1Deck, socketfd);
			} else {
				sendDeck(session.player2Deck, socketfd);
			}

			if(canPlay) {
				receiveCode(activePlayerSocket);
			}
		}

		sendCode(TURN_PLAY_WAIT, activePlayerSocket);
		sendCode(TURN_PLAY_RIVAL_DONE, inactivePlayerSocket);
		//TODO TURN_GAME_LOSE TURN_GAME_WIN

		printf("player1 stack: %i\n player2 stack: %i\n", session.player1Stack, session.player2Stack);
		switchActivePlayer(&activePlayer, &activePlayerSocket,&inactivePlayer, &inactivePlayerSocket, socketPlayer1, socketPlayer2);
	}
	
	// Close sockets
	close(socketPlayer1);
	close(socketPlayer2);
	close(socketfd);
}

