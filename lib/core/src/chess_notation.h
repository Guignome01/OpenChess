#ifndef CORE_CHESS_NOTATION_H
#define CORE_CHESS_NOTATION_H

#include <string>

#include "chess_bitboard.h"
#include "types.h"

struct MoveEntry;  // forward declaration

// Move notation conversion — transforms between internal array coordinates
// and standard chess notation formats.
//
// Supports three notation formats (see MoveFormat in types.h):
//   - Coordinate: "e2e4", "e7e8q"  — origin + destination squares, optional
//     promotion suffix.  This is the notation used by the UCI protocol for
//     communicating moves between a GUI and a chess engine.
//     See: https://www.chessprogramming.org/UCI
//   - SAN (Standard Algebraic Notation): "e4", "Nxf3", "O-O", "e8=Q+"
//     Compact, human-readable.  Piece letter + disambiguation + destination.
//   - LAN (Long Algebraic Notation): "e2-e4", "Ng1xf3", "O-O", "e7-e8=Q+"
//     Like SAN but always includes the origin square.
//
// Design: all functions are pure — board state and position are passed in
// as parameters (consistent with the ChessRules stateless pattern).
//
// Output functions produce notation without check/checkmate suffixes (+ / #).
// The caller (ChessGame::getHistory) appends those after applying the move
// to a temporary board and testing for check/checkmate.

namespace ChessNotation {

// ---------------------------------------------------------------------------
// Output — array coordinates / MoveEntry → string
// ---------------------------------------------------------------------------

// Coordinate notation: "e2e4", "e7e8q".
std::string toCoordinate(int fromRow, int fromCol, int toRow, int toCol, char promotion = ' ');

// Long Algebraic Notation: "e2-e4", "Ng1xf3", "O-O", "e7-e8=Q".
// Requires the MoveEntry for piece/capture/castling/promotion metadata.
std::string toLAN(const MoveEntry& move);

// Standard Algebraic Notation: "e4", "Nxf3", "O-O", "e8=Q".
// Requires the board state *before* the move for disambiguation.
std::string toSAN(const ChessBitboard::BitboardSet& bb, const Piece mailbox[],
                  const PositionState& state, const MoveEntry& move);

// ---------------------------------------------------------------------------
// Input — string → array coordinates
// ---------------------------------------------------------------------------

// Parse coordinate notation: "e2e4", "e7e8q".
bool parseCoordinate(const std::string& move,
                     int& fromRow, int& fromCol,
                     int& toRow, int& toCol,
                     char& promotion);

// Parse Long Algebraic Notation: "e2-e4", "Ng1xf3", "e7-e8=Q", "O-O".
// Strips piece prefix, separators, promotion marker, and check suffixes
// then delegates to parseCoordinate.
bool parseLAN(const std::string& move,
              int& fromRow, int& fromCol,
              int& toRow, int& toCol,
              char& promotion);

// Parse Standard Algebraic Notation: "e4", "Nxf3", "O-O", "e8=Q".
// Requires board state and the side to move to resolve the
// origin square via move generation.
bool parseSAN(const ChessBitboard::BitboardSet& bb, const Piece mailbox[],
              const PositionState& state,
              Color currentTurn, const std::string& san,
              int& fromRow, int& fromCol,
              int& toRow, int& toCol,
              char& promotion);

// Auto-detect format and parse.  Tries coordinate first (fast regex match),
// then LAN, then SAN.  Returns false if no parser succeeds.
bool parseMove(const ChessBitboard::BitboardSet& bb, const Piece mailbox[],
               const PositionState& state,
               Color currentTurn, const std::string& move,
               int& fromRow, int& fromCol,
               int& toRow, int& toCol,
               char& promotion);

}  // namespace ChessNotation

#endif  // CORE_CHESS_NOTATION_H
