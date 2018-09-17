#include "Serial.h"
ServicePortSerial sp;

////COMMUNICATION VARIABLES////
byte isCommunicating = 0;//this is the first digit of the communication. MOST IMPORTANT ONE
enum gameModes {ASSEMBLE, GAME};//so these only occur when isCommunicating is false
byte gameMode = ASSEMBLE;
byte faceComm[6] = {0, 0, 0, 0, 0, 0}; //used to transmit face data during communication mode
byte colorComm[6] = {0, 0, 0, 0, 0, 0}; //used to transmit color data during communication and game mode
byte requestFace = 0;

///ALGORITHM VARIABLES////
byte piecesPlaced = 0;
enum connections {UNDECLARED, APIECE, BPIECE, CPIECE, DPIECE, EPIECE, FPIECE, NONEIGHBOR};
byte neighborsArr[6][6];//filled with the values from above, denotes neighbors. [x][y] x is piece, y is face
byte colorsArr[6][6];//filled with 0-3, denotes color of connection. [x][y] x is piece, y is face

////ASSEMBLY VARIABLES////
bool canBeginAlgorithm = false;
bool isMaster = false;
byte masterFace = 0;//for receivers, this is the face where the master was found

Color displayColors[5] = {OFF, RED, YELLOW, BLUE, WHITE};
byte faceColors[6] = {0, 0, 0, 0, 0, 0};
byte faceBrightness[6] = {0, 0, 0, 0, 0, 0};
byte dimVal = 25;
Timer sparkleTimer;

void setup() {
  // put your setup code here, to run once:
  sp.begin();
}

void loop() {
  if (isCommunicating == 1) {
    communicationLoop();
    communicationDisplay();
  } else {
    if (gameMode == ASSEMBLE) {
      assembleLoop();
      assembleDisplay();
    } else if (gameMode == GAME) {
      gameLoop();
      gameDisplay();
    }
  }
}

void assembleLoop() {
  //all we do here is wait until we have 5 neighbors
  byte numNeighbors = 0;
  FOREACH_FACE(f) {
    if (!isValueReceivedOnFaceExpired(f)) { //neighbor!
      numNeighbors++;
      faceBrightness[f] = 255;
    } else {
      faceBrightness[f] = dimVal;
    }
  }

  if (numNeighbors == 5) {
    canBeginAlgorithm = true;
  } else {
    canBeginAlgorithm = false;
  }

  if (buttonDoubleClicked() && canBeginAlgorithm == true) {//this lets us become the master blink
    makePuzzle();//RUN THE ALGORITHM
    FOREACH_FACE(f) {
      faceComm[f] = 0;//setting all to communicate face 0
      colorComm[f] = colorsArr[f][0];//setting whatever the face 0 color for each face is
    }
    isCommunicating = 1;
    canBeginAlgorithm = false;
    isMaster = true;
  }

  FOREACH_FACE(f) {//here we listen for other blinks to turn us into receiver blinks
    if (!isValueReceivedOnFaceExpired(f)) {//neighbor here
      byte neighborData = getLastValueReceivedOnFace(f);
      if (getCommMode(neighborData) == 1) { //this neighbor is in comm mode (is master)
        isCommunicating = 1;//we are now communicating
        masterFace = f;//will only listen to communication on this face
        requestFace = 0;
      }
    }
  }

  //the last thing we do is communicate our state. All 0s for now, changes later
  setValueSentOnAllFaces(0);
}

void assembleDisplay() {
  if (sparkleTimer.isExpired() && canBeginAlgorithm) {
    FOREACH_FACE(f) {
      setColorOnFace(displayColors[rand(3) + 1], f);
      sparkleTimer.set(50);
    }
  }

  if (!canBeginAlgorithm) {
    FOREACH_FACE(f) {
      setColorOnFace(dim(WHITE, faceBrightness[f]), f);
    }
  }
}

void gameLoop() {
  //all we do here is look at our faces and see if they are touching like colors
  FOREACH_FACE(f) {
    if (!isValueReceivedOnFaceExpired(f)) { //neighbor!
      byte neighborData = getLastValueReceivedOnFace(f);
      byte neighborColor = getColorInfo(neighborData);
      if (neighborColor == faceColors[f]) { //hey, a match!
        faceBrightness[f] = 255;
      } else {//no match :(
        faceBrightness[f] = dimVal;
      }

      //look for neighbors in assemble
      if (getCommMode(neighborData) == 0 && getGameMode(getLastValueReceivedOnFace(f)) == ASSEMBLE) {
        gameMode = ASSEMBLE;
      }

    } else {//no neighbor
      faceBrightness[f] = dimVal;
    }
  }

  //if we are double clicked, we go to assemble mode
  if (buttonDoubleClicked()) {
    gameMode = ASSEMBLE;
  }

  //set communications
  FOREACH_FACE(f) {//[COMM][MODE][----][----][COLR][COLR]
    byte sendData = (isCommunicating << 5) + (gameMode << 4) + (faceColors[f]);
    setValueSentOnFace(sendData, f);
  }
}

void gameDisplay() {
  FOREACH_FACE(f) {
    Color displayColor = displayColors[faceColors[f]];
    byte displayBrightness = faceBrightness[f];
    setColorOnFace(dim(displayColor, displayBrightness), f);
  }
}

/////////////////////////////////
//BEGIN COMMUNICATION PROCEDURE//
/////////////////////////////////

void communicationLoop() {
  if (isMaster) {
    communicationMasterLoop();
  } else {
    communicationReceiverLoop();
  }
}

void communicationMasterLoop() {
  byte completeNeighbors = 0;
  FOREACH_FACE(f) {
    if (!isValueReceivedOnFaceExpired(f)) { //someone is here
      byte requestData = getLastValueReceivedOnFace(f);

      if (getCommMode(requestData) == 0 && getGameMode(requestData) == ASSEMBLE) {//this neighbor is still waiting. Show them face 0 until they catch on
        faceComm[f] = neighborsArr[f][0];
        colorComm[f] = colorsArr[f][0];
      }

      if (getCommMode(requestData) == 1) {//this neighbor is ready for communication
        faceComm[f] = getFaceNum(requestData);//this is the face it wants info about
        colorComm[f] = colorsArr[f][faceComm[f]];//the color for that face of that blink
      }//end communication ready check
    }//end neighbor talk

    if (isValueReceivedOnFaceExpired(f)) { //this is the missing face. Use this info to fill our own face array
      FOREACH_FACE(ff) {
        //just filling our array with the contents of that level of the puzzle
        faceColors[ff] = colorsArr[f][ff];
      }
    }//end missing face check
  }//end request check loop

  byte neighborsInGameMode = 0;
  FOREACH_FACE(f) { //check for neighbors in game mode
    if (!isValueReceivedOnFaceExpired(f)) {
    byte neighborData = getLastValueReceivedOnFace(f);
      if (getCommMode(neighborData) == 0 && getGameMode(neighborData) == GAME) {
        neighborsInGameMode++;
      }
    }
  }
  if(neighborsInGameMode >= 5){//it should never be >, but for safety...
    isCommunicating = 0;
    gameMode = GAME;
    isMaster = false;
  }

  FOREACH_FACE(f) {
    //set up all the communication
    byte sendData = (isCommunicating << 5) + (faceComm[f] << 2) + (colorComm[f]);
    setValueSentOnFace(sendData, f);
  }
}

void communicationReceiverLoop() {
  //so the trick here is to only listen to the master face
  byte receivedData = getLastValueReceivedOnFace(masterFace);
  if (getCommMode(receivedData) == 1) { //we are still in the communication phase of the game
    if (getFaceNum(receivedData) == requestFace) { //we are being told info about our requested face
      faceColors[requestFace] = getColorInfo(receivedData);//take the color info, put it in the correct face
      requestFace ++;
    }
  }

  if (requestFace == 6) { //we have everything!
    isCommunicating = 0;
    gameMode = GAME;
  }

  //now we update our communication
  byte sendData = (isCommunicating << 5) + (requestFace << 2);//[COMM][FACE][FACE][FACE][----][----]
  setValueSentOnFace(sendData, masterFace); //we only communicate on this face. All others are 0 still
}

byte getCommMode(byte data) {
  return (data >> 5);//returns just the first bit
}

byte getFaceNum(byte data) {
  return ((data >> 2) & 7);//returns the 2nd, 3rd, and 4th bit
}

byte getColorInfo(byte data) {
  return (data & 3);//returns the 5th and 6th bits
}

byte getGameMode(byte data) {
  return ((data >> 4) & 1);
}

void communicationDisplay() {
  if (isMaster) {
    setColor(WHITE);
  } else {
    FOREACH_FACE(f) {
      if (f < requestFace) {
        setColorOnFace(WHITE, f);
      }
    }
  }
}

/////////////////////////////////////
//BEGIN PUZZLE GENERATION ALGORITHM//
/////////////////////////////////////

void makePuzzle() {
  resetAll();
  fillFirstLayer();
  fillOuterLayer();
  colorConnections();
  //that does it!
}

void resetAll() {
  piecesPlaced = 0;
  FOREACH_FACE(f) {
    neighborsArr[0][f] = 0;
    neighborsArr[1][f] = 0;
    neighborsArr[2][f] = 0;
    neighborsArr[3][f] = 0;
    neighborsArr[4][f] = 0;
    neighborsArr[5][f] = 0;

    colorsArr[0][f] = 0;
    colorsArr[1][f] = 0;
    colorsArr[2][f] = 0;
    colorsArr[3][f] = 0;
    colorsArr[4][f] = 0;
    colorsArr[5][f] = 0;
  }
}

void fillFirstLayer() {
  //so this is a tricky function
  //it fills the layer around pieceA, which is in the "center" from the algorithm's perspective
  //it actually all operates inside of a face loop, because it only does 5 operations (6 never gets blinks because symmetry)
  //first, we "place" the center piece
  piecesPlaced++;
  FOREACH_FACE(f) {
    bool placingPiece = false;
    switch (f) {//each face has slightly different logic
      case 0:
        //always place B here
        placingPiece = true;
        break;
      case 1:
        //here we have a 7/10 chance of filling the face. Easy check
        if (rand(9) < 7) {//place a face!
          placingPiece = true;
        }
        break;
      case 2:
        //here we have a 5/10 chance of filling the face. Easy check
        if (rand(9) < 5) {//place a face!
          placingPiece = true;
        }
        break;
      case 3:
        //here we have a 4/10 chance of filling the face. Easy check
        if (rand(9) < 4) {//place a face!
          placingPiece = true;
        }
        break;
      case 4:
        //so this is the tricky one
        if (piecesPlaced == 2) { //We have only placed two pieces (center and one other), we MUST place one here
          placingPiece = true;
        } else if (piecesPlaced == 5) { //We have placed 5 pieces in total already, we can't place one here
          //placingPiece remains false
        } else {//We have placed 3 or 4 pieces, 4/10 chance
          if (rand(9) < 4) {//place a face!
            placingPiece = true;
          }
        }
        break;
      case 5:
        //placingPiece remains false
        break;
    }

    //now that we've decided whether or not to place a face, let's do it!
    if (placingPiece) {
      neighborsArr[0][f] = getCurrentPiece();
      piecesPlaced++;
      //now we need to tell B that it has a neighbor
      neighborsArr[piecesPlaced - 1][getNeighborFace(f)] = APIECE;
    } else {//decided not to place a piece
      neighborsArr[0][f] = NONEIGHBOR;
    }
  }//end of face loop

  //so we've placed out first ring. Now we need them each to understand who their lateral neighbors are
  //for this, we need to go to each one that is occupied, and inquire with the central blink about the neighbors
  FOREACH_FACE(f) {
    if (neighborsArr[0][f] != NONEIGHBOR) { //this means there is a blink on this face of the central blink
      byte indexOfRingBlink = neighborsArr[0][f] - 1;//the BPIECE is enumed as 2, but its index in the array is 1, and so on
      //at this stage, we examine both neighbors of the f face
      //and evaluate them in a similar way as above
      //starting with the counterclockwise face
      if (neighborsArr[0][nextCounterclockwise(f)] != 7) { //there is something there
        //to fully make this connection, we need to inform BOTH blinks about this new connection
        neighborsArr[indexOfRingBlink][nextClockwise(getNeighborFace(f))] = neighborsArr[0][nextCounterclockwise(f)];//place the connection into the ringBlink
      } else if (neighborsArr[0][nextCounterclockwise(f)] == 7) { //it is a NONEIGHBOR space
        //to place this connection, we need to put it in the Clockwise next face of the neighboring face to f
        neighborsArr[indexOfRingBlink][nextClockwise(getNeighborFace(f))] = 7;
      }

      //now  the clockwise face
      if (neighborsArr[0][nextClockwise(f)] != 7) { //there is something there
        //to fully make this connection, we need to inform BOTH blinks about this new connection
        neighborsArr[indexOfRingBlink][nextCounterclockwise(getNeighborFace(f))] = neighborsArr[0][nextClockwise(f)];//place the connection into the ringBlink
      } else if (neighborsArr[0][nextClockwise(f)] == 7) { //it is a NONEIGHBOR space
        //to place this connection, we need to put it in the Clockwise next face of the neighboring face to f
        neighborsArr[indexOfRingBlink][nextCounterclockwise(getNeighborFace(f))] = 7;
      }
    }
  }//end of ring face loop
}

void fillOuterLayer() {
  byte piecesRemaining = 6 - piecesPlaced;
  byte sendingIndex = piecesPlaced - 1;
  for (byte i = 0; i < piecesRemaining; i++) {//this adds as many blinks as are remaining
    addOuterBlink(sendingIndex);
  }
}

void addOuterBlink(byte maxSearchIndex) {
  //we begin by evaluating how many eligible spots remain
  byte eligiblePositions = 0;
  for (byte i = 1; i <= maxSearchIndex; i++) {//so here we look through each of the pieces in the first ring
    FOREACH_FACE(f) {
      if (neighborsArr[i][f] == 0) { //this is an eligible spot
        eligiblePositions ++;
      }
    }
  }//end of eligible positions counter

  //now choose a random one of those eligible positions
  byte chosenPosition = rand(eligiblePositions - 1) + 1;//necessary math to get 1-X values
  byte blinkIndex;
  byte faceIndex;
  //now determine which blink this is coming off of
  byte positionCountdown = 0;
  for (byte i = 1; i <= maxSearchIndex; i++) {//same loop as above
    FOREACH_FACE(f) {
      if (neighborsArr[i][f] == 0) { //this is an eligible spot
        positionCountdown ++;
        if (positionCountdown == chosenPosition) {
          //this is it. Record the position!
          blinkIndex = i;
          faceIndex = f;
        }
      }
    }
  }//end of position finder

  //so first we simply place the connection data on the connecting faces
  neighborsArr[blinkIndex][faceIndex] = getCurrentPiece();//placing the new blink on the ring blink
  neighborsArr[piecesPlaced][getNeighborFace(faceIndex)] = blinkIndex + 1;//placing the ring blink on the new blink
  piecesPlaced++;

  //first, the counterclockwise face of the blinked we attached to
  byte counterclockwiseNeighborInfo = neighborsArr[blinkIndex][nextCounterclockwise(faceIndex)];
  if (counterclockwiseNeighborInfo != UNDECLARED && counterclockwiseNeighborInfo != NONEIGHBOR) { //there is a neighbor on the next counterclockwise face of the blink we placed onto
    //we tell the new blink it has a neighbor clockwise from our connection
    byte newNeighborConnectionFace = nextClockwise(getNeighborFace(faceIndex));
    neighborsArr[piecesPlaced - 1][newNeighborConnectionFace] = counterclockwiseNeighborInfo;
    //we also tell the counterclockwise neighbor that it has this new neighbor clockwise
    neighborsArr[counterclockwiseNeighborInfo - 1][getNeighborFace(newNeighborConnectionFace)] = piecesPlaced;
  }

  //now, the clockwise face (everything reversed, but identical)
  byte clockwiseNeighborInfo = neighborsArr[blinkIndex][nextClockwise(faceIndex)];
  if (clockwiseNeighborInfo != UNDECLARED && clockwiseNeighborInfo != NONEIGHBOR) { //there is a neighbor on the next clockwise face of the blink we placed onto
    //we tell the new blink it has a neighbor counterclockwise from our connection
    byte newNeighborConnectionFace = nextCounterclockwise(getNeighborFace(faceIndex));
    neighborsArr[piecesPlaced - 1][newNeighborConnectionFace] = clockwiseNeighborInfo;
    //we also tell the clockwise neighbor that it has this new neighbor clockwise
    neighborsArr[clockwiseNeighborInfo - 1][getNeighborFace(newNeighborConnectionFace)] = piecesPlaced;
  }
}

void colorConnections() {
  //you look through all the neighbor info. When you find a connection with no color, you make it
  FOREACH_FACE(f) {
    FOREACH_FACE(ff) {
      if (neighborsArr[f][ff] != UNDECLARED && neighborsArr[f][ff] != NONEIGHBOR) { //there is a connection here
        byte foundIndex = neighborsArr[f][ff] - 1;
        if (colorsArr[f][ff] == 0) { //we haven't made this connection yet!
          //put a random color there
          byte connectionColor = rand(2) + 1;
          colorsArr[f][ff] = connectionColor;
          FOREACH_FACE(fff) { //go through the faces of the connecting blink, find the connection to the current blink
            if (neighborsArr[foundIndex][fff] == f + 1) {//the connection on the found blink's face is the current blink
              colorsArr[foundIndex][fff] = connectionColor;
            }
          }
        }
      }
    }
  }
}

byte getNeighborFace(byte face) {
  return ((face + 3) % 6);
}

byte nextClockwise (byte face) {
  if (face == 5) {
    return 0;
  } else {
    return face + 1;
  }
}

byte nextCounterclockwise (byte face) {
  if (face == 0) {
    return 5;
  } else {
    return face - 1;
  }
}

byte getCurrentPiece () {
  switch (piecesPlaced) {
    case 0:
      return (APIECE);
      break;
    case 1:
      return (BPIECE);
      break;
    case 2:
      return (CPIECE);
      break;
    case 3:
      return (DPIECE);
      break;
    case 4:
      return (EPIECE);
      break;
    case 5:
      return (FPIECE);
      break;

  }
}

void printAll() {
  //print the connection array
  sp.println();
  FOREACH_FACE(f) {
    sp.print("Piece ");
    sp.print(f + 1);
    sp.print(" ");
    sp.print(neighborsArr[f][0]);
    sp.print(neighborsArr[f][1]);
    sp.print(neighborsArr[f][2]);
    sp.print(neighborsArr[f][3]);
    sp.print(neighborsArr[f][4]);
    sp.println(neighborsArr[f][5]);
  }
  sp.println();
  //print color array
  FOREACH_FACE(f) {
    sp.print("Piece ");
    sp.print(f + 1);
    sp.print(" ");
    sp.print(colorsArr[f][0]);
    sp.print(colorsArr[f][1]);
    sp.print(colorsArr[f][2]);
    sp.print(colorsArr[f][3]);
    sp.print(colorsArr[f][4]);
    sp.println(colorsArr[f][5]);
  }
  sp.println();
}

