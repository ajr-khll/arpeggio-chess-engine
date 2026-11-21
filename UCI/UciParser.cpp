#include "UciParser.h"
#include "../Moves.h"
#include "../TextureManager.h"
#include "../minimax.h"
#include "TimeState.h"
#include <SFML/System/Vector2.hpp>
#include <SFML/Window/Keyboard.hpp>
#include <sstream>
#include <string>
#include <vector>

std::vector<std::string> UciParser::commandParse(std::string line) {
  std::string buffer;
  std::vector<std::string> commands;
  for (char c : line) {
    if (c == ' ') {
      // skip whitespace
      if (buffer.empty()) {
        continue;
      } else {
        commands.push_back(buffer);
        buffer.clear();
      }
    } else {
      buffer += c;
    }
  }

  return commands;
}

// takes fen-generated board, applies moves
static GameState moveParser(std::vector<std::string> moves,
                            GameState startingState) {
  // convert moves to Move objects
  GameState state = startingState;
  for (std::string m : moves) {
    Move move;
    move.from.x = m[0] - 'a';
    move.from.y = '8' - m[1];
    move.to.x = m[2] - 'a';
    move.to.y = '8' - m[3];
    if (m.size() >= 5) {
      bool white = state.sideToMove == 0;
      switch (m[4]) {
      case 'q':
        move.promotion = white ? W_QUEEN : B_QUEEN;
        break;
      case 'r':
        move.promotion = white ? W_ROOK : B_ROOK;
        break;
      case 'b':
        move.promotion = white ? W_BISHOP : B_BISHOP;
        break;
      case 'n':
        move.promotion = white ? W_KNIGHT : B_KNIGHT;
        break;
      }
    }
    state = makeMove(state, move);
    positionHistory.push_back(state.hash);
  }
  return state;
}

// fen parsing
GameState UciParser::positionParse(std::vector<std::string> fenInput) {
  int BOARD[8][8];
  GameState state{};
  positionHistory.clear();

  // either fen or startpos
  if (fenInput[0] == "startpos") {
    startPos(state);
    positionHistory.push_back(state.hash);
    if (fenInput.size() > 2) {
      state = moveParser(
          std::vector<std::string>(fenInput.begin() + 2, fenInput.end()),
          state);
    }
    return state;
  }

  // grab board file info

  std::string boardInfo = fenInput[1];
  std::vector<int> rankbuffer;
  int rank = 0;
  int file = 0;
  for (char c : boardInfo) {
    if (c == 'R') {
      state.board[rank][file] = W_ROOK;
      file++;
    } else if (c == 'Q') {
      state.board[rank][file] = W_QUEEN;
      file++;
    } else if (c == 'K') {
      state.board[rank][file] = W_KING;
      state.kingPos[0] = {file, rank};
      file++;
    } else if (c == 'N') {
      state.board[rank][file] = W_KNIGHT;
      file++;
    } else if (c == 'B') {
      state.board[rank][file] = W_BISHOP;
      file++;
    } else if (c == 'P') {
      state.board[rank][file] = W_PAWN;
      file++;
    } else if (c == 'r') {
      state.board[rank][file] = B_ROOK;
      file++;
    } else if (c == 'q') {
      state.board[rank][file] = B_QUEEN;
      file++;
    } else if (c == 'k') {
      state.board[rank][file] = B_KING;
      state.kingPos[1] = {file, rank};
      file++;
    } else if (c == 'n') {
      state.board[rank][file] = B_KNIGHT;
      file++;
    } else if (c == 'b') {
      state.board[rank][file] = B_BISHOP;
      file++;
    } else if (c == 'p') {
      state.board[rank][file] = B_PAWN;
      file++;
    } else if (c == '/') {
      std::copy(rankbuffer.begin(), rankbuffer.end(), BOARD[rank]);
      rankbuffer.clear();
      rank++;
      file = 0;
    } else {
      // should be a number of empty spaces
      int empty = c - '0';
      for (int i = 0; i < empty; i++) {
        state.board[rank][file] = EMPTY;
        file++;
      };
    }
  }
  // next grab the turn
  int side;
  if (fenInput[2] == "w") {
    side = 0;
  } else {
    side = 1;
  }
  state.sideToMove = side;

  // next grab the castling
  std::string castling = fenInput[3];
  for (char c : castling) {
    if (c == 'K')
      state.castling |= W_KINGSIDE;
    else if (c == 'Q')
      state.castling |= W_QUEENSIDE;
    else if (c == 'k')
      state.castling |= B_KINGSIDE;
    else if (c == 'q')
      state.castling |= B_QUEENSIDE;
  }

  // All of the following parameters are yet to be implemented in game logic.
  // Likely, the GameState struct needs to be expanded.
  std::string enPassantSquare = fenInput[4];
  if (enPassantSquare != "-") {
    sf::Vector2i square = {enPassantSquare[0] - 'a', '8' - enPassantSquare[1]};
    if (canCaptureEnPassant(state.board, square, state.sideToMove))
      state.epSquare = square;
  }

  std::string halfmove = fenInput[5];
  state.halfmoveClock = std::stoi(halfmove);

  std::string fullmove = fenInput[6];

  state.setHash();
  positionHistory.push_back(state.hash);

  // parse remaining moves
  if (fenInput.size() >
      8) { // check if there exists a moves section in the input
    state = moveParser(
        std::vector<std::string>(fenInput.begin() + 8, fenInput.end()), state);
  }

  return state;
}

std::vector<std::string> UciParser::split(std::string line) {
  std::vector<std::string> out;
  std::istringstream iss(line);
  std::string buffer;

  while (iss >> buffer) {
    out.push_back(buffer);
  }

  return out;
}

TimeState UciParser::goParser(std::vector<std::string> line) {
  TimeState time;
  for (int i = 0; i < line.size(); i++) {
    if (line[i] == "wtime") {
      time.wtime = std::stoi(line[i + 1]);
      i++;
    } else if (line[i] == "btime") {
      time.btime = std::stoi(line[i + 1]);
      i++;
    } else if (line[i] == "winc") {
      time.winc = std::stoi(line[i + 1]);
      i++;
    } else if (line[i] == "binc") {
      time.binc = std::stoi(line[i + 1]);
      i++;
    } else if (line[i] == "movetime") {
      time.movetime = std::stoi(line[i + 1]);
      i++;
    }
  }
  return time;
}
