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
Timer sparkleTimer;

Timer messageTimer;
#define MESSAGE_DURATION 750

////GAME VARIABLES////
Color displayColors[5] = {OFF, RED, YELLOW, BLUE, WHITE};
byte faceColors[6] = {0, 0, 0, 0, 0, 0};
byte faceBrightness[6] = {0, 0, 0, 0, 0, 0};
byte dimVal = 64;


void setup() {
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

  // clean button presses
  buttonPressed();
  buttonDoubleClicked();
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
        messageTimer.set(MESSAGE_DURATION);
        gameMode = GAME;//not immediately important, but will come into play later
        masterFace = f;//will only listen to communication on this face
        requestFace = 0;
      }
    }
  }

  //the last thing we do is communicate our state. All 0s for now, changes later
  byte sendData = (isCommunicating << 5) + (gameMode << 4);
  setValueSentOnAllFaces(sendData);
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
  if (neighborsInGameMode >= 5) { //it should never be >, but for safety...
    isCommunicating = 0;
    gameMode = GAME;//redundant technically, but leaving for now
    isMaster = false;
  }

  FOREACH_FACE(f) {
    //set up all the communication
    byte sendData = (isCommunicating << 5) + (faceComm[f] << 2) + (colorComm[f]);
    setValueSentOnFace(sendData, f);
  }
}

void communicationReceiverLoop() {
  // if button pressed, advance the face we are asking for
  if ( messageTimer.isExpired( ) ) {
    requestFace ++;
    messageTimer.set(MESSAGE_DURATION);
  }

  //so the trick here is to only listen to the master face
  byte receivedData = getLastValueReceivedOnFace(masterFace);
  if (getCommMode(receivedData) == 1) { //we are still in the communication phase of the game
    if (getFaceNum(receivedData) == requestFace) { //we are being told info about our requested face
      faceColors[requestFace] = getColorInfo(receivedData);//take the color info, put it in the correct face
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
    // display WHITE if waiting
    // display color if received
    FOREACH_FACE(f) {
      if (f < requestFace) {
        Color displayColor = displayColors[faceColors[f]];
        setColorOnFace(displayColor, f);
      }
      else {
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
  piecesPlaced++;//this symbolically places the first blink in the center
  //place 2-4 NONEIGHBORS in first ring
  byte emptySpots = rand(2) + 2;//this is how many NONEIGHBORS we're putting in
  FOREACH_FACE(f) {
    if (f < emptySpots) {
      neighborsArr[0][f] = NONEIGHBOR;
    }
  }

  for (int j = 0; j < 12; j++) {//quick shuffle method, random enough for our needs
    byte swapA = rand(5);
    byte swapB = rand(5);
    byte temp = neighborsArr[0][swapA];
    neighborsArr[0][swapA] = neighborsArr[0][swapB];
    neighborsArr[0][swapB] = temp;
  }

  //place blinks in remainings open spots
  for (byte j = 0; j < 6 - emptySpots; j++) {
    addBlink(0, 0);
  }

  byte remainingBlinks = 6 - piecesPlaced;
  byte lastRingBlinkIndex = piecesPlaced - 1;
  for (byte k = 0; k < remainingBlinks; k++) {
    addBlink(1, lastRingBlinkIndex);
  }
  colorConnections();
  //that does it!
  printAll();
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

void addBlink(byte minSearchIndex, byte maxSearchIndex) {
  //we begin by evaluating how many eligible spots remain
  byte eligiblePositions = 0;
  for (byte i = minSearchIndex; i <= maxSearchIndex; i++) {
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
  for (byte i = minSearchIndex; i <= maxSearchIndex; i++) {//same loop as above
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
  if (counterclockwiseNeighborInfo != UNDECLARED) { //there is a neighbor or NONEIGHBOR on the next counterclockwise face of the blink we placed onto
    //we tell the new blink it has a neighbor or NONEIGHBOR clockwise from our connection
    byte newNeighborConnectionFace = nextClockwise(getNeighborFace(faceIndex));
    neighborsArr[piecesPlaced - 1][newNeighborConnectionFace] = counterclockwiseNeighborInfo;

    if (counterclockwiseNeighborInfo != NONEIGHBOR) { //if it's an actual blink, it needs to know about the new connection
      neighborsArr[counterclockwiseNeighborInfo - 1][getNeighborFace(newNeighborConnectionFace)] = piecesPlaced;
    }
  }

  //now, the clockwise face (everything reversed, but identical)
  byte clockwiseNeighborInfo = neighborsArr[blinkIndex][nextClockwise(faceIndex)];
  if (clockwiseNeighborInfo != UNDECLARED) { //there is a neighbor or NONEIGHBOR on the next clockwise face of the blink we placed onto
    //we tell the new blink it has a neighbor or NONEIGHBOR counterclockwise from our connection
    byte newNeighborConnectionFace = nextCounterclockwise(getNeighborFace(faceIndex));
    neighborsArr[piecesPlaced - 1][newNeighborConnectionFace] = clockwiseNeighborInfo;

    if (clockwiseNeighborInfo != NONEIGHBOR) { //if it's an actual blink, it needs to know about the new connection
      neighborsArr[clockwiseNeighborInfo - 1][getNeighborFace(newNeighborConnectionFace)] = piecesPlaced;
    }
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

