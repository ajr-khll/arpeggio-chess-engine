#include "../Moves.h"
#include "../TextureManager.h"
#include "../TranspositionTable.h"
#include "../minimax.h"
#include "nnue.h"
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <random>
#include <string>
#include <vector>

evaluatedMove rootSearch(GameState state, int maxDepth, Move lastBest);

const int WHITE_SIDE = 0;
const int BLACK_SIDE = 1;
const int RANDOM_OPENING_MOVES = 8;
const int MAXIMUM_GAME_LENGTH = 400;
const int MATE_SCORE_THRESHOLD = 900000;

struct RecordedPosition {
  std::string fen;
  int whiteScore;
};

char pieceToCharacter(int piece) {
  switch (piece) {
  case W_PAWN:
    return 'P';
  case W_ROOK:
    return 'R';
  case W_KNIGHT:
    return 'N';
  case W_BISHOP:
    return 'B';
  case W_QUEEN:
    return 'Q';
  case W_KING:
    return 'K';
  case B_PAWN:
    return 'p';
  case B_ROOK:
    return 'r';
  case B_KNIGHT:
    return 'n';
  case B_BISHOP:
    return 'b';
  case B_QUEEN:
    return 'q';
  case B_KING:
    return 'k';
  default:
    return '?';
  }
}

std::string squareName(sf::Vector2i square) {
  std::string name;
  name += static_cast<char>('a' + square.x);
  name += static_cast<char>('8' - square.y);
  return name;
}

std::string boardToFen(const GameState &state, int fullmoveNumber) {
  std::string fen;

  for (int y = 0; y < 8; y++) {
    int emptySquares = 0;
    for (int x = 0; x < 8; x++) {
      int piece = state.board[y][x];
      if (piece == EMPTY) {
        emptySquares++;
      } else {
        if (emptySquares > 0) {
          fen += std::to_string(emptySquares);
          emptySquares = 0;
        }
        fen += pieceToCharacter(piece);
      }
    }
    if (emptySquares > 0) {
      fen += std::to_string(emptySquares);
    }
    if (y < 7) {
      fen += '/';
    }
  }

  if (state.sideToMove == WHITE_SIDE) {
    fen += " w ";
  } else {
    fen += " b ";
  }

  std::string castling;
  if (state.castling & W_KINGSIDE) {
    castling += 'K';
  }
  if (state.castling & W_QUEENSIDE) {
    castling += 'Q';
  }
  if (state.castling & B_KINGSIDE) {
    castling += 'k';
  }
  if (state.castling & B_QUEENSIDE) {
    castling += 'q';
  }
  if (castling.empty()) {
    castling = "-";
  }
  fen += castling;

  if (state.epSquare == NO_SQUARE) {
    fen += " -";
  } else {
    fen += " " + squareName(state.epSquare);
  }

  fen += " " + std::to_string(state.halfmoveClock);
  fen += " " + std::to_string(fullmoveNumber);
  return fen;
}

bool isCapture(const GameState &state, const Move &move) {
  if (state.board[move.to.y][move.to.x] != EMPTY) {
    return true;
  }
  return isEnPassant(state, move);
}

bool isThreefoldRepetition(const GameState &state) {
  int occurrences = 0;
  for (uint64_t hash : positionHistory) {
    if (hash == state.hash) {
      occurrences++;
    }
  }
  return occurrences >= 3;
}

bool isInsufficientMaterial(const GameState &state) {
  int minorPieces = 0;
  int otherPieces = 0;

  for (int y = 0; y < 8; y++) {
    for (int x = 0; x < 8; x++) {
      int piece = state.board[y][x];
      if (piece == EMPTY || piece == W_KING || piece == B_KING) {
        continue;
      }
      if (piece == W_KNIGHT || piece == W_BISHOP || piece == B_KNIGHT ||
          piece == B_BISHOP) {
        minorPieces++;
      } else {
        otherPieces++;
      }
    }
  }

  return otherPieces == 0 && minorPieces <= 1;
}

evaluatedMove searchToDepth(const GameState &state, int depth) {
  stop_search = false;
  deadline = std::chrono::steady_clock::now() + std::chrono::hours(1);

  evaluatedMove bestMove;
  bestMove.from = sf::Vector2i(-1, -1);
  bestMove.to = sf::Vector2i(-1, -1);

  for (int currentDepth = 1; currentDepth <= depth; currentDepth++) {
    bestMove = rootSearch(state, currentDepth, bestMove);
  }
  return bestMove;
}

Move chooseRandomMove(const std::vector<Move> &moves, std::mt19937 &generator) {
  std::uniform_int_distribution<int> distribution(0, moves.size() - 1);
  return moves[distribution(generator)];
}

int playGame(std::mt19937 &generator, int depth, std::ofstream &output) {
  GameState state;
  startPos(state);
  positionHistory.clear();
  positionHistory.push_back(state.hash);
  zobrist_TT.clear();

  int fullmoveNumber = 1;

  for (int i = 0; i < RANDOM_OPENING_MOVES; i++) {
    std::vector<Move> moves = gatherMoves(state, state.sideToMove);
    if (moves.empty()) {
      return 0;
    }
    Move move = chooseRandomMove(moves, generator);
    if (state.sideToMove == BLACK_SIDE) {
      fullmoveNumber++;
    }
    state = makeMove(state, move);
    positionHistory.push_back(state.hash);
  }

  std::vector<RecordedPosition> recordedPositions;
  std::string result = "0.5";

  for (int ply = 0; ply < MAXIMUM_GAME_LENGTH; ply++) {
    std::vector<Move> moves = gatherMoves(state, state.sideToMove);
    bool inCheck = isInCheck(state, state.sideToMove);

    if (moves.empty()) {
      if (inCheck && state.sideToMove == WHITE_SIDE) {
        result = "0.0";
      } else if (inCheck && state.sideToMove == BLACK_SIDE) {
        result = "1.0";
      } else {
        result = "0.5";
      }
      break;
    }
    if (state.halfmoveClock >= 100 || isThreefoldRepetition(state) ||
        isInsufficientMaterial(state)) {
      result = "0.5";
      break;
    }

    evaluatedMove bestMove = searchToDepth(state, depth);
    int whiteScore = -bestMove.score;

    bool isMateScore = std::abs(whiteScore) >= MATE_SCORE_THRESHOLD;
    bool isQuiet = !inCheck && !isCapture(state, bestMove);
    if (isQuiet && !isMateScore) {
      RecordedPosition position;
      position.fen = boardToFen(state, fullmoveNumber);
      position.whiteScore = whiteScore;
      recordedPositions.push_back(position);
    }

    if (state.sideToMove == BLACK_SIDE) {
      fullmoveNumber++;
    }
    state = makeMove(state, bestMove);
    positionHistory.push_back(state.hash);
  }

  for (const RecordedPosition &position : recordedPositions) {
    output << position.fen << " | " << position.whiteScore << " | " << result
           << "\n";
  }
  output.flush();

  return recordedPositions.size();
}

int main(int argumentCount, char *argumentValues[]) {
  if (argumentCount != 6) {
    std::cerr << "usage: datagen <seed> <positions> <depth> <output file> "
                 "<network file>\n";
    return 1;
  }

  unsigned int seed = std::stoul(argumentValues[1]);
  long targetPositions = std::stol(argumentValues[2]);
  int depth = std::stoi(argumentValues[3]);
  std::string outputPath = argumentValues[4];
  std::string networkPath = argumentValues[5];

  if (!loadNetwork(networkPath.c_str())) {
    std::cerr << "could not load network " << networkPath << "\n";
    return 1;
  }

  std::ofstream output(outputPath);
  if (!output) {
    std::cerr << "could not open " << outputPath << "\n";
    return 1;
  }

  std::mt19937 generator(seed);
  std::chrono::steady_clock::time_point startTime =
      std::chrono::steady_clock::now();

  long positionsWritten = 0;
  long gamesPlayed = 0;

  while (positionsWritten < targetPositions) {
    positionsWritten += playGame(generator, depth, output);
    gamesPlayed++;

    std::chrono::steady_clock::duration elapsed =
        std::chrono::steady_clock::now() - startTime;
    double seconds = std::chrono::duration<double>(elapsed).count();
    double positionsPerSecond = positionsWritten / seconds;

    std::cerr << "[seed " << seed << "] games " << gamesPlayed
              << "  positions " << positionsWritten << " / " << targetPositions
              << "  (" << static_cast<int>(positionsPerSecond)
              << " positions/s)\n";
  }

  return 0;
}
