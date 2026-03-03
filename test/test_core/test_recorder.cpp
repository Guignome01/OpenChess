#include <unity.h>

#include "../test_helpers.h"
#include <codec.h>
#include <game_controller.h>
#include <game_observer.h>
#include <game_storage.h>
#include <logger.h>
#include <recorder.h>
#include <types.h>

#include <cstring>
#include <string>
#include <vector>

// ---------------------------------------------------------------------------
// MockLogger — captures log messages for verification
// ---------------------------------------------------------------------------

class MockLogger : public ILogger {
 public:
  std::vector<std::string> infos;
  std::vector<std::string> errors;
  void info(const char* msg) override { infos.push_back(msg); }
  void error(const char* msg) override { errors.push_back(msg); }
};

// ---------------------------------------------------------------------------
// MockGameStorage — in-memory storage for testing
// ---------------------------------------------------------------------------

class MockGameStorage : public IGameStorage {
 public:
  bool initialized = false;
  bool gameActive = false;
  bool gameFinalized = false;
  bool gameDiscarded = false;
  GameHeader storedHeader;
  std::vector<uint8_t> moveData;
  std::vector<std::pair<size_t, std::string>> fenEntries;  // (offset, fen)
  int headerUpdateCount = 0;
  int deleteCount = 0;

  void initialize() override {
    initialized = true;
  }
  void beginGame(const GameHeader& header) override {
    storedHeader = header;
    gameActive = true;
    gameFinalized = false;
    gameDiscarded = false;
    moveData.clear();
    fenEntries.clear();
    headerUpdateCount = 0;
  }
  void appendMoveData(const uint8_t* data, size_t len) override {
    for (size_t i = 0; i < len; i++)
      moveData.push_back(data[i]);
  }
  void updateHeader(const GameHeader& header) override {
    storedHeader = header;
    headerUpdateCount++;
  }
  size_t appendFenEntry(const std::string& fen) override {
    size_t offset = moveData.size();
    fenEntries.push_back({offset, fen});
    return offset;
  }
  void finalizeGame(const GameHeader& header) override {
    storedHeader = header;
    gameFinalized = true;
    gameActive = false;
  }
  void discardGame() override {
    gameDiscarded = true;
    gameActive = false;
  }
  bool hasActiveGame() override { return gameActive; }
  bool readHeader(GameHeader& header) override {
    if (!gameActive && !gameFinalized) return false;
    header = storedHeader;
    return true;
  }
  bool readMoveData(std::vector<uint8_t>& out) override {
    out = moveData;
    return true;
  }
  bool readFenAt(size_t offset, std::string& fen) override {
    for (auto& entry : fenEntries) {
      if (entry.first == offset) {
        fen = entry.second;
        return true;
      }
    }
    return false;
  }
  bool deleteGame(int id) override {
    deleteCount++;
    return true;
  }
  void enforceStorageLimits() override {}
};

// ---------------------------------------------------------------------------
// MockGameObserver — captures notifications
// ---------------------------------------------------------------------------

class MockGameObserver : public IGameObserver {
 public:
  int callCount = 0;
  std::string lastFen;
  float lastEval = 0.0f;

  void onBoardStateChanged(const std::string& fen, float evaluation) override {
    callCount++;
    lastFen = fen;
    lastEval = evaluation;
  }
};

// ---------------------------------------------------------------------------
// GameRecorder tests
// ---------------------------------------------------------------------------

static MockLogger logger;
static MockGameStorage storage;
static GameRecorder recorder(&storage, &logger);

static void setupRecorder() {
  logger = MockLogger();
  storage = MockGameStorage();
  recorder = GameRecorder(&storage, &logger);
}

void test_recorder_start_recording(void) {
  setupRecorder();
  recorder.startRecording(GameMode::CHESS_MOVES, '?', 0);
  TEST_ASSERT_TRUE(recorder.isRecording());
  TEST_ASSERT_TRUE(storage.gameActive);
  TEST_ASSERT_ENUM_EQ(GameMode::CHESS_MOVES, storage.storedHeader.mode);
}

void test_recorder_record_move(void) {
  setupRecorder();
  recorder.startRecording(GameMode::CHESS_MOVES, '?', 0);
  recorder.recordMove(6, 4, 4, 4, ' ');  // e2e4
  TEST_ASSERT_EQUAL(2, (int)storage.moveData.size());  // 2-byte encoded move
  // Header not flushed yet (turn-based: every 2 half-moves)
  TEST_ASSERT_EQUAL(0, storage.headerUpdateCount);
}

void test_recorder_record_fen(void) {
  setupRecorder();
  recorder.startRecording(GameMode::CHESS_MOVES, '?', 0);
  std::string fen = "rnbqkbnr/pppppppp/8/8/4P3/8/PPPP1PPP/RNBQKBNR b KQkq e3 0 1";
  recorder.recordFen(fen);
  TEST_ASSERT_EQUAL(1, (int)storage.fenEntries.size());
  TEST_ASSERT_EQUAL_STRING(fen.c_str(), storage.fenEntries[0].second.c_str());
  TEST_ASSERT_EQUAL_UINT8(1, storage.storedHeader.fenEntryCnt);
}

void test_recorder_finish_recording(void) {
  setupRecorder();
  recorder.startRecording(GameMode::CHESS_MOVES, '?', 0);
  recorder.recordMove(6, 4, 4, 4, ' ');
  recorder.finishRecording(GameResult::CHECKMATE, 'w');
  TEST_ASSERT_FALSE(recorder.isRecording());
  TEST_ASSERT_TRUE(storage.gameFinalized);
  TEST_ASSERT_ENUM_EQ(GameResult::CHECKMATE, storage.storedHeader.result);
  TEST_ASSERT_EQUAL_CHAR('w', storage.storedHeader.winnerColor);
}

void test_recorder_discard_recording(void) {
  setupRecorder();
  recorder.startRecording(GameMode::CHESS_MOVES, '?', 0);
  recorder.discardRecording();
  TEST_ASSERT_FALSE(recorder.isRecording());
  TEST_ASSERT_TRUE(storage.gameDiscarded);
}

void test_recorder_not_recording_noop(void) {
  setupRecorder();
  // recordMove and recordFen should be no-ops when not recording
  recorder.recordMove(6, 4, 4, 4, ' ');
  recorder.recordFen("some fen");
  TEST_ASSERT_EQUAL(0, (int)storage.moveData.size());
  TEST_ASSERT_EQUAL(0, (int)storage.fenEntries.size());
}

void test_recorder_has_active_game(void) {
  setupRecorder();
  TEST_ASSERT_FALSE(recorder.hasActiveGame());
  recorder.startRecording(GameMode::BOT, 'w', 5);
  TEST_ASSERT_TRUE(recorder.hasActiveGame());
}

void test_recorder_get_active_game_info(void) {
  setupRecorder();
  storage.gameActive = true;
  storage.storedHeader.version = FORMAT_VERSION;
  storage.storedHeader.mode = GameMode::BOT;
  storage.storedHeader.playerColor = 'b';
  storage.storedHeader.botDepth = 3;

  GameRecorder recWithLive(&storage, &logger);
  GameMode mode;
  uint8_t color, depth;
  TEST_ASSERT_TRUE(recWithLive.getActiveGameInfo(mode, color, depth));
  TEST_ASSERT_ENUM_EQ(GameMode::BOT, mode);
  TEST_ASSERT_EQUAL_CHAR('b', (char)color);
  TEST_ASSERT_EQUAL_UINT8(3, depth);
}

void test_recorder_replay_into_board(void) {
  setupRecorder();
  recorder.startRecording(GameMode::CHESS_MOVES, '?', 0);

  // Record initial FEN
  std::string initialFen = "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1";
  recorder.recordFen(initialFen);
  // Record e2e4
  recorder.recordMove(6, 4, 4, 4, ' ');
  // Record d7d5
  recorder.recordMove(1, 3, 3, 3, ' ');

  // Now replay into a fresh ChessBoard
  ChessBoard board;
  board.newGame();
  bool ok = recorder.replayInto(board);
  TEST_ASSERT_TRUE(ok);

  // Board should have e4 and d5 played
  TEST_ASSERT_EQUAL_CHAR('P', board.getSquare(4, 4));
  TEST_ASSERT_EQUAL_CHAR('p', board.getSquare(3, 3));
  TEST_ASSERT_EQUAL_CHAR('w', board.currentTurn());
}

// ---------------------------------------------------------------------------
// GameController tests
// ---------------------------------------------------------------------------

static MockGameObserver observer;
static GameController* ctrl = nullptr;

static void setupController() {
  logger = MockLogger();
  storage = MockGameStorage();
  observer = MockGameObserver();
  recorder = GameRecorder(&storage, &logger);
  delete ctrl;
  ctrl = new GameController(&recorder, &observer);
}

static void teardownController() {
  delete ctrl;
  ctrl = nullptr;
}

void test_controller_new_game(void) {
  setupController();
  ctrl->newGame();
  TEST_ASSERT_EQUAL_CHAR('w', ctrl->currentTurn());
  TEST_ASSERT_FALSE(ctrl->isGameOver());
  // Observer should have been notified
  TEST_ASSERT_TRUE(observer.callCount > 0);
  teardownController();
}

void test_controller_make_move(void) {
  setupController();
  ctrl->newGame();
  ctrl->startRecording(GameMode::CHESS_MOVES);
  observer.callCount = 0;

  MoveResult r = ctrl->makeMove(6, 4, 4, 4);  // e2e4
  // startRecording auto-records initial FEN (2-byte marker) + move (2 bytes) = 4
  TEST_ASSERT_EQUAL(4, (int)storage.moveData.size());
  TEST_ASSERT_EQUAL_CHAR('b', ctrl->currentTurn());

  // Observer notified
  TEST_ASSERT_TRUE(observer.callCount > 0);

  // FEN marker flushed the header (moveCount=1), but the single move hasn't
  // triggered a turn-based flush yet (flushes every 2 half-moves)
  TEST_ASSERT_EQUAL_UINT16(1, storage.storedHeader.moveCount);
  teardownController();
}

void test_controller_make_move_auto_finish(void) {
  setupController();
  // Set up a scholars mate position
  ctrl->newGame();
  ctrl->startRecording(GameMode::CHESS_MOVES);

  // Scholar's mate sequence
  ctrl->makeMove(6, 4, 4, 4);  // e2-e4
  ctrl->makeMove(1, 4, 3, 4);  // e7-e5
  ctrl->makeMove(7, 5, 4, 2);  // Bf1-c4
  ctrl->makeMove(1, 0, 2, 0);  // a7-a6
  ctrl->makeMove(7, 3, 3, 7);  // Qd1-h5
  ctrl->makeMove(1, 1, 2, 1);  // b7-b6
  MoveResult r = ctrl->makeMove(3, 7, 1, 5);  // Qh5xf7 checkmate

  TEST_ASSERT_ENUM_EQ(GameResult::CHECKMATE, r.gameResult);
  TEST_ASSERT_TRUE(ctrl->isGameOver());

  // Recording should have been auto-finalized
  TEST_ASSERT_TRUE(storage.gameFinalized);
  TEST_ASSERT_ENUM_EQ(GameResult::CHECKMATE, storage.storedHeader.result);
  teardownController();
}

void test_controller_end_game(void) {
  setupController();
  ctrl->newGame();
  ctrl->startRecording(GameMode::CHESS_MOVES);

  ctrl->endGame(GameResult::RESIGNATION, 'b');
  TEST_ASSERT_TRUE(ctrl->isGameOver());

  // Recording finalized
  TEST_ASSERT_TRUE(storage.gameFinalized);
  TEST_ASSERT_ENUM_EQ(GameResult::RESIGNATION, storage.storedHeader.result);
  TEST_ASSERT_EQUAL_CHAR('b', storage.storedHeader.winnerColor);
  teardownController();
}

void test_controller_load_fen(void) {
  setupController();
  ctrl->newGame();
  ctrl->startRecording(GameMode::CHESS_MOVES);
  observer.callCount = 0;

  std::string fen = "rnbqkbnr/pppppppp/8/8/4P3/8/PPPP1PPP/RNBQKBNR b KQkq e3 0 1";
  bool ok = ctrl->loadFEN(fen);
  TEST_ASSERT_TRUE(ok);
  TEST_ASSERT_EQUAL_CHAR('b', ctrl->currentTurn());

  // Observer notified
  TEST_ASSERT_TRUE(observer.callCount > 0);

  // FEN recorded (1 from startRecording + 1 from loadFEN)
  TEST_ASSERT_EQUAL(2, (int)storage.fenEntries.size());
  teardownController();
}

void test_controller_no_recorder(void) {
  // Controller with nullptr recorder — mutations still work, no recording
  observer = MockGameObserver();
  GameController noRec(nullptr, &observer);
  noRec.newGame();
  MoveResult r = noRec.makeMove(6, 4, 4, 4);
  TEST_ASSERT_TRUE(r.valid);
  TEST_ASSERT_TRUE(observer.callCount > 0);
}

void test_controller_no_observer(void) {
  // Controller with nullptr observer — mutations still work, no notification
  logger = MockLogger();
  storage = MockGameStorage();
  GameRecorder rec(&storage, &logger);
  GameController noObs(&rec, nullptr);
  noObs.newGame();
  noObs.startRecording(GameMode::CHESS_MOVES);
  MoveResult r = noObs.makeMove(6, 4, 4, 4);
  TEST_ASSERT_TRUE(r.valid);
  // startRecording FEN marker (2 bytes) + move (2 bytes) = 4
  TEST_ASSERT_EQUAL(4, (int)storage.moveData.size());
}

void test_controller_discard_recording(void) {
  setupController();
  ctrl->newGame();
  ctrl->startRecording(GameMode::CHESS_MOVES);
  ctrl->makeMove(6, 4, 4, 4);

  ctrl->discardRecording();
  TEST_ASSERT_TRUE(storage.gameDiscarded);
  teardownController();
}

void test_controller_pass_throughs(void) {
  setupController();
  ctrl->newGame();

  // Verify pass-through methods return correct values
  TEST_ASSERT_EQUAL_CHAR('R', ctrl->getSquare(7, 0));
  TEST_ASSERT_EQUAL_CHAR('w', ctrl->currentTurn());
  TEST_ASSERT_FALSE(ctrl->isGameOver());
  TEST_ASSERT_ENUM_EQ(GameResult::IN_PROGRESS, ctrl->gameResult());

  std::string fen = ctrl->getFen();
  TEST_ASSERT_EQUAL_STRING(
      "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1",
      fen.c_str());
  teardownController();
}

// ---------------------------------------------------------------------------
// Turn-based header flush tests
// ---------------------------------------------------------------------------

void test_recorder_turn_based_header_flush(void) {
  setupRecorder();
  recorder.startRecording(GameMode::CHESS_MOVES, '?', 0);
  int initial = storage.headerUpdateCount;

  recorder.recordMove(6, 4, 4, 4, ' ');  // e2e4 (1st half-move)
  TEST_ASSERT_EQUAL(initial, storage.headerUpdateCount);  // Not flushed

  recorder.recordMove(1, 4, 3, 4, ' ');  // e7e5 (2nd half-move)
  TEST_ASSERT_EQUAL(initial + 1, storage.headerUpdateCount);  // Flushed!
  TEST_ASSERT_EQUAL_UINT16(2, storage.storedHeader.moveCount);
}

void test_recorder_fen_always_flushes_header(void) {
  setupRecorder();
  recorder.startRecording(GameMode::CHESS_MOVES, '?', 0);
  int initial = storage.headerUpdateCount;

  recorder.recordMove(6, 4, 4, 4, ' ');  // 1 move, no flush
  TEST_ASSERT_EQUAL(initial, storage.headerUpdateCount);

  recorder.recordFen("rnbqkbnr/pppppppp/8/8/4P3/8/PPPP1PPP/RNBQKBNR b KQkq e3 0 1");
  TEST_ASSERT_EQUAL(initial + 1, storage.headerUpdateCount);  // FEN always flushes
}

void test_recorder_replay_rejects_invalid_move(void) {
  setupRecorder();
  recorder.startRecording(GameMode::CHESS_MOVES, '?', 0);

  // Record initial FEN
  std::string initialFen = "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1";
  recorder.recordFen(initialFen);

  // Record valid e2e4
  recorder.recordMove(6, 4, 4, 4, ' ');

  // Inject an illegal move directly into storage (d2d4 — but it's black's turn)
  uint16_t illegal = ChessCodec::encodeMove(6, 3, 4, 3, ' ');
  storage.moveData.push_back(illegal & 0xFF);
  storage.moveData.push_back((illegal >> 8) & 0xFF);

  ChessBoard board;
  board.newGame();
  bool ok = recorder.replayInto(board);
  TEST_ASSERT_FALSE(ok);  // Replay should fail on the illegal move
}

void test_recorder_start_recording_lichess_mode(void) {
  setupRecorder();
  recorder.startRecording(GameMode::LICHESS, 'w', 0);
  TEST_ASSERT_TRUE(recorder.isRecording());
  TEST_ASSERT_ENUM_EQ(GameMode::LICHESS, storage.storedHeader.mode);
}

void test_recorder_start_recording_while_active(void) {
  setupRecorder();
  recorder.startRecording(GameMode::CHESS_MOVES, '?', 0);
  recorder.recordMove(6, 4, 4, 4, ' ');  // record a move
  TEST_ASSERT_TRUE(recorder.isRecording());

  // Start again — previous game should be discarded
  recorder.startRecording(GameMode::BOT, 'w', 5);
  TEST_ASSERT_TRUE(recorder.isRecording());
  TEST_ASSERT_ENUM_EQ(GameMode::BOT, storage.storedHeader.mode);
  TEST_ASSERT_EQUAL_UINT8(5, storage.storedHeader.botDepth);
  // Move data should be cleared (new game)
  TEST_ASSERT_EQUAL(0, (int)storage.moveData.size());
}

void test_recorder_finish_not_recording(void) {
  setupRecorder();
  // finishRecording when not recording should be a safe no-op
  recorder.finishRecording(GameResult::CHECKMATE, 'w');
  TEST_ASSERT_FALSE(recorder.isRecording());
  TEST_ASSERT_FALSE(storage.gameFinalized);
}

void test_recorder_discard_not_recording(void) {
  setupRecorder();
  // discardRecording when not recording should not crash
  recorder.discardRecording();
  TEST_ASSERT_FALSE(recorder.isRecording());
}

void test_recorder_replay_wrong_version(void) {
  setupRecorder();
  // Manually set up storage with a bad version header
  storage.gameActive = true;
  storage.storedHeader.version = 99;
  storage.storedHeader.fenEntryCnt = 1;

  ChessBoard board;
  board.newGame();
  bool ok = recorder.replayInto(board);
  TEST_ASSERT_FALSE(ok);
}

void test_recorder_replay_no_fen_entry(void) {
  setupRecorder();
  // Header has version=1 but fenEntryCnt=0
  storage.gameActive = true;
  storage.storedHeader.version = FORMAT_VERSION;
  storage.storedHeader.fenEntryCnt = 0;

  ChessBoard board;
  board.newGame();
  bool ok = recorder.replayInto(board);
  TEST_ASSERT_FALSE(ok);
}

void test_recorder_replay_null_storage(void) {
  // Recorder with null storage — replayInto should return false
  GameRecorder nullRec(nullptr, &logger);
  ChessBoard board;
  board.newGame();
  bool ok = nullRec.replayInto(board);
  TEST_ASSERT_FALSE(ok);
}

void test_recorder_replay_empty_after_fen(void) {
  setupRecorder();
  recorder.startRecording(GameMode::CHESS_MOVES, '?', 0);

  // Record only an initial FEN and no moves
  std::string initialFen = "rnbqkbnr/pppppppp/8/8/4P3/8/PPPP1PPP/RNBQKBNR b KQkq e3 0 1";
  recorder.recordFen(initialFen);

  // Replay — should load FEN with no moves to replay
  ChessBoard board;
  board.newGame();
  bool ok = recorder.replayInto(board);
  TEST_ASSERT_TRUE(ok);
  TEST_ASSERT_EQUAL_CHAR('b', board.currentTurn());
  TEST_ASSERT_EQUAL_CHAR('P', board.getSquare(4, 4));  // e4 has white pawn
}

void test_recorder_replay_with_promotion(void) {
  setupRecorder();
  recorder.startRecording(GameMode::CHESS_MOVES, '?', 0);

  // Record a position where white can promote
  std::string fen = "8/4P3/8/8/8/8/8/4K2k w - - 0 1";
  recorder.recordFen(fen);
  recorder.recordMove(1, 4, 0, 4, 'q');  // e7-e8=Q

  // Replay and verify promotion
  ChessBoard board;
  board.newGame();
  bool ok = recorder.replayInto(board);
  TEST_ASSERT_TRUE(ok);
  TEST_ASSERT_EQUAL_CHAR('Q', board.getSquare(0, 4));  // Queen on e8
}

// ---------------------------------------------------------------------------
// GameController: endGame guard & startNewGame
// ---------------------------------------------------------------------------

void test_controller_end_game_idempotent(void) {
  setupController();
  ctrl->newGame();
  ctrl->startRecording(GameMode::CHESS_MOVES);

  ctrl->endGame(GameResult::RESIGNATION, 'b');
  TEST_ASSERT_TRUE(ctrl->isGameOver());

  observer.callCount = 0;

  // Second endGame should be a no-op (guard prevents overwrite)
  ctrl->endGame(GameResult::TIMEOUT, 'w');
  TEST_ASSERT_EQUAL(0, observer.callCount);
  TEST_ASSERT_ENUM_EQ(GameResult::RESIGNATION, ctrl->gameResult());
  TEST_ASSERT_EQUAL_CHAR('b', ctrl->winnerColor());
  teardownController();
}

void test_controller_start_new_game(void) {
  setupController();
  ctrl->startNewGame(GameMode::BOT, 'w', 5);

  TEST_ASSERT_EQUAL_CHAR('w', ctrl->currentTurn());
  TEST_ASSERT_FALSE(ctrl->isGameOver());

  // Should have both started recording and initialized board
  TEST_ASSERT_TRUE(storage.gameActive);
  TEST_ASSERT_TRUE(recorder.isRecording());
  TEST_ASSERT_ENUM_EQ(GameMode::BOT, storage.storedHeader.mode);
  TEST_ASSERT_EQUAL_CHAR('w', storage.storedHeader.playerColor);
  TEST_ASSERT_EQUAL_UINT8(5, storage.storedHeader.botDepth);

  // Initial FEN should have been recorded
  TEST_ASSERT_EQUAL(1, (int)storage.fenEntries.size());
  teardownController();
}

void test_controller_resume_game(void) {
  setupController();
  ctrl->startNewGame(GameMode::CHESS_MOVES);
  ctrl->makeMove(6, 4, 4, 4);  // e2-e4
  ctrl->makeMove(1, 4, 3, 4);  // e7-e5

  // Now create a new controller and resume from storage
  observer = MockGameObserver();
  GameController* ctrl2 = new GameController(&recorder, &observer);
  bool ok = ctrl2->resumeGame();
  TEST_ASSERT_TRUE(ok);
  TEST_ASSERT_EQUAL_CHAR('w', ctrl2->currentTurn());
  TEST_ASSERT_EQUAL_CHAR('P', ctrl2->getSquare(4, 4));  // e4
  TEST_ASSERT_EQUAL_CHAR('p', ctrl2->getSquare(3, 4));  // e5
  TEST_ASSERT_TRUE(observer.callCount > 0);  // Observer notified
  delete ctrl2;
  teardownController();
}

void test_controller_make_move_records_promotion(void) {
  setupController();
  ctrl->newGame();
  ctrl->startRecording(GameMode::CHESS_MOVES);
  ctrl->loadFEN("8/4P3/8/8/8/8/8/4K2k w - - 0 1");

  MoveResult r = ctrl->makeMove(1, 4, 0, 4);  // e7-e8=Q (auto-queen)
  TEST_ASSERT_TRUE(r.valid);
  TEST_ASSERT_TRUE(r.isPromotion);
  TEST_ASSERT_EQUAL_CHAR('Q', r.promotedTo);

  // Verify the correct promotion type was recorded (compact encoding stores lowercase)
  // moveData: FEN_MARKER(2) + FEN_MARKER(2, from loadFEN) + move(2) = 6 bytes
  TEST_ASSERT_TRUE(storage.moveData.size() >= 6);
  uint16_t lastEntry;
  memcpy(&lastEntry, &storage.moveData[storage.moveData.size() - 2], 2);
  int fr, fc, tr, tc;
  char promo;
  ChessCodec::decodeMove(lastEntry, fr, fc, tr, tc, promo);
  TEST_ASSERT_EQUAL_CHAR('q', promo);  // compact encoding normalizes to lowercase
  teardownController();
}

// ---------------------------------------------------------------------------
// Registration
// ---------------------------------------------------------------------------

void register_recorder_tests() {
  // GameRecorder
  RUN_TEST(test_recorder_start_recording);
  RUN_TEST(test_recorder_record_move);
  RUN_TEST(test_recorder_record_fen);
  RUN_TEST(test_recorder_finish_recording);
  RUN_TEST(test_recorder_discard_recording);
  RUN_TEST(test_recorder_not_recording_noop);
  RUN_TEST(test_recorder_has_active_game);
  RUN_TEST(test_recorder_get_active_game_info);
  RUN_TEST(test_recorder_replay_into_board);
  RUN_TEST(test_recorder_turn_based_header_flush);
  RUN_TEST(test_recorder_fen_always_flushes_header);
  RUN_TEST(test_recorder_replay_rejects_invalid_move);
  RUN_TEST(test_recorder_start_recording_lichess_mode);
  RUN_TEST(test_recorder_start_recording_while_active);
  RUN_TEST(test_recorder_finish_not_recording);
  RUN_TEST(test_recorder_discard_not_recording);
  RUN_TEST(test_recorder_replay_wrong_version);
  RUN_TEST(test_recorder_replay_no_fen_entry);
  RUN_TEST(test_recorder_replay_null_storage);
  RUN_TEST(test_recorder_replay_empty_after_fen);
  RUN_TEST(test_recorder_replay_with_promotion);

  // GameController
  RUN_TEST(test_controller_new_game);
  RUN_TEST(test_controller_make_move);
  RUN_TEST(test_controller_make_move_auto_finish);
  RUN_TEST(test_controller_end_game);
  RUN_TEST(test_controller_load_fen);
  RUN_TEST(test_controller_no_recorder);
  RUN_TEST(test_controller_no_observer);
  RUN_TEST(test_controller_discard_recording);
  RUN_TEST(test_controller_pass_throughs);
  RUN_TEST(test_controller_end_game_idempotent);
  RUN_TEST(test_controller_start_new_game);
  RUN_TEST(test_controller_resume_game);
  RUN_TEST(test_controller_make_move_records_promotion);
}
