#pragma once
#include <Application.h>
#include <Window.h>
#include <View.h>
#include <MenuBar.h>
#include <Menu.h>
#include <MenuItem.h>
#include <MessageRunner.h>
#include <TextControl.h>
#include <Button.h>
#include <StringView.h>
#include <vector>
#include <cmath>

// Messages
#define MSG_TICK            'tick'
#define MSG_NEW_GAME        'newg'
#define MSG_NEXT_LEVEL      'next'
#define MSG_PAUSE           'paus'
#define MSG_DIFF_EASY       'dife'
#define MSG_DIFF_MEDIUM     'difm'
#define MSG_DIFF_HARD       'difh'
#define MSG_HIGH_SCORES     'hisc'
#define MSG_GAME_OVER_CHECK 'govk'
#define MSG_SAVE_SCORE      'savs'
#define MSG_HOW_TO_PLAY     'hwtp'
#define MSG_ABOUT           'abwn'

static const int MAX_HIGH_SCORES = 10;

enum Difficulty {
    DIFF_EASY = 0,
    DIFF_MEDIUM,
    DIFF_HARD
};

enum CellState {
    CELL_FREE = 0,
    CELL_WALL,
    CELL_CAPTURED
};

enum WallDir {
    WALL_NONE = 0,
    WALL_HORIZONTAL,
    WALL_VERTICAL
};

struct Ball {
    float x, y;
    float vx, vy;
    float radius;
    float spinAngle;
    float spinSpeed;
    float col1[3];
    float col2[3];
};

struct WallArm {
    int   originX, originY;
    WallDir dir;
    int   sign;
    int   extent;
    bool  growing;
    bool  completed;
    bool  dead;
    int   id;
    int   pairedId;
};

// ---------------------------------------------------------------------------
// High scores
// ---------------------------------------------------------------------------

struct HighScoreEntry {
    char  name[64];
    int32 score;
};

std::vector<HighScoreEntry> LoadHighScores(Difficulty d);
void SaveHighScores(const std::vector<HighScoreEntry>& list, Difficulty d);
bool IsHighScore(int32 score, Difficulty d);
void InsertHighScore(const HighScoreEntry& entry, Difficulty d);

// ---------------------------------------------------------------------------
// HighScoreWindow
// ---------------------------------------------------------------------------

class HighScoreView : public BView {
public:
    HighScoreView(BRect frame);
    virtual void Draw(BRect updateRect) override;
    virtual void MouseDown(BPoint where) override;
    void Refresh();
    void SetHighlight(int32 score) { fHighlightScore = score; }
    void SetDifficulty(Difficulty d) { fCurrentDiff = d; }
private:
    std::vector<HighScoreEntry> fScores;
    int32      fHighlightScore;
    Difficulty fCurrentDiff;
    BRect      fTabRects[3];
};

class HighScoreWindow : public BWindow {
public:
    HighScoreWindow(BWindow* parent);
    void         Refresh(int32 highlightScore = -1, Difficulty d = DIFF_MEDIUM);
    virtual bool QuitRequested() override;
private:
    HighScoreView* fView;
};

// ---------------------------------------------------------------------------
// NameEntryWindow  (shown when the player earns a high score)
// ---------------------------------------------------------------------------

class NameEntryWindow : public BWindow {
public:
    NameEntryWindow(BMessenger target, int32 score);
    virtual void MessageReceived(BMessage* msg) override;
    virtual bool QuitRequested() override { return true; }
private:
    BTextControl* fNameField;
    BMessenger    fTarget;
    int32         fScore;
};

// ---------------------------------------------------------------------------
// GameView
// ---------------------------------------------------------------------------

class GameView : public BView {
public:
    GameView(BRect frame);
    virtual ~GameView();

    virtual void Draw(BRect updateRect) override;
    virtual void MouseDown(BPoint where) override;
    virtual void FrameResized(float newW, float newH) override;
    virtual void KeyDown(const char* bytes, int32 numBytes) override;
    virtual void MessageReceived(BMessage* msg) override;
    virtual void AttachedToWindow() override;
    virtual void GetPreferredSize(float* w, float* h) override;

    void NewGame();
    void NextLevel();
    void TogglePause();
    void SetDifficulty(Difficulty d);
    Difficulty GetDifficulty() const { return fDifficulty; }
    int32      GetScore()      const { return fScore; }
    void Tick();

private:
    void _InitLevel();
    void _AwardLevelPoints();
    void _GrowWalls();
    void _MoveBalls();
    void _CollideBalls();
    void _CheckWallBallCollisions();
    void _KillArm(WallArm& arm);
    void _CheckPairCompletion(WallArm& arm);
    WallArm* _FindArm(int id);
    void _FillCapturedAreas();
    bool _CellHasBall(int cx, int cy);
    float _ComputeCapturedPercent();
    void _CheckLevelComplete();
    bool _AnyArmGrowing() const;

    void _DrawSphereBall(float cx, float cy, float radius, float spinAngle,
                         const float col1[3], const float col2[3]);
    void _DrawHUD(BRect hudRect);
    void _DrawField(BRect fieldRect);
    void _DrawProgressBar(BRect r, float pct);
    void _DrawBevelRect(BRect r, bool inset);

    BRect _FieldRect();
    void  _ArmCell(const WallArm& arm, int e, int& gx, int& gy) const;

    static const int   GRID_W = 64;
    static const int   GRID_H = 48;
    static const float HUD_H;
    static const float BORDER;

    std::vector<std::vector<CellState>> fGrid;
    std::vector<Ball>    fBalls;
    std::vector<WallArm> fWallArms;

    int        fNextArmId;
    int        fLevel;
    int        fLives;
    int32      fScore;
    int32      fLastLevelPoints; // points earned on most-recently completed level
    float      fTimeLeft;
    float      fLevelTime;
    bool       fGameOver;
    bool       fGameOverPosted; // guard: post MSG_GAME_OVER_CHECK only once
    bool       fLevelComplete;
    bool       fPaused;
    Difficulty fDifficulty;

    BMessageRunner* fTicker;
    float fCellW, fCellH;
    float fOldFieldW, fOldFieldH;
};

// ---------------------------------------------------------------------------
// JezzikuWindow
// ---------------------------------------------------------------------------

class JezzikuWindow : public BWindow {
public:
    JezzikuWindow();
    virtual bool QuitRequested() override;
    virtual void MessageReceived(BMessage* msg) override;
    void _UpdateDifficultyMarks();
private:
    void _ShowHighScores(int32 highlightScore = -1, Difficulty d = DIFF_MEDIUM);
    void _ShowHowToPlay();
    void _ShowAbout();

    GameView*        fView;
    BMenuItem*       fDiffItems[3];
    HighScoreWindow* fHighScoreWin;
    BWindow*         fHowToPlayWin;
    BWindow*         fAboutWin;
};

class JezzikuApp : public BApplication {
public:
    JezzikuApp();
};
