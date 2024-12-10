#include <bits/stdc++.h>

using namespace std;

using std::chrono::high_resolution_clock;
using std::chrono::duration;
using std::chrono::duration_cast;

struct Piece {
    // true is white, false is black
    bool color;
    char name;
    vector<string> possibleMove;
};

struct Task {
    vector<vector<Piece>> boardState;
    string move;
};


// global queue
queue<Task> taskQueue;

pthread_mutex_t queueLock;
pthread_mutex_t resultsLock;
vector<pair<int, string>> results;

// functions for establishing the graph
void initialBoard(vector<vector<Piece>>& boardState);
void printBoard(vector<vector<Piece>>& boardState);
void printPossibleMoves(vector<vector<Piece>>& boardState);
vector<string> possibleMoves(vector<vector<Piece>>& boardState, int i, int j);
bool inCheck(vector<vector<Piece>>& boardState, bool team);


bool validSpot(string& pos, vector<vector<Piece>>& boardState);


// functions to play the game
void playFirstMoves(vector<vector<Piece>>& boardState, vector<string>& moveList);
void convertToIJ(string& move, int& iVal, int& jVal);
string convertToUCI(int iCurr, int jCurr, int iEnd, int jEnd);

// minimax functions
pair<int, string> minimax(vector<vector<Piece>> boardState, int depth, bool team, string bestMove, int alpha, int beta);
int evaluateScore(vector<vector<Piece>>& boardState, bool team);
void simulateMove(vector<vector<Piece>>& boardState, int i, int j, string& move);


// parallel function
// no need for trampoline, queue is globally used
void* worker(void* arg) {
    while (true) {
        Task task;

        // Lock
        pthread_mutex_lock(&queueLock);
        if (taskQueue.empty()) {
            pthread_mutex_unlock(&queueLock);
            // nothing else for this thread to do!
            break;
        }

        // Pop a task from the queue
        task = taskQueue.front();
        taskQueue.pop();
        pthread_mutex_unlock(&queueLock);

        // Compute the minimax result
        string bestMove;
        int score = minimax(task.boardState, 1, false, bestMove, -INT_MAX, INT_MAX).first;

        // Lock
        pthread_mutex_lock(&resultsLock);
        results.push_back({score, task.move});
        pthread_mutex_unlock(&resultsLock);
    }
    return NULL;
}


int main(int argc, char* argv[]) {

    if(argc != 2) throw runtime_error("Please include number of threads in arguements.");

    int ntasks = stoi(argv[1]);

    ios_base::sync_with_stdio(false);
    cin.tie(NULL);

    vector<vector<Piece>> boardState(8, vector<Piece>(8));
    initialBoard(boardState);

    // Example input:

    // If depth is odd olways chooses a black move
    /*

    Example Game vs Stockfish
    Graph Generated on this Input
    24
    e2e4 e7e5 g1f3 b8c6 f1c4 g8f6 d2d3 f8c5 c2c3 d7d6 c4f7 e8f7 c3c4 f7e8 b1c3 h7h6 f3e5 d6e5 d1h5 f6h5 c3d5 c5f2 e1f2 h8f8
    
    4
    e2e4 g8f6 e4e5 f6d5


    */

    int numInput;
    cin >> numInput;
    vector<string> moveList(numInput);
    for(auto& move : moveList) cin >> move;

    playFirstMoves(boardState, moveList);
    printBoard(boardState);

    for(int i = 0; i < 8; ++i) {
        for(int j = 0; j < 8; ++j) {
            if(boardState[i][j].name != '-') {
                boardState[i][j].possibleMove = possibleMoves(boardState, i, j);
            }
        }
    }

    printPossibleMoves(boardState);


    // here the board has the most updated moves, so we want to set up all of the tasks
    // each task holds a new updated board state, and the move associated with that state
    for(int i = 0; i < 8; ++i) {
        for(int j = 0; j < 8; ++j) {
            if(boardState[i][j].name != '-' && boardState[i][j].color) {
                for(auto x : boardState[i][j].possibleMove) {
                    // void simulateMove(vector<vector<Piece>>& boardState, int i, int j, string& move);
                    vector<vector<Piece>> boardStateCpy = boardState;
                    simulateMove(boardStateCpy, i, j, x);
                    Task toPush;
                    toPush.boardState = boardStateCpy;
                    toPush.move = to_string(i) + to_string(j) + x;
                    taskQueue.push(toPush);
                }
            }
        }
    }


    pthread_mutex_init(&queueLock, NULL);
    pthread_mutex_init(&resultsLock, NULL);


    string printMove;

    bool team = true;

    // Start a timer
    high_resolution_clock::time_point begin = high_resolution_clock::now();

    // setup thread vector
    vector<pthread_t> threads(ntasks);

    for(int i=0; i < ntasks; ++i) {
        int status = ::pthread_create(&threads[i], nullptr, worker, nullptr);
        if (status != 0) {
            ::perror("thread create");
            return 1;
        }
    }

    // Wait for all to finish
    for(int i=0; i < ntasks; ++i) {
        pthread_join(threads[i], nullptr);
    }

    int bestScore = -INT_MAX;
    string bestMove;
    cout << endl << endl << endl;
    for(int i = 0; i < results.size(); ++i) {
        if(results[i].first > bestScore) {
            bestScore = results[i].first;
            bestMove = results[i].second;
        }
    }
    cout << endl << bestScore << "  " << bestMove << endl;

    // bestMove = minimax(boardState, 0, team, bestMove, -INT_MAX, INT_MAX).second;
    // int score = minimax(boardState, 0, !team, bestMove, -INT_MAX, INT_MAX).first;

    // cout << "score " << score << endl << endl;


    printMove = convertToUCI(stoi(bestMove.substr(0, 1)), stoi(bestMove.substr(1, 1)), stoi(bestMove.substr(2, 1)), stoi(bestMove.substr(3, 1)));

    cout << printMove << endl;

    int currI = stoi(bestMove.substr(0, 1));
    int currJ = stoi(bestMove.substr(1, 1));

    string tempMove = bestMove.substr(2, 2);

    for(int i = 0; i < 8; ++i) {
        for(int j = 0; j < 8; ++j) {
            if(boardState[i][j].name != '-') {
                boardState[i][j].possibleMove = possibleMoves(boardState, i, j);
            }
        }
    }
    simulateMove(boardState, currI, currJ, tempMove);

    printBoard(boardState);

    auto time_span = duration_cast<duration<double>>(high_resolution_clock::now() - begin);

    std::cerr << endl << ntasks << " Total Threads: " << time_span.count() << '\n';


    return 0;
}

pair<int, string> minimax(vector<vector<Piece>> boardState, int depth, bool team, string bestMove, int alpha, int beta) {
    // Base case: when the depth limit is reached, evaluate the board
    if(depth == 4) {
        return make_pair(evaluateScore(boardState, team), bestMove);
    }

    vector<vector<Piece>> copyState;
    int bestScore = team ? -INT_MAX : INT_MAX;

    for(int i = 0; i < 8; ++i) {
        for(int j = 0; j < 8; ++j) {
            if(boardState[i][j].name != '-') {
                boardState[i][j].possibleMove = possibleMoves(boardState, i, j);

                if(boardState[i][j].color == team) {
                    for(auto& move : boardState[i][j].possibleMove) {
                        copyState = boardState;
                        simulateMove(copyState, i, j, move);
                        //cout << move << endl;

                        // printBoard(copyState);
                        int tempScore = minimax(copyState, depth + 1, !team, bestMove, alpha, beta).first;

                        //cout << "Score   " << temp << endl;

                        if (team) { // Max
                            if (tempScore > bestScore) {
                                bestScore = tempScore;
                                bestMove = to_string(i) + to_string(j) + move;
                            }
                            // tracks best possible score
                            alpha = max(alpha, bestScore);
                        } else { // Mini
                            if (tempScore < bestScore) {
                                bestScore = tempScore;
                                bestMove = to_string(i) + to_string(j) + move;
                            }
                            // tracks worst possible score
                            beta = min(beta, bestScore);
                        }
                        // since the beta route is chosen by the opponent, this wont actually be able to run
                        // so we prune
                        if (alpha >= beta) {
                            break;
                        }

                    }
                }
            }
        }
    }

    return make_pair(bestScore, bestMove); // Return the best score found
}

bool inCheck(vector<vector<Piece>>& boardState, bool team) {
    // find position of king for team

    int kingI = -1;
    int kingJ = -1;

    for(int i = 0; i < 8; ++i) {
        for(int j = 0; j < 8; ++j) {
            if(boardState[i][j].name == 'K' && boardState[i][j].color == team) {
                kingI = i;
                kingJ = j;
                break;
            }
        }
    }

    if (kingI == -1 || kingJ == -1) return false;

    for(int i = 0; i < 8; ++i) {
        for(int j = 0; j < 8; ++j) {
            if(boardState[i][j].name != '-') {
                boardState[i][j].possibleMove = possibleMoves(boardState, i, j);
            }
        }
    }

    string kingPos = to_string(kingI) + to_string(kingJ);

    for(int i = 0; i < 8; ++i) {
        for(int j = 0; j < 8; ++j) {
            if(boardState[i][j].color != team) {
                for(auto& x : boardState[i][j].possibleMove) {
                    if (x == kingPos) return true;
                }
            }
        }
    }

    return false;
}


int evaluateScore(vector<vector<Piece>>& boardState, bool team) {

    // used ai for the scoring tables
    // only effective for midgame

    int pawnTable[8][8] = {
        {  0,  5,  5, -10, -10,  5,  5,  0 },
        {  0, 10, 10,   0,   0, 10, 10,  0 },
        {  0, 10, 20,  30,  30, 20, 10,  0 },
        {  0, 10, 10,  20,  20, 10, 10,  0 },
        {  0, 10, 10,  20,  20, 10, 10,  0 },
        {  0, 10, 10,   0,   0, 10, 10,  0 },
        {  0,  5,  5, -10, -10,  5,  5,  0 },
        {  0,  0,  0,   0,   0,  0,  0,  0 }
    };

    int knightTable[8][8] = {
        { -50, -40, -30, -30, -30, -30, -40, -50 },
        { -40, -20,   0,   0,   0,   0, -20, -40 },
        { -30,   0,  10,  15,  15,  10,   0, -30 },
        { -30,   5,  15,  20,  20,  15,   5, -30 },
        { -30,   0,  15,  20,  20,  15,   0, -30 },
        { -30,   5,  10,  15,  15,  10,   5, -30 },
        { -40, -20,   0,   5,   5,   0, -20, -40 },
        { -50, -40, -30, -30, -30, -30, -40, -50 }
    };

    int bishopTable[8][8] = {
        { -20, -10, -10, -10, -10, -10, -10, -20 },
        { -10,   5,   0,   0,   0,   0,   5, -10 },
        { -10,  10,  10,  10,  10,  10,  10, -10 },
        { -10,   0,  10,  10,  10,  10,   0, -10 },
        { -10,   5,   5,  10,  10,   5,   5, -10 },
        { -10,   0,   5,  10,  10,   5,   0, -10 },
        { -10,   0,   0,   0,   0,   0,   0, -10 },
        { -20, -10, -10, -10, -10, -10, -10, -20 }
    };

    int rookTable[8][8] = {
        {  0,   0,   0,   5,   5,   0,   0,   0 },
        {  0,   0,   0,   5,   5,   0,   0,   0 },
        {  0,   0,   0,   5,   5,   0,   0,   0 },
        {  5,   5,   5,  10,  10,   5,   5,   5 },
        {  5,   5,   5,  10,  10,   5,   5,   5 },
        {  0,   0,   0,   5,   5,   0,   0,   0 },
        {  0,   0,   0,   5,   5,   0,   0,   0 },
        {  0,   0,   0,   0,   0,   0,   0,   0 }
    };

    int queenTable[8][8] = {
        { -20, -10, -10,  -5,  -5, -10, -10, -20 },
        { -10,   0,   0,   0,   0,   0,   0, -10 },
        { -10,   0,   5,   5,   5,   5,   0, -10 },
        {  -5,   0,   5,   5,   5,   5,   0,  -5 },
        {   0,   0,   5,   5,   5,   5,   0,  -5 },
        { -10,   5,   5,   5,   5,   5,   0, -10 },
        { -10,   0,   5,   0,   0,   0,   0, -10 },
        { -20, -10, -10,  -5,  -5, -10, -10, -20 }
    };

    int kingTable[8][8] = {
        { -30, -40, -40, -50, -50, -40, -40, -30 },
        { -30, -40, -40, -50, -50, -40, -40, -30 },
        { -30, -40, -40, -50, -50, -40, -40, -30 },
        { -30, -40, -40, -50, -50, -40, -40, -30 },
        { -20, -30, -30, -40, -40, -30, -30, -20 },
        { -10, -20, -20, -20, -20, -20, -20, -10 },
        {  20,  20,   0,   0,   0,   0,  20,  20 },
        {  20,  30,  10,   0,   0,  10,  30,  20 }
    };


    int score = 0;
    for (int i = 0; i < 8; ++i) {
        for (int j = 0; j < 8; ++j) {
            if (boardState[i][j].name == '-') {
                continue;
            }

            int pieceValue = 0;

            if (boardState[i][j].name == 'K') pieceValue = 100 + kingTable[i][j];
            else if (boardState[i][j].name == 'Q') pieceValue = 9 + queenTable[i][j];
            else if (boardState[i][j].name == 'B') pieceValue = 3 + bishopTable[i][j];
            else if (boardState[i][j].name == 'R') pieceValue = 3 + rookTable[i][j];
            else if (boardState[i][j].name == 'N') pieceValue = 5 + knightTable[i][j];
            else if (boardState[i][j].name == 'P') pieceValue = 1 + pawnTable[i][j];

            if (boardState[i][j].color == team) {
                score += pieceValue;
            } else {
                score -= pieceValue;
            }
        }
    }

    if (inCheck(boardState, team)) {
        score -= 60; // Apply a large penalty if the king is in check
    }

    // Check if the opponent's king is in check
    if (inCheck(boardState, !team)) {
        score += 60; // Apply a large reward if the opponent's king is in check
    }

    return score;
}

void simulateMove(vector<vector<Piece>>& boardState, int i, int j, string& move) {
    string endPos, convertedEndPos;
    int currI, currJ, endI, endJ;
    Piece currPiece;

    currI = i;
    currJ = j;
    endI = stoi(move.substr(0, 1));
    endJ = stoi(move.substr(1, 1));
    convertedEndPos = to_string(endI) + to_string(endJ); 

    if(boardState[currI][currJ].name != '-') {
        boardState[i][j].possibleMove = possibleMoves(boardState, i, j);
        for(auto& possibleMove : boardState[currI][currJ].possibleMove) {
            if(possibleMove == convertedEndPos) {
                if(boardState[endI][endJ].name == '-') {
                    swap(boardState[currI][currJ], boardState[endI][endJ]);

                }

                else {
                    boardState[endI][endJ].name = '-';
                    boardState[endI][endJ].color = true;
                    swap(boardState[currI][currJ], boardState[endI][endJ]);
                }

                break;
            }
        }
    }
}


void playFirstMoves(vector<vector<Piece>>& boardState, vector<string>& moveList) {
    string currPos, endPos, convertedEndPos;
    int currI, currJ, endI, endJ;
    Piece currPiece;

    // we want to start as white
    bool colorOrder = true;
    for(auto& move : moveList) {
        currPos = move.substr(0, 2);
        convertToIJ(currPos, currI, currJ);
        endPos = move.substr(2, 2);
        convertToIJ(endPos, endI, endJ);

        convertedEndPos = to_string(endI) + to_string(endJ); 

        boardState[currI][currJ].possibleMove = possibleMoves(boardState, currI, currJ);


        if(boardState[currI][currJ].name != '-' && boardState[currI][currJ].color == colorOrder) {
            for(auto& possibleMove : boardState[currI][currJ].possibleMove) {
                if(possibleMove == convertedEndPos) {
                    if(boardState[endI][endJ].name == '-') {
                        swap(boardState[currI][currJ], boardState[endI][endJ]);
                    }

                    else {
                        boardState[endI][endJ].name = '-';
                        boardState[endI][endJ].color = true;
                        swap(boardState[currI][currJ], boardState[endI][endJ]);
                    }

                    continue;
                }
            }
        }
        else throw runtime_error("Incorrect Piece Position Called");

        colorOrder = !colorOrder;
    }
}

void convertToIJ(string& move, int& iVal, int& jVal) {
    iVal = 7 - (move[1] - 49);
    jVal = move[0] - 'a';
    
}

string convertToUCI(int iCurr, int jCurr, int iEnd, int jEnd) {
    char rowStart = '1' + (7 - iCurr);
    char colStart = 'a' + jCurr;

    char rowEnd = '1' + (7 - iEnd);
    char colEnd = 'a' + jEnd;

    return string(1, colStart) + string(1, rowStart) + string(1, colEnd) + string(1, rowEnd);
}


void initialBoard(vector<vector<Piece>>& boardState) {
    for(int i = 2; i < 6; ++i) {
        for(int j = 0; j < 8; ++j) {
            Piece basic;
            basic.color = true;
            basic.name = '-';
            boardState[i][j] = basic;
        }
    }

    for(int j = 0; j < 8; ++j) {
        Piece pawnWhite;
        pawnWhite.color = true;
        pawnWhite.name = 'P';

        Piece pawnBlack;
        pawnBlack.color = false;
        pawnBlack.name = 'P';
        
        boardState[6][j] = pawnWhite;
        boardState[1][j] = pawnBlack;
    }

    Piece knightWhite;
    knightWhite.color = true;
    knightWhite.name = 'N';

    Piece knightBlack;
    knightBlack.color = false;
    knightBlack.name = 'N';

    boardState[7][1] = knightWhite;
    boardState[0][1] = knightBlack;

    Piece knightWhite2;
    knightWhite2.color = true;
    knightWhite2.name = 'N';

    Piece knightBlack2;
    knightBlack2.color = false;
    knightBlack2.name = 'N';

    boardState[7][6] = knightWhite2;
    boardState[0][6] = knightBlack2;


    Piece rookWhite;
    rookWhite.color = true;
    rookWhite.name = 'R';

    Piece rookBlack;
    rookBlack.color = false;
    rookBlack.name = 'R';

    boardState[7][0] = rookWhite;
    boardState[0][0] = rookBlack;

    Piece rookWhite2;
    rookWhite2.color = true;
    rookWhite2.name = 'R';

    Piece rookBlack2;
    rookBlack2.color = false;
    rookBlack2.name = 'R';

    boardState[7][7] = rookWhite2;
    boardState[0][7] = rookBlack2;

    Piece bishopWhite;
    bishopWhite.color = true;
    bishopWhite.name = 'B';

    Piece bishopBlack;
    bishopBlack.color = false;
    bishopBlack.name = 'B';

    boardState[7][2] = bishopWhite;
    boardState[0][2] = bishopBlack;

    Piece bishopWhite2;
    bishopWhite2.color = true;
    bishopWhite2.name = 'B';

    Piece bishopBlack2;
    bishopBlack2.color = false;
    bishopBlack2.name = 'B';

    boardState[7][5] = bishopWhite2;
    boardState[0][5] = bishopBlack2;

    Piece kingWhite;
    kingWhite.color = true;
    kingWhite.name = 'K';

    Piece kingBlack;
    kingBlack.color = false;
    kingBlack.name = 'K';

    boardState[7][4] = kingWhite;
    boardState[0][4] = kingBlack;

    Piece queenWhite;
    queenWhite.color = true;
    queenWhite.name = 'Q';

    Piece queenBlack;
    queenBlack.color = false;
    queenBlack.name = 'Q';

    boardState[7][3] = queenWhite;
    boardState[0][3] = queenBlack;
}

void printBoard(vector<vector<Piece>>& boardState) {
    for(int i = 0; i < 8; ++i) {
        for(int j = 0; j < 8; ++j) {
            if(boardState[i][j].name == '-') cout << " " << boardState[i][j].name << " ";
            else {
                cout << boardState[i][j].name;
                if(boardState[i][j].color) cout << "w ";
                else cout << "b ";
            }
        }
        cout << endl;
    }
}

void printPossibleMoves(vector<vector<Piece>>& boardState) {
    for(int i = 0; i < 8; ++i) {
        for(int j = 0; j < 8; ++j) {
            if(boardState[i][j].name != '-') {
                cout << boardState[i][j].name;
                if(boardState[i][j].color) cout << "w ";
                else cout << "b ";
                vector<string> moves = possibleMoves(boardState, i, j);
                for(auto& s : boardState[i][j].possibleMove) cout << s << " ";
                cout << endl;
            }
        }
    }
}


vector<string> possibleMoves(vector<vector<Piece>>& boardState, int i, int j) {
    vector<string> toReturn;
    if(boardState[i][j].name == '-') return toReturn;

    else if(boardState[i][j].name == 'P') {
        if(boardState[i][j].color) { // if white
            if(i > 0) {
                string toAdd = to_string(i-1) + to_string(j);
                if(validSpot(toAdd, boardState)) {
                    toReturn.push_back(toAdd);
                    if(i == 6) {
                        string toAdd = to_string(i-2) + to_string(j);
                        if(validSpot(toAdd, boardState)) toReturn.push_back(toAdd);
                    }
                }
            }
            if(i > 0) {
                if(j > 0) {
                    if(boardState[i-1][j-1].name != '-' && !boardState[i-1][j-1].color) {
                        string toAdd = to_string(i-1) + to_string(j-1);
                        // no need to check if "valid", taking a piece
                        toReturn.push_back(toAdd);
                    }
                }
                if(j < 7) {
                    if(boardState[i-1][j+1].name != '-' && !boardState[i-1][j+1].color) {
                        string toAdd = to_string(i-1) + to_string(j+1);
                        // no need to check if "valid", taking a piece
                        toReturn.push_back(toAdd);
                    }
                }
            }
        }
        else { // if black
            if(i < 7) {
                string toAdd = to_string(i+1) + to_string(j);
                if(validSpot(toAdd, boardState)) {
                    toReturn.push_back(toAdd);
                    if(i == 1) {
                        string toAdd = to_string(i+2) + to_string(j);
                        if(validSpot(toAdd, boardState)) toReturn.push_back(toAdd);
                    }
                }
            }
            if(i < 7) {
                if(j > 0) {
                    if(boardState[i+1][j-1].name != '-' && boardState[i+1][j-1].color) {
                        string toAdd = to_string(i+1) + to_string(j-1);
                        // no need to check if "valid", taking a piece
                        toReturn.push_back(toAdd);
                    }
                }
                if(j < 7) {
                    if(boardState[i+1][j+1].name != '-' && boardState[i+1][j+1].color) {
                        string toAdd = to_string(i+1) + to_string(j+1);
                        // no need to check if "valid", taking a piece
                        toReturn.push_back(toAdd);
                    }
                }
            }

        }
        return toReturn;
    }

    else if(boardState[i][j].name == 'N') { // Knight
        if(boardState[i][j].color) { // if white
            if(i > 0 && j < 6) {
                if(boardState[i-1][j+2].name == '-' || !boardState[i-1][j+2].color) {
                    string toAdd = to_string(i-1) + to_string(j+2);
                    toReturn.push_back(toAdd);
                }
            }
            if(i > 0 && j > 2) {
                if(boardState[i-1][j-2].name == '-' || !boardState[i-1][j-2].color) {
                    string toAdd = to_string(i-1) + to_string(j-2);
                    toReturn.push_back(toAdd);
                }
            }

            if(i < 7 && j < 6) {
                if(boardState[i+1][j+2].name == '-' || !boardState[i+1][j+2].color) {
                    string toAdd = to_string(i+1) + to_string(j+2);
                    toReturn.push_back(toAdd);
                }
            }
            if(i < 7 && j > 2) {
                if(boardState[i+1][j-2].name == '-' || !boardState[i+1][j-2].color) {
                    string toAdd = to_string(i+1) + to_string(j-2);
                    toReturn.push_back(toAdd);
                }
            }

            if(i > 1 && j < 7) {
                if(boardState[i-2][j+1].name == '-' || !boardState[i-2][j+1].color) {
                    string toAdd = to_string(i-2) + to_string(j+1);
                    toReturn.push_back(toAdd);
                }
            }
            if(i > 1 && j > 0) {
                if(boardState[i-2][j-1].name == '-' || !boardState[i-2][j-1].color) {
                    string toAdd = to_string(i-2) + to_string(j-1);
                    toReturn.push_back(toAdd);
                }
            }

            if(i < 6 && j < 7) {
                if(boardState[i+2][j+1].name == '-' || !boardState[i+2][j+1].color) {
                    string toAdd = to_string(i+2) + to_string(j+1);
                    toReturn.push_back(toAdd);
                }
            }
            if(i < 6 && j > 0) {
                if(boardState[i+2][j-1].name == '-' || !boardState[i+2][j-1].color) {
                    string toAdd = to_string(i+2) + to_string(j-1);
                    toReturn.push_back(toAdd);
                }
            }

        }

        else { // if black
            if(i > 0 && j < 6) {
                if(boardState[i-1][j+2].color) {
                    string toAdd = to_string(i-1) + to_string(j+2);
                    toReturn.push_back(toAdd);
                }
            }
            if(i > 0 && j > 2) {
                if(boardState[i-1][j-2].color) {
                    string toAdd = to_string(i-1) + to_string(j-2);
                    toReturn.push_back(toAdd);
                }
            }

            if(i < 7 && j < 6) {
                if(boardState[i+1][j+2].color) {
                    string toAdd = to_string(i+1) + to_string(j+2);
                    toReturn.push_back(toAdd);
                }
            }
            if(i < 7 && j > 1) {
                if(boardState[i+1][j-2].color) {
                    string toAdd = to_string(i+1) + to_string(j-2);
                    toReturn.push_back(toAdd);
                }
            }

            if(i > 1 && j < 7) {
                if(boardState[i-2][j+1].color) {
                    string toAdd = to_string(i-2) + to_string(j+1);
                    toReturn.push_back(toAdd);
                }
            }
            if(i > 1 && j > 0) {
                if(boardState[i-2][j-1].color) {
                    string toAdd = to_string(i-2) + to_string(j-1);
                    toReturn.push_back(toAdd);
                }
            }

            if(i < 6 && j < 7) {
                if(boardState[i+2][j+1].color) {
                    string toAdd = to_string(i+2) + to_string(j+1);
                    toReturn.push_back(toAdd);
                }
            }
            if(i < 6 && j > 0) {
                if(boardState[i+2][j-1].color) {
                    string toAdd = to_string(i+2) + to_string(j-1);
                    toReturn.push_back(toAdd);
                }
            }

        }
        return toReturn;

    }

    else if(boardState[i][j].name == 'R') { // Rook
        if(boardState[i][j].color) { // if white
            bool yesPlus = true;
            bool yesMinus = true;
            for(int c = 1; c < 8; ++c) {
                if(!yesPlus && !yesMinus) break;
                if(yesPlus && i + c < 8) {
                    if(boardState[i + c][j].name == '-' || !boardState[i + c][j].color) {
                        string toAdd = to_string(i+c) + to_string(j);
                        toReturn.push_back(toAdd);
                        if (!boardState[i + c][j].color) yesPlus = false;
                    }
                    else yesPlus = false;

                }
                else yesPlus = false;

                if(yesMinus && i - c > -1) {
                    if(boardState[i - c][j].name == '-' || !boardState[i - c][j].color) {
                        string toAdd = to_string(i-c) + to_string(j);
                        toReturn.push_back(toAdd);

                        if (!boardState[i - c][j].color) yesMinus = false;
                    }
                    else yesMinus = false;

                }
                else yesMinus = false;
            }

            yesPlus = true;
            yesMinus = true;
            for(int c = 1; c < 8; ++c) {
                if(!yesPlus && !yesMinus) break;
                if(yesPlus && j + c < 8) {
                    if(boardState[i][j + c].name == '-' || !boardState[i][j + c].color) {
                        string toAdd = to_string(i) + to_string(j+c);
                        toReturn.push_back(toAdd);
                        if (!boardState[i][j + c].color) yesPlus = false;
                    }
                    else yesPlus = false;

                }
                else yesPlus = false;

                if(yesMinus && j - c > -1) {
                    if(boardState[i][j - c].name == '-' || !boardState[i][j - c].color) {
                        string toAdd = to_string(i) + to_string(j - c);
                        toReturn.push_back(toAdd);
                        if (!boardState[i][j - c].color) yesMinus = false;
                    }
                    else yesMinus = false;

                }
                else yesMinus = false;
            }
        }

        else { // if black
            bool yesPlus = true;
            bool yesMinus = true;
            for(int c = 1; c < 8; ++c) {
                if(!yesPlus && !yesMinus) break;
                if(yesPlus && i + c < 8) {
                    if(boardState[i + c][j].name == '-' || boardState[i + c][j].color) {
                        string toAdd = to_string(i+c) + to_string(j);
                        toReturn.push_back(toAdd);
                        if (boardState[i + c][j].name != '-' && boardState[i + c][j].color) yesPlus = false;
                    }
                    else yesPlus = false;

                }
                else yesPlus = false;

                if(yesMinus && i - c > -1) {
                    if(boardState[i - c][j].name == '-' || boardState[i - c][j].color) {
                        string toAdd = to_string(i-c) + to_string(j);
                        toReturn.push_back(toAdd);
                        if (boardState[i - c][j].name != '-' && boardState[i - c][j].color) yesMinus = false;
                    }
                    else yesMinus = false;

                }
                else yesMinus = false;
            }

            yesPlus = true;
            yesMinus = true;
            for(int c = 1; c < 8; ++c) {
                if(!yesPlus && !yesMinus) break;
                if(yesPlus && j + c < 8) {
                    if(boardState[i][j + c].name == '-' || boardState[i][j + c].color) {
                        string toAdd = to_string(i) + to_string(j+c);
                        toReturn.push_back(toAdd);
                        if (boardState[i][j + c].name != '-' && boardState[i][j + c].color) yesPlus = false;
                    }
                    else yesPlus = false;

                }
                else yesPlus = false;

                if(yesMinus && j - c > -1) {
                    if(boardState[i][j - c].name == '-' || boardState[i][j - c].color) {
                        string toAdd = to_string(i) + to_string(j - c);
                        toReturn.push_back(toAdd);
                        if (boardState[i][j - c].name != '-' && boardState[i][j - c].color) yesMinus = false;
                    }
                    else yesMinus = false;

                }
                else yesMinus = false;
            }
        }
    }

    else if(boardState[i][j].name == 'B') { // Bishop
        if(boardState[i][j].color) { // if white
            bool yesPlus = true;
            bool yesMinus = true;
            for(int c = 1; c < 8; ++c) {
                if(!yesPlus && !yesMinus) break;
                if(yesPlus && i + c < 8 && j + c < 8) {
                    if(boardState[i + c][j + c].name == '-' || !boardState[i + c][j + c].color) {
                        string toAdd = to_string(i+c) + to_string(j+c);
                        toReturn.push_back(toAdd);
                        if (!boardState[i + c][j + c].color) yesPlus = false;
                    }
                    else yesPlus = false;

                }
                else yesPlus = false;

                if(yesMinus && i - c > -1 && j - c > -1) {
                    if(boardState[i - c][j - c].name == '-' || !boardState[i - c][j - c].color) {
                        string toAdd = to_string(i-c) + to_string(j-c);
                        toReturn.push_back(toAdd);
                        if (!boardState[i - c][j - c].color) yesMinus = false;
                    }
                    else yesMinus = false;

                }
                else yesMinus = false;
            }

            yesPlus = true;
            yesMinus = true;
            for(int c = 1; c < 8; ++c) {
                if(!yesPlus && !yesMinus) break;
                if(yesPlus && i + c < 8 && j - c > -1) {
                    if(boardState[i + c][j - c].name == '-' || !boardState[i + c][j - c].color) {
                        string toAdd = to_string(i+c) + to_string(j-c);
                        toReturn.push_back(toAdd);
                        if (!boardState[i + c][j - c].color) yesPlus = false;
                    }
                    else yesPlus = false;

                }
                else yesPlus = false;

                if(yesMinus && i - c > -1 && j + c < 8) {
                    if(boardState[i - c][j + c].name == '-' || !boardState[i - c][j + c].color) {
                        string toAdd = to_string(i-c) + to_string(j+c);
                        toReturn.push_back(toAdd);
                        if (!boardState[i - c][j + c].color) yesMinus = false;
                    }
                    else yesMinus = false;

                }
                else yesMinus = false;
            }
        }

        else { // if black
            bool yesPlus = true;
            bool yesMinus = true;
            for(int c = 1; c < 8; ++c) {
                if(!yesPlus && !yesMinus) break;
                if(yesPlus && i + c < 8 && j + c < 8) {
                    if(boardState[i + c][j + c].name == '-' || boardState[i + c][j + c].color) {
                        string toAdd = to_string(i+c) + to_string(j+c);
                        toReturn.push_back(toAdd);
                        if (boardState[i + c][j + c].name != '-' && boardState[i + c][j + c].color) yesPlus = false;
                        
                    }
                    else yesPlus = false;

                }
                else yesPlus = false;

                if(yesMinus && i - c > -1 && j - c > -1) {
                    if(boardState[i - c][j - c].name == '-' || boardState[i - c][j - c].color) {
                        string toAdd = to_string(i-c) + to_string(j-c);
                        toReturn.push_back(toAdd);
                         if (boardState[i - c][j - c].name != '-' &&boardState[i - c][j - c].color) yesMinus = false;
                    }
                    else yesMinus = false;

                }
                else yesMinus = false;
            }

            yesPlus = true;
            yesMinus = true;
            for(int c = 1; c < 8; ++c) {
                if(!yesPlus && !yesMinus) break;
                if(yesPlus && i + c < 8 && j - c > -1) {
                    if(boardState[i + c][j - c].name == '-' || boardState[i + c][j - c].color) {
                        string toAdd = to_string(i+c) + to_string(j-c);
                        toReturn.push_back(toAdd);
                         if (boardState[i + c][j - c].name != '-' &&boardState[i + c][j - c].color) yesPlus = false;
                    }
                    else yesPlus = false;

                }
                else yesPlus = false;

                if(yesMinus && i - c > -1 && j + c < 8) {
                    if(boardState[i - c][j + c].name == '-' || boardState[i - c][j + c].color) {
                        string toAdd = to_string(i-c) + to_string(j+c);
                        toReturn.push_back(toAdd);
                         if (boardState[i - c][j + c].name != '-' && boardState[i - c][j + c].color) yesMinus = false;
                    }
                    else yesMinus = false;

                }
                else yesMinus = false;
            }
        }
    }

    else if(boardState[i][j].name == 'Q') { // Queen
        if(boardState[i][j].color) { // if white
            bool yesPlus = true;
            bool yesMinus = true;
            for(int c = 1; c < 8; ++c) {
                if(!yesPlus && !yesMinus) break;
                if(yesPlus && i + c < 8 && j + c < 8) {
                    if(boardState[i + c][j + c].name == '-' || !boardState[i + c][j + c].color) {
                        string toAdd = to_string(i+c) + to_string(j+c);
                        toReturn.push_back(toAdd);
                        if (!boardState[i + c][j + c].color) yesPlus = false;
                    }
                    else yesPlus = false;

                }
                else yesPlus = false;

                if(yesMinus && i - c > -1 && j - c > -1) {
                    if(boardState[i - c][j - c].name == '-' || !boardState[i - c][j - c].color) {
                        string toAdd = to_string(i-c) + to_string(j-c);
                        toReturn.push_back(toAdd);
                        if (!boardState[i - c][j - c].color) yesMinus = false;
                    }
                    else yesMinus = false;

                }
                else yesMinus = false;
            }

            yesPlus = true;
            yesMinus = true;
            for(int c = 1; c < 8; ++c) {
                if(!yesPlus && !yesMinus) break;
                if(yesPlus && i + c < 8 && j - c > -1) {
                    if(boardState[i + c][j - c].name == '-' || !boardState[i + c][j - c].color) {
                        string toAdd = to_string(i+c) + to_string(j-c);
                        toReturn.push_back(toAdd);
                        if (!boardState[i + c][j - c].color) yesPlus = false;
                    }
                    else yesPlus = false;

                }
                else yesPlus = false;

                if(yesMinus && i - c > -1 && j + c < 8) {
                    if(boardState[i - c][j + c].name == '-' || !boardState[i - c][j + c].color) {
                        string toAdd = to_string(i-c) + to_string(j+c);
                        toReturn.push_back(toAdd);
                        if (!boardState[i - c][j + c].color) yesMinus = false;
                    }
                    else yesMinus = false;

                }
                else yesMinus = false;
            }
            yesPlus = true;
            yesMinus = true;
            for(int c = 1; c < 8; ++c) {
                if(!yesPlus && !yesMinus) break;
                if(yesPlus && i + c < 8) {
                    if(boardState[i + c][j].name == '-' || !boardState[i + c][j].color) {
                        string toAdd = to_string(i+c) + to_string(j);
                        toReturn.push_back(toAdd);
                        if (!boardState[i + c][j].color) yesPlus = false;
                    }
                    else yesPlus = false;

                }
                else yesPlus = false;

                if(yesMinus && i - c > -1) {
                    if(boardState[i - c][j].name == '-' || !boardState[i - c][j].color) {
                        string toAdd = to_string(i-c) + to_string(j);
                        toReturn.push_back(toAdd);

                        if (!boardState[i - c][j].color) yesMinus = false;
                    }
                    else yesMinus = false;

                }
                else yesMinus = false;
            }

            yesPlus = true;
            yesMinus = true;
            for(int c = 1; c < 8; ++c) {
                if(!yesPlus && !yesMinus) break;
                if(yesPlus && j + c < 8) {
                    if(boardState[i][j + c].name == '-' || !boardState[i][j + c].color) {
                        string toAdd = to_string(i) + to_string(j+c);
                        toReturn.push_back(toAdd);
                        if (!boardState[i][j + c].color) yesPlus = false;
                    }
                    else yesPlus = false;

                }
                else yesPlus = false;

                if(yesMinus && j - c > -1) {
                    if(boardState[i][j - c].name == '-' || !boardState[i][j - c].color) {
                        string toAdd = to_string(i) + to_string(j - c);
                        toReturn.push_back(toAdd);
                        if (!boardState[i][j - c].color) yesMinus = false;
                    }
                    else yesMinus = false;

                }
                else yesMinus = false;
            }
        }

        else { // if black
            bool yesPlus = true;
            bool yesMinus = true;
            for(int c = 1; c < 8; ++c) {
                if(!yesPlus && !yesMinus) break;
                if(yesPlus && i + c < 8 && j + c < 8) {
                    if(boardState[i + c][j + c].name == '-' || boardState[i + c][j + c].color) {
                        string toAdd = to_string(i+c) + to_string(j+c);
                        toReturn.push_back(toAdd);
                        if (boardState[i + c][j + c].name != '-' && boardState[i + c][j + c].color) yesPlus = false;
                        
                    }
                    else yesPlus = false;

                }
                else yesPlus = false;

                if(yesMinus && i - c > -1 && j - c > -1) {
                    if(boardState[i - c][j - c].name == '-' || boardState[i - c][j - c].color) {
                        string toAdd = to_string(i-c) + to_string(j-c);
                        toReturn.push_back(toAdd);
                         if (boardState[i - c][j - c].name != '-' && boardState[i - c][j - c].color) yesMinus = false;
                    }
                    else yesMinus = false;

                }
                else yesMinus = false;
            }

            yesPlus = true;
            yesMinus = true;
            for(int c = 1; c < 8; ++c) {
                if(!yesPlus && !yesMinus) break;
                if(yesPlus && i + c < 8 && j - c > -1) {
                    if(boardState[i + c][j - c].name == '-' || boardState[i + c][j - c].color) {
                        string toAdd = to_string(i+c) + to_string(j-c);
                        toReturn.push_back(toAdd);
                         if (boardState[i + c][j - c].name != '-' && boardState[i + c][j - c].color) yesPlus = false;
                    }
                    else yesPlus = false;

                }
                else yesPlus = false;

                if(yesMinus && i - c > -1 && j + c < 8) {
                    if(boardState[i - c][j + c].name == '-' || boardState[i - c][j + c].color) {
                        string toAdd = to_string(i-c) + to_string(j+c);
                        toReturn.push_back(toAdd);
                         if (boardState[i - c][j + c].name != '-' && boardState[i - c][j + c].color) yesMinus = false;
                    }
                    else yesMinus = false;

                }
                else yesMinus = false;
            }

            yesPlus = true;
            yesMinus = true;
            for(int c = 1; c < 8; ++c) {
                if(!yesPlus && !yesMinus) break;
                if(yesPlus && i + c < 8) {
                    if(boardState[i + c][j].name == '-' || boardState[i + c][j].color) {
                        string toAdd = to_string(i+c) + to_string(j);
                        toReturn.push_back(toAdd);
                        if (boardState[i + c][j].name != '-' && boardState[i + c][j].color) yesPlus = false;
                    }
                    else yesPlus = false;

                }
                else yesPlus = false;

                if(yesMinus && i - c > -1) {
                    if(boardState[i - c][j].name == '-' || boardState[i - c][j].color) {
                        string toAdd = to_string(i-c) + to_string(j);
                        toReturn.push_back(toAdd);
                        if (boardState[i - c][j].name != '-' && boardState[i - c][j].color) yesMinus = false;
                    }
                    else yesMinus = false;

                }
                else yesMinus = false;
            }

            yesPlus = true;
            yesMinus = true;
            for(int c = 1; c < 8; ++c) {
                if(!yesPlus && !yesMinus) break;
                if(yesPlus && j + c < 8) {
                    if(boardState[i][j + c].name == '-' || boardState[i][j + c].color) {
                        string toAdd = to_string(i) + to_string(j+c);
                        toReturn.push_back(toAdd);
                        if (boardState[i][j + c].name != '-' && boardState[i][j + c].color) yesPlus = false;
                    }
                    else yesPlus = false;

                }
                else yesPlus = false;

                if(yesMinus && j - c > -1) {
                    if(boardState[i][j - c].name == '-' || boardState[i][j - c].color) {
                        string toAdd = to_string(i) + to_string(j - c);
                        toReturn.push_back(toAdd);
                        if (boardState[i][j - c].name != '-' && boardState[i][j - c].color) yesMinus = false;
                    }
                    else yesMinus = false;

                }
                else yesMinus = false;
            }
        }
    }

    else if(boardState[i][j].name == 'K') { // King
        if(boardState[i][j].color) { // if white
            if(i + 1 < 8 && (boardState[i + 1][j].name == '-' || !boardState[i + 1][j].color)) {
                string toAdd = to_string(i+1) + to_string(j);
                toReturn.push_back(toAdd);
            }

            if(i - 1 > -1 && (boardState[i - 1][j].name == '-' || !boardState[i - 1][j].color)) {
                string toAdd = to_string(i-1) + to_string(j);
                toReturn.push_back(toAdd);
            }

            if(j + 1 < 8 && (boardState[i][j + 1].name == '-' || !boardState[i][j + 1].color)) {
                string toAdd = to_string(i) + to_string(j+1);
                toReturn.push_back(toAdd);
            }

            if(j - 1 > -1 && (boardState[i][j - 1].name == '-' || !boardState[i][j - 1].color)) {
                string toAdd = to_string(i) + to_string(j-1);
                toReturn.push_back(toAdd);
            }

            if(i + 1 < 8 && j + 1 < 8 && (boardState[i + 1][j + 1].name == '-' || !boardState[i + 1][j + 1].color)) {
                string toAdd = to_string(i+1) + to_string(j+1);
                toReturn.push_back(toAdd);
            }

            if(i - 1 > -1 && j - 1 > -1 && (boardState[i - 1][j - 1].name == '-' || !boardState[i - 1][j - 1].color)) {
                string toAdd = to_string(i-1) + to_string(j-1);
                toReturn.push_back(toAdd);
            }

            if(j + 1 < 8 && i - 1 > -1 && (boardState[i - 1][j + 1].name == '-' || !boardState[i - 1][j + 1].color)) {
                string toAdd = to_string(i-1) + to_string(j+1);
                toReturn.push_back(toAdd);
            }

            if(j - 1 > -1 && i + 1 < 8 && (boardState[i + 1][j - 1].name == '-' || !boardState[i + 1][j - 1].color)) {
                string toAdd = to_string(i+1) + to_string(j-1);
                toReturn.push_back(toAdd);
            }

        }

        else { // if black
            if(i + 1 < 8 && (boardState[i + 1][j].name == '-' || boardState[i + 1][j].color)) {
                string toAdd = to_string(i+1) + to_string(j);
                toReturn.push_back(toAdd);
            }

            if(i - 1 > -1 && (boardState[i - 1][j].name == '-' || boardState[i - 1][j].color)) {
                string toAdd = to_string(i-1) + to_string(j);
                toReturn.push_back(toAdd);
            }

            if(j + 1 < 8 && (boardState[i][j + 1].name == '-' || boardState[i][j + 1].color)) {
                string toAdd = to_string(i) + to_string(j+1);
                toReturn.push_back(toAdd);
            }

            if(j - 1 > -1 && (boardState[i][j - 1].name == '-' || boardState[i][j - 1].color)) {
                string toAdd = to_string(i) + to_string(j-1);
                toReturn.push_back(toAdd);
            }

            if(i + 1 < 8 && j + 1 < 8 && (boardState[i + 1][j + 1].name == '-' || boardState[i + 1][j + 1].color)) {
                string toAdd = to_string(i+1) + to_string(j+1);
                toReturn.push_back(toAdd);
            }

            if(i - 1 > -1 && j - 1 > -1 && (boardState[i - 1][j - 1].name == '-' || boardState[i - 1][j - 1].color)) {
                string toAdd = to_string(i-1) + to_string(j-1);
                toReturn.push_back(toAdd);
            }

            if(j + 1 < 8 && i - 1 > -1 && (boardState[i - 1][j + 1].name == '-' || boardState[i - 1][j + 1].color)) {
                string toAdd = to_string(i-1) + to_string(j+1);
                toReturn.push_back(toAdd);
            }

            if(j - 1 > -1 && i + 1 < 8 && (boardState[i + 1][j - 1].name == '-' || boardState[i + 1][j - 1].color)) {
                string toAdd = to_string(i+1) + to_string(j-1);
                toReturn.push_back(toAdd);
            }
        }
    }

    return toReturn;
}

bool validSpot(string& pos, vector<vector<Piece>>& boardState) {
    int a = pos[0] - 48;
    int b = pos[1] - 48;
    if (a < 0 || a >= 8 || b < 0 || b >= 8) return false;
    if(boardState[a][b].name == '-') return true;
    return false;
}
