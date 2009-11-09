/**
    SCORE

   Calculate what the player's score would be if he quit now.
   This may be the end of the game, or he may just be wondering
   how he is doing.

   The present scoring algorithm is as follows:
   (treasure points are explained in a following comment)
      objective:          points:        present total possible:
   getting well into cave   25                    25
   total possible for treasures (+mag)           426
   reaching "closing"       20                    20
   "closed": quit/killed    10
             klutzed        20
             wrong way      25
             success        30                    30
				total:   501
   (points can also be deducted for using hints or deaths.)

*/

#include 	<stdio.h>
#include	"advent.h"
#include	"advdec.h"

void score(scorng)
boolean scorng;
{
    int cur_score, max_score, qk[3];
    int obj, num_treas, k, i;
    long t;
    char *kk2c;

    cur_score = 0;
    max_score = 0;
    num_treas = 0;

/** First tally up the treasures.  Must be in building and not broken.
   give the poor guy partial score just for finding each treasure.
   Gets full score, qk[3], for obj if:
        obj is at loc qk[1], and
        obj has prop value of qk[2]

                weight          total possible
   magazine     1 (absolute)            1

   all the following are multiplied by 5 (range 5-25):
   book         2
   cask         3 (with wine only)
   chain        4 (must enter via styx)
   chest        5
   cloak        3
   clover       1
   coins        5
   crown        2
   crystal-ball 2
   diamonds     2
   eggs         3
   emerald      3
   grail        2
   horn         2
   jewels       1
   lyre         1
   nugget       2
   pearl        4
   pyramid      4
   radium	4
   ring         4
   rug          3
   sapphire     1
   shoes        3
   spices       1
   sword        4
   trident      2
   vase         2
   droplet	5
   tree		5
        total: 85 * 5 = 425 + 1 ==> 426
*/

    for (obj = 1; obj < MAXOBJ; obj++) {
	if (g.points[obj] == 0)
	    continue;
	t = g.points[obj];
 	qk[0] = (int) (t < 0L ? -((t = -t) % 1000) : (t % 1000));
 	t /= 1000;
	qk[1] = (int) (t % 1000);
	qk[2] = (int) (t / 1000);
	k = 0;
	if (treasr(obj)) {
	    num_treas++;
	    k = qk[2] * 2;
	    if (g.prop[obj] >= 0)
		cur_score += k;
	    qk[2] *= 5;
	}
	if ((g.place[obj] == qk[0]) && (g.prop[obj] == qk[1])
	    && ((g.place[obj] != -CHEST) || (g.place[CHEST] == 3))
	  && ((g.place[obj] != -SHIELD) || (g.place[SHIELD] == -SAFE))
	    )
	    cur_score += qk[2] - k;
	max_score += qk[2];
    }


/**
   Now look at how he finished and how far he got.  Maxdie and numdie tell us
   how well he survived.  Gaveup says whether he exited via quit.  Dflag will
   tell us if he ever got suitably deep into the cave.  Closing still indicates
   whether he reached the endgame.  And if he got as far as "cave closed"
   (indicated by "closed"), then bonus is zero for mundane exits or 133, 134,
   135 if he blew it (so to speak).
*/

    if (g.dflag)
	cur_score += 25;
    max_score += 25;
    if (g.closing)
	cur_score += 20;
    max_score += 20;
    if (g.closed) {
	if (g.bonus == 0)
	    cur_score += 10;
	else if (g.bonus == 135)
	    cur_score += 20;
	else if (g.bonus == 134)
	    cur_score += 25;
	else if (g.bonus == 133)
	    cur_score += 30;
    }
    max_score += 30;

/*  Deduct points for hints, deaths and quiting.
    hints < hntmin are special; see database description
*/
    for (i = 1; i <= HNTMAX; i++)
	if (g.hinted[i])
	    cur_score -= g.hints[i][2];
    cur_score -= g.numdie * 10;
    if (gaveup)
	cur_score -= 4;

    fprintf(stdout, "You have found   %3d out of %3d Treasures,",
	    num_treas - g.tally, num_treas);
    fprintf(stdout, " using %4d turns\n", g.turns);
    fprintf(stdout, "For a score of: %4d", cur_score);
    fprintf(stdout, " out of a possible %4d\n", max_score);

    if (cur_score < 110) {
	fprintf(stdout, "You are obviously a rank amateur.");
	if (!scorng)
	    fprintf(stdout, "  Better luck next time.");
	fputc('\n', stdout);
	k = 110 - cur_score;
    } else if (cur_score < 152) {
	fprintf(stdout,
	  "Your score qualifies you as a Novice Class Adventurer.\n");
	k = 152 - cur_score;
    } else if (cur_score < 200) {
	fprintf(stdout,
	"You have achieved the rating: \"Experienced Adventurer\".\n");
	k = 200 - cur_score;
    } else if (cur_score < 277) {
	fprintf(stdout,
	"You may now consider yourself a \"Seasoned Adventurer\".\n");
	k = 277 - cur_score;
    } else if (cur_score < 345) {
	fprintf(stdout,
		"You have reached \"Junior Master\" status.\n");
	k = 345 - cur_score;
    } else if (cur_score < 451) {
	fprintf(stdout,
		"Your score puts you in Master Adventurer Class C.\n");
	k = 451 - cur_score;
    } else if (cur_score < 471) {
	fprintf(stdout,
		"Your score puts you in Master Adventurer Class B.\n");
	k = 471 - cur_score;
    } else if (cur_score < 501) {
	fprintf(stdout,
		"Your score puts you in Master Adventurer Class A.\n");
	k = 501 - cur_score;
    } else {
	fprintf(stdout,
		"All of Adventuredom gives tribute to you, Adventurer Grandmaster!\n");
	k = 0;
    }

    if (!scorng) {
	kk2c = (k == 1) ? "." : "s.";
	printf("\nTo acheive the next higher rating,");
	if (cur_score == 501)
	    printf(" would be a neat trick!\n\n  CONGRATULATIONS!!\n");
	else
	    printf(" you need %3d more point%s\n", k, kk2c);
    }
    return;
}
