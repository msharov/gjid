// Copyright (c) 1995 by Mike Sharov <msharov@users.sourceforge.net>
// This file is free software, distributed under the MIT License.

#include "gjid.h"
#include <time.h>

//{{{ Game data --------------------------------------------------------

#define char const char
#include "data/tileset.xpm"
#include "data/logo.xpm"
#undef char
#define unsigned const unsigned
#include "data/title.xbm"
#include "data/winner.xbm"
#include "data/loser.xbm"
#undef unsigned
#include "data/levels.txt"

/*static*/ const GJID::SImageTile GJID::c_Tiles [NumberOfPics] = {
    { 32, 48, 16, 16 },	// DisposePix
    { 48, 48, 16, 16 },	// ExitPix
    { 48, 16, 16, 16 },	// FloorPix
    { 16, 32, 16, 16 },	// OWDNorthPix
    { 32, 32, 16, 16 },	// OWDSouthPix
    {  0, 32, 16, 16 },	// OWDEastPix
    { 48, 32, 16, 16 },	// OWDWestPix
    {  0, 16, 16, 16 },	// Wall1Pix
    { 16, 16, 16, 16 },	// Wall2Pix
    { 32, 16, 16, 16 },	// Wall3Pix
    {  0, 64, 16, 16 },	// Back1Pix
    { 16, 64, 16, 16 },	// Back2Pix
    { 32, 64, 16, 16 },	// Back3Pix
    { 16,  0, 16, 16 },	// RobotNorthPix
    { 32,  0, 16, 16 },	// RobotSouthPix
    {  0,  0, 16, 16 },	// RobotEastPix
    { 48,  0, 16, 16 },	// RobotWestPix
    {  0, 48, 16, 16 },	// Barrel1Pix
    { 16, 48, 16, 16 },	// Barrel2Pix
    {  0,  0, 50, 50 },	// LogoGPix
    { 50,  0, 50, 50 },	// LogoJPix
    {100,  0, 50, 50 },	// LogoIPix
    {150,  0, 50, 50 }	// LogoDPix
};

//}}}-------------------------------------------------------------------

MainApp (GJID)

//----------------------------------------------------------------------

GJID::GJID (void)
: CXApp()
,_state (state_Title)
,_storyPage (0)
,_level (0)
,_moves (0)
,_imgtiles()
,_imglogo()
,_curLevel()
,_levels()
{
}

int GJID::Run (void)
{
    CreateWindow ("GJID", 320, 240);

    _imgtiles = LoadImage (tileset_xpm);	// Map tiles and objects
    _imglogo = LoadImage (logo_xpm);		// Big text for the story

    for (auto ldata = levels_data; ldata;) {	// levels.txt
	_levels.push_back (Level());
	ldata = _levels.back().Load (ldata);
    }
    _curLevel = _levels[0];			// Moving crates changes level data, so make a working copy

    return CXApp::Run();
}

//----------------------------------------------------------------------
// Tile screen helpers

void GJID::FillWithTile (PicIndex tidx)
{
    for (auto y = 0u; y < Height(); y += TILE_H)
	for (auto x = 0u; x < Width(); x += TILE_W)
	    PutTile (tidx, x, y);
}

// This creates the big letter screens for the title, winner, and loser
void GJID::DecodeBitmapWithTile (const uint8_t* p, PicIndex fg, PicIndex bg)
{
    FillWithTile (bg);
    for (auto y = 0u; y < 6; ++y, ++p)
	for (auto x = 0u; x < 16; p+=(++x==8))
	    if ((*p>>(x%8))&1)
		PutTile (fg, (x+2)*TILE_W, (y+3)*TILE_H);
}

//----------------------------------------------------------------------
// Screen drawing

inline void GJID::PrintStory (void)
{
    // Make a stone tile border
    FillWithTile (Back1Pix);
    for (auto y = 0u; y < Height(); y += TILE_H) {
	PutTile (Wall1Pix, 0, y);
	PutTile (Wall1Pix, Width() - TILE_W, y);
    }
    for (unsigned x = TILE_W; x < Width(); x += TILE_W) {
	PutTile (Wall1Pix, x, 0);
	PutTile (Wall1Pix, x, Height() - TILE_H);
    }
    DrawText (145, Height() - 9, "Hit any key", RGB(0,0,0));
    DrawText (144, Height() - 10, "Hit any key", RGB(128,128,128));

    auto row = 0u;
    if (_storyPage == 0) {
	DrawImageTile (_imglogo, c_Tiles[LogoGPix], 40, TILE_H * 2); 
	DrawImageTile (_imglogo, c_Tiles[LogoJPix], 100, TILE_H * 2); 
	DrawImageTile (_imglogo, c_Tiles[LogoIPix], 160, TILE_H * 2); 
	DrawImageTile (_imglogo, c_Tiles[LogoDPix], 220, TILE_H * 2); 
	row = 8;
    }
    if (_storyPage < 2) {
	static const char* storyText[] = {
	    "In the year 32333 AD two robot cities on the planet Nikarade were\0"
	    "arming themselves against each other. Both set up large complexes\0"
	    "in which powerful photon disruptors were stored. After many years\0"
	    "of increasing tension on of the cities elected another leader who\0"
	    "attempted to make peace with the enemy. The cities finally agreed\0"
	    "to an entente in 32407 and to recycle some of the created weapons\0"
	    " \0"
	    "The problem was that no robot wanted to go down into the dungeons\0"
	    "and accomplish this dangerous task. Finally, one robot named GJID\0"
	    "came forward to take up the job. He was a simple robot and little\0"
	    "to lose. Besides, there was a reward offered for the job :)\0"
	    ,
	    "In this game you play GJID, whose task is to move each crate into\0"
	    "recycling bins. At times complex mazes and one-way doors can make\0"
	    "this quite difficult. GJID is not very powerful and can only move\0"
	    "one crate at a time. Also, he can only push the crates, not pull.\0"
	    " \0"
	    "When you have recycled all the crates on the level you should use\0"
	    "the exit door to move on to the next. This weapons complex has 14\0"
	    "levels for you to clear.\0"
	    " \0"
	    "     Controls:   Cursor keys to move\0"
	    "                 F1  show this help\0"
	    "                 F6  restart the level\0"
	    "                 F8  skip the level\0"
	    "                 F10 quit the game\0"
	};
	for (auto l = storyText[_storyPage]; strlen(l); ++row, l += strlen(l)+1)
	    DrawText (TILE_W*2, TILE_H*2 + (row+1)*8, l, RGB(128,128,0));
    } else if (_storyPage == 2) {
	auto x = 2u*TILE_W, y = 2u*TILE_H+7;
	DrawText (x+50, y, "Things you will find in the maze:", RGB(255,255,255));
	y += 17;
	static const PicIndex pic[] = { Barrel2Pix, Barrel1Pix, DisposePix, OWDEastPix, ExitPix };
	static const char* desc[] = { "- Photon disruptor", "- Nuclear weapon", "- Recycling bin", "- One-way door", "- The exit" };
	for (auto i = 0u; i < size(pic); ++ i) {
	    PutTile (pic[i], x, y);
	    DrawText (x+TILE_W*2, y+5, desc[i], RGB(128,128,0));
	    y += 17;
	}
    }
}

inline void GJID::DrawLevel (void)
{
    auto it (_curLevel.Map().begin());
    // Fill with default background
    FillWithTile (PicIndex(_curLevel.Map().back()));
    // Map tiles on top of that (map is shorter than the screen)
    for (auto y = 0u; y < MAP_HEIGHT*TILE_H; y += TILE_H) {
	for (auto x = 0u; x < MAP_WIDTH*TILE_W; x += TILE_W) {
	    auto pic = PicIndex(*it++);
	    if (pic == ExitPix && !_curLevel.Objects().empty())
		pic = FloorPix;
	    PutTile (pic, x, y);
	}
    }
    // Objects are composited on top of the tile underneath
    for (const auto& o : _curLevel.Objects())
	PutTile (PicIndex(o.pic), o.x*TILE_W, o.y*TILE_H);
    PutTile (PicIndex(_curLevel.Robot().pic), _curLevel.Robot().x*TILE_W, _curLevel.Robot().y*TILE_H);
    // Move count
    if (_moves) {
	char mbuf [24];
	snprintf (mbuf, sizeof(mbuf), "Moves: %u", _moves);
	DrawText (TILE_W*17+TILE_W/4, Height()-TILE_H*2/3, mbuf, RGB(128,128,0));
    }
}

void GJID::OnDraw (void)
{
    CXApp::OnDraw();
    switch (_state) {
	default:
	case state_Title:	return DecodeBitmapWithTile (title_bits, Wall2Pix, Back1Pix);
	case state_Story:	return PrintStory();
	case state_Game:	return DrawLevel();
	case state_Winner:	return DecodeBitmapWithTile (winner_bits, RobotNorthPix, Wall1Pix);
	case state_Loser:	return DecodeBitmapWithTile (loser_bits, DisposePix, Back3Pix);
    }
}

//----------------------------------------------------------------------
// Input handling

void GJID::TitleKeys (key_t key)
{
    GoToState (key == XK_Escape ? state_Game : state_Story);
}

void GJID::StoryKeys (key_t key)
{
    if (key == XK_Page_Up || key == XK_Up || key == XKM_Ctrl+'b')
	_storyPage -= !!_storyPage;
    else if (++_storyPage > 2 || key == XK_Escape) {
	_storyPage = 0;
	GoToState (state_Game);
    }
    Update();
}

void GJID::LevelKeys (key_t key)
{
    switch (key) {
	case 'k':
	case XK_Up:	_moves += _curLevel.MoveRobot (North);	break;
	case 'j':
	case XK_Down:	_moves += _curLevel.MoveRobot (South);	break;
	case 'l':
	case XK_Right:	_moves += _curLevel.MoveRobot (East);	break;
	case 'h':
	case XK_Left:	_moves += _curLevel.MoveRobot (West);	break;
	case XK_F1:	GoToState (state_Story);		break;
	case 'q':
	case XK_Escape:	Quit();					break;
	case XK_F10:	GoToState (state_Loser);		break;
	case XK_F8:	_level = (_level + 1) % _levels.size();	// fallthrough
	case XK_F6:	_curLevel = _levels [_level];		break;
    }
    if (_curLevel.Finished()) {
	_moves = 0;
	if (++_level < _levels.size())
	    _curLevel = _levels [_level];
	else {
	    _level = 0;
	    GoToState (state_Winner);
	}
    }
    Update();
}

void GJID::OnKey (key_t key)
{
    switch (_state) {
	default:
	case state_Title:	return TitleKeys (key);
	case state_Story:	return StoryKeys (key);
	case state_Game:	return LevelKeys (key);
	case state_Winner:
	case state_Loser:	return Quit();
    }
}
