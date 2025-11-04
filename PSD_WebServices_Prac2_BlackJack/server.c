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
	printf("First active player = %d\n", firstActivePlayer);
	if(firstActivePlayer) {
		games[gameIndex].currentPlayer = player2;
	}
	else {
		games[gameIndex].currentPlayer = player1;	
	}
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

int checkStacks(int gameIndex) {
	int loser = 0;
	
	if(games[gameIndex].player1Stack == 0)  {
		loser = 1;
	}
	else if(games[gameIndex].player2Stack == 0)  {
		loser = 2;
	}

	return loser; // Devolvemos el perdedor o 0 si nadie pierde
}

int endRound(int gameIndex) {
	int endOfRound = FALSE;
	
	if(games[gameIndex].currentPlayer == player1) {
		games[gameIndex].player1Bet = TRUE;
	}
	else {
		games[gameIndex].player2Bet = TRUE;
	}
	
	// Actualizamos los stacks de los jugadores o los dejamos igual en caso de empate
	if( (games[gameIndex].player1Bet == TRUE) && (games[gameIndex].player2Bet == TRUE) ) {
		endOfRound = TRUE;
		int roundWinner = winner( calculatePoints(&games[gameIndex].player1Deck), calculatePoints(&games[gameIndex].player2Deck) );
		
		if(roundWinner == 1) {
			games[gameIndex].player1Stack += DEFAULT_BET;
			games[gameIndex].player2Stack -= DEFAULT_BET;
		}
		else if(roundWinner == 2) {
			games[gameIndex].player2Stack += DEFAULT_BET;
			games[gameIndex].player1Stack -= DEFAULT_BET;
		}


		// Reseteamos los decks de los jugadores
		clearDeck(&(games[gameIndex].player1Deck));
        clearDeck(&(games[gameIndex].player2Deck));
        initDeck(&(games[gameIndex].gameDeck));

		// Reseteamos los bets de los jugadores
		games[gameIndex].player1Bet = FALSE;
        games[gameIndex].player2Bet = FALSE;

	}


	games[gameIndex].currentPlayer = calculateNextPlayer(games[gameIndex].currentPlayer);

	return endOfRound; // Devolvemos si la ronda ha finalizado
}

unsigned int endGame (int gameIndex, tPlayer player) {
	int stacks = checkStacks(gameIndex);
	unsigned int code;

	if(stacks) {
		if (stacks == 1 && player == player1) {
			code = GAME_LOSE;
		}
		else if (stacks == 2 && player == player2) {
			code = GAME_LOSE;
		}
		else {
			code = GAME_WIN;
		}
	}
	else {
		code = FALSE;
	}

	return code;
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

			pthread_cond_wait(&games[gameIndex].registerCond, &games[gameIndex].registerMutex); // pthread_cond_wait para el primer jugador hasta que se registre el segundo
		}
		else {
			if(strcmp(playerName.msg, games[gameIndex].player1Name) == 0) { //the name is already taken
				*result = ERROR_NAME_REPEATED;
			}
			else { //the name is not already taken in the game
				strcpy(games[gameIndex].player2Name, playerName.msg);
				games[gameIndex].status = gameReady;
				*result = gameIndex;

				//eleccion aleatoria del primer jugador
				setFirstPlayer(gameIndex);
				printf("El primer turno es para %s\n", games[gameIndex].currentPlayer ? games[gameIndex].player2Name : games[gameIndex].player1Name);

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


	tPlayer statusPlayer; // Player que esta pidiendo el servico getStatus
	if((strcmp(playerName.msg, games[gameIndex].player1Name) == 0)) {
		statusPlayer = player1;
	}
	else {
		statusPlayer = player2;
	}


	// Hacemos el doble hit inicial
	if( (statusPlayer == player1 && games[gameIndex].player1Deck.__size == 0) || (statusPlayer == player2 && games[gameIndex].player2Deck.__size == 0) ) {
		printf("Doble hit de %d chaval\n", statusPlayer);
		hit(statusPlayer, gameIndex);
		hit(statusPlayer, gameIndex);
	}

	playerName.msg[playerName.__size - 1] = 0;

	if( ((strcmp(playerName.msg, games[gameIndex].player1Name) == 0) && (games[gameIndex].currentPlayer != player1)) ||
    	((strcmp(playerName.msg, games[gameIndex].player2Name) == 0) && (games[gameIndex].currentPlayer != player2)) ) {
			printf("%s is waiting...\n", playerName.msg);
			pthread_cond_wait(&games[gameIndex].statusCond, &games[gameIndex].statusMutex);
			printf("%s woke up...\n", playerName.msg);
		}

	//devolvemos TURN_PLAY o TURN_WAIT si todo va bien o ERROR_ACTIVE_PLAYER en caso de algo raro o ERROR_PLAYER_NOT_FOUND si no esta en la partida
	if (strcmp(playerName.msg, games[gameIndex].player1Name) == 0 || strcmp(playerName.msg, games[gameIndex].player2Name) == 0) {

		blackJackns__tDeck *activePlayerDeck;
		blackJackns__tDeck *inactivePlayerDeck;
		activePlayerDeck = (games[gameIndex].currentPlayer == player1) ? &games[gameIndex].player1Deck : &games[gameIndex].player2Deck;
		inactivePlayerDeck = (games[gameIndex].currentPlayer == player1) ? &games[gameIndex].player2Deck : &games[gameIndex].player1Deck;



		/*printf("DEBUG:\n");
		printf("playerName.msg = '%s'\n", playerName.msg);
		printf("player1Name = '%s'\n", games[gameIndex].player1Name);
		printf("player2Name = '%s'\n", games[gameIndex].player2Name);
		printf("currentPlayer = %d\n", games[gameIndex].currentPlayer);
		printf("player1 = %d\n", player1);
		printf("player2 = %d\n", player2);

		printf("strcmp(playerName.msg, player1Name) = %d\n", strcmp(playerName.msg, games[gameIndex].player1Name));
		printf("strcmp(playerName.msg, player2Name) = %d\n", strcmp(playerName.msg, games[gameIndex].player2Name));
		printf("Game index: %d\n", gameIndex);*/


		unsigned int resultCode;
		

		// Loser = 0 si nadie pierde
		// Loser = gameWin en caso de que pierda el otro
		// Loser = gameLose en caso de que pierda statusPlayer
		unsigned int loser = endGame(gameIndex, statusPlayer);
		if(loser) { // Si alguien ha perdido
			resultCode = loser;
			char replyMessage[64];
			if(resultCode == GAME_WIN) {
				sprintf(replyMessage, "You win !!\n");
			}
			else {
				sprintf(replyMessage, "You Lose :(\n");
			}
			
			initGame(&games[gameIndex]); // reseteamos el game ??
			copyGameStatusStructure(gameStatus, replyMessage, activePlayerDeck, resultCode);
		}
		// El jugador pasa a ser el activo
		else if (statusPlayer == games[gameIndex].currentPlayer) {
			copyGameStatusStructure(gameStatus,"Ahora eres el jugador activo\n" ,activePlayerDeck, TURN_PLAY); 
		}
		//el jugador inactivo sigue siendo inactivo
		else {
			
			char replyMessage[64];
			sprintf(replyMessage, "Points del rival: %d", calculatePoints(activePlayerDeck));
			copyGameStatusStructure(gameStatus, replyMessage, activePlayerDeck, TURN_WAIT);
		}
	}
	else {
		gameStatus->code = ERROR_PLAYER_NOT_FOUND;
	}

	

	// Mutex unlock
	pthread_mutex_unlock(&games[gameIndex].statusMutex);
	
	return SOAP_OK;
}

//TODO REINICIAR LOS DECKS DESPUES DE CADA RONDA (AL ELEGIR QUIEN GANA LA RONDA) , TAMBIEN CORREGIR EL WINNER QUE NO VA BIEN
int blackJackns__playerMove(struct soap *soap, blackJackns__tMessage playerName, int gameIndex, unsigned int move, blackJackns__tBlock *playerMove) {
	allocClearBlock (soap, playerMove);
	blackJackns__tDeck *activePlayerDeck;
	blackJackns__tDeck *inactivePlayerDeck;
	unsigned int resultCode;

	pthread_mutex_lock(&games[gameIndex].statusMutex); //mutex lock

	activePlayerDeck = (games[gameIndex].currentPlayer == player1) ? &games[gameIndex].player1Deck : &games[gameIndex].player2Deck;
	inactivePlayerDeck = (games[gameIndex].currentPlayer == player1) ? &games[gameIndex].player2Deck : &games[gameIndex].player1Deck;

	



	int auxCode;
	// Si el jugador pertenece a la partida
	if(strcmp(playerName.msg, games[gameIndex].player1Name) == 0 || strcmp(playerName.msg, games[gameIndex].player2Name) == 0) {
		if (move == PLAYER_STAND) { // Player stand
			resultCode = TURN_WAIT;

			// Comprobamos si alguien ha perdido
			if(endRound(gameIndex)) {
				auxCode = endGame(gameIndex, games[gameIndex].currentPlayer);
				if(auxCode) {
					resultCode = auxCode;
				}
			}

			copyGameStatusStructure(playerMove,"Has realizado un stand\n" ,activePlayerDeck, resultCode);
		}
		else if (move == PLAYER_HIT_CARD) { // Player hit
			tPlayer hitPlayer = (games[gameIndex].currentPlayer == player1) ? player1 : player2;
			hit(hitPlayer, gameIndex);

			if(calculatePoints(activePlayerDeck) > 21) { // Si el jugador se pasa de 21
				resultCode = TURN_WAIT;

				// Comprobamos si alguien ha perdido
				if(endRound(gameIndex)) {
					auxCode = endGame(gameIndex, games[gameIndex].currentPlayer);
					if(auxCode) {
						resultCode = auxCode;
					}
				}
			}
			else {
				resultCode = TURN_PLAY;
			}
			char replyMessage[64];
			sprintf(replyMessage, "Has realizado un hit\nPoints: %d", calculatePoints(activePlayerDeck));
			copyGameStatusStructure(playerMove,replyMessage ,activePlayerDeck, resultCode); 
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
