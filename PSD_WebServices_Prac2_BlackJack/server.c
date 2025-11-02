#include "server.h"

/** Shared array that contains all the games. */
tGame games[MAX_GAMES];

pthread_cond_t reg; // TODO
// register que espera al jugador 2
// status para ver si le toca jugar o no

void initGame (tGame *game){

	// Init players' name
	memset (game->player1Name, 0, STRING_LENGTH);	
	memset (game->player2Name, 0, STRING_LENGTH);

	// Alloc memory for the decks		
	clearDeck (&(game->player1Deck));	
	clearDeck (&(game->player2Deck));	
	initDeck (&(game->gameDeck));
	
	// Bet and stack
	game->player1Bet = 0;
	game->player2Bet = 0;
	game->player1Stack = INITIAL_STACK;
	game->player2Stack = INITIAL_STACK;
	
	// Game status variables
	game->endOfGame = FALSE;
	game->status = gameEmpty;
}

void initServerStructures (struct soap *soap){

	if (DEBUG_SERVER)
		printf ("Initializing structures...\n");

	// Init seed
	srand (time(NULL));

	// Init each game (alloc memory and init)
	for (int i=0; i<MAX_GAMES; i++){
		games[i].player1Name = (xsd__string) soap_malloc (soap, STRING_LENGTH);
		games[i].player2Name = (xsd__string) soap_malloc (soap, STRING_LENGTH);
		allocDeck(soap, &(games[i].player1Deck));	
		allocDeck(soap, &(games[i].player2Deck));	
		allocDeck(soap, &(games[i].gameDeck));
		initGame (&(games[i]));
	}	
}

void initDeck (blackJackns__tDeck *deck){

	deck->__size = DECK_SIZE;

	for (int i=0; i<DECK_SIZE; i++)
		deck->cards[i] = i;
}

void clearDeck (blackJackns__tDeck *deck){

	// Set number of cards
	deck->__size = 0;

	for (int i=0; i<DECK_SIZE; i++)
		deck->cards[i] = UNSET_CARD;
}

tPlayer calculateNextPlayer (tPlayer currentPlayer){
	return ((currentPlayer == player1) ? player2 : player1);
}

unsigned int getRandomCard (blackJackns__tDeck* deck){

	unsigned int card, cardIndex, i;

	// Get a random card
	cardIndex = rand() % deck->__size;
	card = deck->cards[cardIndex];

	// Remove the gap
	for (i=cardIndex; i<deck->__size-1; i++)
		deck->cards[i] = deck->cards[i+1];

	// Update the number of cards in the deck
	deck->__size--;
	deck->cards[deck->__size] = UNSET_CARD;

	return card;
}

unsigned int calculatePoints (blackJackns__tDeck *deck){

	unsigned int points = 0;
		
		for (int i=0; i<deck->__size; i++){

			if (deck->cards[i] % SUIT_SIZE < 9)
				points += (deck->cards[i] % SUIT_SIZE) + 1;
			else
				points += FIGURE_VALUE;
		}

	return points;
}

void copyGameStatusStructure (blackJackns__tBlock* status, char* message, blackJackns__tDeck *newDeck, int newCode){

	printf("%s\n",message);
	// Copy the message
    if (!status->msgStruct.msg) return; 

	memset((status->msgStruct).msg, 0, STRING_LENGTH);
	strcpy ((status->msgStruct).msg, message);
	(status->msgStruct).__size = strlen ((status->msgStruct).msg);

	//esto peta
	//printf("%s\n",status->msgStruct);

	// Copy the deck, only if it is not NULL
	if (newDeck->__size > 0)
		memcpy ((status->deck).cards, newDeck->cards, DECK_SIZE*sizeof (unsigned int));	
	else
		(status->deck).cards = NULL;

	(status->deck).__size = newDeck->__size;

	// Set the new code
	status->code = newCode;	
}

int iterateGames() {
	int i = 0;
	int found = FALSE;
	//int indexEmpty = MAX_GAMES;
	//int waitingPlayer = FALSE;

	while(i < MAX_GAMES && !found) {
		pthread_mutex_lock(&games[i].registerMutex);
		if(games[i].status != gameReady) {
			found = TRUE;
		}
		else {
			pthread_mutex_unlock(&games[i].registerMutex);
			i++;
		}
	}
	
	return i;
}

void hit(tPlayer player, int gameIndex) {

	unsigned int card = getRandomCard(&games[gameIndex].gameDeck);
	//printf("Selected card: %i", card);
	if(player == player1) {
		games[gameIndex].player1Deck.cards[games[gameIndex].player1Deck.__size] = card;
		games[gameIndex].player1Deck.__size++;
	} 
	else {
		games[gameIndex].player2Deck.cards[games[gameIndex].player2Deck.__size] = card;
		games[gameIndex].player2Deck.__size++;
	}

}

void setFirstPlayer(int gameIndex) {
	int firstActivePlayer = rand() % 2;
	if(firstActivePlayer) {
		games[gameIndex].currentPlayer = player2;
	}
	else {
		games[gameIndex].currentPlayer = player1;	
	}
}

int blackJackns__register (struct soap *soap, blackJackns__tMessage playerName, int* result){
	//pthread_cond_wait del jugador 1 hasta que se registre el segundo?
	int gameIndex;

	// Set \0 at the end of the string
	playerName.msg[playerName.__size - 1] = 0;

	if (DEBUG_SERVER) {
		printf ("[Register] Registering new player -> [%s]\n", playerName.msg);
	}
		
	//block games[] mutex
	//iterate looking for game[i] == gaeWaitingPlayer
	//else iterate looking for game empty
	//modify things
	
	gameIndex = iterateGames();

	printf("Game index: %i\n", gameIndex);
	
	if(gameIndex == MAX_GAMES) { // game is full
		*result = ERROR_SERVER_FULL;
	}
	else {
		printf("Status of current game: %i\n", games[gameIndex].status);
		if(games[gameIndex].status == gameEmpty) {
			strcpy(games[gameIndex].player1Name, playerName.msg);
			games[gameIndex].status = gameWaitingPlayer;
			*result = gameIndex;

			//eleccion aleatoria del primer jugador
			setFirstPlayer(gameIndex);
			printf("El primer turno es para %s\n", games[gameIndex].currentPlayer ? games[gameIndex].player1Name : games[gameIndex].player2Name);

			pthread_cond_wait(&games[gameIndex].registerCond, &games[gameIndex].registerMutex); // pthread_cond_wait para el primer jugador hasta que se registre el segundo?
		}
		else {
			if(strcmp(playerName.msg, games[gameIndex].player1Name) == 0) { //the name is already taken
				*result = ERROR_NAME_REPEATED;
			}
			else { //the name is not already taken in the game
				strcpy(games[gameIndex].player2Name, playerName.msg);
				games[gameIndex].status = gameReady;
				*result = gameIndex;

				pthread_cond_signal(&games[gameIndex].registerCond); // Despierta al jugador 1
			}
		}
		pthread_mutex_unlock(&games[gameIndex].registerMutex);
	}
	printf("End of register\n");

  	return SOAP_OK;
}


int blackJackns__getStatus(struct soap *soap, blackJackns__tMessage playerName, int gameIndex, blackJackns__tBlock *gameStatus) {
	allocClearBlock (soap, gameStatus);

	printf("Iniciamos el getStatus\n");

	pthread_mutex_lock(&games[gameIndex].statusMutex);

	while ((strcmp(playerName.msg, games[gameIndex].player1Name) == 0 && player1 == games[gameIndex].currentPlayer) ||
    	(strcmp(playerName.msg, games[gameIndex].player2Name) == 0 && player2 == games[gameIndex].currentPlayer)) {
		printf("%s waiting...\n", playerName.msg);
		pthread_cond_wait(&games[gameIndex].statusCond, &games[gameIndex].statusMutex);
		printf("%s woke up...\n", playerName.msg);
	}

	//devolvemos TURN_PLAY o TURN_WAIT si todo va bien o ERROR_ACTIVE_PLAYER en caso de algo raro o ERROR_PLAYER_NOT_FOUND si no esta en la partida
	if (strcmp(playerName.msg, games[gameIndex].player1Name) == 0 || strcmp(playerName.msg, games[gameIndex].player2Name) == 0) {

		blackJackns__tDeck *activePlayerDeck;
		blackJackns__tDeck *inactivePlayerDeck;
		activePlayerDeck = (games[gameIndex].currentPlayer == player1) ? &games[gameIndex].player1Deck : &games[gameIndex].player2Deck;
		inactivePlayerDeck = (games[gameIndex].currentPlayer == player1) ? &games[gameIndex].player2Deck : &games[gameIndex].player1Deck;

		//el jugador inactivo pasa a activo
		if ((strcmp(playerName.msg, games[gameIndex].player1Name) == 0 && player1 != games[gameIndex].currentPlayer) ||
    		(strcmp(playerName.msg, games[gameIndex].player2Name) == 0 && player2 != games[gameIndex].currentPlayer)) {
			copyGameStatusStructure(gameStatus,"Ahora eres el jugador activo\n" ,inactivePlayerDeck, TURN_PLAY); //le mandamos el deck del otro jugador ?
		}

		//el jugador inactivo sigue siendo inactivo
		else if ((strcmp(playerName.msg, games[gameIndex].player1Name) == 0 && player2 == games[gameIndex].currentPlayer) || 
				(strcmp(playerName.msg, games[gameIndex].player2Name) == 0 && player1 == games[gameIndex].currentPlayer)) {
			
			copyGameStatusStructure(gameStatus,"Sigue siendo el turno del rival\n" ,activePlayerDeck, TURN_WAIT); //le mandamos el deck del otro jugador ?
		}
		
		else {
			gameStatus->code = ERROR_ACTIVE_PLAYER;
		}
	}
	else {
		gameStatus->code = ERROR_PLAYER_NOT_FOUND;
	}
	
	pthread_mutex_unlock(&games[gameIndex].statusMutex);
	
	return SOAP_OK;
}

int blackJackns__playerMove(struct soap *soap, blackJackns__tMessage playerName, int gameIndex, unsigned int move, blackJackns__tBlock *playerMove) { // ToDo
	allocClearBlock (soap, playerMove);
	blackJackns__tDeck *activePlayerDeck;
	blackJackns__tDeck *inactivePlayerDeck;
	unsigned int resultCode;

	pthread_mutex_lock(&games[gameIndex].statusMutex); //mutex lock

	activePlayerDeck = (games[gameIndex].currentPlayer == player1) ? &games[gameIndex].player1Deck : &games[gameIndex].player2Deck;
	inactivePlayerDeck = (games[gameIndex].currentPlayer == player1) ? &games[gameIndex].player2Deck : &games[gameIndex].player1Deck;

	if(strcmp(playerName.msg, games[gameIndex].player1Name) || strcmp(playerName.msg, games[gameIndex].player2Name)) {
		if (move == PLAYER_STAND) {
			resultCode = TURN_WAIT;
			copyGameStatusStructure(playerMove,"Has realizado un stand\n" ,activePlayerDeck, resultCode);
			games[gameIndex].currentPlayer = calculateNextPlayer(games[gameIndex].currentPlayer);
		}
		else if (move == PLAYER_HIT_CARD) {
			tPlayer hitPlayer = (games[gameIndex].currentPlayer == player1) ? player1 : player2; //creo
			hit(hitPlayer, gameIndex);
			if(calculatePoints(activePlayerDeck) > 21) {
				resultCode = TURN_WAIT;
			}
			else {
				resultCode = TURN_PLAY;
			}
			copyGameStatusStructure(playerMove,"Has realizado un hit\n" ,activePlayerDeck, resultCode); 
		}
		else {
			resultCode = ERROR_ACTIVE_PLAYER;
			copyGameStatusStructure(playerMove,"Creo que ha habido un error\n" ,activePlayerDeck, resultCode); 
		}
	}
	else {
		resultCode = ERROR_PLAYER_NOT_FOUND;
		copyGameStatusStructure(playerMove,"No se ha encontrado al jugador en la partida\n" ,activePlayerDeck, resultCode); 
	}

	// Cambiar el turno
	// games[gameIndex].currentPlayer = calculateNextPlayer(games[gameIndex].currentPlayer);

	// Despertar al otro jugador
	pthread_cond_signal(&games[gameIndex].statusCond);

	pthread_mutex_unlock(&games[gameIndex].statusMutex);

	return SOAP_OK;
}

void *processRequest(void *soap){

	pthread_detach(pthread_self());

	printf ("Processing a new request...\n");

	soap_serve((struct soap*)soap);
	soap_destroy((struct soap*)soap);
	soap_end((struct soap*)soap);
	soap_done((struct soap*)soap);
	free(soap);

	return NULL;
}

int main(int argc, char **argv){ 

	struct soap soap;
	struct soap *tsoap;
	pthread_t tid;
	int port;
	SOAP_SOCKET m, s;

	// Check arguments
	if (argc !=2) {
		printf("Usage: %s port\n",argv[0]);
		exit(0);
	}

	// Init soap environment
	soap_init(&soap);

	// Configure timeouts
	soap.send_timeout = 60; // 60 seconds
	soap.recv_timeout = 60; // 60 seconds
	soap.accept_timeout = 3600; // server stops after 1 hour of inactivity
	soap.max_keep_alive = 100; // max keep-alive sequence

	initServerStructures(&soap);

	for(int i = 0; i< MAX_GAMES;i++) {
		pthread_mutex_init(&games[i].registerMutex, NULL);
		pthread_cond_init(&games[i].registerCond, NULL);
		pthread_mutex_init(&games[i].statusMutex, NULL);
		pthread_cond_init(&games[i].statusCond, NULL);
	}

	// Get listening port
	port = atoi(argv[1]);

	// Bind
	m = soap_bind(&soap, NULL, port, 100);

	if (!soap_valid_socket(m)){
		exit(1);
	}

	printf("Server is ON!\n");

	while (TRUE){

		// Accept a new connection
		s = soap_accept(&soap);
		// Socket is not valid :(
		if (!soap_valid_socket(s)){

			if (soap.errnum){
				soap_print_fault(&soap, stderr);
				exit(1);
			}

			fprintf(stderr, "Time out!\n");
			break;
		}

		// Copy the SOAP environment
		tsoap = soap_copy(&soap);

		if (!tsoap){
			printf ("SOAP copy error!\n");
			break;
		}

		// Create a new thread to process the request
		pthread_create(&tid, NULL, (void*(*)(void*))processRequest, (void*)tsoap);
	}

	// Detach SOAP environment
	soap_done(&soap);

	// Delete threads
	for(int i = 0; i< MAX_GAMES;i++) {
		pthread_mutex_destroy(&games[i].registerMutex);
		pthread_cond_destroy(&games[i].registerCond);
		pthread_mutex_destroy(&games[i].statusMutex);
		pthread_cond_destroy(&games[i].statusCond);
	}
	
	return 0;
}
