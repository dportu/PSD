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

void createClient(int socketPlayer, tSession *session, int player) {

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
	printf("\n--- SWITCH ACTIVE PLAYER ---\n");
	printf("Active player before switch: player%i\n", *activePlayer);
	// printf("inactive player before switch: player%i\n", *inactivePlayer);
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
	printf("Active player after switch: player%i\n", *activePlayer);
	// printf("inactive player before switch: player%i\n", *inactivePlayer);
}

unsigned int activePLayerStack(tSession session, tPlayer activePlayer) {
	if(activePlayer == player2)  {
		return session.player2Stack;
	}
	else{
		return session.player1Stack;
	}
}

void rondaDeApuestas(tPlayer *activePlayer, int *activePlayerSocket, tPlayer *inactivePlayer, int *inactivePlayerSocket, tSession *session, int socketPlayer1, int socketPlayer2) {
	for(int i =0;i<2;i++) {
		printf("Player%i\n", *activePlayer);
		sendCode(TURN_BET, *activePlayerSocket);
		unsigned int stack = activePLayerStack(*session, *activePlayer);
		sendCode(stack, *activePlayerSocket); // TODO: añadir una funcion que devuelva la session del activePlayer
		printf("- Stack: %i\n", stack);

		unsigned int bet = receiveInt(*activePlayerSocket);
		printf("- Bet: %i\n", bet);
		
		while(bet > stack) {
			printf("Invalid bet (>%i)...\n", stack);
			sendCode(TURN_BET, *activePlayerSocket);
			bet = receiveInt(*activePlayerSocket);
		}
		
		sendCode(TURN_BET_OK, *activePlayerSocket);
		// printf("Valid bet: %i\n", bet);
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
	unsigned int card = getRandomCard(&session->gameDeck);
	//printf("Selected card: %i", card);
	if(player == player1) {
		session->player1Deck.cards[session->player1Deck.numCards] = card;
		session->player1Deck.numCards++;
	} 
	else {
		session->player2Deck.cards[session->player2Deck.numCards] = card;
		session->player2Deck.numCards++;
	}
}
void broadcastCode(unsigned int code, int activePlayerSocket, int inactivePlayerSocket) {
	sendCode(code, activePlayerSocket);
	sendCode(code, inactivePlayerSocket);
}

unsigned int winner(unsigned int points1, unsigned int points2) {
	// Ambos se pasan o empatan, nadie gana
    if ((points1 > 21 && points2 > 21) || (points1 == points2))
        return 0;

    // Si uno se pasa, gana el otro
    if (points1 > 21) {
		return 2;
	}
    if (points2 > 21) {
		return 1;
	}

    // Ninguno se pasa, gana el que más puntos tenga
	if (points1 > points2) {
		return 1;
	}
    else {
		return 2;
	}
}

/**
 * Create, bind and listen
 */
int createBindListenSocket (unsigned short port){

	int socketId;
	struct sockaddr_in echoServAddr;

		if ((socketId = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) < 0)
			showError("Error while creating a socket") ;

		// Set server address
		memset(&echoServAddr, 0, sizeof(echoServAddr));
		echoServAddr.sin_family = AF_INET;
		echoServAddr.sin_addr.s_addr = htonl(INADDR_ANY);
		echoServAddr.sin_port = htons(port);

		// Bind
		if (bind(socketId, (struct sockaddr *) &echoServAddr, sizeof(echoServAddr)) < 0)
			showError ("Error while binding");

		if (listen(socketId, MAX_CONNECTIONS) < 0)
			showError("Error while listening") ;

	return socketId;
}

/**
 * Accept connection
 */
int acceptConnection (int socketServer){

	int clientSocket;
	struct sockaddr_in clientAddress;
	unsigned int clientAddressLength;

		// Get length of client address
		clientAddressLength = sizeof(clientAddress);

		// Accept
		if ((clientSocket = accept(socketServer, (struct sockaddr *) &clientAddress, &clientAddressLength)) < 0)
			showError("Error while accepting connection");

		printf("Connection established with client: %s\n", inet_ntoa(clientAddress.sin_addr));

	return clientSocket;
}

/**
 * Thread processing function
 */
void *threadProcessing(void *threadArgs){
	int socketPlayer1 = ((tThreadArgs *) threadArgs)->socketPlayer1;
	int socketPlayer2 = ((tThreadArgs *) threadArgs)->socketPlayer2;
	tSession session;

	//pedir nombres
	createClient(socketPlayer1, &session, 1);
	createClient(socketPlayer2, &session, 2);

	initSession(&session);

	int activePlayerSocket = socketPlayer1;
	tPlayer activePlayer = player1;
	int inactivePlayerSocket = socketPlayer2;
	tPlayer inactivePlayer = player2;


	while(session.player1Stack > 0 && session.player2Stack > 0) {

		printf("\n--- Betting begins ---\n");
		rondaDeApuestas(&activePlayer, &activePlayerSocket, &inactivePlayer, &inactivePlayerSocket, &session, socketPlayer1, socketPlayer2);
		//enviamos turn_play, puntos de jugada actual y deck actual al jugador activo


		printf("--- Round starts ---\n");
		unsigned int pointsPlayer1 = 0;
		unsigned int pointsPlayer2 = 0;
		
		for(int k = 0;k<2;k++) { //cada ronda tiene dos vueltas, una por cada jugador
			//TURN BET
			sendCode(TURN_PLAY, activePlayerSocket);
			sendCode(TURN_PLAY_WAIT, inactivePlayerSocket);

			if (activePlayer == player1) {
				printf("%s's turn\n", session.player1Name);
			}
			else {
				printf("%s's turn\n", session.player2Name);
			}	
			
			// Repartimos dos cartas
			hit(&session, activePlayer);
			hit(&session, activePlayer);

			//enviamos puntos
			broadcastCode(calculatePoints(activePlayer == player1 ? &session.player1Deck : &session.player2Deck), activePlayerSocket, inactivePlayerSocket);

			//enviamos el deck
			
			if(activePlayer == player1) {
				sendDeck(&session.player1Deck, activePlayerSocket);
				sendDeck(&session.player1Deck, inactivePlayerSocket);
			} else {
				sendDeck(&session.player2Deck, activePlayerSocket);
				sendDeck(&session.player2Deck, inactivePlayerSocket);
			}

			printf("Waiting player%i...\n", activePlayer);
			unsigned int code = receiveCode(activePlayerSocket); //recibimos el stand o hit

			int canPlay = TRUE;
			while (code == TURN_PLAY_HIT && canPlay) {
				hit(&session, activePlayer);
				unsigned int points = calculatePoints(activePlayer == player1 ? &session.player1Deck : &session.player2Deck);
				
				//Actualizamos los puntos de la ronda del player activo
				if(activePlayer == player1) {
					pointsPlayer1 = points;
				}
				else {
					pointsPlayer2 = points;
				}

				if(points > 21) {
					broadcastCode(TURN_PLAY_OUT, activePlayerSocket, inactivePlayerSocket);
					canPlay = FALSE;
				}
				else {
					sendCode(TURN_PLAY, activePlayerSocket);
    				sendCode(TURN_PLAY_WAIT, inactivePlayerSocket);
				}
				
				broadcastCode(points, activePlayerSocket, inactivePlayerSocket);

				if(activePlayer == player1) {
					sendDeck(&session.player1Deck, activePlayerSocket);
					sendDeck(&session.player1Deck, inactivePlayerSocket);
				} else {
					sendDeck(&session.player2Deck, activePlayerSocket);
					sendDeck(&session.player2Deck, inactivePlayerSocket);
				}

				if(canPlay) {
					code = receiveCode(activePlayerSocket);
				}
			}

			sendCode(TURN_PLAY_WAIT, activePlayerSocket);
			sendCode(TURN_PLAY_RIVAL_DONE, inactivePlayerSocket);


			//printf("player1 stack: %i\nplayer2 stack: %i\n", session.player1Stack, session.player2Stack);
			printSession(&session);
			switchActivePlayer(&activePlayer, &activePlayerSocket,&inactivePlayer, &inactivePlayerSocket, socketPlayer1, socketPlayer2);
			
		} //final del bucle for

		//ACTUALIZAR LAS FICHAS DE CADA JUGADOR
		printf("Player1's points: %i\nPlayer2's points: %i\n", pointsPlayer1, pointsPlayer2);
		unsigned int roundWinner = winner(pointsPlayer1, pointsPlayer2);
		printf("The winner of the round is player%i\n", roundWinner);
		if(roundWinner == 1) {
			session.player1Stack += session.player2Bet;
			session.player2Stack -= session.player2Bet;
		}
		else if(roundWinner == 2) {
			session.player1Stack -= session.player1Bet;
			session.player2Stack += session.player1Bet;
		}


		//COMPROBAR SI ALGUIEN HA PERDIDO Y MANDAR MENSAJES EN CASO AFIRMATIVO

		if(session.player1Stack == 0) {
			sendCode(TURN_GAME_LOSE, socketPlayer1);
			sendCode(TURN_GAME_WIN, socketPlayer2);

			printf("%s has won the game and %s has lost", session.player2Name, session.player1Name);
		}
		else if(session.player2Stack == 0) {
			sendCode(TURN_GAME_LOSE, socketPlayer2);
			sendCode(TURN_GAME_WIN, socketPlayer1);

			printf("%s has won the game and %s has lost", session.player1Name, session.player2Name);
		}
		else {
			broadcastCode(TURN_BET, socketPlayer1, socketPlayer2);
			clearDeck(&session.player1Deck);
			clearDeck(&session.player2Deck);
			switchActivePlayer(&activePlayer, &activePlayerSocket,&inactivePlayer, &inactivePlayerSocket, socketPlayer1, socketPlayer2);
		}
	}
	
	// Close sockets
	close(socketPlayer1);
	close(socketPlayer2);

	return (NULL);
}

int main(int argc, char *argv[]){

	int socketfd;						/** Socket descriptor */
	struct sockaddr_in serverAddress;	/** Server address structure */
	unsigned int port;					/** Listening port */
	int socketPlayer1;					/** Socket descriptor for player 1 */
	int socketPlayer2;					/** Socket descriptor for player 2 */
	tThreadArgs *threadArgs; 			/** Thread parameters */
	pthread_t threadID;					/** Thread ID */
	

	// Seed
	srand(time(0));

	// Check arguments
	if (argc != 2) {
		fprintf(stderr,"ERROR wrong number of arguments\n");
		fprintf(stderr,"Usage:\n$>%s port\n", argv[0]);
		exit(1);
	}

	// Init server structure
	memset(&serverAddress, 0, sizeof(serverAddress));

	// Get listening port
	port = atoi(argv[1]);

	socketfd = createBindListenSocket(port);

	while(1){

		// Establish connection with a client
		socketPlayer1 = acceptConnection(socketfd);
		socketPlayer2 = acceptConnection(socketfd);

		// Allocate memory
		if ((threadArgs = (tThreadArgs *) malloc(sizeof(tThreadArgs))) == NULL)
			showError("Error while allocating memory");

		// Set socket to the thread's parameter structure
		threadArgs->socketPlayer1 = socketPlayer1;
		threadArgs->socketPlayer2 = socketPlayer2;

		// Create a new thread
		if (pthread_create(&threadID, NULL, threadProcessing, (void *) threadArgs) != 0)
			showError("pthread_create() failed");
	}
}