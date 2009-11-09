/* gomoku - 5 in a row game		Author: ? */

/* This program plays a very old Japanese game called GO-MOKU,
   perhaps better known as  5-in-line.   The game is played on
   a board with 19 x 19 squares, and the object of the game is
   to get 5 stones in a row.
*/

#include <sys/types.h>
#include <curses.h>
#include <ctype.h>
#include <stdlib.h>
#include <unistd.h>

/* Size of the board */
#define SIZE 19

/* Importance of attack (1..16) */
#define AttackFactor 4

/* Value of having 0, 1,2,3,4 or 5 pieces in line */
int Weight[7] = {0, 0, 4, 20, 100, 500, 0};

#define Null 0
#define Horiz 1
#define DownLeft 2
#define DownRight 3
#define Vert 4

/* The two players */
#define Empty 0
#define Cross 1
#define Nought 2

char PieceChar[Nought + 1] = {' ', 'X', '0'};

int Board[SIZE + 1][SIZE + 1];/* The board */
int Player;			/* The player whose move is next */
int TotalLines;			/* The number of Empty lines left */
int GameWon;			/* Set if one of the players has won */

int Line[4][SIZE + 1][SIZE + 1][Nought + 1];

/* Value of each square for each player */
int Value[SIZE + 1][SIZE + 1][Nought + 1];

int X, Y;			/* Move coordinates */
char Command;			/* Command from keyboard */
int AutoPlay = FALSE;		/* The program plays against itself */

_PROTOTYPE(void Initialize, (void));
_PROTOTYPE(int Abort, (char *s));
_PROTOTYPE(void WriteLetters, (void));
_PROTOTYPE(void WriteLine, (int j, int *s));
_PROTOTYPE(void WriteBoard, (int N, int *Top, int *Middle, int *Bottom));
_PROTOTYPE(void SetUpScreen, (void));
_PROTOTYPE(void GotoSquare, (int x, int y));
_PROTOTYPE(void PrintMove, (int Piece, int X, int Y));
_PROTOTYPE(void ClearMove, (void));
_PROTOTYPE(void PrintMsg, (char *Str));
_PROTOTYPE(void ClearMsg, (void));
_PROTOTYPE(void WriteCommand, (char *S));
_PROTOTYPE(void ResetGame, (int FirstGame));
_PROTOTYPE(int OpponentColor, (int Player));
_PROTOTYPE(void BlinkRow, (int X, int Y, int Dx, int Dy, int Piece));
_PROTOTYPE(void BlinkWinner, (int Piece, int X, int Y, int WinningLine));
_PROTOTYPE(int Random, (int x));
_PROTOTYPE(void Add, (int *Num));
_PROTOTYPE(void Update, (int Lin[], int Valu[], int Opponent));
_PROTOTYPE(void MakeMove, (int X, int Y));
_PROTOTYPE(int GameOver, (void));
_PROTOTYPE(void FindMove, (int *X, int *Y));
_PROTOTYPE(char GetChar, (void));
_PROTOTYPE(void ReadCommand, (int X, int Y, char *Command));
_PROTOTYPE(void InterpretCommand, (int Command));
_PROTOTYPE(void PlayerMove, (void));
_PROTOTYPE(void ProgramMove, (void));
_PROTOTYPE(int main, (void));

/* Set terminal to raw mode. */
void Initialize()
{
  srand(getpid() + 13);		/* Initialize the random seed with our pid */
  initscr();
  raw();
  noecho();
  clear();
}

/* Reset terminal and exit from the program. */
int Abort(s)
char *s;
{
  move(LINES - 1, 0);
  refresh();
  endwin();
  exit(0);
}

/* Set up the screen ----------------------------------------------- */

/* Write the letters */
void WriteLetters()
{
  int i;

  addch(' ');
  addch(' ');
  for (i = 1; i <= SIZE; i++) printw(" %c", 'A' + i - 1);
  addch('\n');
}

/* Write one line of the board */
void WriteLine(j, s)
int j;
int *s;
{
  int i;

  printw("%2d ", j);
  addch(s[0]);
  for (i = 2; i <= SIZE - 1; i++) {
	addch(s[1]);
	addch(s[2]);
  }
  addch(s[1]);
  addch(s[3]);
  printw(" %-2d\n", j);
}

/* Print the Empty board and the border */
void WriteBoard(N, Top, Middle, Bottom)
int N;
int *Top, *Middle, *Bottom;
{
  int j;

  move(1, 0);
  WriteLetters();
  WriteLine(N, Top);
  for (j = N - 1; j >= 2; j--) WriteLine(j, Middle);
  WriteLine(1, Bottom);
  WriteLetters();
}

/* Sets up the screen with an Empty board */
void SetUpScreen()
{
  int top[4], middle[4], bottom[4];

  top[0] = ACS_ULCORNER;
  top[1] = ACS_HLINE;
  top[2] = ACS_TTEE;
  top[3] = ACS_URCORNER;

  middle[0] = ACS_LTEE;
  middle[1] = ACS_HLINE;
  middle[2] = ACS_PLUS;
  middle[3] = ACS_RTEE;

  bottom[0] = ACS_LLCORNER;
  bottom[1] = ACS_HLINE;
  bottom[2] = ACS_BTEE;
  bottom[3] = ACS_LRCORNER;

  WriteBoard(SIZE, top, middle, bottom);
}

/* Show moves ----------------------------------------------- */

void GotoSquare(x, y)
int x, y;
{
  move(SIZE + 2 - y, 1 + x * 2);
}

/* Prints a move */
void PrintMove(Piece, X, Y)
int Piece;
int X, Y;
{
  move(22, 49);
  printw("%c %c %d", PieceChar[Piece], 'A' + X - 1, Y);
  clrtoeol();
  GotoSquare(X, Y);
  addch(PieceChar[Piece]);
  GotoSquare(X, Y);
  refresh();
}

/* Clears the line where a move is displayed */
void ClearMove()
{
  move(22, 49);
  clrtoeol();
}

/* Message handling ---------------------------------------------- */

/* Prints a message */
void PrintMsg(Str)
char *Str;
{
  mvprintw(23, 1, "%s", Str);
}

/* Clears the message about the winner */
void ClearMsg()
{
  move(23, 1);
  clrtoeol();
}

/* Highlights the first letter of S */
void WriteCommand(S)
char *S;
{
  standout();
  addch(*S);
  standend();
  printw("%s", S + 1);
}

/* Display the board ----------------------------------------------- */

/* Resets global variables to start a new game */
void ResetGame(FirstGame)
int FirstGame;
{
  int I, J;
  int C, D;

  SetUpScreen();
  if (FirstGame) {
	move(1, 49);
	addstr("G O M O K U");
	move(3, 49);
	WriteCommand("Newgame    ");
	WriteCommand("Quit ");
	move(5, 49);
	WriteCommand("Auto");
	move(7, 49);
	WriteCommand("Play");
	move(9, 49);
	WriteCommand("Hint");
	move(14, 60);
	WriteCommand("Left, ");
	WriteCommand("Right, ");
	move(16, 60);
	WriteCommand("Up, ");
	WriteCommand("Down");
	move(18, 60);
	standout();
	addstr("SPACE");
	move(20, 49);
	WriteCommand(" NOTE: Use Num Lock & arrows");
	standend();
	mvaddstr(14, 49, "7  8  9");
	mvaddch(15, 52, ACS_UARROW);
	mvaddch(16, 49, '4');
	addch(ACS_LARROW);
	mvaddch(16, 54, ACS_RARROW);
	addch('6');
	mvaddch(17, 52, ACS_DARROW);
	mvaddstr(18, 49, "1  2  3");
	FirstGame = FALSE;
  } else {
	ClearMsg();
	ClearMove();
  }

  /* Clear tables */
  for (I = 1; I <= SIZE; I++) for (J = 1; J <= SIZE; J++) {
		Board[I][J] = Empty;
		for (C = Cross; C <= Nought; C++) {
			Value[I][J][C] = 0;
			for (D = 0; D <= 3; D++) Line[D][I][J][C] = 0;
		}
	}

  /* Cross starts */
  Player = Cross;
  /* Total number of lines */
  TotalLines = 2 * 2 * (SIZE * (SIZE - 4) + (SIZE - 4) * (SIZE - 4));
  GameWon = FALSE;
}

int OpponentColor(Player)
int Player;
{
  if (Player == Cross)
	return Nought;
  else
	return Cross;
}

/* Blink the row of 5 stones */
void BlinkRow(X, Y, Dx, Dy, Piece)
int X, Y, Dx, Dy, Piece;
{
  int I;

  attron(A_BLINK);
  for (I = 1; I <= 5; I++) {
	GotoSquare(X, Y);
	addch(PieceChar[Piece]);
	X = X - Dx;
	Y = Y - Dy;
  }
  attroff(A_BLINK);
}

/* Prints the 5 winning stones in blinking color */
void BlinkWinner(Piece, X, Y, WinningLine)
int Piece, X, Y, WinningLine;
{
  /* Used to store the position of the winning move */
  int XHold, YHold;
  /* Change in X and Y */
  int Dx, Dy;

  /* Display winning move */
  PrintMove(Piece, X, Y);
  /* Preserve winning position */
  XHold = X;
  YHold = Y;
  switch (WinningLine) {
      case Horiz:
	{
		Dx = 1;
		Dy = 0;
		break;
	}

      case DownLeft:
	{
		Dx = 1;
		Dy = 1;
		break;
	}

      case Vert:
	{
		Dx = 0;
		Dy = 1;
		break;
	}

      case DownRight:
	{
		Dx = -1;
		Dy = 1;
		break;
	}
  }

  /* Go to topmost, leftmost */
  while (Board[X + Dx][Y + Dy] != Empty && Board[X + Dx][Y + Dy] == Piece) {
	X = X + Dx;
	Y = Y + Dy;
  }
  BlinkRow(X, Y, Dx, Dy, Piece);
  /* Restore winning position */
  X = XHold;
  Y = YHold;
  /* Go back to winning square */
  GotoSquare(X, Y);
}

/* Functions for playing a game -------------------------------- */

int Random(x)
int x;
{
  return((rand() / 19) % x);
}

/* Adds one to the number of pieces in a line */
void Add(Num)
int *Num;
{
  /* Adds one to the number.     */
  *Num = *Num + 1;
  /* If it is the first piece in the line, then the opponent cannot use
   * it any more.  */
  if (*Num == 1) TotalLines = TotalLines - 1;
  /* The game is won if there are 5 in line. */
  if (*Num == 5) GameWon = TRUE;
}

/* Updates the value of a square for each player, taking into
   account that player has placed an extra piece in the square.
   The value of a square in a usable line is Weight[Lin[Player]+1]
   where Lin[Player] is the number of pieces already placed
in the line */
void Update(Lin, Valu, Opponent)
int Lin[];
int Valu[];
int Opponent;
{
  /* If the opponent has no pieces in the line, then simply update the
   * value for player */
  if (Lin[Opponent] == 0)
	Valu[Player] += Weight[Lin[Player] + 1] - Weight[Lin[Player]];
  else
	/* If it is the first piece in the line, then the line is
	 * spoiled for the opponent */
  if (Lin[Player] == 1) Valu[Opponent] -= Weight[Lin[Opponent] + 1];
}

/* Performs the move X,Y for player, and updates the global variables
(Board, Line, Value, Player, GameWon, TotalLines and the screen) */
void MakeMove(X, Y)
int X, Y;
{
  int Opponent;
  int X1, Y1;
  int K, L, WinningLine;

  WinningLine = Null;
  Opponent = OpponentColor(Player);
  GameWon = FALSE;

  /* Each square of the board is part of 20 different lines. The adds
   * one to the number of pieces in each of these lines. Then it
   * updates the value for each of the 5 squares in each of the 20
   * lines. Finally Board is updated, and the move is printed on the
   * screen. */

  /* Horizontal lines, from left to right */
  for (K = 0; K <= 4; K++) {
	X1 = X - K;		/* Calculate starting point */
	Y1 = Y;
	if ((1 <= X1) && (X1 <= SIZE - 4)) {	/* Check starting point */
		Add(&Line[0][X1][Y1][Player]);	/* Add one to line */
		if (GameWon && (WinningLine == Null))	/* Save winning line */
			WinningLine = Horiz;
		for (L = 0; L <= 4; L++)	/* Update value for the
						 * 5 squares in the line */
			Update(Line[0][X1][Y1], Value[X1 + L][Y1], Opponent);
	}
  }

  for (K = 0; K <= 4; K++) {	/* Diagonal lines, from lower left to
				 * upper right */
	X1 = X - K;
	Y1 = Y - K;
	if ((1 <= X1) && (X1 <= SIZE - 4) &&
	    (1 <= Y1) && (Y1 <= SIZE - 4)) {
		Add(&Line[1][X1][Y1][Player]);
		if (GameWon && (WinningLine == Null))	/* Save winning line */
			WinningLine = DownLeft;
		for (L = 0; L <= 4; L++)
			Update(Line[1][X1][Y1], Value[X1 + L][Y1 + L], Opponent);
	}
  }				/* for */

  for (K = 0; K <= 4; K++) {	/* Diagonal lines, down right to upper left */
	X1 = X + K;
	Y1 = Y - K;
	if ((5 <= X1) && (X1 <= SIZE) &&
	    (1 <= Y1) && (Y1 <= SIZE - 4)) {
		Add(&Line[3][X1][Y1][Player]);
		if (GameWon && (WinningLine == Null))	/* Save winning line */
			WinningLine = DownRight;
		for (L = 0; L <= 4; L++)
			Update(Line[3][X1][Y1], Value[X1 - L][Y1 + L], Opponent);
	}
  }				/* for */

  for (K = 0; K <= 4; K++) {	/* Vertical lines, from down to up */
	X1 = X;
	Y1 = Y - K;
	if ((1 <= Y1) && (Y1 <= SIZE - 4)) {
		Add(&Line[2][X1][Y1][Player]);
		if (GameWon && (WinningLine == Null))	/* Save winning line */
			WinningLine = Vert;
		for (L = 0; L <= 4; L++)
			Update(Line[2][X1][Y1], Value[X1][Y1 + L], Opponent);
	}
  }

  Board[X][Y] = Player;		/* Place piece in board */
  if (GameWon)
	BlinkWinner(Player, X, Y, WinningLine);
  else
	PrintMove(Player, X, Y);/* Print move on screen */
  Player = Opponent;		/* The opponent is next to move */
}

int GameOver()
/* A game is over if one of the players have
won, or if there are no more Empty lines */
{
  return(GameWon || (TotalLines <= 0));
}

/* Finds a move X,Y for player, simply by picking the one with the
highest value */
void FindMove(X, Y)
int *X, *Y;
{
  int Opponent;
  int I, J;
  int Max, Valu;

  Opponent = OpponentColor(Player);
  Max = -10000;
  /* If no square has a high value then pick the one in the middle */
  *X = (SIZE + 1) / 2;
  *Y = (SIZE + 1) / 2;
  if (Board[*X][*Y] == Empty) Max = 4;
  /* The evaluation for a square is simply the value of the square for
   * the player (attack points) plus the value for the opponent
   * (defense points). Attack is more important than defense, since it
   * is better to get 5 in line yourself than to prevent the op- ponent
   * from getting it. */

  /* For all Empty squares */
  for (I = 1; I <= SIZE; I++) for (J = 1; J <= SIZE; J++)
		if (Board[I][J] == Empty) {
			/* Calculate evaluation */
			Valu = Value[I][J][Player] * (16 + AttackFactor) / 16 + Value[I][J][Opponent] + Random(4);
			/* Pick move with highest value */
			if (Valu > Max) {
				*X = I;
				*Y = J;
				Max = Valu;
			}
		}
}

char GetChar()
/* Get a character from the keyboard */
{
  int c;

  c = getch();
  if (c < 0) abort();
  if (c == '\033') {	/* arrow key */
	if ((c = getch()) == '[') {
		c = getch();
		switch (c) {
		case 'A': c = 'U'; break;
		case 'B': c = 'D'; break;
		case 'C': c = 'R'; break;
		case 'D': c = 'L'; break;
		default:
			c = '?';
			break;
		}
	}
	else
		c = '?';
  }
  if (islower(c))
	return toupper(c);
  else
	return c;
}

/* Reads in a valid command character */
void ReadCommand(X, Y, Command)
int X, Y;
char *Command;
{
  int ValidCommand;

  do {
	ValidCommand = TRUE;
	GotoSquare(X, Y);	/* Goto square */
	refresh();
	*Command = GetChar();	/* Read from keyboard */
	switch (*Command) {
	    case '\n':		/* '\n', '\r' or space means place a */
	    case '\r':
	    case ' ':
		*Command = 'E';
		break;		/* stone at the cursor position  */

	    case 'L':
	    case 'R':
	    case 'U':
	    case 'D':
	    case '7':
	    case '9':
	    case '1':
	    case '3':
	    case 'N':
	    case 'Q':
	    case 'A':
	    case 'P':
	    case 'H':
		break;

	    case '8':	*Command = 'U';	break;
	    case '2':	*Command = 'D';	break;
	    case '4':	*Command = 'L';	break;
	    case '6':	*Command = 'R';	break;
	    default:
		{
			if (GameOver())
				*Command = 'P';
			else
				ValidCommand = FALSE;
			break;
		}
	}
  } while (!ValidCommand);
}

void InterpretCommand(Command)
char Command;
{
  int Temp;

  switch (Command) {
      case 'N':{		/* Start new game */
		ResetGame(FALSE);	/* ResetGame but only redraw
					 * the board */
		X = (SIZE + 1) / 2;
		Y = X;
		break;
	}
      case 'H':
	FindMove(&X, &Y);
	break;			/* Give the user a hint */
      case 'L':
	X = (X + SIZE - 2) % SIZE + 1;
	break;			/* Left  */
      case 'R':
	X = X % SIZE + 1;
	break;			/* Right */
      case 'D':
	Y = (Y + SIZE - 2) % SIZE + 1;
	break;			/* Down  */
      case 'U':
	Y = Y % SIZE + 1;
	break;			/* Up    */
      case '7':{
		if ((X == 1) || (Y == SIZE)) {	/* Move diagonally    *//* t
						 * owards upper left */
			Temp = X;
			X = Y;
			Y = Temp;
		} else {
			X = X - 1;
			Y = Y + 1;
		}
		break;
	}
      case '9':{		/* Move diagonally    */
		if (X == SIZE) {/* toward upper right */
			X = (SIZE - Y) + 1;
			Y = 1;
		} else if (Y == SIZE) {
			Y = (SIZE - X) + 1;
			X = 1;
		} else {
			X = X + 1;
			Y = Y + 1;
		}
		break;
	}
      case '1':{		/* Move diagonally   */
		if (Y == 1) {	/* toward lower left */
			Y = (SIZE - X) + 1;
			X = SIZE;
		} else if (X == 1) {
			X = (SIZE - Y) + 1;
			Y = SIZE;
		} else {
			X = X - 1;
			Y = Y - 1;
		}
		break;
	}
      case '3':{		/* Move diagonally    */
		if ((X == SIZE) || (Y == 1)) {	/* toward lower right */
			Temp = X;
			X = Y;
			Y = Temp;
		} else {
			X = X + 1;
			Y = Y - 1;
		}
		break;
	}
      case 'A':
	AutoPlay = TRUE;
	break;			/* Auto play mode */
  }				/* case */
}				/* InterpretCommand */

void PlayerMove()
/* Enter and make a move */
{
  if (Board[X][Y] == Empty) {
	MakeMove(X, Y);
	if (GameWon) PrintMsg("Congratulations, You won!");
	Command = 'P';
  }
  refresh();
}				/* PlayerMove */

void ProgramMove()
/* Find and perform programs move */
{
  do {
	if (GameOver()) {
		AutoPlay = FALSE;
		if ((Command != 'Q') && (!GameWon)) PrintMsg("Tie game!");
	} else {
		FindMove(&X, &Y);
		MakeMove(X, Y);
		if (GameWon) PrintMsg("I won!");
	}
	refresh();
  } while (AutoPlay);
}

int main()
{
  Initialize();
  ResetGame(TRUE);		/* ResetGame and draw the entire screen */
  refresh();
  X = (SIZE + 1) / 2;		/* Set starting position to */
  Y = X;			/* the middle of the board  */
  do {
	ReadCommand(X, Y, &Command);
	if (GameOver())
		if (Command != 'Q') Command = 'N';
	InterpretCommand(Command);
	if (Command == 'E') PlayerMove();
	if (Command == 'P' || Command == 'A') ProgramMove();
  } while (Command != 'Q');
  Abort("Good bye!");
  return(0);
}
