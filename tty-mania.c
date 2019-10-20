#include "tty-mania.h"
#include "config.h"

int kbhit(void) {
    int ch = getch();

    if (ch != ERR) {
        ungetch(ch);
        return 1;
    } else {
        return 0;
    }
}

long long getCurTime(void){
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (((long long)tv.tv_sec)*1000)+(tv.tv_usec/1000);
}

int g_screenHeight, g_screenWidth;
int g_lanePos[8];
int g_statBoardDimensions[4];

int setupLanePos(){
    if(g_screenWidth > 0 && g_screenHeight > 0){
        g_lanePos[3] = g_screenWidth/2 - 1;
        g_lanePos[2] = g_lanePos[3] - noteWidth;

        g_lanePos[1] = g_lanePos[2] - 2;
        g_lanePos[0] = g_lanePos[1] - noteWidth;

        g_lanePos[4] = g_screenWidth/2 + 1;
        g_lanePos[5] = g_lanePos[4]  + noteWidth;

        g_lanePos[6] = g_lanePos[5] + 2;
        g_lanePos[7] =  g_lanePos[6] + noteWidth;

        return 1;
    }else{
        return 0;
    }
}

int setupStatBoardDimensions(){
    //left
    g_statBoardDimensions[0] = g_lanePos[0] - 26;
    //right
    g_statBoardDimensions[1] = g_lanePos[0] - 4;
    //top
    g_statBoardDimensions[2] = chartHeight - 18;
    //bottom
    g_statBoardDimensions[3]  = chartHeight - 10;
}

double g_noteSpeed = 70;
char g_osuFile[128] = "ttb.osu";
char g_musicFile[128] = "ttb.mp3";
double g_beatmapOffset = 2300;
double g_songStartDelay = 1.9;

int main(int argc, char **argv){
    int opt;
    int c;
    while ((c = getopt (argc, argv, "hs:b:m:")) != -1) {
        switch (c){
            case 'h':
                printf("Key Controls: 'D', 'F', 'J', 'K'\n");
                printf("Press 'Q' to exit\n");
                printf("-h : Help\n");
                printf("-s : Speed [Default: 70]\n");
                printf("-b : Beatmap File (.osu) [Default: ttb.osu]\n");
                printf("-m : Audio File [Default: ttb.mp3]\n");
                exit(1);
                break;
            case 's':
                g_noteSpeed = atof(optarg);
                if(!g_noteSpeed){
                    printf("Invalid speed!\n");
                    exit(-1);
                } else if(g_noteSpeed < 10 || g_noteSpeed > 300){
                    printf("Speed must be in between 10 and 300!\n");
                    exit(-1);
                }
                break;
            case 'b':
                strcpy(g_osuFile, optarg);
                break;
            case 'm':
                strcpy(g_musicFile, optarg);
                break;
            default:
                abort ();
        }
    }

    char input;
    char *p_input = &input;

    note *p_notes = NULL;
    note **p_p_notes = &p_notes;
    int notesSize;
    int *p_notesSize = &notesSize;
    *p_notesSize = 0;
    
    note *p_queuedNotes = NULL;
    note **p_p_queuedNotes = &p_queuedNotes;
    int queuedNotesSize;
    int *p_queuedNotesSize = &queuedNotesSize;
    *p_queuedNotesSize = 0;
    int **p_p_queuedNotesSize = &p_queuedNotesSize;

    key keys;
    key* p_keys = &keys;
    p_keys = malloc(sizeof(key) * keysCount);

    int playerCombo = 0;
    int* p_playerCombo = &playerCombo;
    int playerScore = 0;
    int* p_playerScore = &playerScore;

    double noteFallHeightDivSpeed = (chartHeight - noteSpawnPosY) / (double)g_noteSpeed;

    FILE *fp = fopen(g_osuFile, "r");
    if(fp == NULL){
        printf("Can't find osu file: %s\n", g_osuFile);
        refresh();
        exit(-1);
    }
    
    char line[128];
    bool reachedHitObjects = 0;
    int counter = 0;
    while(fgets(line, sizeof (line), fp) != NULL){
        if(reachedHitObjects){
            char *array[128];
            int i = 0;
            array[i] = strtok(line, ",");
            while(array[i] != NULL){
                array[++i] = strtok(NULL, ",");
            }

            double hitTimeMs = (atof(array[2]) + g_beatmapOffset) / 1000.0;
            if(atoi(array[0]) == 64){
                queueNote(p_p_queuedNotes, p_queuedNotesSize, laneA, hitTimeMs);
            }else if(atoi(array[0]) == 192){
                queueNote(p_p_queuedNotes, p_queuedNotesSize, laneB, hitTimeMs);
            }else if(atoi(array[0]) == 320){
                queueNote(p_p_queuedNotes, p_queuedNotesSize, laneC, hitTimeMs);
            }else if(atoi(array[0]) == 448){
                queueNote(p_p_queuedNotes, p_queuedNotesSize, laneD, hitTimeMs);
            }
        } else if(strcmp(line, "[HitObjects]")){
            reachedHitObjects = 1;
        }
    }
    fclose(fp);

    libvlc_instance_t *p_vlcInstance;
    libvlc_instance_t **p_p_vlcInstance = &p_vlcInstance;
    libvlc_media_player_t *p_vlcMp;
    libvlc_media_player_t **p_p_vlcMp = &p_vlcMp;
    libvlc_media_t *p_vlcM;
    libvlc_media_t **p_p_vlcM = &p_vlcM;

    setupScreen();
    setupGame();

    drawStatBoard();

    resetCombo(p_playerCombo);
    addScore(p_playerScore, 0);

    drawKey(laneA, keyColor);
    drawKey(laneB, keyColor);
    drawKey(laneC, keyColor);
    drawKey(laneD, keyColor);
    drawKeyTop();

    refresh();

    long prevTime;
    long *p_prevTime = &prevTime;
    *p_prevTime = getCurTime();

    double elapsedTime = 0;
    double *p_elapsedTime = &elapsedTime;

    double deltaTime;
    double *p_deltaTime = &deltaTime;
    long prevMusicTime = 0;

    bool musicStarted = false;
    while (*p_input == '\0' || toupper(*p_input) != QUIT){
        if(kbhit()){
            *p_input = getch();
        }else{
            *p_input = '\0';
        }

        *p_deltaTime = (getCurTime() - *p_prevTime)/1000.0;
        *p_prevTime = getCurTime();

        *p_elapsedTime += *p_deltaTime;

        if(!musicStarted){
            if(*p_elapsedTime >= g_songStartDelay){
                startMusic(p_p_vlcInstance, p_p_vlcMp, p_p_vlcM, g_musicFile);
                musicStarted = true;
            }
        }

        update(p_notesSize, p_keys, p_deltaTime, p_input, p_elapsedTime,
            noteFallHeightDivSpeed, p_p_notes,
            p_p_queuedNotes, p_queuedNotesSize, p_playerScore, p_playerCombo);

        usleep(1200);
    }

    standend();

    if(musicStarted){
        stopMusic(p_p_vlcInstance, p_p_vlcMp);
    }
    

    endwin();

    return 0;
}

int startMusic(libvlc_instance_t **p_p_vlcInstance, libvlc_media_player_t **p_p_vlcMp, libvlc_media_t **p_p_vlcM, char musicFile[]){
    *p_p_vlcInstance = libvlc_new(0, NULL);
    *p_p_vlcM = libvlc_media_new_path(*p_p_vlcInstance, musicFile);
    *p_p_vlcMp = libvlc_media_player_new_from_media(*p_p_vlcM);
    libvlc_media_release(*p_p_vlcM);

    return libvlc_media_player_play(*p_p_vlcMp);
}
int stopMusic(libvlc_instance_t **p_p_vlcInstance, libvlc_media_player_t **p_p_vlcMp){
    libvlc_media_player_stop(*p_p_vlcMp);
    libvlc_media_player_release(*p_p_vlcMp);
    libvlc_release(*p_p_vlcInstance);

    return 1;
}

int update(int *p_notesSize, key *p_keys, double *p_deltaTime,
    char *p_input, double *p_elapsedTime, double noteFallHeightDivSpeed, note **p_p_notes,
    note **p_p_queuedNotes, int *p_queuedNotesSize, int *p_playerScore, int *p_playerCombo){

    updateQueuedNotes(p_elapsedTime, noteFallHeightDivSpeed, p_notesSize, p_p_notes, p_p_queuedNotes, p_queuedNotesSize);
    
    updateKeys(p_keys, p_deltaTime, p_input, p_p_notes, p_notesSize, p_elapsedTime, p_playerScore, p_playerCombo);

    return 1;
}

int updateQueuedNotes(double *p_elapsedTime, double noteFallHeightDivSpeed, int *p_notesSize,
    note **p_p_notes, note **p_p_queuedNotes, int *p_queuedNotesSize){
      
    for(int i = 0; i < *p_queuedNotesSize; i++){
        if(*p_elapsedTime >= (*p_p_queuedNotes)[i].hitTime - noteFallHeightDivSpeed){
            double offset = (*p_elapsedTime - ((*p_p_queuedNotes)[i].hitTime - noteFallHeightDivSpeed)) * g_noteSpeed;

            newNote(p_p_notes, p_notesSize, (*p_p_queuedNotes)[i].lane, (*p_p_queuedNotes)[i].hitTime, 
                noteSpawnPosY + offset);
            dequeueNote(p_p_queuedNotes, p_queuedNotesSize, i);
        }
    }
}

int updateKeys(key *p_keys, double *p_deltaTime, char *p_input, note **p_p_notes, int *p_notesSize, double *p_elapsedTime,
    int *p_playerScore, int *p_playerCombo){
    int lanePosPressedIndex = -1;
    if(*p_input){
        switch (toupper(*p_input)){
            case KEY_LANE_A:
                lanePosPressedIndex = 0;
                break;
            case KEY_LANE_B:
                lanePosPressedIndex = 1;
                break;
            case KEY_LANE_C:
                lanePosPressedIndex = 2;
                break;
            case KEY_LANE_D:
                lanePosPressedIndex = 3;
                break;
        }
    }
    
    for(int i = 0; i < keysCount; i++){
        if(i == lanePosPressedIndex){
            p_keys[i].pressedTime = keyPressedTimeSec;
            drawKey(i, keyPressedColor);
        }
        if(p_keys[i].pressedTime > 0){
            p_keys[i].pressedTime -= (*p_deltaTime);
        }else{
            drawKey(i, keyColor);
        }
    }

    updateNotes(p_p_notes, p_notesSize, p_deltaTime, lanePosPressedIndex, p_elapsedTime, p_keys, p_playerScore, p_playerCombo);
}

int updateNotes(note **p_p_notes, int* p_notesSize, double* p_deltaTime, int lanePosPressedIndex,
    double* p_elapsedTime, key* p_keys, int *p_playerScore, int *p_playerCombo){
    int laneTaken[] = { 0, 0, 0, 0 };
    for(int i = 0; i < *p_notesSize; i++){
        int oldIntPosY = (int) ((*p_p_notes)[i].posY);
        (*p_p_notes)[i].posY = ((*p_p_notes)[i].posY) + (*p_deltaTime * g_noteSpeed);
        int noteLane = (*p_p_notes)[i].lane;
        
        if((*p_p_notes)[i].posY >= noteSpaceHeight){
            p_keys[noteLane].pressedTime = keyPressedTimeSec;
            resetCombo(p_playerCombo);
            printAccuracyText (missText, missKeyColor);
            drawKey(noteLane, missKeyColor);

            if(oldIntPosY < chartHeight){
                drawNote(noteLane, oldIntPosY, true);
            }
            deleteNote(p_p_notes, p_notesSize, i);
        } else { 
            int notePosY = (int) ((*p_p_notes)[i].posY);
            
            if(oldIntPosY != notePosY && oldIntPosY < chartHeight){
                drawNote(noteLane, oldIntPosY, true);
            }

            if(!laneTaken[noteLane] && noteLane == lanePosPressedIndex){
                laneTaken[noteLane] = 1;
                double noteAccuracy = fabs((*p_p_notes)[i].hitTime - *p_elapsedTime);

                p_keys[noteLane].pressedTime = keyPressedTimeSec;
                
                if(noteAccuracy <= minPerfectAccuracy){
                    printAccuracyText (perfectText, perfectKeyColor);
                    addCombo(p_playerCombo);
                    addScore(p_playerScore, perfectScore);
                    drawKey(noteLane, perfectKeyColor);
                }else if(noteAccuracy <= minGreatAccuracy){
                    printAccuracyText (greatText, greatKeyColor);
                    addCombo(p_playerCombo);
                    addScore(p_playerScore, greatScore);
                    drawKey(noteLane, greatKeyColor);
                }else if(noteAccuracy <= minBooAccuracy){
                    printAccuracyText (booText, booKeyColor);
                    addCombo(p_playerCombo);
                    addScore(p_playerScore, booScore);
                    drawKey(noteLane, booKeyColor);
                }

                if(noteAccuracy <= minBooAccuracy){
                    if(notePosY < chartHeight){
                        drawNote(noteLane, notePosY, true);
                    }
                    deleteNote(p_p_notes, p_notesSize, i);
                }
            }else if (notePosY < chartHeight){
                drawNote(noteLane, notePosY, false);
            }
        }
    }
}

int newNote(note** p_p_notes, int *p_notesSize, int lane, double hitTime, double posY){
    *p_notesSize += 1;
    *p_p_notes = (note*) realloc(*p_p_notes, (*p_notesSize) * sizeof(note));
    (*p_p_notes)[*p_notesSize-1].lane = lane;
    (*p_p_notes)[*p_notesSize-1].posY = posY;
    (*p_p_notes)[*p_notesSize-1].hitTime = hitTime;
    return 1;
}
int deleteNote(note** p_p_notes, int *p_notesSize, int noteIndex){
    *p_notesSize -= 1;
    for(int i = noteIndex; i < *p_notesSize; i++){
        *(*p_p_notes + i) = *(*p_p_notes + i + 1);
    }
    *p_p_notes = (note*) realloc(*p_p_notes, *p_notesSize * sizeof(note));
    return 1;
}

int queueNote(note **p_p_queuedNotes, int *p_queuedNotesSize, int lane, double hitTime){
    *p_queuedNotesSize += 1;
    *p_p_queuedNotes = (note*) realloc(*p_p_queuedNotes, (*p_queuedNotesSize) * sizeof(note));
    (*p_p_queuedNotes)[*p_queuedNotesSize-1].lane = lane;
    (*p_p_queuedNotes)[*p_queuedNotesSize-1].hitTime = hitTime;
    return 1;
}
int dequeueNote(note **p_p_queuedNotes, int *p_queuedNotesSize, int noteIndex){
    *p_queuedNotesSize -= 1;
    for(int i = noteIndex; i < *p_queuedNotesSize; i++){
        *(*p_p_queuedNotes + i) = *(*p_p_queuedNotes + i + 1);
    }
    *p_p_queuedNotes = (note*) realloc(*p_p_queuedNotes, *p_queuedNotesSize * sizeof(note));
    return 1;
}

int drawKeyTopLane(int lane){
    attron(COLOR_PAIR(keyTopColor));
    for(int j = *(g_lanePos + (lane * 2)); j <= *(g_lanePos + (lane * 2) + 1); j++){
        move(chartHeight-1, j); addch(ACS_BLOCK);
    }
    attroff(COLOR_PAIR(keyTopColor));
}

int drawKeyTop(){
    attron(COLOR_PAIR(keyTopColor));
    for(int j = g_lanePos[0]; j <= g_lanePos[7]; j++){
        move(chartHeight-1, j); addch(ACS_BLOCK);
    }
    attroff(COLOR_PAIR(keyTopColor));
}

int drawKey(int lane, int color){
    attron(COLOR_PAIR(color));
    for(int j = g_lanePos[lane * 2]; j <= g_lanePos[(lane * 2) + 1]; j++){
        move(chartHeight, j); addch(ACS_BLOCK);
        move(chartHeight+1, j); addch(ACS_BLOCK);
    }
    attroff(COLOR_PAIR(color));
    return 1;
}

int drawNote(int lane, int posY, bool erase){
    for(int i = g_lanePos[lane * 2]; i <= g_lanePos[(lane * 2) + 1]; i++){
        if(!erase){
            if(lane == 0 || lane == 3){
                attron(COLOR_PAIR(noteADColor));
            }else{
                attron(COLOR_PAIR(noteBCColor));
            }

            move(posY, i);
            addch(ACS_BLOCK);

            if(lane == 0 || lane == 3){
                attroff(COLOR_PAIR(noteADColor));
            }else{
                attroff(COLOR_PAIR(noteBCColor));
            }
        }else{
            if(posY == chartHeight-1){
                drawKeyTopLane(lane);
            }else{
                mvprintw(posY, i, " ");
            }
        }
    }
    refresh();
    
    return 1;
}

int setupColor(){
    start_color();

    init_color(noteADColor + 15, COLOR_NOTE_AD);
    init_color(noteBCColor + 15, COLOR_NOTE_BC);
    init_color(keyColor + 15, COLOR_KEY);
    init_color(perfectKeyColor + 15, COLOR_PERFECT_KEY);
    init_color(greatKeyColor + 15, COLOR_GREAT_KEY);
    init_color(booKeyColor + 15, COLOR_BOO_KEY);
    init_color(missKeyColor + 15, COLOR_MISS_KEY);
    init_color(keyPressedColor + 15, COLOR_PRESSED_KEY);
    init_color(keyTopColor + 15, COLOR_KEY_TOP);
    init_color(statBoardTextColor + 15, COLOR_STAT_BOARD_TEXT);
    init_color(statBoardBgColor + 15, COLOR_STAT_BOARD_BG);

    init_pair(noteADColor, noteADColor + 15, use_default_colors());
    init_pair(noteBCColor, noteBCColor + 15, use_default_colors());
    init_pair(keyColor, keyColor + 15, use_default_colors());
    init_pair(perfectKeyColor, perfectKeyColor + 15, statBoardBgColor + 15);
    init_pair(greatKeyColor, greatKeyColor + 15, statBoardBgColor + 15);
    init_pair(booKeyColor, booKeyColor + 15, statBoardBgColor + 15);
    init_pair(missKeyColor, missKeyColor + 15, statBoardBgColor + 15);
    init_pair(keyPressedColor, keyPressedColor + 15, use_default_colors());
    init_pair(keyTopColor, keyTopColor + 15, use_default_colors());
    init_pair(statBoardTextColor, statBoardTextColor + 15, statBoardBgColor + 15);
}

int setupScreen(){
    initscr();
    keypad(stdscr, TRUE);
    nodelay(stdscr, TRUE);
    noecho();
    curs_set(0);
    getmaxyx(stdscr, g_screenHeight, g_screenWidth);
    setupLanePos();
    setupColor();
    setupStatBoardDimensions();
    return 1;
}

int setupGame(){
    return 1;
}

int addScore(int *p_playerScore, int additionalScore){
    attron(COLOR_PAIR(statBoardTextColor));
    *p_playerScore += additionalScore;
    mvprintw(g_statBoardDimensions[2] + 2,g_statBoardDimensions[0] + 3,"Score: %d", *p_playerScore);
    attroff(COLOR_PAIR(statBoardTextColor));
}
int addCombo(int *p_playerCombo){
    attron(COLOR_PAIR(statBoardTextColor));
    *p_playerCombo += 1;
    mvprintw(g_statBoardDimensions[2] + 4,g_statBoardDimensions[0] + 3,"Combo: %d", *p_playerCombo);
    attroff(COLOR_PAIR(statBoardTextColor));
}
int resetCombo(int *p_playerCombo){
    attron(COLOR_PAIR(statBoardTextColor));
    *p_playerCombo = 0;
    mvprintw(g_statBoardDimensions[2] + 4,g_statBoardDimensions[0] + 3,"Combo: 0         ");
    attroff(COLOR_PAIR(statBoardTextColor));
}
int printAccuracyText(char accuracyText[], int color){
    attron(COLOR_PAIR(color));
    mvprintw(g_statBoardDimensions[2] + 6,g_statBoardDimensions[0] + 3,blankText);
    mvprintw(g_statBoardDimensions[2] + 6,g_statBoardDimensions[0] + 3, "%s", accuracyText);
    attroff(COLOR_PAIR(color));
}

int drawStatBoard(){

    //0 = left
    //1 = right
    //2 = top
    //3 = bottom

    attron(COLOR_PAIR(statBoardTextColor));
    for(int i = g_statBoardDimensions[0]; i < g_statBoardDimensions[1]; i++){
        move(g_statBoardDimensions[2], i); addch(ACS_BLOCK);
    }
    for(int i = g_statBoardDimensions[0]; i < g_statBoardDimensions[1]; i++){
        move(g_statBoardDimensions[3], i); addch(ACS_BLOCK);
    }
    for(int i = g_statBoardDimensions[2] + 1; i <= g_statBoardDimensions[3] - 1; i++){
        move(i, g_statBoardDimensions[0]); addch(ACS_BLOCK);
        for(int j = g_statBoardDimensions[0] + 1; j < g_statBoardDimensions[1] - 1; j++){
            mvprintw(i, j, " ");
        }
    }
    for(int i = g_statBoardDimensions[2] + 1; i <= g_statBoardDimensions[3] - 1; i++){
        move(i, g_statBoardDimensions[1] - 1); addch(ACS_BLOCK);
    }
    attroff(COLOR_PAIR(statBoardTextColor));

}