/*
 * player.cc
 */
#include <iostream>
#include <vector>
#include <algorithm>
#include <cmath>
#include "player.h"
#include "evaluation.h"
#include "board.h"
#include "timer.h"
#include "functions.h"
#include "transposition.h"
#include "game.h"

namespace checkers {

	Player::Player(Game* g) : game(g)
	{
		movement = new std::vector<unsigned int>;
		capture_movement = new std::vector<unsigned int>;
		timer = new timer::Timer(TOTAL_TIME);
#ifdef HISTORY_HEURISTIC
		for(int a=0; a<32; ++a) {
			for(int b=0; b<32; ++b) {
				history[a][b] = 0;
			}
		}
#endif // HISTORY_HEURISTIC
#ifdef TRANS_TABLE
		trans_table = new TranspositionTable;
#endif // TRANS_TABLE

	}

	Player::~Player()
	{
		delete movement;
		delete capture_movement;
		delete timer;
#ifdef TRANS_TABLE
		delete trans_table;
#endif // TRANS_TABLE
	}

	SearchResult Player::search()
	{
		SearchResult result;

		int value = 0;
		nrOfNodes = 0;
		maxdepth = 2;
		extendedDepth = 0;
		finished_search = true;
		time_check = 0;
		max_time = timer->getMaxTime();

		timer->startTimer();

		Board newboard;
		newboard.createBoard();
		movement->clear();
		capture_movement->clear();
		if(newboard == game->board) {
			movement->push_back(0x400);
			movement->push_back(0x4000);
			result.move = *movement;
			result.time = 0;
			result.value = 0;
			result.extendedDepth = 0;
			result.nodes = 0;
			result.depth = 0;
			return result;
		}
		if(!singleJump(game->board))
		{
			while(finished_search)
			{
				maxdepth++;
				value = alphabeta(game->board, 0, -999999, 999999);
				if(finished_search)
				{
					std::reverse(movement->begin(), movement->end());
					result.move = *movement;
					result.depth = maxdepth;
					result.extendedDepth = extendedDepth;
					result.value = value;
				}
				if(timer->getTime() > max_time/4)
					finished_search = false;
			}
		}
		else {
			result.move = *movement;
			result.depth = 0;
			result.extendedDepth = extendedDepth;
			result.value = value;
		}

		result.time = timer->stopTimer();
		result.nodes = nrOfNodes;

		return result;
	}

	/*
	 * ALPHABETA
	 */
	int Player::alphabeta(Board& board, int depth, int alpha, int beta)
	{
		unsigned int pieces = 0x0u;
		unsigned int moves = 0x0u;
		unsigned int from = 0x0u;
		unsigned int to = 0x0u;
		bool capture = false;
		int tmp = 0;
		unsigned int movelist[96];
#ifdef HISTORY_HEURISTIC
		int movevalues[48] = {0};
		unsigned int best_from = 0x0u;
		unsigned int best_to = 0x0u;
#endif // HISTORY_HEURISTIC
		unsigned int movecount = 0;
#ifdef SCOUT
		int testBeta = beta;
#endif // SCOUT

		if(time_check > 10000)
		{
			if(timer->getTime() > max_time)
			{
				finished_search = false;
			}
			time_check = 0;
		}
		if(!finished_search)
		{
			return alpha;
		}
		time_check++;

		nrOfNodes++;

		if(depth > extendedDepth)
			extendedDepth = depth;


		/*************************
		 * CHECK FOR 3-STEPS DRAW
		 *************************/
		if(depth == 1 && game->countHistoryMatches(board) == 2)
		{
			return 200;
		}



#ifdef TRANS_TABLE
		/******************************
		 * CHECK IF THE NODE IS CACHED
		 ******************************/
		int hash_flag = FLAG_ALPHA;
		int trans_alpha;
		if( depth > 2 && (trans_alpha = trans_table->get(board, maxdepth-depth, alpha, beta)) != TRANS_NULL)
		{
			// If we found a good entry for
			// this node in the table,
			// we don't have to search further.
			return trans_alpha;
		}
#endif // TRANS_TABLE

		board.player == BLACK ? pieces = board.black : pieces = board.white;
		/*********************************
		 * Check if there is capture moves
		 *********************************/
		if(board.getCaptureMoves() != 0x0u)
		{
			capture = true;
		}

		/*******************************
		 * Check if its the end node.
		 * If there is capture moves,
		 * try one more ply.
		 *******************************/
		if(((depth >= maxdepth) && !capture))
		{
			alpha = board.player == BLACK ? evaluate(board, depth) : -evaluate(board, depth);
#ifdef TRANS_TABLE
			trans_table->update(board, maxdepth-depth, alpha-depth, FLAG_EXACT);
#endif // TRANS_TABLE
			return alpha;
		}

		/**********************
		 * GENERATE THE MOVES
		 **********************/
		while(pieces != 0x0u)
		{
			from = (pieces &(pieces-1)) ^ pieces;
			pieces &= pieces-1;

			if(capture)
			{
				moves = board.getCaptureMoves(from);
			}
			else
			{
				moves = board.getMoves(from);
			}
			while(moves != 0x0u)
			{
				to = (moves & (moves-1)) ^ moves;
				moves &= moves-1;

#ifdef HISTORY_HEURISTIC
				insertMove(movelist, movevalues, from, to, history[bitToDec(from)][bitToDec(to)], movecount);
#else
				movelist[movecount<<1] = from;
				movelist[(movecount<<1)+1] = to;
				movecount++;
#endif // HISTORY_HEURISTIC
			}
		}

		/************************
		 * IF SOMEONE CAN'T MOVE
		 * THIS IS AN END NODE
		 ************************/
		// TODO:
		// Might evaluate quicker here
		if(movecount == 0)
		{
			return board.player == BLACK ? evaluate(board, depth) : -evaluate(board, depth);
		}

		/*****************
		 * FOR EACH MOVE
		 *****************/
		for(unsigned int i=0; i<movecount; i++)
		{
			Board nextboard = board;

			from = movelist[i<<1];
			to = movelist[(i<<1)+1];

			nextboard.move(from, to);

			// TODO:
			// Add scout for capture-moves
			if(capture)
			{
				tmp = captureAlphaBeta(nextboard, depth, alpha, beta, to);
			}
			else
			{
				nextboard.changePlayer();
				nextboard.updateKings();
#ifdef SCOUT
				tmp = -alphabeta(nextboard, depth+1, -testBeta, -alpha);
				if(tmp >= testBeta && testBeta < beta)
				{
					tmp = -alphabeta(nextboard, depth+1, -beta, -alpha);
				}
#else
				tmp = -alphabeta(nextboard, depth+1, -beta, -alpha);
#endif // SCOUT
			}
			if(tmp > alpha)
			{
#ifdef TRANS_TABLE
				hash_flag = FLAG_EXACT;
#endif // TRANS_TABLE
				alpha = tmp;
				if(depth == 0)
				{
					newBestMove(board, from, to);
				}
				if(alpha >= beta)
				{
#ifdef TRANS_TABLE
					trans_table->update(board, maxdepth-depth, beta, FLAG_BETA);
#endif // TRANS_TABLE
					return beta;
				}
#ifdef HISTORY_HEURISTIC
				best_from = from;
				best_to = to;
#endif
#ifdef SCOUT
				testBeta = alpha+1;
#endif // SCOUT
			}
		}

#ifdef HISTORY_HEURISTIC
		history[bitToDec(best_from)][bitToDec(best_to)] += depth*depth;
#endif // HISTORY_HEURISTIC

#ifdef TRANS_TABLE
		/*********************************
		 * UPDATE THE TRANSPOSITION TABLE
		 *********************************/
		trans_alpha = alpha;
		if(hash_flag == FLAG_EXACT)
			trans_alpha += depth;
		trans_table->update(board, maxdepth-depth, trans_alpha, hash_flag);
#endif // TRANS_TABLE 

		return alpha;
	}

	/*
	 * Help function for alphabeta
	 * used for jump-moves
	 */
	int Player::captureAlphaBeta(Board& board, int depth, int alpha, int beta, unsigned int from) {
		unsigned int moves = board.getCaptureMoves(from);
		int nrOfMoves = countBits(moves);
		unsigned int moveTo = 0x0u;
		unsigned int to = 0x0u;
		int tmp;

		if(moves == 0) {
			Board nextboard = board;
			nextboard.changePlayer();
			nextboard.updateKings();
			tmp = -alphabeta(nextboard, depth+1, -beta, -alpha);
			if(tmp > alpha) {
				alpha = tmp;
				if(depth == 0) {
					capture_movement->clear();
					capture_movement->push_back(from);
				}
			}
		}

		while(moves != 0) {
			to = (moves & (moves-1)) ^ moves;
			moves &= moves-1;

			Board nextboard = board;
			nextboard.move(from, to);

			tmp = captureAlphaBeta(nextboard, depth, alpha, beta, to);
			if(tmp > alpha) {
				alpha = tmp;
				moveTo = to;
			}
		}

		if((nrOfMoves > 1) && (depth == 0) && (capture_movement->back() != moveTo)) {
			capture_movement->push_back(moveTo);
		}

		return alpha;
	}

	/*
	 * Check if there is just one option this move
	 */
	bool Player::singleJump(Board board)
	{
		unsigned int to;
		int bits;
		unsigned int from = board.getJumpPieces();

		if(countBits(from) != 1)
			return false;

		movement->clear();
		movement->push_back(from);

		to = board.getCaptureMoves(from);
		bits = countBits(to);
		while(bits == 1) {
			board.move(from, to);
			from = to;
			to = board.getCaptureMoves(from);
			bits = countBits(to);
		}
		movement->push_back(from);

		return bits == 0;
	}

	/**
	 * Best move so far!
	 */
	void Player::newBestMove(Board& board, unsigned int from, unsigned int to)
	{
		movement->clear();
		for(unsigned int i=0; i < capture_movement->size(); i++)
		{
			movement->push_back((*capture_movement)[i]);
		}
		if(capture_movement->size() == 0)
		{
			movement->push_back(to);
		}
		else if(countBits(board.getCaptureMoves(from)) > 1 && movement->back() != to)
		{
			movement->push_back(to);
		}
		movement->push_back(from);
	}

	inline void Player::swap(unsigned int movelist[], int movevalues[], int a, int b)
	{
		int tmp = movevalues[a];
		movevalues[a] = movevalues[b];
		movevalues[b] = tmp;

		unsigned int utmp = movelist[a<<1];
		movelist[a<<1] = movelist[b<<1];
		movelist[b<<1] = utmp;

		utmp = movelist[(a<<1) +1];
		movelist[(a<<1) +1] = movelist[(b<<1) +1];
		movelist[(b<<1) +1] = utmp;
	}

	inline void Player::insertMove(unsigned int movelist[], int movevalues[], unsigned int from, unsigned int to, int newValue, unsigned int& movecount)
	{
		movevalues[movecount] = newValue;
		movelist[movecount<<1] = from;
		movelist[(movecount<<1)+1] = to;
		int pos = movecount-1;
		movecount++;
		while(pos >= 0 && movevalues[pos+1] > movevalues[pos])
		{
			swap(movelist, movevalues, pos+1, pos);
			pos--;
		}
	}

	int Player::getTime()
	{
		return timer->getTotalTime();
	}
}