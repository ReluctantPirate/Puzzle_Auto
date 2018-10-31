#include "Serial.h"
ServicePortSerial sp;

////COMMUNICATION VARIABLES////
enum gameModes {SETUPAUTO, PACKETREADY, PACKETSENDING, PACKETLISTENING, PACKETRECEIVED, GAMEAUTO, TOMANUAL, SETUPMANUAL, LOCKING, GAMEMANUAL, TOAUTO};
byte gameMode = SETUPAUTO;
byte packetStates[6] = {PACKETREADY, PACKETREADY, PACKETREADY, PACKETREADY, PACKETREADY, PACKETREADY};

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

Timer packetTimer;
#define TIMEOUT_DURATION 500
byte sendFailures = 0;

////GAME VARIABLES////
Color autoColors[5] = {OFF, RED, YELLOW, BLUE, WHITE};
Color manualColors[5] = {OFF, ORANGE, GREEN, MAGENTA, WHITE};
byte faceColors[6] = {0, 0, 0, 0, 0, 0};
byte faceBrightness[6] = {0, 0, 0, 0, 0, 0};
byte dimVal = 64;


void setup() {
  sp.begin();
}

void loop() {
  switch (gameMode) {
    case SETUPAUTO:
      setupAutoLoop();
      assembleDisplay();
      break;
    case PACKETREADY:
      communicationMasterLoop();
      communicationDisplay();
      break;
    case PACKETSENDING:
      communicationMasterLoop();
      communicationDisplay();
      break;
    case PACKETLISTENING:
      communicationReceiverLoop();
      communicationDisplay();
      break;
    case PACKETRECEIVED:
      communicationReceiverLoop();
      communicationDisplay();
      break;
    case GAMEAUTO:
      gameLoop();
      gameDisplay();
      break;
    case TOMANUAL:
      break;
    case SETUPMANUAL:
      assembleDisplay();
      break;
    case LOCKING:
      break;
    case GAMEMANUAL:
      gameLoop();
      gameDisplay();
      break;
    case TOAUTO:
      break;
  }

  buttonDoubleClicked();
}

void setupAutoLoop() {
  //all we do here is wait until we have 5 neighbors
  byte numNeighbors = 0;
  FOREACH_FACE(f) {
    if (!isValueReceivedOnFaceExpired(f)) { //neighbor!
      byte neighborData = getLastValueReceivedOnFace(f);
      if (getGameMode(neighborData) == SETUPAUTO) { //this neighbor is ready for puzzling
        numNeighbors++;
        faceBrightness[f] = 255;
      } else {
        faceBrightness[f] = dimVal;
      }
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
    gameMode = PACKETREADY;
    canBeginAlgorithm = false;
    isMaster = true;
  }

  FOREACH_FACE(f) {//here we listen for other blinks to turn us into receiver blinks
    if (!isValueReceivedOnFaceExpired(f)) {//neighbor here
      byte neighborData = getLastValueReceivedOnFace(f);
      if (getGameMode(neighborData) == PACKETREADY) { //this neighbor will send a puzzle soon
        gameMode = PACKETLISTENING;
        masterFace = f;//will only listen for packets on this face
      }
    }
  }

  //the last thing we do is communicate our state. All 0s for now, changes later
  setValueSentOnAllFaces(gameMode << 2);
}

void assembleDisplay() {
  if (sparkleTimer.isExpired() && canBeginAlgorithm) {
    FOREACH_FACE(f) {
      setColorOnFace(autoColors[rand(3) + 1], f);
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

      //look for neighbors turning us back to setup
      if (gameMode == GAMEAUTO) {
        if (getGameMode(neighborData) == SETUPAUTO) {
          gameMode = SETUPAUTO;
        }
      } else if (gameMode == GAMEMANUAL) {
        if (getGameMode(neighborData) == SETUPMANUAL) {
          gameMode = SETUPMANUAL;
        }
      }

    } else {//no neighbor
      faceBrightness[f] = dimVal;
    }
  }

  //if we are double clicked, we go to assemble mode
  if (buttonDoubleClicked()) {
    if (gameMode == GAMEAUTO) {
      gameMode = SETUPAUTO;
    } else if (gameMode == GAMEMANUAL) {
      gameMode = SETUPMANUAL;
    }
  }

  //set communications
  FOREACH_FACE(f) {//[COMM][MODE][----][----][COLR][COLR]
    byte sendData = (gameMode << 2) + (faceColors[f]);
    setValueSentOnFace(sendData, f);
  }
}

void gameDisplay() {
  Color displayColor;
  FOREACH_FACE(f) {
    if (gameMode = GAMEAUTO) {
      displayColor = autoColors[faceColors[f]];
    } else if (gameMode == GAMEMANUAL) {
      displayColor = manualColors[faceColors[f]];
    }
    byte displayBrightness = faceBrightness[f];
    setColorOnFace(dim(displayColor, displayBrightness), f);
  }
}

/////////////////////////////////
//BEGIN COMMUNICATION PROCEDURE//
/////////////////////////////////

void communicationMasterLoop() {

  if (gameMode == PACKETREADY) {//here we wait to send packets to listening neighbors

    byte neighborsListening = 0;
    byte emptyFace;
    FOREACH_FACE(f) {
      if (!isValueReceivedOnFaceExpired(f)) {
        byte neighborData = getLastValueReceivedOnFace(f);
        if (getGameMode(neighborData) == PACKETLISTENING) {//this neighbor is ready to get a packet.
          neighborsListening++;
        }
      } else {
        emptyFace = f;
      }
    }

    if (neighborsListening == 5) {
      gameMode = PACKETSENDING;
      sendPuzzlePackets(emptyFace);
      packetTimer.set(TIMEOUT_DURATION);
      sendFailures = 0;
    }

  } else if (gameMode == PACKETSENDING) {//here we listen for neighbors who have received packets

    byte neighborsReceived = 0;
    byte emptyFace;
    FOREACH_FACE(f) {
      if (!isValueReceivedOnFaceExpired(f)) {
        byte neighborData = getLastValueReceivedOnFace(f);
        if (getGameMode(neighborData) == PACKETRECEIVED) {//this neighbor is ready to play
          neighborsReceived++;
        }
      } else {
        emptyFace = f;
      }
    }

    if (neighborsReceived == 5) { //hooray, we did it!
      gameMode = GAMEAUTO;
      return;
    }

    if (gameMode != GAMEAUTO && packetTimer.isExpired()) { //so we've gone a long time without this working out
      sendPuzzlePackets(emptyFace);
      packetTimer.set(TIMEOUT_DURATION);
      sendFailures ++;
    }
  }

  byte sendData = (gameMode << 2);
  setValueSentOnAllFaces(sendData);
}

void sendPuzzlePackets(byte blankFace) {
  //declare packets
  byte packet0[6];
  byte packet1[6];
  byte packet2[6];
  byte packet3[6];
  byte packet4[6];
  byte packet5[6];

  //fill packets
  FOREACH_FACE(f) {
    packet0[f] = colorsArr[0][f];
    packet1[f] = colorsArr[1][f];
    packet2[f] = colorsArr[2][f];
    packet3[f] = colorsArr[3][f];
    packet4[f] = colorsArr[4][f];
    packet5[f] = colorsArr[5][f];
  }

  //SEND PACKETS
  sendPacketOnFace(0, (byte *) packet0, 6);
  sendPacketOnFace(1, (byte *) packet1, 6);
  sendPacketOnFace(2, (byte *) packet2, 6);
  sendPacketOnFace(3, (byte *) packet3, 6);
  sendPacketOnFace(4, (byte *) packet4, 6);
  sendPacketOnFace(5, (byte *) packet5, 6);

  //assign self the correct info
  FOREACH_FACE(f) {
    faceColors[f] = colorsArr[blankFace][f];
  }
}

void communicationReceiverLoop() {
  if (gameMode == PACKETLISTENING) {

    //listen for a packet on master face
    if (isPacketReadyOnFace(masterFace)) {//is there a packet?
      if (getPacketLengthOnFace(masterFace) == 6) {//is it the right length?
        byte *data = (byte *) getPacketDataOnFace(masterFace);//grab the data
        //fill our array with this data
        FOREACH_FACE(f) {
          faceColors[f] = data[f];
        }
        //let them know we heard them
        gameMode = PACKETRECEIVED;
      }
    }

    //also listen for the master face to suddenly change back to setup, which is bad
    if (getGameMode(getLastValueReceivedOnFace(masterFace)) == SETUPAUTO) { //looks like we are reverting
      gameMode = SETUPAUTO;
    }

  } else if (gameMode == PACKETRECEIVED) {
    //wait for the master blink to transition to game
    if (getGameMode(getLastValueReceivedOnFace(masterFace)) == GAMEAUTO) { //time to play!
      gameMode = GAMEAUTO;
    }
  }

  byte sendData = (gameMode << 2);
  setValueSentOnAllFaces(sendData);
}

byte getGameMode(byte data) {
  return (data >> 2);//1st, 2nd, 3rd, and 4th bits
}

byte getColorInfo(byte data) {
  return (data & 3);//returns the 5th and 6th bits
}

void communicationDisplay() {
  switch (gameMode) {
    case PACKETREADY:
      setColor(ORANGE);
      break;
    case PACKETSENDING:
      setColor(YELLOW);
      break;
    case PACKETLISTENING:
      setColor(BLUE);
      break;
    case PACKETRECEIVED:
      setColor(GREEN);
      break;
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

