#include "Jezziku.h"
#include <Application.h>
#include <Window.h>
#include <View.h>
#include <MenuBar.h>
#include <Menu.h>
#include <MenuItem.h>
#include <MessageRunner.h>
#include <Bitmap.h>
#include <InterfaceDefs.h>
#include <Screen.h>
#include <ScrollView.h>
#include <AppFileInfo.h>
#include <IconUtils.h>
#include <Resources.h>
#include <TextView.h>
#include <Cursor.h>
#include <Roster.h>
#include <FindDirectory.h>
#include <Path.h>
#include <File.h>
#include <Directory.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <algorithm>
#include <queue>
#include <string>

const float GameView::HUD_H  = 62.0f;
const float GameView::BORDER = 4.0f;

// HSV (h=0..360, s=0..1, v=0..1) → linear RGB floats written into out[3]
static void HsvToRgb(float h, float s, float v, float out[3])
{
    h = fmodf(h, 360.0f);
    if (h < 0) h += 360.0f;
    float c = v * s;
    float x = c * (1.0f - fabsf(fmodf(h / 60.0f, 2.0f) - 1.0f));
    float m = v - c;
    float r, g, b;
    if      (h < 60)  { r=c; g=x; b=0; }
    else if (h < 120) { r=x; g=c; b=0; }
    else if (h < 180) { r=0; g=c; b=x; }
    else if (h < 240) { r=0; g=x; b=c; }
    else if (h < 300) { r=x; g=0; b=c; }
    else              { r=c; g=0; b=x; }
    out[0] = r + m;
    out[1] = g + m;
    out[2] = b + m;
}

// ---------------------------------------------------------------------------
// High-score persistence
// ---------------------------------------------------------------------------

static BPath GetScorePath(Difficulty d)
{
    BPath path;
    find_directory(B_USER_SETTINGS_DIRECTORY, &path);
    path.Append("Jezziku");
    create_directory(path.Path(), 0755);
    switch (d) {
    case DIFF_EASY: path.Append("scores_easy.txt");   break;
    case DIFF_HARD: path.Append("scores_hard.txt");   break;
    default:        path.Append("scores_medium.txt"); break;
    }
    return path;
}

std::vector<HighScoreEntry> LoadHighScores(Difficulty d)
{
    std::vector<HighScoreEntry> list;
    BFile f(GetScorePath(d).Path(), B_READ_ONLY);
    if (f.InitCheck() != B_OK) return list;
    off_t sz = 0;
    f.GetSize(&sz);
    if (sz <= 0) return list;
    std::string buf(sz, '\0');
    f.Read(&buf[0], sz);
    size_t pos = 0;
    while (pos < buf.size()) {
        size_t nl  = buf.find('\n', pos);
        if (nl == std::string::npos) nl = buf.size();
        std::string line = buf.substr(pos, nl - pos);
        size_t tab = line.find('\t');
        if (tab != std::string::npos) {
            HighScoreEntry e;
            e.score = (int32)std::stol(line.substr(0, tab));
            std::string nm = line.substr(tab + 1);
            strncpy(e.name, nm.c_str(), 63);
            e.name[63] = '\0';
            list.push_back(e);
        }
        pos = nl + 1;
    }
    return list;
}

void SaveHighScores(const std::vector<HighScoreEntry>& list, Difficulty d)
{
    BFile f(GetScorePath(d).Path(), B_WRITE_ONLY | B_CREATE_FILE | B_ERASE_FILE);
    if (f.InitCheck() != B_OK) return;
    for (const auto& e : list) {
        char line[128];
        snprintf(line, sizeof(line), "%d\t%s\n", (int)e.score, e.name);
        f.Write(line, strlen(line));
    }
}

bool IsHighScore(int32 score, Difficulty d)
{
    auto list = LoadHighScores(d);
    return (int)list.size() < MAX_HIGH_SCORES
        || score > list.back().score;
}

void InsertHighScore(const HighScoreEntry& entry, Difficulty d)
{
    auto list = LoadHighScores(d);
    list.push_back(entry);
    std::sort(list.begin(), list.end(),
        [](const HighScoreEntry& a, const HighScoreEntry& b){ return a.score > b.score; });
    if ((int)list.size() > MAX_HIGH_SCORES)
        list.resize(MAX_HIGH_SCORES);
    SaveHighScores(list, d);
}

// ---------------------------------------------------------------------------
// HighScoreView
// ---------------------------------------------------------------------------

HighScoreView::HighScoreView(BRect frame)
    : BView(frame, "HighScoreView", B_FOLLOW_ALL,
            B_WILL_DRAW | B_FULL_UPDATE_ON_RESIZE),
      fHighlightScore(-1), fCurrentDiff(DIFF_MEDIUM)
{
    SetViewColor(ui_color(B_PANEL_BACKGROUND_COLOR));
}

void HighScoreView::Refresh()
{
    fScores = LoadHighScores(fCurrentDiff);
    Invalidate();
}

void HighScoreView::MouseDown(BPoint where)
{
    for (int i = 0; i < 3; i++) {
        if (fTabRects[i].Contains(where) && fCurrentDiff != (Difficulty)i) {
            fCurrentDiff     = (Difficulty)i;
            fHighlightScore  = -1; // clear highlight when switching tabs
            Refresh();
            return;
        }
    }
}

void HighScoreView::Draw(BRect /*update*/)
{
    rgb_color bg     = ui_color(B_PANEL_BACKGROUND_COLOR);
    rgb_color txt    = ui_color(B_PANEL_TEXT_COLOR);
    rgb_color tab    = ui_color(B_WINDOW_TAB_COLOR);
    BRect b = Bounds();

    SetHighColor(bg);
    FillRect(b);

    // ---- Title bar (matches HowToPlayView style) ----
    const float kTitleH = 52.0f;
    SetHighColor(tint_color(tab, B_DARKEN_1_TINT));
    FillRect(BRect(b.left, b.top, b.right, b.top + kTitleH));
    SetHighColor(tint_color(tab, B_DARKEN_3_TINT));
    StrokeLine(BPoint(b.left, b.top + kTitleH), BPoint(b.right, b.top + kTitleH));

    BFont titleFont(*be_bold_font);
    titleFont.SetSize(be_bold_font->Size() + 5);
    SetFont(&titleFont);
    SetHighColor(ui_color(B_WINDOW_TEXT_COLOR));
    const char* title = "High Scores";
    DrawString(title, BPoint(b.left + (b.Width() - titleFont.StringWidth(title)) * 0.5f,
                             b.top + (kTitleH + titleFont.Size() * 0.72f) * 0.5f));

    // ---- Difficulty selector — native Haiku button style ----
    // A strip of panel-background between the title bar and the column header
    // gives the buttons room to look raised.
    const float kTabH    = 30.0f;
    const float kTabPadV = 4.0f;   // vertical space above/below the buttons
    const float kTabPadH = 6.0f;   // horizontal gap between buttons
    float tabStripTop = b.top + kTitleH;

    SetHighColor(bg);
    FillRect(BRect(b.left, tabStripTop, b.right, tabStripTop + kTabH));

    float tabTop = tabStripTop + kTabPadV;
    float tabBtm = tabStripTop + kTabH - kTabPadV;
    float usableW = b.Width() - kTabPadH * 4;   // padding on each side + between
    float tabW    = usableW / 3.0f;
    float tabX0   = b.left + kTabPadH;

    const char* kTabLabels[] = { "Easy", "Medium", "Hard" };

    for (int i = 0; i < 3; i++) {
        float left  = tabX0 + i * (tabW + kTabPadH);
        float right = left + tabW;
        BRect tr(left, tabTop, right, tabBtm);
        fTabRects[i] = tr;
        bool active = (fCurrentDiff == (Difficulty)i);

        // Fill — active tab slightly lighter (like a pressed/selected control),
        // inactive a touch darker to read as a normal button.
        SetHighColor(active ? tint_color(bg, B_LIGHTEN_1_TINT)
                            : tint_color(bg, B_DARKEN_1_TINT));
        FillRect(tr);

        // Bevel — outset for inactive (raised), inset for active (pressed)
        rgb_color light = tint_color(bg, B_LIGHTEN_2_TINT);
        rgb_color dark  = tint_color(bg, B_DARKEN_3_TINT);
        // top & left edge
        SetHighColor(active ? dark : light);
        StrokeLine(tr.LeftTop(),  tr.RightTop());
        StrokeLine(tr.LeftTop(),  tr.LeftBottom());
        // bottom & right edge
        SetHighColor(active ? light : dark);
        StrokeLine(tr.RightTop(), tr.RightBottom());
        StrokeLine(tr.LeftBottom(), tr.RightBottom());

        // Label — bold + panel text when active, plain + dimmed when inactive
        SetFont(active ? be_bold_font : be_plain_font);
        SetHighColor(active ? txt : tint_color(txt, B_LIGHTEN_1_TINT));
        const BFont* lf = active ? be_bold_font : be_plain_font;
        float lw = lf->StringWidth(kTabLabels[i]);
        DrawString(kTabLabels[i],
                   BPoint(left + (tabW - lw) * 0.5f,
                          tabTop + (tr.Height() + lf->Size() * 0.72f) * 0.5f));
    }

    // ---- Column header row ----
    const float kColHdrH = 28.0f;
    float colHdrTop = tabStripTop + kTabH;
    SetHighColor(tint_color(bg, B_DARKEN_2_TINT));
    FillRect(BRect(b.left, colHdrTop, b.right, colHdrTop + kColHdrH));
    // Bottom rule only — no accent stripe
    SetHighColor(tint_color(bg, B_DARKEN_3_TINT));
    StrokeLine(BPoint(b.left, colHdrTop + kColHdrH),
               BPoint(b.right, colHdrTop + kColHdrH));

    const float kPad    = 14.0f;
    const float colRank = b.left  + kPad;
    const float colName = b.left  + kPad + 36.0f;
    const float colScore = b.right - kPad;

    float colHdrBaseline = colHdrTop + (kColHdrH + be_bold_font->Size() * 0.72f) * 0.5f;
    SetFont(be_bold_font);
    SetHighColor(txt);
    DrawString("#",     BPoint(colRank, colHdrBaseline));
    DrawString("Name",  BPoint(colName, colHdrBaseline));
    float sw = be_bold_font->StringWidth("Score");
    DrawString("Score", BPoint(colScore - sw, colHdrBaseline));

    // ---- Data rows ----
    float bodyTop = colHdrTop + kColHdrH;
    float bodyH   = b.Height() - bodyTop;
    float rowH    = bodyH / MAX_HIGH_SCORES;
    rowH = std::max(18.0f, std::min(rowH, 42.0f));

    BFont plain(*be_plain_font);

    for (int i = 0; i < MAX_HIGH_SCORES; i++) {
        float rowTop    = bodyTop + rowH * i;
        float rowBottom = rowTop  + rowH;
        float baseline  = rowTop  + rowH * 0.72f;

        bool hasEntry  = (i < (int)fScores.size());
        bool highlight = hasEntry && fHighlightScore >= 0
                         && fScores[i].score == fHighlightScore;

        // Row background
        if (highlight) {
            // Tinted tab colour — matches the accent theme
            SetHighColor(tint_color(tab, B_LIGHTEN_MAX_TINT));
            FillRect(BRect(b.left, rowTop, b.right, rowBottom));
            // Left accent bar on highlighted row
            SetHighColor(tab);
            FillRect(BRect(b.left, rowTop, b.left + 4, rowBottom));
        } else if (i % 2 == 1) {
            SetHighColor(tint_color(bg, B_DARKEN_1_TINT));
            FillRect(BRect(b.left, rowTop, b.right, rowBottom));
        }

        // Subtle row separator
        SetHighColor(tint_color(bg, B_DARKEN_1_TINT));
        StrokeLine(BPoint(b.left, rowBottom - 1), BPoint(b.right, rowBottom - 1));

        if (!hasEntry) {
            // Empty placeholder
            SetFont(&plain);
            SetHighColor(tint_color(txt, B_LIGHTEN_2_TINT));
            char rank[16]; snprintf(rank, sizeof(rank), "%d.", i + 1);
            DrawString(rank, BPoint(colRank, baseline));
            continue;
        }

        const HighScoreEntry& e = fScores[i];

        // Rank number — bold for top 3
        if (i < 3) {
            SetFont(be_bold_font);
            // Gold / silver / bronze tints
            rgb_color medalCol = (i == 0) ? make_color(180, 140, 20, 255)
                               : (i == 1) ? make_color(140, 140, 148, 255)
                                          : make_color(150, 90, 40, 255);
            SetHighColor(highlight ? txt : medalCol);
        } else {
            SetFont(&plain);
            SetHighColor(highlight ? txt : tint_color(txt, B_LIGHTEN_1_TINT));
        }
        char rank[16]; snprintf(rank, sizeof(rank), "%d.", i + 1);
        DrawString(rank, BPoint(colRank, baseline));

        // Name
        SetFont(&plain);
        SetHighColor(highlight ? txt : txt);
        DrawString(e.name, BPoint(colName, baseline));

        // Score — right-aligned, bold
        SetFont(be_bold_font);
        SetHighColor(highlight ? txt : tint_color(txt, B_DARKEN_1_TINT));
        char sc[32];
        int32 v = e.score;
        if (v >= 1000000)
            snprintf(sc, sizeof(sc), "%d,%03d,%03d",
                     v/1000000, (v/1000)%1000, v%1000);
        else if (v >= 1000)
            snprintf(sc, sizeof(sc), "%d,%03d", v/1000, v%1000);
        else
            snprintf(sc, sizeof(sc), "%d", (int)v);
        sw = be_bold_font->StringWidth(sc);
        DrawString(sc, BPoint(colScore - sw, baseline));
    }

    if (fScores.empty()) {
        SetFont(be_plain_font);
        SetHighColor(tint_color(txt, B_LIGHTEN_1_TINT));
        const char* none = "No scores yet — play a game!";
        DrawString(none, BPoint(b.left + (b.Width() - StringWidth(none)) * 0.5f,
                                bodyTop + bodyH * 0.5f));
    }
}

// ---------------------------------------------------------------------------
// HighScoreWindow
// ---------------------------------------------------------------------------

HighScoreWindow::HighScoreWindow(BWindow* parent)
    : BWindow(BRect(0, 0, 360, 320), "High Scores",
              B_TITLED_WINDOW, 0)
{
    fView = new HighScoreView(Bounds());
    AddChild(fView);
    // Do NOT call fView->Refresh() here — we're in the caller's thread,
    // not HighScoreWindow's thread.  Refresh() is called after construction.

    // Minimum: wide enough to read names + scores; tall enough for header +
    // column labels + all ten rows (≈22 px each) + a little padding.
    SetSizeLimits(280, 32767, 290, 32767);

    if (parent) CenterIn(parent->Frame());
}

void HighScoreWindow::Refresh(int32 highlightScore, Difficulty d)
{
    // Must be called with this window locked (Lock/Unlock around the call).
    fView->SetDifficulty(d);
    fView->SetHighlight(highlightScore);
    fView->Refresh();
}

bool HighScoreWindow::QuitRequested()
{
    Hide();
    return false; // keep alive so it can be re-opened
}

// ---------------------------------------------------------------------------
// NameEntryWindow
// ---------------------------------------------------------------------------

NameEntryWindow::NameEntryWindow(BMessenger target, int32 score)
    : BWindow(BRect(0, 0, 340, 155), "New High Score!",
              B_TITLED_WINDOW_LOOK, B_MODAL_APP_WINDOW_FEEL,
              B_NOT_RESIZABLE),
      fTarget(target), fScore(score)
{
    BView* bg = new BView(Bounds(), "bg", B_FOLLOW_ALL, B_WILL_DRAW);
    bg->SetViewColor(ui_color(B_PANEL_BACKGROUND_COLOR));
    AddChild(bg);

    // Score message
    char msg[128];
    int32 v = score;
    char scoreStr[32];
    if (v >= 1000000)
        snprintf(scoreStr, sizeof(scoreStr), "%d,%03d,%03d", v/1000000, (v/1000)%1000, v%1000);
    else if (v >= 1000)
        snprintf(scoreStr, sizeof(scoreStr), "%d,%03d", v/1000, v%1000);
    else
        snprintf(scoreStr, sizeof(scoreStr), "%d", (int)v);
    snprintf(msg, sizeof(msg), "You scored %s points — a new high score!", scoreStr);

    BStringView* label = new BStringView(BRect(14, 14, 325, 34), "lbl", msg);
    label->SetFont(be_bold_font);
    bg->AddChild(label);

    BStringView* prompt = new BStringView(BRect(14, 42, 200, 60), "prm", "Enter your name:");
    bg->AddChild(prompt);

    fNameField = new BTextControl(BRect(14, 62, 325, 82), "name", "", "", nullptr);
    fNameField->SetDivider(0);
    bg->AddChild(fNameField);
    fNameField->MakeFocus(true);

    BButton* ok = new BButton(BRect(240, 100, 325, 125), "ok", "Save Score",
                              new BMessage(MSG_SAVE_SCORE));
    bg->AddChild(ok);
    SetDefaultButton(ok);

    BButton* cancel = new BButton(BRect(150, 100, 235, 125), "cancel", "Skip",
                                  new BMessage(B_QUIT_REQUESTED));
    bg->AddChild(cancel);

    // Centre on screen
    BRect screen = BScreen().Frame();
    MoveTo((screen.Width()  - 340) * 0.5f,
           (screen.Height() - 155) * 0.5f);
}

void NameEntryWindow::MessageReceived(BMessage* msg)
{
    if (msg->what == MSG_SAVE_SCORE) {
        const char* name = fNameField->Text();
        if (name == nullptr || strlen(name) == 0) name = "Anonymous";
        BMessage out(MSG_SAVE_SCORE);
        out.AddString("name",  name);
        out.AddInt32("score",  fScore);
        fTarget.SendMessage(&out);
        Quit();
        return;
    }
    BWindow::MessageReceived(msg);
}

// ---------------------------------------------------------------------------
// HowToPlayWindow — richly-styled custom scrollable view
// ---------------------------------------------------------------------------

// Simple word-wrap: splits text into lines that fit within maxWidth.
static std::vector<std::string>
WrapWords(const char* text, const BFont& font, float maxWidth)
{
    std::vector<std::string> result;
    std::string s(text);
    std::string line;
    size_t i = 0;
    while (i <= s.size()) {
        size_t sp = (i < s.size()) ? s.find(' ', i) : std::string::npos;
        if (sp == std::string::npos) sp = s.size();
        std::string word = s.substr(i, sp - i);
        if (!word.empty()) {
            std::string candidate = line.empty() ? word : line + " " + word;
            if (!line.empty() && font.StringWidth(candidate.c_str()) > maxWidth) {
                result.push_back(line);
                line = word;
            } else {
                line = candidate;
            }
        }
        i = sp + 1;
    }
    if (!line.empty()) result.push_back(line);
    return result;
}

class HowToPlayView : public BView {
public:
    HowToPlayView(BRect frame)
        : BView(frame, "content", B_FOLLOW_LEFT_RIGHT | B_FOLLOW_TOP,
                B_WILL_DRAW | B_FULL_UPDATE_ON_RESIZE)
    {
        SetViewColor(ui_color(B_PANEL_BACKGROUND_COLOR));
    }

    void AttachedToWindow() override { _UpdateHeight(); }
    void FrameResized(float, float) override { _UpdateHeight(); }
    void Draw(BRect) override { _Render(true); }

private:
    static const float kPad;
    static const float kTitleH;
    static const float kHeadH;
    static const float kGap;

    void _UpdateHeight()
    {
        float h = _Render(false);
        ResizeTo(Bounds().Width(), h);

        // Tell the scrollbar about the new content height
        BScrollView* sv = dynamic_cast<BScrollView*>(Parent());
        if (sv) {
            BScrollBar* sb = sv->ScrollBar(B_VERTICAL);
            if (sb) {
                float visH = sv->Bounds().Height();
                float range = std::max(0.0f, h - visH);
                sb->SetRange(0.0f, range);
                sb->SetSteps(be_plain_font->Size() * 1.5f, visH * 0.85f);
                sb->SetProportion(h > 0 ? std::min(1.0f, visH / h) : 1.0f);
            }
        }

        Invalidate();
    }

    // ---- Element primitives (return y after the element) ----------------

    float _Title(float y, float w, bool draw)
    {
        if (draw) {
            rgb_color tab = ui_color(B_WINDOW_TAB_COLOR);
            SetHighColor(tint_color(tab, B_DARKEN_1_TINT));
            FillRect(BRect(0, y, w, y + kTitleH));
            SetHighColor(tint_color(tab, B_DARKEN_3_TINT));
            StrokeLine(BPoint(0, y + kTitleH), BPoint(w, y + kTitleH));

            BFont tf(*be_bold_font);
            tf.SetSize(be_bold_font->Size() + 5);
            SetFont(&tf);
            SetHighColor(ui_color(B_WINDOW_TEXT_COLOR));
            const char* t = "How to Play Jezziku";
            DrawString(t, BPoint((w - tf.StringWidth(t)) * 0.5f,
                                 y + (kTitleH + tf.Size() * 0.72f) * 0.5f));
        }
        return y + kTitleH + kGap;
    }

    float _SectionHeader(const char* title, float y, float w, bool draw)
    {
        if (draw) {
            rgb_color bg = ui_color(B_PANEL_BACKGROUND_COLOR);
            SetHighColor(tint_color(bg, B_DARKEN_2_TINT));
            FillRect(BRect(0, y, w, y + kHeadH));
            // Accent bar
            SetHighColor(ui_color(B_WINDOW_TAB_COLOR));
            FillRect(BRect(0, y, 4, y + kHeadH));
            // Bottom rule
            SetHighColor(tint_color(bg, B_DARKEN_3_TINT));
            StrokeLine(BPoint(0, y + kHeadH), BPoint(w, y + kHeadH));

            SetFont(be_bold_font);
            SetHighColor(ui_color(B_PANEL_TEXT_COLOR));
            DrawString(title, BPoint(kPad,
                y + (kHeadH + be_bold_font->Size() * 0.72f) * 0.5f));
        }
        return y + kHeadH;
    }

    float _BodyText(const char* text, float x, float y, float w, bool draw)
    {
        float lh = ceilf(be_plain_font->Size() * 1.5f);
        auto lines = WrapWords(text, *be_plain_font, w);
        if (draw) { SetFont(be_plain_font); SetHighColor(ui_color(B_PANEL_TEXT_COLOR)); }
        for (const auto& ln : lines) {
            if (draw) DrawString(ln.c_str(), BPoint(x, y + be_plain_font->Size() * 0.85f));
            y += lh;
        }
        return y;
    }

    // Draw a keyboard-key-style label + description value on one row.
    float _KeyRow(const char* key, const char* val,
                  float x, float y, float keyColW, bool draw)
    {
        float lh   = ceilf(be_bold_font->Size() * 1.9f);
        float capH = be_bold_font->Size() + 7.0f;
        float capW = be_bold_font->StringWidth(key) + 12.0f;
        float capY = y + (lh - capH) * 0.5f;

        if (draw) {
            rgb_color bg  = ui_color(B_PANEL_BACKGROUND_COLOR);
            // Key cap fill + bevel
            SetHighColor(tint_color(bg, B_DARKEN_2_TINT));
            FillRoundRect(BRect(x, capY, x + capW, capY + capH), 3, 3);
            SetHighColor(tint_color(bg, B_DARKEN_4_TINT));
            StrokeRoundRect(BRect(x, capY, x + capW, capY + capH), 3, 3);
            // Subtle inner highlight on top edge
            SetHighColor(tint_color(bg, B_LIGHTEN_1_TINT));
            StrokeRoundRect(BRect(x+1, capY+1, x + capW - 1, capY + capH * 0.5f), 2, 2);

            // Key text centred in cap
            SetFont(be_bold_font);
            SetHighColor(ui_color(B_PANEL_TEXT_COLOR));
            float kw = be_bold_font->StringWidth(key);
            DrawString(key, BPoint(x + (capW - kw) * 0.5f,
                                   capY + (capH + be_bold_font->Size() * 0.72f) * 0.5f));

            // Value text
            SetFont(be_plain_font);
            DrawString(val, BPoint(x + keyColW,
                                   y + (lh + be_plain_font->Size() * 0.72f) * 0.5f));
        }
        return y + lh;
    }

    // Coloured square bullet + word-wrapped text.
    float _Bullet(const char* text, float x, float y, float w, bool draw)
    {
        const float indent = 16.0f;
        float lh  = ceilf(be_plain_font->Size() * 1.5f);
        float sqSz = 5.0f;
        auto lines = WrapWords(text, *be_plain_font, w - indent);
        if (draw) {
            float sqY = y + be_plain_font->Size() * 0.5f - sqSz * 0.5f;
            SetHighColor(ui_color(B_WINDOW_TAB_COLOR));
            FillRect(BRect(x, sqY, x + sqSz, sqY + sqSz));
            SetFont(be_plain_font);
            SetHighColor(ui_color(B_PANEL_TEXT_COLOR));
            for (const auto& ln : lines) {
                DrawString(ln.c_str(), BPoint(x + indent,
                                              y + be_plain_font->Size() * 0.85f));
                y += lh;
            }
        } else {
            y += lh * (float)lines.size();
        }
        return y;
    }

    // Tinted box with left accent and fixed-width formula text.
    float _FormulaBox(const char* line1, const char* line2,
                      float x, float y, float w, bool draw)
    {
        float lh   = ceilf(be_plain_font->Size() * 1.5f);
        float boxH = lh * 2 + 16.0f;
        if (draw) {
            rgb_color bg  = ui_color(B_PANEL_BACKGROUND_COLOR);
            SetHighColor(tint_color(bg, B_DARKEN_1_TINT));
            FillRect(BRect(x, y, x + w, y + boxH));
            SetHighColor(tint_color(bg, B_DARKEN_3_TINT));
            StrokeRect(BRect(x, y, x + w, y + boxH));
            SetHighColor(ui_color(B_WINDOW_TAB_COLOR));
            FillRect(BRect(x, y, x + 4, y + boxH));

            BFont mono(*be_fixed_font);
            mono.SetSize(be_plain_font->Size());
            SetFont(&mono);
            SetHighColor(ui_color(B_PANEL_TEXT_COLOR));
            DrawString(line1, BPoint(x + 12, y + 7 + mono.Size() * 0.85f));
            DrawString(line2, BPoint(x + 12, y + 7 + lh + mono.Size() * 0.85f));
        }
        return y + boxH;
    }

    // ---- Main layout ----------------------------------------------------
    float _Render(bool draw)
    {
        float w  = Bounds().Width();
        float bx = kPad;
        float bw = w - kPad * 2.0f;

        if (draw) {
            SetHighColor(ui_color(B_PANEL_BACKGROUND_COLOR));
            FillRect(Bounds());
        }

        float y = 0;
        y = _Title(y, w, draw);

        // Objective
        y = _SectionHeader("Objective", y, w, draw);
        y = _BodyText("Capture 75% or more of the playing field before "
                      "the timer runs out to advance to the next level.",
                      bx, y + 8, bw, draw);
        y += 8 + kGap;

        // Controls
        y = _SectionHeader("Controls", y, w, draw);
        y += 6;
        float kColW = 115.0f;
        y = _KeyRow("Left-click",  "Draw a horizontal wall", bx + 8, y, kColW, draw);
        y = _KeyRow("Right-click", "Draw a vertical wall",   bx + 8, y, kColW, draw);
        y = _KeyRow("P",           "Pause / resume",          bx + 8, y, kColW, draw);
        y += 6 + kGap;

        // Wall Mechanics
        y = _SectionHeader("Wall Mechanics", y, w, draw);
        y = _BodyText("Clicking starts a wall that grows outward from the "
                      "click point in both directions simultaneously. "
                      "The two halves are independent:",
                      bx, y + 8, bw, draw);
        y += 6;
        y = _Bullet("A ball can destroy one arm while the other keeps growing.",
                    bx + 8, y, bw - 16, draw);
        y = _Bullet("Each destroyed arm costs one life.",
                    bx + 8, y, bw - 16, draw);
        y = _Bullet("When both arms complete (reach a wall or boundary), any "
                    "enclosed area with no balls is captured.",
                    bx + 8, y, bw - 16, draw);
        y = _Bullet("Only one wall can be drawn at a time.",
                    bx + 8, y, bw - 16, draw);
        y += 8 + kGap;

        // Lives
        y = _SectionHeader("Lives", y, w, draw);
        y = _BodyText("You start with 5 lives. Each time a growing wall arm "
                      "is hit by a ball you lose one life. Losing all lives "
                      "ends the game.",
                      bx, y + 8, bw, draw);
        y += 8 + kGap;

        // Timer
        y = _SectionHeader("Timer", y, w, draw);
        y = _BodyText("Each level has a countdown timer. Running out of time "
                      "ends the game. The time limit decreases with each "
                      "level and with higher difficulty settings.",
                      bx, y + 8, bw, draw);
        y += 8 + kGap;

        // Scoring
        y = _SectionHeader("Scoring", y, w, draw);
        y = _BodyText("Points are awarded when you complete a level:",
                      bx, y + 8, bw, draw);
        y += 6;
        y = _FormulaBox("Base:   area captured %  x  level  x  100",
                        "Bonus:  seconds remaining  x  level  x  5",
                        bx + 8, y, bw - 16, draw);
        y += 6;
        y = _BodyText("Completing higher levels with more area captured "
                      "and more time remaining earns more points.",
                      bx, y + 4, bw, draw);
        y += 8 + kGap;

        // Difficulty
        y = _SectionHeader("Difficulty", y, w, draw);
        y += 6;
        float dkColW = 90.0f;
        y = _KeyRow("Easy",   "Moderate speed  —  120 s/level  (-5 s per level)",
                    bx + 8, y, dkColW, draw);
        y = _KeyRow("Medium", "Fast  —  90 s/level  (-4 s per level)",
                    bx + 8, y, dkColW, draw);
        y = _KeyRow("Hard",   "Very fast  —  60 s/level  (-3 s per level)",
                    bx + 8, y, dkColW, draw);
        y += 6;
        y = _BodyText("The minimum time per level is 20 seconds regardless "
                      "of difficulty.",
                      bx, y + 4, bw, draw);
        y += kPad;

        return y;
    }
};

const float HowToPlayView::kPad    = 16.0f;
const float HowToPlayView::kTitleH = 52.0f;
const float HowToPlayView::kHeadH  = 28.0f;
const float HowToPlayView::kGap    = 12.0f;

class HowToPlayWindow : public BWindow {
public:
    HowToPlayWindow()
        : BWindow(BRect(120, 80, 640, 580), "How to Play",
                  B_TITLED_WINDOW, 0)
    {
        BRect bounds = Bounds();
        // Give the content view the full initial height so BScrollView fills
        // the window. _UpdateHeight() will resize it to the real content
        // height once attached, and the scrollbar range will be updated then.
        float contentW = bounds.Width() - B_V_SCROLL_BAR_WIDTH;
        HowToPlayView* content = new HowToPlayView(
            BRect(0, 0, contentW, bounds.Height()));
        BScrollView* sv = new BScrollView("scroll", content,
                                          B_FOLLOW_ALL, 0, false, true);
        AddChild(sv);
        SetSizeLimits(340, 32767, 300, 32767);
        CenterIn(BScreen().Frame());
    }
    bool QuitRequested() override { Hide(); return false; }
};

// ---------------------------------------------------------------------------
// AboutWindow  (with clickable hyperlink)
// ---------------------------------------------------------------------------

class AboutView : public BView {
public:
    AboutView(BRect frame)
        : BView(frame, "about", B_FOLLOW_ALL,
                B_WILL_DRAW | B_FULL_UPDATE_ON_RESIZE),
          fLinkRect(0, 0, 0, 0), fIcon(nullptr)
    {
        fVersionStr[0] = '\0';
        SetViewColor(ui_color(B_PANEL_BACKGROUND_COLOR));
        _LoadIcon();
        _LoadVersion();
    }

    ~AboutView() { delete fIcon; }

    void Draw(BRect) override
    {
        BRect b = Bounds();

        // Coloured header band — tall enough for name + version
        const float kHdrH = 68.0f;
        BRect hdr(b.left, b.top, b.right, b.top + kHdrH);
        rgb_color tabCol = ui_color(B_WINDOW_TAB_COLOR);
        SetHighColor(tint_color(tabCol, B_DARKEN_1_TINT));
        FillRect(hdr);
        SetHighColor(tint_color(tabCol, B_DARKEN_3_TINT));
        StrokeLine(BPoint(b.left, hdr.bottom), BPoint(b.right, hdr.bottom));

        BFont titleFont(*be_bold_font);
        titleFont.SetSize(be_bold_font->Size() + 5);
        BFont verFont(*be_plain_font);

        // Centre the two-line block (name + version) vertically in the header
        float totalTextH = titleFont.Size() + 4.0f + verFont.Size();
        float blockTop   = hdr.top + (kHdrH - totalTextH) * 0.5f;

        // App name
        SetFont(&titleFont);
        SetHighColor(ui_color(B_WINDOW_TEXT_COLOR));
        const char* name = "Jezziku";
        float nw = titleFont.StringWidth(name);
        DrawString(name, BPoint(b.left + (b.Width() - nw) * 0.5f,
                                blockTop + titleFont.Size() * 0.88f));

        // Version string (read from resources via BAppFileInfo)
        if (fVersionStr[0] != '\0') {
            SetFont(&verFont);
            SetHighColor(0, 0, 0);
            float vw = verFont.StringWidth(fVersionStr);
            DrawString(fVersionStr,
                       BPoint(b.left + (b.Width() - vw) * 0.5f,
                              blockTop + titleFont.Size() + 4.0f + verFont.Size() * 0.88f));
        }

        // Body background
        SetHighColor(ui_color(B_PANEL_BACKGROUND_COLOR));
        FillRect(BRect(b.left, hdr.bottom + 1, b.right, b.bottom));

        // Layout: [icon] [gap] ["Created by David Freeman"]
        // The whole group is centred in the body area.
        SetFont(be_plain_font);
        rgb_color txt     = ui_color(B_PANEL_TEXT_COLOR);
        rgb_color linkCol = { 0, 90, 210, 255 };
        const char* prefix = "Created by ";
        const char* link   = "David Freeman";
        float pw = be_plain_font->StringWidth(prefix);
        float lw = be_plain_font->StringWidth(link);

        const float kIconSize = 48.0f;
        const float kIconGap  = 14.0f;
        float iconBlockW = fIcon ? kIconSize + kIconGap : 0.0f;
        float totalW     = iconBlockW + pw + lw;

        float bodyMidY = hdr.bottom + (b.bottom - hdr.bottom) * 0.5f;
        float groupX   = b.left + (b.Width() - totalW) * 0.5f;

        // Icon
        if (fIcon) {
            float iconY = bodyMidY - kIconSize * 0.5f;
            SetDrawingMode(B_OP_ALPHA);
            SetBlendingMode(B_PIXEL_ALPHA, B_ALPHA_OVERLAY);
            DrawBitmap(fIcon, BPoint(groupX, iconY));
            SetDrawingMode(B_OP_COPY);
        }

        // Text baseline centred on icon
        float textX    = groupX + iconBlockW;
        float baseline = bodyMidY + be_plain_font->Size() * 0.35f;

        SetHighColor(txt);
        DrawString(prefix, BPoint(textX, baseline));

        float linkX = textX + pw;
        SetHighColor(linkCol);
        DrawString(link, BPoint(linkX, baseline));
        StrokeLine(BPoint(linkX,      baseline + 2),
                   BPoint(linkX + lw - 1, baseline + 2));

        fLinkRect.Set(linkX, bodyMidY - 12, linkX + lw, bodyMidY + 12);
    }

    void MouseMoved(BPoint where, uint32 /*transit*/, const BMessage*) override
    {
        if (fLinkRect.Contains(where)) {
            BCursor cur(B_CURSOR_ID_FOLLOW_LINK);
            SetViewCursor(&cur);
        } else {
            SetViewCursor(B_CURSOR_SYSTEM_DEFAULT);
        }
    }

    void MouseDown(BPoint where) override
    {
        if (fLinkRect.Contains(where)) {
            char* argv[] = { (char*)"https://d3.codes/about" };
            be_roster->Launch("text/html", 1, argv);
        }
    }

private:
    void _LoadIcon()
    {
        app_info ai;
        if (be_app->GetAppInfo(&ai) != B_OK) return;

        BFile appFile(&ai.ref, B_READ_ONLY);
        BResources res;
        if (res.SetTo(&appFile) != B_OK) return;

        size_t dataSize = 0;
        const void* data = res.LoadResource('VICN', 1, &dataSize);
        if (!data || dataSize == 0) return;

        fIcon = new BBitmap(BRect(0, 0, 47, 47), B_RGBA32);
        if (BIconUtils::GetVectorIcon((const uint8*)data, dataSize, fIcon) != B_OK) {
            delete fIcon;
            fIcon = nullptr;
        }
    }

    void _LoadVersion()
    {
        app_info ai;
        if (be_app->GetAppInfo(&ai) != B_OK) return;

        BFile appFile(&ai.ref, B_READ_ONLY);
        BAppFileInfo info(&appFile);
        version_info vi;
        if (info.GetVersionInfo(&vi, B_APP_VERSION_KIND) != B_OK) return;

        snprintf(fVersionStr, sizeof(fVersionStr), "v%u.%u.%u",
                 vi.major, vi.middle, vi.minor);
    }

    mutable BRect  fLinkRect;
    BBitmap*       fIcon;
    char           fVersionStr[256];
};

class AboutWindow : public BWindow {
public:
    AboutWindow()
        : BWindow(BRect(0, 0, 340, 155), "About Jezziku",
                  B_TITLED_WINDOW, B_NOT_RESIZABLE)
    {
        AddChild(new AboutView(Bounds()));
        CenterIn(BScreen().Frame());
    }
    bool QuitRequested() override { Hide(); return false; }
};

// ---------------------------------------------------------------------------
// GameView — construction
// ---------------------------------------------------------------------------

GameView::GameView(BRect frame)
    : BView(frame, "GameView", B_FOLLOW_ALL, B_WILL_DRAW | B_FRAME_EVENTS | B_NAVIGABLE),
      fNextArmId(0), fLevel(1), fLives(5),
      fScore(0), fLastLevelPoints(0),
      fTimeLeft(90.0f), fLevelTime(90.0f),
      fGameOver(false), fGameOverPosted(false),
      fLevelComplete(false), fPaused(false),
      fDifficulty(DIFF_MEDIUM),
      fTicker(nullptr), fCellW(1), fCellH(1), fOldFieldW(1), fOldFieldH(1)
{
    SetViewColor(ui_color(B_PANEL_BACKGROUND_COLOR));
    srand((unsigned)time(nullptr));
    fGrid.assign(GRID_W, std::vector<CellState>(GRID_H, CELL_FREE));
}

GameView::~GameView() { delete fTicker; }

void GameView::AttachedToWindow() { MakeFocus(true); NewGame(); }

void GameView::GetPreferredSize(float* w, float* h)
{
    *w = GRID_W * 10.0f + BORDER * 2;
    *h = HUD_H  + GRID_H * 10.0f + BORDER * 2;
}

BRect GameView::_FieldRect()
{
    BRect b = Bounds();
    return BRect(BORDER, HUD_H + BORDER, b.Width() - BORDER, b.Height() - BORDER);
}

// ---------------------------------------------------------------------------
// Game lifecycle
// ---------------------------------------------------------------------------

void GameView::NewGame()
{
    fLevel = 1; fLives = 5;
    fScore = 0; fLastLevelPoints = 0;
    fGameOver = false; fGameOverPosted = false;
    fLevelComplete = false;
    _InitLevel();
}

void GameView::NextLevel()
{
    fLevel++; fLevelComplete = false; fGameOverPosted = false;
    _InitLevel();
}

void GameView::TogglePause()
{
    fPaused = !fPaused;
    Invalidate();
}

void GameView::SetDifficulty(Difficulty d)
{
    fDifficulty = d;
    // Restart so the new speed takes effect immediately
    NewGame();
}

void GameView::_InitLevel()
{
    fGrid.assign(GRID_W, std::vector<CellState>(GRID_H, CELL_FREE));
    fWallArms.clear();
    fBalls.clear();
    fNextArmId = 0;

    BRect field = _FieldRect();
    fCellW = field.Width()  / GRID_W;
    fCellH = field.Height() / GRID_H;
    fOldFieldW = field.Width();
    fOldFieldH = field.Height();

    int   numBalls = 1 + fLevel;
    float baseSpeed, perLevel, baseTime, timePerLevel;
    switch (fDifficulty) {
    case DIFF_EASY:
        baseSpeed = 80.0f;  perLevel = 12.0f;
        baseTime  = 120.0f; timePerLevel = 5.0f; break;
    case DIFF_HARD:
        baseSpeed = 160.0f; perLevel = 28.0f;
        baseTime  = 60.0f;  timePerLevel = 3.0f; break;
    default: // MEDIUM
        baseSpeed = 115.0f; perLevel = 18.0f;
        baseTime  = 90.0f;  timePerLevel = 4.0f; break;
    }
    float speed = baseSpeed + fLevel * perLevel;
    fLevelTime = std::max(20.0f, baseTime - (fLevel - 1) * timePerLevel);
    fTimeLeft  = fLevelTime;

    for (int i = 0; i < numBalls; i++) {
        Ball ball;
        ball.radius = std::max(fCellW, fCellH) * 0.75f;
        float minX = ball.radius + 2, maxX = fOldFieldW - ball.radius - 2;
        float minY = ball.radius + 2, maxY = fOldFieldH - ball.radius - 2;
        ball.x  = minX + (float)rand() / RAND_MAX * (maxX - minX);
        ball.y  = minY + (float)rand() / RAND_MAX * (maxY - minY);
        float a = (float)rand() / RAND_MAX * 2.0f * M_PI;
        ball.vx = cosf(a) * speed;
        ball.vy = sinf(a) * speed;
        if (fabsf(ball.vx) < 12.0f) ball.vx = (ball.vx >= 0 ? 12.0f : -12.0f);
        if (fabsf(ball.vy) < 12.0f) ball.vy = (ball.vy >= 0 ? 12.0f : -12.0f);
        ball.spinAngle = (float)rand() / RAND_MAX * 2.0f * M_PI;
        float revPerSec = 0.7f + (float)rand() / RAND_MAX * 0.8f;
        ball.spinSpeed  = revPerSec * 2.0f * M_PI * (ball.vx >= 0 ? 1.0f : -1.0f);

        // Snap colours to primary/secondary hue centres (0°=red, 60°=yellow,
        // 120°=green, 180°=cyan, 240°=blue, 300°=magenta) with a small jitter,
        // then pick a second centre at least 2 steps (120°) away.
        static const float kHues[] = { 0.f, 60.f, 120.f, 180.f, 240.f, 300.f };
        int   h1idx  = rand() % 6;
        float jitter = ((float)rand() / RAND_MAX - 0.5f) * 24.0f; // ±12°
        float hue1   = kHues[h1idx] + jitter;

        int   step   = 2 + rand() % 3;           // 2–4 steps: 120°, 180°, or 240°
        int   h2idx  = (h1idx + step) % 6;
        float jitter2 = ((float)rand() / RAND_MAX - 0.5f) * 24.0f;
        float hue2   = kHues[h2idx] + jitter2;

        float sat = 0.85f + (float)rand() / RAND_MAX * 0.15f;
        HsvToRgb(hue1, sat, 0.92f, ball.col1);
        HsvToRgb(hue2, sat, 0.88f, ball.col2);

        fBalls.push_back(ball);
    }

    delete fTicker;
    BMessenger me(this);
    fTicker = new BMessageRunner(me, new BMessage(MSG_TICK), 16000);
    Invalidate();
}

// ---------------------------------------------------------------------------
// Resize
// ---------------------------------------------------------------------------

void GameView::FrameResized(float /*newW*/, float /*newH*/)
{
    BRect field = _FieldRect();
    float nfw = field.Width(), nfh = field.Height();

    if (fOldFieldW > 0 && fOldFieldH > 0) {
        float sx = nfw / fOldFieldW, sy = nfh / fOldFieldH;
        float sr = (sx + sy) * 0.5f;
        for (auto& ball : fBalls) {
            ball.x *= sx; ball.y *= sy; ball.radius *= sr;
        }
    }

    fCellW = nfw / GRID_W;
    fCellH = nfh / GRID_H;
    fOldFieldW = nfw;
    fOldFieldH = nfh;

    for (auto& ball : fBalls) {
        ball.x = std::max(ball.radius, std::min(ball.x, nfw - ball.radius));
        ball.y = std::max(ball.radius, std::min(ball.y, nfh - ball.radius));
    }
    Invalidate();
}

// ---------------------------------------------------------------------------
// Messaging
// ---------------------------------------------------------------------------

void GameView::MessageReceived(BMessage* msg)
{
    switch (msg->what) {
    case MSG_TICK:        Tick();                        break;
    case MSG_NEW_GAME:    NewGame();                     break;
    case MSG_NEXT_LEVEL:  NextLevel();                   break;
    case MSG_PAUSE:       TogglePause();                 break;
    case MSG_DIFF_EASY:   SetDifficulty(DIFF_EASY);      break;
    case MSG_DIFF_MEDIUM: SetDifficulty(DIFF_MEDIUM);    break;
    case MSG_DIFF_HARD:   SetDifficulty(DIFF_HARD);      break;
    default:              BView::MessageReceived(msg);   break;
    }
}

void GameView::Tick()
{
    if (fGameOver || fLevelComplete || fPaused) return;

    fTimeLeft -= 0.016f;
    if (fTimeLeft <= 0.0f) {
        fTimeLeft = 0.0f;
        fGameOver = true;
        Invalidate();
        return;
    }

    _GrowWalls();
    _MoveBalls();
    _CollideBalls();
    _CheckWallBallCollisions();
    if (!_AnyArmGrowing())
        _CheckLevelComplete();

    if (fGameOver && !fGameOverPosted) {
        fGameOverPosted = true;
        Window()->PostMessage(MSG_GAME_OVER_CHECK);
    }

    Invalidate();
}

// ---------------------------------------------------------------------------
// Arm helpers
// ---------------------------------------------------------------------------

void GameView::_ArmCell(const WallArm& arm, int e, int& gx, int& gy) const
{
    if (arm.dir == WALL_HORIZONTAL) {
        gx = arm.originX + arm.sign * e;
        gy = arm.originY;
    } else {
        gx = arm.originX;
        gy = arm.originY + arm.sign * e;
    }
}

WallArm* GameView::_FindArm(int id)
{
    for (auto& arm : fWallArms)
        if (arm.id == id) return &arm;
    return nullptr;
}

bool GameView::_AnyArmGrowing() const
{
    for (const auto& arm : fWallArms)
        if (arm.growing) return true;
    return false;
}

// ---------------------------------------------------------------------------
// Physics — wall growth
// ---------------------------------------------------------------------------

void GameView::_GrowWalls()
{
    static const float WALL_SPEED = 12.0f; // cells/sec
    static float accum = 0.0f;
    accum += WALL_SPEED * 0.016f;
    int steps = (int)accum;
    accum -= steps;
    if (steps == 0) return;

    for (auto& arm : fWallArms) {
        if (!arm.growing || arm.dead) continue;

        for (int s = 0; s < steps; s++) {
            // next cell to grow into
            int gx, gy;
            _ArmCell(arm, arm.extent + 1, gx, gy);

            bool outOfBounds = (gx < 0 || gx >= GRID_W || gy < 0 || gy >= GRID_H);
            bool blocked     = !outOfBounds && (fGrid[gx][gy] != CELL_FREE);

            if (outOfBounds || blocked) {
                // arm hits a wall or boundary — it completes
                arm.growing   = false;
                arm.completed = true;
                _CheckPairCompletion(arm);
                break;
            }

            arm.extent++;
            fGrid[gx][gy] = CELL_WALL;
        }
    }
}

// Called when an arm finishes growing (completed OR dead).
// If the paired arm is also settled, trigger flood-fill iff both completed.
void GameView::_CheckPairCompletion(WallArm& arm)
{
    WallArm* paired = (arm.pairedId >= 0) ? _FindArm(arm.pairedId) : nullptr;

    bool pairedSettled = !paired || paired->completed || paired->dead;
    if (!pairedSettled) return; // partner still growing — wait

    // Both arms are now settled.
    // Only do a flood-fill if both arms completed (neither was killed).
    bool bothComplete = arm.completed && (paired == nullptr || paired->completed);
    if (bothComplete)
        _FillCapturedAreas();
}

// Destroy an arm hit by a ball.
// The origin cell (e=0) is only cleared if the paired arm is also dead/gone.
void GameView::_KillArm(WallArm& arm)
{
    WallArm* paired = (arm.pairedId >= 0) ? _FindArm(arm.pairedId) : nullptr;

    // Determine whether we can clear the shared origin cell.
    // Keep it if the partner is still alive (growing or completed).
    bool clearOrigin = (paired == nullptr || paired->dead);

    int startE = clearOrigin ? 0 : 1;
    for (int e = startE; e <= arm.extent; e++) {
        int gx, gy;
        _ArmCell(arm, e, gx, gy);
        if (gx >= 0 && gx < GRID_W && gy >= 0 && gy < GRID_H)
            fGrid[gx][gy] = CELL_FREE;
    }

    arm.dead    = true;
    arm.growing = false;

    fLives--;
    if (fLives <= 0) fGameOver = true;

    _CheckPairCompletion(arm);
}

// ---------------------------------------------------------------------------
// Physics — ball movement & collisions
// ---------------------------------------------------------------------------

void GameView::_MoveBalls()
{
    const float dt   = 0.016f;
    float fieldW = fOldFieldW, fieldH = fOldFieldH;

    for (auto& ball : fBalls) {
        ball.x += ball.vx * dt;
        ball.y += ball.vy * dt;
        ball.spinAngle += ball.spinSpeed * dt;

        // Boundary bounce
        if (ball.x - ball.radius < 0)       { ball.x = ball.radius;        ball.vx =  fabsf(ball.vx); }
        if (ball.x + ball.radius > fieldW)   { ball.x = fieldW-ball.radius; ball.vx = -fabsf(ball.vx); }
        if (ball.y - ball.radius < 0)        { ball.y = ball.radius;        ball.vy =  fabsf(ball.vy); }
        if (ball.y + ball.radius > fieldH)   { ball.y = fieldH-ball.radius; ball.vy = -fabsf(ball.vy); }

        // Bounce off wall / captured cells
        int cx = (int)(ball.x / fCellW);
        int cy = (int)(ball.y / fCellH);
        for (int ddx = -1; ddx <= 1; ddx++) {
            for (int ddy = -1; ddy <= 1; ddy++) {
                int nx = cx+ddx, ny = cy+ddy;
                if (nx<0||nx>=GRID_W||ny<0||ny>=GRID_H) continue;
                if (fGrid[nx][ny] == CELL_FREE) continue;
                float L=nx*fCellW, R=(nx+1)*fCellW, T=ny*fCellH, B=(ny+1)*fCellH;
                float nearX=std::max(L,std::min(ball.x,R));
                float nearY=std::max(T,std::min(ball.y,B));
                float dx=ball.x-nearX, dy=ball.y-nearY;
                if (dx*dx+dy*dy < ball.radius*ball.radius) {
                    if (fabsf(dx) > fabsf(dy))
                        ball.vx = (dx>0) ?  fabsf(ball.vx) : -fabsf(ball.vx);
                    else
                        ball.vy = (dy>0) ?  fabsf(ball.vy) : -fabsf(ball.vy);
                }
            }
        }
    }
}

void GameView::_CollideBalls()
{
    for (size_t i = 0; i < fBalls.size(); i++) {
        for (size_t j = i + 1; j < fBalls.size(); j++) {
            Ball& a = fBalls[i];
            Ball& b = fBalls[j];

            float dx = b.x - a.x;
            float dy = b.y - a.y;
            float dist2 = dx*dx + dy*dy;
            float minDist = a.radius + b.radius;

            if (dist2 >= minDist * minDist) continue;

            float dist = sqrtf(dist2);
            if (dist < 1e-4f) {
                // Exactly overlapping — push apart along an arbitrary axis
                dx = 1.0f; dy = 0.0f; dist = 1.0f;
            }

            // Collision normal (unit vector from a toward b)
            float nx = dx / dist;
            float ny = dy / dist;

            // Relative velocity projected onto the normal
            float dvn = (b.vx - a.vx) * nx + (b.vy - a.vy) * ny;

            // Balls already separating — nothing to do
            if (dvn > 0.0f) continue;

            // Record speeds before touching the velocities
            float speedA = sqrtf(a.vx*a.vx + a.vy*a.vy);
            float speedB = sqrtf(b.vx*b.vx + b.vy*b.vy);

            // Equal-mass elastic collision: exchange the normal components
            a.vx += dvn * nx;
            a.vy += dvn * ny;
            b.vx -= dvn * nx;
            b.vy -= dvn * ny;

            // Rescale each ball back to its pre-collision speed so balls
            // always travel at constant speed regardless of collision angle.
            float newSpeedA = sqrtf(a.vx*a.vx + a.vy*a.vy);
            float newSpeedB = sqrtf(b.vx*b.vx + b.vy*b.vy);
            if (newSpeedA > 1e-4f) { a.vx *= speedA/newSpeedA; a.vy *= speedA/newSpeedA; }
            if (newSpeedB > 1e-4f) { b.vx *= speedB/newSpeedB; b.vy *= speedB/newSpeedB; }

            // Push balls apart so they no longer overlap
            float overlap = (minDist - dist) * 0.5f;
            a.x -= nx * overlap;
            a.y -= ny * overlap;
            b.x += nx * overlap;
            b.y += ny * overlap;
        }
    }
}

void GameView::_CheckWallBallCollisions()
{
    for (auto& arm : fWallArms) {
        if (!arm.growing || arm.dead) continue; // only growing arms can be hit

        for (const auto& ball : fBalls) {
            bool hit = false;
            for (int e = 0; e <= arm.extent && !hit; e++) {
                int gx, gy;
                _ArmCell(arm, e, gx, gy);
                if (gx<0||gx>=GRID_W||gy<0||gy>=GRID_H) continue;
                float L=gx*fCellW, R=(gx+1)*fCellW, T=gy*fCellH, B=(gy+1)*fCellH;
                float nearX=std::max(L,std::min(ball.x,R));
                float nearY=std::max(T,std::min(ball.y,B));
                float dx=ball.x-nearX, dy=ball.y-nearY;
                if (dx*dx+dy*dy < ball.radius*ball.radius) hit = true;
            }
            if (hit) { _KillArm(arm); break; }
        }
    }

    // Purge fully settled arms (dead or completed and partner also settled)
    fWallArms.erase(
        std::remove_if(fWallArms.begin(), fWallArms.end(), [&](const WallArm& arm) {
            if (arm.growing) return false;             // still active
            WallArm* paired = (arm.pairedId >= 0) ? _FindArm(arm.pairedId) : nullptr;
            bool pairSettled = !paired || !paired->growing;
            return pairSettled && arm.dead;            // keep completed arms (they're permanent walls)
        }),
        fWallArms.end());
}

// ---------------------------------------------------------------------------
// Flood-fill capture
// ---------------------------------------------------------------------------

bool GameView::_CellHasBall(int cx, int cy)
{
    float L=cx*fCellW, R=(cx+1)*fCellW, T=cy*fCellH, B=(cy+1)*fCellH;
    for (const auto& ball : fBalls)
        if (ball.x>=L && ball.x<R && ball.y>=T && ball.y<B) return true;
    return false;
}

void GameView::_FillCapturedAreas()
{
    std::vector<std::vector<bool>> visited(GRID_W, std::vector<bool>(GRID_H, false));
    for (int x = 0; x < GRID_W; x++) {
        for (int y = 0; y < GRID_H; y++) {
            if (fGrid[x][y] != CELL_FREE || visited[x][y]) continue;
            std::vector<std::pair<int,int>> region;
            bool hasBall = false;
            std::queue<std::pair<int,int>> q;
            q.push({x,y}); visited[x][y] = true;
            while (!q.empty()) {
                auto [cx,cy] = q.front(); q.pop();
                region.push_back({cx,cy});
                if (_CellHasBall(cx,cy)) hasBall = true;
                const int ddx[]={1,-1,0,0}, ddy[]={0,0,1,-1};
                for (int d=0; d<4; d++) {
                    int nx=cx+ddx[d], ny=cy+ddy[d];
                    if (nx<0||nx>=GRID_W||ny<0||ny>=GRID_H) continue;
                    if (fGrid[nx][ny]!=CELL_FREE||visited[nx][ny]) continue;
                    visited[nx][ny]=true; q.push({nx,ny});
                }
            }
            if (!hasBall)
                for (auto [rx,ry]:region) fGrid[rx][ry] = CELL_CAPTURED;
        }
    }
}

float GameView::_ComputeCapturedPercent()
{
    int cap = 0;
    for (int x=0; x<GRID_W; x++)
        for (int y=0; y<GRID_H; y++)
            if (fGrid[x][y] != CELL_FREE) cap++;
    return 100.0f * cap / (GRID_W * GRID_H);
}

void GameView::_AwardLevelPoints()
{
    float pct = _ComputeCapturedPercent();
    // Base: captured% × level × 100; time bonus: seconds left × level × 5
    int32 pts = (int32)(pct * fLevel * 100.0f)
              + (int32)(fTimeLeft * fLevel * 5.0f);
    fLastLevelPoints = pts;
    fScore += pts;
}

void GameView::_CheckLevelComplete()
{
    if (_ComputeCapturedPercent() >= 75.0f) {
        _AwardLevelPoints();
        fLevelComplete = true;
        for (auto& arm : fWallArms) arm.growing = false;
    }
}

// ---------------------------------------------------------------------------
// Input
// ---------------------------------------------------------------------------

void GameView::MouseDown(BPoint where)
{
    if (fGameOver)      { NewGame();   return; }
    if (fLevelComplete) { NextLevel(); return; }
    if (fPaused || _AnyArmGrowing()) return;

    BRect field = _FieldRect();
    float fx = where.x - field.left, fy = where.y - field.top;
    if (fx < 0 || fy < 0 || fx >= field.Width() || fy >= field.Height()) return;

    int gx = (int)(fx / fCellW), gy = (int)(fy / fCellH);
    if (gx<0||gx>=GRID_W||gy<0||gy>=GRID_H) return;
    if (fGrid[gx][gy] != CELL_FREE) return;

    uint32 buttons; BPoint dummy;
    GetMouse(&dummy, &buttons);
    WallDir dir = (buttons & B_SECONDARY_MOUSE_BUTTON) ? WALL_VERTICAL : WALL_HORIZONTAL;

    // Place origin cell
    fGrid[gx][gy] = CELL_WALL;

    int idA = fNextArmId++, idB = fNextArmId++;

    WallArm armA, armB;
    armA.originX=gx; armA.originY=gy; armA.dir=dir; armA.sign=-1; armA.extent=0;
    armA.growing=true; armA.completed=false; armA.dead=false;
    armA.id=idA; armA.pairedId=idB;

    armB.originX=gx; armB.originY=gy; armB.dir=dir; armB.sign=+1; armB.extent=0;
    armB.growing=true; armB.completed=false; armB.dead=false;
    armB.id=idB; armB.pairedId=idA;

    // If the origin is already at the boundary, the arm for that direction
    // completes immediately (extent = 0, the origin cell itself closes it).
    // Just let _GrowWalls handle it next tick.

    fWallArms.push_back(armA);
    fWallArms.push_back(armB);
}

void GameView::KeyDown(const char* bytes, int32 /*numBytes*/)
{
    switch (bytes[0]) {
    case 'p': case 'P': TogglePause(); break;
    }
}

// ---------------------------------------------------------------------------
// Drawing helpers
// ---------------------------------------------------------------------------

static inline rgb_color Tint(rgb_color c, float t) { return tint_color(c, t); }

void GameView::_DrawBevelRect(BRect r, bool inset)
{
    rgb_color base  = ui_color(B_PANEL_BACKGROUND_COLOR);
    rgb_color light = Tint(base, B_LIGHTEN_2_TINT);
    rgb_color dark  = Tint(base, B_DARKEN_3_TINT);
    SetHighColor(inset ? dark : light);
    StrokeLine(r.LeftTop(),  r.RightTop());
    StrokeLine(r.LeftTop(),  r.LeftBottom());
    SetHighColor(inset ? light : dark);
    StrokeLine(r.RightTop(), r.RightBottom());
    StrokeLine(r.LeftBottom(), r.RightBottom());
}

void GameView::_DrawProgressBar(BRect r, float pct)
{
    _DrawBevelRect(r, true);
    r.InsetBy(1,1);
    SetHighColor(Tint(ui_color(B_PANEL_BACKGROUND_COLOR), B_DARKEN_1_TINT));
    FillRect(r);
    float filled = r.Width() * (pct / 100.0f);
    if (filled > 0) {
        BRect bar(r.left, r.top, r.left+filled, r.bottom);
        rgb_color fill = (pct < 50.0f) ? make_color(180,60,60,255)
                       : (pct < 70.0f) ? make_color(200,160,30,255)
                                       : make_color(60,160,60,255);
        SetHighColor(fill);
        FillRect(bar);
        SetHighColor(Tint(fill, B_LIGHTEN_2_TINT));
        StrokeLine(bar.LeftTop(), BPoint(bar.right, bar.top));
    }
}

// ---------------------------------------------------------------------------
// Drawing — HUD
// ---------------------------------------------------------------------------

void GameView::_DrawHUD(BRect hud)
{
    rgb_color panelBg  = ui_color(B_PANEL_BACKGROUND_COLOR);
    rgb_color panelTxt = ui_color(B_PANEL_TEXT_COLOR);

    // Background + bottom border
    SetHighColor(panelBg);
    FillRect(hud);
    SetHighColor(Tint(panelBg, B_DARKEN_2_TINT));
    StrokeLine(BPoint(hud.left, hud.bottom), BPoint(hud.right, hud.bottom));

    // ---- Geometry ----
    // The HUD is split: main row (top) + hint strip (bottom ~18 px).
    const float kHintH   = 20.0f;
    const float kPadX    = 10.0f;
    const float kBoxPad  = 4.0f;   // inset from edges of main-row area

    float mainTop    = hud.top;
    float mainBottom = hud.bottom - kHintH;
    float hintY      = hud.bottom - 4.0f;  // text baseline for hint strip

    // Thin rule above hint strip
    SetHighColor(Tint(panelBg, B_DARKEN_1_TINT));
    StrokeLine(BPoint(hud.left, mainBottom), BPoint(hud.right, mainBottom));

    // ---- Shared "main row" font — used by every element ----
    BFont mainFont(be_bold_font);
    mainFont.SetSize(be_bold_font->Size() + 1);   // same size as timer

    // Timer box geometry (computed before anything else so we know rightBound)
    int  timerSecs = (int)ceilf(fTimeLeft);
    char timerStr[16];
    snprintf(timerStr, sizeof(timerStr), "%d:%02d", timerSecs / 60, timerSecs % 60);

    float timerTextW = mainFont.StringWidth(timerStr);
    float boxTop     = mainTop    + kBoxPad;
    float boxBottom  = mainBottom - kBoxPad;
    float boxH       = boxBottom  - boxTop;
    float boxW       = timerTextW + kBoxPad * 2 + 2.0f;
    float boxRight   = hud.right  - kPadX;
    float boxLeft    = boxRight   - boxW;
    BRect timerBox(boxLeft, boxTop, boxRight, boxBottom);

    // Shared text baseline — vertically centred within the box
    float textBaseline = boxTop + (boxH + mainFont.Size() * 0.72f) * 0.5f;

    // ---- Hint / status strip ----
    BFont hintFont(be_plain_font);
    hintFont.SetSize(be_plain_font->Size() * 1.0f);
    SetFont(&hintFont);
    const char* hint = fPaused ? "PAUSED — press P to resume"
                                : "L-click: horizontal wall    R-click: vertical wall    P: pause";
    SetHighColor(Tint(panelTxt, B_DARKEN_1_TINT));
    DrawString(hint, BPoint(hud.left + (hud.Width() - hintFont.StringWidth(hint)) * 0.5f, hintY));

    // ---- Game-over / level-complete overlay ----
    if (fGameOver) {
        SetFont(&mainFont); SetHighColor(make_color(180, 30, 30, 255));
        char msg[128];
        snprintf(msg, sizeof(msg), "Game Over — Score: %d — click to play again", (int)fScore);
        float msgW = mainFont.StringWidth(msg);
        DrawString(msg, BPoint(hud.left + (hud.Width() - msgW) * 0.5f, textBaseline));
        return;
    }
    if (fLevelComplete) {
        SetFont(&mainFont); SetHighColor(make_color(30, 120, 30, 255));
        char msg[128];
        snprintf(msg, sizeof(msg), "Level %d complete! +%d pts — click for next level",
                 fLevel, (int)fLastLevelPoints);
        float msgW = mainFont.StringWidth(msg);
        DrawString(msg, BPoint(hud.left + (hud.Width() - msgW) * 0.5f, textBaseline));
        return;
    }

    // ---- Timer box ----
    rgb_color urgentCol, boxFill;
    if (fTimeLeft <= 10.0f) {
        urgentCol = make_color(210, 35, 35, 255);
        boxFill   = make_color(80,  10, 10, 255);
    } else if (fTimeLeft <= 30.0f) {
        urgentCol = make_color(195, 145, 15, 255);
        boxFill   = make_color(60,  45,  5, 255);
    } else {
        urgentCol = panelTxt;
        boxFill   = Tint(panelBg, B_DARKEN_1_TINT);
    }
    SetHighColor(boxFill);
    FillRect(timerBox);
    _DrawBevelRect(timerBox, /*inset=*/true);

    SetFont(&mainFont); SetHighColor(urgentCol);
    float tx = timerBox.left + (timerBox.Width() - timerTextW) * 0.5f;
    DrawString(timerStr, BPoint(tx, textBaseline));

    // ---- Left-side stats (all use mainFont + textBaseline) ----
    SetFont(&mainFont);
    float rightBound = boxLeft - kPadX;
    float cx = hud.left + kPadX;

    // Level
    SetHighColor(panelTxt);
    char lvl[32]; snprintf(lvl, sizeof(lvl), "Level %d", fLevel);
    DrawString(lvl, BPoint(cx, textBaseline));
    cx += mainFont.StringWidth(lvl) + 18.0f;

    // Lives label + hearts
    SetHighColor(panelTxt);
    DrawString("Lives:", BPoint(cx, textBaseline));
    cx += mainFont.StringWidth("Lives:") + 6.0f;
    const char* heart = "\xe2\x99\xa5";
    float hw = mainFont.StringWidth(heart) + 3.0f;
    for (int i = 0; i < 5; i++) {
        SetHighColor(i < fLives ? make_color(210, 50, 50, 255)
                                : Tint(panelBg, B_DARKEN_2_TINT));
        DrawString(heart, BPoint(cx + i * hw, textBaseline));
    }
    cx += 5.0f * hw + 18.0f;

    // Captured label + progress bar (fills remaining width) + % number
    float pctLabelW = mainFont.StringWidth("Captured:") + 6.0f;
    float pctNumW   = mainFont.StringWidth("100%") + 8.0f;
    float barW      = rightBound - cx - pctLabelW - pctNumW;
    if (barW > 30.0f) {
        float pct = _ComputeCapturedPercent();
        SetHighColor(panelTxt);
        DrawString("Captured:", BPoint(cx, textBaseline));
        cx += pctLabelW;
        _DrawProgressBar(BRect(cx, boxTop, cx + barW, boxBottom), pct);
        cx += barW + 6.0f;
        char ps[16]; snprintf(ps, sizeof(ps), "%.0f%%", pct);
        SetHighColor(panelTxt);
        DrawString(ps, BPoint(cx, textBaseline));
    }
}

// ---------------------------------------------------------------------------
// Sphere ball renderer
// ---------------------------------------------------------------------------

void GameView::_DrawSphereBall(float cx, float cy, float radius, float spinAngle,
                               const float col1[3], const float col2[3])
{
    int ir   = (int)ceilf(radius);
    int size = ir * 2 + 1;

    BBitmap bmp(BRect(0, 0, size - 1, size - 1), B_RGBA32);
    uint8*  bits     = (uint8*)bmp.Bits();
    int32   rowBytes = bmp.BytesPerRow();

    // Fixed light direction (upper-left, slightly towards viewer)
    const float lx = -0.45f, ly = -0.60f, lz = 0.66f;

    // Precompute spin rotation
    float cosA = cosf(spinAngle);
    float sinA = sinf(spinAngle);

    for (int py = 0; py < size; py++) {
        for (int px = 0; px < size; px++) {
            uint8* pixel = bits + py * rowBytes + px * 4;

            float dx = (px - ir) / radius;
            float dy = (py - ir) / radius;
            float dz2 = 1.0f - dx * dx - dy * dy;

            if (dz2 < 0.0f) {
                // Outside sphere — fully transparent
                pixel[0] = pixel[1] = pixel[2] = pixel[3] = 0;
                continue;
            }

            float dz = sqrtf(dz2);
            // Screen-space surface normal
            float nx = dx, ny = dy, nz = dz;

            // Rotate normal around Y-axis to get texture-space coords
            float txNx =  cosA * nx + sinA * nz;
            float txNy =  ny;
            float txNz = -sinA * nx + cosA * nz;

            // Spherical UV (longitude / latitude)
            float u = atan2f(txNx, txNz) * (1.0f / (2.0f * M_PI)) + 0.5f; // 0..1
            float v = asinf(std::max(-1.0f, std::min(1.0f, txNy))) * (1.0f / M_PI) + 0.5f;

            // --- Texture: 6 vertical stripes, alternating col1 / col2 ---
            int stripe = (int)(u * 6.0f) & 1;
            const float* sc = (stripe == 0) ? col1 : col2;
            float baseR = sc[0], baseG = sc[1], baseB = sc[2];

            // Subtle latitude shading (poles slightly darker)
            float latFade = 0.82f + 0.18f * cosf((v - 0.5f) * M_PI);
            baseR *= latFade;
            baseG *= latFade;
            baseB *= latFade;

            // --- Phong lighting ---
            // Diffuse
            float diff = std::max(0.0f, nx * lx + ny * ly + nz * lz);

            // Specular (view dir = (0,0,1) for orthographic projection)
            float nl   = nx * lx + ny * ly + nz * lz;
            float rz   = 2.0f * nl * nz - lz;
            float spec = powf(std::max(0.0f, rz), 48.0f);

            float ambient = 0.12f;
            float light   = ambient + diff * 0.78f;

            float R = std::min(1.0f, baseR * light + spec * 0.95f);
            float G = std::min(1.0f, baseG * light + spec * 0.90f);
            float B = std::min(1.0f, baseB * light + spec * 0.95f);

            // B_RGBA32 in memory: B G R A
            pixel[0] = (uint8)(B * 255.0f);
            pixel[1] = (uint8)(G * 255.0f);
            pixel[2] = (uint8)(R * 255.0f);
            pixel[3] = 255;
        }
    }

    // Soft drop-shadow (drawn before the ball bitmap, so it shows behind)
    SetHighColor(0, 0, 0, 70);
    FillEllipse(BPoint(cx + radius * 0.18f, cy + radius * 0.22f),
                radius * 0.92f, radius * 0.55f);

    DrawBitmap(&bmp, BPoint(cx - ir, cy - ir));
}

// ---------------------------------------------------------------------------
// Drawing — field
// ---------------------------------------------------------------------------

void GameView::_DrawField(BRect field)
{
    // Inset bevel border
    BRect outer = field; outer.InsetBy(-BORDER, -BORDER);
    _DrawBevelRect(outer, true);
    _DrawBevelRect(outer.InsetByCopy(1,1), true);

    // Dark game background
    SetHighColor(18, 22, 45);
    FillRect(field);

    // Grid cells
    for (int x = 0; x < GRID_W; x++) {
        for (int y = 0; y < GRID_H; y++) {
            CellState s = fGrid[x][y];
            if (s == CELL_FREE) continue;
            float px = field.left + x*fCellW, py = field.top + y*fCellH;
            BRect cell(px, py, px+fCellW-0.5f, py+fCellH-0.5f);
            if (s == CELL_CAPTURED) {
                SetHighColor(28, 88, 58); FillRect(cell);
                SetHighColor(40, 110, 75);
                StrokeLine(cell.LeftTop(), BPoint(cell.right, cell.top));
            } else { // CELL_WALL (permanent)
                SetHighColor(160, 155, 60); FillRect(cell);
            }
        }
    }

    // Draw arms
    for (const auto& arm : fWallArms) {
        if (arm.dead) continue;

        // Colour: growing arm = bright yellow; completed arm = muted gold
        rgb_color col = arm.growing
            ? make_color(245, 235, 80, 255)
            : make_color(160, 155, 60, 255);
        SetHighColor(col);

        for (int e = 0; e <= arm.extent; e++) {
            int gx, gy; _ArmCell(arm, e, gx, gy);
            if (gx<0||gx>=GRID_W||gy<0||gy>=GRID_H) continue;
            float px = field.left + gx*fCellW, py = field.top + gy*fCellH;
            FillRect(BRect(px, py, px+fCellW-0.5f, py+fCellH-0.5f));

            // Leading-edge highlight on the growing tip
            if (arm.growing && e == arm.extent) {
                SetHighColor(255, 255, 200);
                if (arm.dir == WALL_HORIZONTAL) {
                    // highlight the outer vertical edge (the tip)
                    float ex = (arm.sign > 0) ? px+fCellW-1 : px;
                    StrokeLine(BPoint(ex, py), BPoint(ex, py+fCellH-1));
                } else {
                    float ey = (arm.sign > 0) ? py+fCellH-1 : py;
                    StrokeLine(BPoint(px, ey), BPoint(px+fCellW-1, ey));
                }
                SetHighColor(col);
            }
        }
    }

    // Subtle grid lines when cells are large enough
    if (fCellW >= 6 && fCellH >= 6) {
        SetHighColor(28, 32, 60);
        for (int x=1; x<GRID_W; x++)
            StrokeLine(BPoint(field.left+x*fCellW, field.top),
                       BPoint(field.left+x*fCellW, field.bottom));
        for (int y=1; y<GRID_H; y++)
            StrokeLine(BPoint(field.left, field.top+y*fCellH),
                       BPoint(field.right, field.top+y*fCellH));
    }

    // Balls — per-pixel sphere rendering
    SetDrawingMode(B_OP_ALPHA);
    SetBlendingMode(B_PIXEL_ALPHA, B_ALPHA_OVERLAY);
    for (const auto& ball : fBalls)
        _DrawSphereBall(field.left + ball.x, field.top + ball.y,
                        ball.radius, ball.spinAngle, ball.col1, ball.col2);
    SetDrawingMode(B_OP_COPY);

    // Pause overlay
    if (fPaused) {
        SetHighColor(0, 0, 0, 120);
        SetDrawingMode(B_OP_ALPHA);
        FillRect(field);
        SetDrawingMode(B_OP_COPY);
    }
}

// ---------------------------------------------------------------------------
// Draw
// ---------------------------------------------------------------------------

void GameView::Draw(BRect /*updateRect*/)
{
    BRect b = Bounds();
    SetHighColor(ui_color(B_PANEL_BACKGROUND_COLOR));
    FillRect(b);
    _DrawHUD(BRect(b.left, b.top, b.right, b.top+HUD_H-1));
    _DrawField(_FieldRect());
}

// ---------------------------------------------------------------------------
// JezzikuWindow
// ---------------------------------------------------------------------------

void JezzikuWindow::_UpdateDifficultyMarks()
{
    Difficulty d = fView->GetDifficulty();
    fDiffItems[0]->SetMarked(d == DIFF_EASY);
    fDiffItems[1]->SetMarked(d == DIFF_MEDIUM);
    fDiffItems[2]->SetMarked(d == DIFF_HARD);
}

void JezzikuWindow::_ShowHowToPlay()
{
    if (fHowToPlayWin == nullptr)
        fHowToPlayWin = new HowToPlayWindow();
    if (fHowToPlayWin->IsHidden())
        fHowToPlayWin->Show();
    else
        fHowToPlayWin->Activate();
}

void JezzikuWindow::_ShowAbout()
{
    if (fAboutWin == nullptr)
        fAboutWin = new AboutWindow();
    if (fAboutWin->IsHidden())
        fAboutWin->Show();
    else
        fAboutWin->Activate();
}

void JezzikuWindow::_ShowHighScores(int32 highlightScore, Difficulty d)
{
    if (fHighScoreWin == nullptr)
        fHighScoreWin = new HighScoreWindow(this);

    if (fHighScoreWin->Lock()) {
        fHighScoreWin->Refresh(highlightScore, d);
        if (fHighScoreWin->IsHidden())
            fHighScoreWin->Show();
        else
            fHighScoreWin->Activate();
        fHighScoreWin->Unlock();
    }
}

JezzikuWindow::JezzikuWindow()
    : BWindow(BRect(60,40,60+900,40+660), "Jezziku",
              B_TITLED_WINDOW, B_QUIT_ON_WINDOW_CLOSE),
      fHighScoreWin(nullptr), fHowToPlayWin(nullptr), fAboutWin(nullptr)
{
    BMenuBar* menuBar = new BMenuBar(BRect(0,0,0,0), "menuBar");

    BMenu* gameMenu = new BMenu("Game");
    gameMenu->AddItem(new BMenuItem("New Game",       new BMessage(MSG_NEW_GAME), 'N'));
    gameMenu->AddItem(new BMenuItem("Pause / Resume", new BMessage(MSG_PAUSE),    'P'));
    gameMenu->AddSeparatorItem();
    gameMenu->AddItem(new BMenuItem("High Scores",    new BMessage(MSG_HIGH_SCORES)));
    gameMenu->AddSeparatorItem();
    gameMenu->AddItem(new BMenuItem("Quit",           new BMessage(B_QUIT_REQUESTED), 'Q'));
    menuBar->AddItem(gameMenu);

    BMenu* diffMenu = new BMenu("Difficulty");
    diffMenu->SetRadioMode(true);
    fDiffItems[0] = new BMenuItem("Easy",   new BMessage(MSG_DIFF_EASY));
    fDiffItems[1] = new BMenuItem("Medium", new BMessage(MSG_DIFF_MEDIUM));
    fDiffItems[2] = new BMenuItem("Hard",   new BMessage(MSG_DIFF_HARD));
    diffMenu->AddItem(fDiffItems[0]);
    diffMenu->AddItem(fDiffItems[1]);
    diffMenu->AddItem(fDiffItems[2]);
    menuBar->AddItem(diffMenu);

    BMenu* helpMenu = new BMenu("Help");
    helpMenu->AddItem(new BMenuItem("How to Play", new BMessage(MSG_HOW_TO_PLAY)));
    helpMenu->AddItem(new BMenuItem("About",       new BMessage(MSG_ABOUT)));
    menuBar->AddItem(helpMenu);

    AddChild(menuBar);

    float mbH = menuBar->Bounds().Height() + 1;
    BRect vf = Bounds(); vf.top = mbH;
    fView = new GameView(vf);
    AddChild(fView);

    _UpdateDifficultyMarks();
    SetSizeLimits(580, 10000, 460, 10000);
}

bool JezzikuWindow::QuitRequested()
{
    // Force-quit the high-score window so it doesn't block app shutdown
    // (its own QuitRequested returns false to allow re-opening).
    if (fHighScoreWin && fHighScoreWin->Lock()) {
        fHighScoreWin->Quit();
        fHighScoreWin = nullptr;
    }
    if (fHowToPlayWin && fHowToPlayWin->Lock()) {
        fHowToPlayWin->Quit();
        fHowToPlayWin = nullptr;
    }
    if (fAboutWin && fAboutWin->Lock()) {
        fAboutWin->Quit();
        fAboutWin = nullptr;
    }
    be_app->PostMessage(B_QUIT_REQUESTED);
    return true;
}

void JezzikuWindow::MessageReceived(BMessage* msg)
{
    switch (msg->what) {
    case MSG_NEW_GAME:
    case MSG_NEXT_LEVEL:
    case MSG_PAUSE:
        fView->MessageReceived(msg);
        break;
    case MSG_DIFF_EASY:
    case MSG_DIFF_MEDIUM:
    case MSG_DIFF_HARD:
        fView->MessageReceived(msg);
        _UpdateDifficultyMarks();
        break;
    case MSG_HIGH_SCORES:
        _ShowHighScores(-1, fView->GetDifficulty());
        break;
    case MSG_HOW_TO_PLAY:
        _ShowHowToPlay();
        break;
    case MSG_ABOUT:
        _ShowAbout();
        break;
    case MSG_GAME_OVER_CHECK:
    {
        int32      score = fView->GetScore();
        Difficulty diff  = fView->GetDifficulty();
        if (score > 0 && IsHighScore(score, diff)) {
            NameEntryWindow* nw = new NameEntryWindow(BMessenger(this), score);
            nw->Show();
        }
        break;
    }
    case MSG_SAVE_SCORE:
    {
        const char* name  = nullptr;
        int32       score = 0;
        msg->FindString("name",  &name);
        msg->FindInt32("score",  &score);
        if (name && score > 0) {
            Difficulty diff = fView->GetDifficulty();
            HighScoreEntry e;
            strncpy(e.name, name, 63); e.name[63] = '\0';
            e.score = score;
            InsertHighScore(e, diff);
            _ShowHighScores(score, diff);
        }
        break;
    }
    default:
        BWindow::MessageReceived(msg);
    }
}

// ---------------------------------------------------------------------------
// JezzikuApp
// ---------------------------------------------------------------------------

JezzikuApp::JezzikuApp() : BApplication("application/x-vnd.haiku.jezziku")
{
    (new JezzikuWindow())->Show();
}

int main()
{
    JezzikuApp app;
    app.Run();
    return 0;
}
