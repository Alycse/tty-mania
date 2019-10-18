#include <ncurses.h>
#include <stdlib.h>
#include <sys/time.h>
#include <curses.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <ctype.h>
#include <vlc/vlc.h>
#include <math.h>
#include <getopt.h>

enum {laneA, laneB, laneC, laneD} Lane;
enum {white, noteADColor, noteBCColor, keyColor, perfectKeyColor, greatKeyColor, booKeyColor, missKeyColor,
    keyPressedColor, keyTopColor, statBoardTextColor, statBoardBgColor} Color;

#define RGB_MULTIPLIER 3.92
#define COLOR_NOTE_AD 240 * RGB_MULTIPLIER, 240 * RGB_MULTIPLIER, 240 * RGB_MULTIPLIER
#define COLOR_NOTE_BC 255 * RGB_MULTIPLIER, 153 * RGB_MULTIPLIER, 255 * RGB_MULTIPLIER

#define COLOR_KEY 170 * RGB_MULTIPLIER, 170 * RGB_MULTIPLIER, 170 * RGB_MULTIPLIER

#define COLOR_PERFECT_KEY 227 * RGB_MULTIPLIER, 220 * RGB_MULTIPLIER, 89 * RGB_MULTIPLIER
#define COLOR_GREAT_KEY 89 * RGB_MULTIPLIER, 227 * RGB_MULTIPLIER, 102 * RGB_MULTIPLIER
#define COLOR_BOO_KEY 165 * RGB_MULTIPLIER, 89 * RGB_MULTIPLIER, 227 * RGB_MULTIPLIER
#define COLOR_MISS_KEY 219 * RGB_MULTIPLIER, 55 * RGB_MULTIPLIER, 55 * RGB_MULTIPLIER

#define COLOR_PRESSED_KEY 235 * RGB_MULTIPLIER, 235 * RGB_MULTIPLIER, 235 * RGB_MULTIPLIER
#define COLOR_KEY_TOP 140 * RGB_MULTIPLIER, 140 * RGB_MULTIPLIER, 140 * RGB_MULTIPLIER

#define COLOR_STAT_BOARD_TEXT 235 * RGB_MULTIPLIER, 235 * RGB_MULTIPLIER, 235 * RGB_MULTIPLIER
#define COLOR_STAT_BOARD_BG 50 * RGB_MULTIPLIER, 39 * RGB_MULTIPLIER, 61 * RGB_MULTIPLIER

typedef struct note{
    int lane;
    double posY;
    double hitTime;
} note;
typedef struct key{
    double pressedTime;
} key;

int setupScreen();
int setupLanePos();
int setupGame();
int drawKeyTop();
int drawNote(int lane, int posY, bool erase);
int drawKey(int lane, int color);
int handleInput(char* input);
int kbhit(void);
int newNote(note** p_p_notes, int* p_notesSize, int lane, double hitTime, double posY);
int deleteNote(note** notes, int* notesSize, int noteIndex);
int dequeueNote(note** p_p_queuedNotes, int* p_queuedNotesSize, int noteIndex);
int update(int *p_notesSize, key *p_keys, double *p_deltaTime,
    char *p_input, double *p_elapsedTime, double noteFallHeightDivSpeed, note **p_p_notes,
    note **p_p_queuedNotes, int *p_queuedNotesSize, int *p_playerScore, int *p_playerCombo);
int updateQueuedNotes(double* p_elapsedTime, double noteFallHeightDivSpeed, int* p_notesSize,
    note** p_p_notes, note** p_p_queuedNotes, int* p_queuedNotesSize);
int updateNotes(note **p_p_notes, int* p_notesSize, double* p_deltaTime, int lanePosPressedIndex,
    double* p_elapsedTime, key* p_keys, int *p_playerScore, int *p_playerCombo);
int updateKeys(key *p_keys, double *p_deltaTime, char *p_input, note **p_p_notes, int *p_notesSize, double *p_elapsedTime,
    int *p_playerScore, int *p_playerCombo);
long long getCurTime(void);
int queueNote(note** p_p_queuedNotes, int* p_queuedNotesSize, int lane, double hitTime);
int loadOsuFile(note ***p_p_p_queuedNotes, int **p_p_queuedNotesSize);
int startMusic(libvlc_instance_t **p_p_vlcInstance, libvlc_media_player_t **p_p_vlcMp, libvlc_media_t **p_p_vlcM, char musicFile[]);
int stopMusic(libvlc_instance_t **p_p_vlcInstance, libvlc_media_player_t **p_p_vlcMp);
int addScore(int *p_playerScore, int additionalScore);
int addCombo(int *p_playerCombo);
int resetCombo(int *p_playerCombo);
int printAccuracyText(char accuracyText[], int color);
int drawStatBoard();
int setupStatBoardDimensions();

const int noteWidth = 10;
const int noteSpawnPosY = 5;
const int chartHeight = 57;
const int noteSpaceHeight = 68;
const int keysCount = 4;
const double keyPressedTimeSec = 0.1;

const double minPerfectAccuracy = 0.06;
const double minGreatAccuracy = 0.08;
const double minBooAccuracy = 0.1;

const int perfectScore = 100;
const int greatScore = 60;
const int booScore = 20;

char blankText[] = "         ";
char perfectText[] = "PERFECT!";
char greatText[] = "Great!";
char booText[] = "Boo!";
char missText[] = "Miss!";