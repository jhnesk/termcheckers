#ifndef TRANSPOSITION_H
#define TRANSPOSITION_H

//#define TABLE_SIZE 1000003	// ~ 26MB / player
#define TABLE_SIZE 2097593	// ~ 56MB / player
//#define TABLE_SIZE 12500003	// ~ 333 / player
#define TRANS_NULL 64000

#define FLAG_EXACT 0
#define FLAG_ALPHA 1
#define FLAG_BETA 2

#include <vector>
#include "board.h"

namespace checkers {
	class Board;

	struct Position
	{
		int depth;
		int value;
		int flag;
		int movecount;
		Board board;
	};

	class TranspositionTable
	{
		public:
			TranspositionTable();
			~TranspositionTable();
			void update(Board& board, int depth, int value, int flag, int movecount);
			int get(Board& board, int depth, int alpha, int beta);
			// debug values:
			int new_values;
			int collisions;
			int no_updates;
			int used_values;
			int total_get;
			int size;
		private:
			inline unsigned int hash(Board& board);

			unsigned int zobristvalues[161];
			Position* table;
	};
}

#endif // TRANPOSITION_H
