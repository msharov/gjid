// Copyright (c) 1995 by Mike Sharov <msharov@users.sourceforge.net>
// This file is free software, distributed under the MIT License.

#include "xapp.h"
#include <xcb/xcb.h>
#include <xcb/render.h>
#include <signal.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#define unsigned const unsigned
#include "data/font3x5.xbm"
#undef unsigned

//----------------------------------------------------------------------

/// Called when a signal is received.
static void OnSignal (int sig)
{
    printf ("[S] Fatal error: %s\n", strsignal(sig));
    exit (-sig);
}

/// Called by the framework on unrecoverable exception handling errors.
static void Terminate (void)
{
    printf ("[T] Unexpected fatal error\n");
    exit (EXIT_FAILURE);
}

//----------------------------------------------------------------------

CXApp::CXApp (void)
:_ksyms()
,_pconn (NULL)
,_pscreen (NULL)
,_window (XCB_NONE)
,_wpict (XCB_NONE)
,_bpict (XCB_NONE)
,_glyphset (XCB_NONE)
,_glyphpen (XCB_NONE)
,_pencolor (0)
,_xgc (XCB_NONE)
,_width()
,_height()
,_winWidth()
,_winHeight()
,_minKeycode()
,_keysymsPerKeycode()
,_wantQuit (false)
{
    // Initialize cleanup handlers
    static const int8_t c_Signals[] = {
	SIGILL, SIGABRT, SIGBUS,  SIGFPE,  SIGSEGV, SIGSYS, SIGALRM, SIGXCPU,
	SIGHUP, SIGINT,  SIGQUIT, SIGTERM, SIGCHLD, SIGXFSZ, SIGPWR, SIGPIPE
    };
    for (size_t i = 0; i < VectorSize(c_Signals); ++ i)
	signal (c_Signals[i], OnSignal);
    std::set_terminate (Terminate);

    // Establish X server connection
    if (!(_pconn = xcb_connect (NULL, NULL)))
	throw runtime_error ("unable to connect to the X server");
    const xcb_setup_t* xsetup = xcb_get_setup (_pconn);
    if (!xsetup)
	throw runtime_error ("unable to connect to the X server");
    _pscreen = xcb_setup_roots_iterator(xsetup).data;
    // Request RENDER extension, keyboard mappings, and WM atoms
    xcb_get_keyboard_mapping_cookie_t kbcookie = xcb_get_keyboard_mapping (_pconn, xsetup->min_keycode, xsetup->max_keycode-xsetup->min_keycode);
    xcb_render_query_version_cookie_t rendcook = xcb_render_query_version (_pconn, XCB_RENDER_MAJOR_VERSION, XCB_RENDER_MINOR_VERSION);
    static const char* c_AtomNames[xa_Count] = { "STRING", "ATOM", "WM_NAME", "WM_PROTOCOLS", "WM_DELETE_WINDOW", "_NET_WM_STATE", "_NET_WM_STATE_FULLSCREEN" };
    for (int i = 0; i < xa_Count; ++i)
	_atoms[i] = xcb_intern_atom (_pconn, false, strlen(c_AtomNames[i]), c_AtomNames[i]).sequence;

    // Receive and store keyboard mappings
    const xcb_get_keyboard_mapping_reply_t* kbreply = xcb_get_keyboard_mapping_reply (_pconn, kbcookie, NULL);
    size_t szkeysyms = xcb_get_keyboard_mapping_keysyms_length (kbreply);
    const wchar_t* psyms = (const wchar_t*) xcb_get_keyboard_mapping_keysyms (kbreply);
    _ksyms.assign (psyms, psyms + szkeysyms);
    _minKeycode = xsetup->min_keycode;
    _keysymsPerKeycode = kbreply->keysyms_per_keycode;

    // Acknowledge render version and assign atom values
    xcb_render_query_version_reply (_pconn, rendcook, NULL);
    for (int i = 0; i < xa_Count; ++i)
	_atoms[i] = xcb_intern_atom_reply(_pconn, *(xcb_intern_atom_cookie_t*)&_atoms[i], NULL)->atom;

    // Find the root visual
    const xcb_visualtype_t* visual = NULL;
    for (xcb_depth_iterator_t depth_iter = xcb_screen_allowed_depths_iterator(_pscreen); depth_iter.rem; xcb_depth_next(&depth_iter)) {
	if (depth_iter.data->depth != _pscreen->root_depth)
	    continue;
	for (xcb_visualtype_iterator_t visual_iter = xcb_depth_visuals_iterator(depth_iter.data); visual_iter.rem; xcb_visualtype_next(&visual_iter))
	    if (_pscreen->root_visual == visual_iter.data->visual_id)
		visual = visual_iter.data;
    }
    // Get standard RENDER formats
    xcb_render_query_pict_formats_cookie_t qpfcook = xcb_render_query_pict_formats (_pconn);
    xcb_render_query_pict_formats_reply_t* qpfr = xcb_render_query_pict_formats_reply (_pconn, qpfcook, NULL);
    for (xcb_render_pictforminfo_iterator_t i = xcb_render_query_pict_formats_formats_iterator(qpfr); i.rem; xcb_render_pictforminfo_next(&i)) {
	if (i.data->depth == _pscreen->root_depth && i.data->direct.red_mask == visual->red_mask >> i.data->direct.red_shift)
	    _xrfmt[rfmt_Default] = i.data->id;
	else if (i.data->depth == 1)
	    _xrfmt[rfmt_Bitmask] = i.data->id;
	else if (i.data->depth == 8)
	    _xrfmt[rfmt_Font] = i.data->id;
	else if (i.data->depth == 32 && i.data->direct.red_shift == 16 && i.data->direct.alpha_mask == 0xff)
	    _xrfmt[rfmt_Pixmap] = i.data->id;
    }
}

/// Closes all active resources, windows, and server connections.
CXApp::~CXApp (void)
{
    if (!_pconn) return;
    xcb_disconnect (_pconn);
    _pconn = NULL;
}

int CXApp::Run (void)
{
    _wantQuit = false;
    for (xcb_generic_event_t* e; !_wantQuit && xcb_flush(_pconn)>0 && (e = xcb_wait_for_event(_pconn)); free(e)) {
	switch (e->response_type & 0x7f) {
	    case XCB_MAP_NOTIFY:	OnMap(); break;
	    case XCB_EXPOSE:		Update(); break;
	    case XCB_CONFIGURE_NOTIFY:	OnResize(e); break;
	    case XCB_KEY_PRESS:		OnKey (TranslateKeycode(e)); break;
	    case XCB_CLIENT_MESSAGE:	OnClientMessage(e); break;
	}
    }
    return (EXIT_SUCCESS);
}

void CXApp::Update (void)
{
    if (!_pconn || !_window)
	return;
    OnDraw();
    xcb_render_composite (_pconn, XCB_RENDER_PICT_OP_SRC, _bpict, XCB_NONE, _wpict, 0, 0, 0, 0, 0, 0, _winWidth, _winHeight);
}

//----------------------------------------------------------------------
// Window and mode management
//----------------------------------------------------------------------

void CXApp::CreateWindow (const char* title, int width, int height)
{
    _width = width; _height = height;
    // Create the window with given dimensions
    static const uint32_t winvals[] = { XCB_NONE, XCB_EVENT_MASK_EXPOSURE| XCB_EVENT_MASK_KEY_PRESS| XCB_EVENT_MASK_STRUCTURE_NOTIFY };
    xcb_create_window (_pconn, XCB_COPY_FROM_PARENT, _window=xcb_generate_id(_pconn),
	    _pscreen->root, 0, 0, _winWidth = width, _winHeight = height, 0,
	    XCB_WINDOW_CLASS_INPUT_OUTPUT, XCB_COPY_FROM_PARENT,
	    XCB_CW_BACK_PIXMAP| XCB_CW_EVENT_MASK, winvals);
    // Create backing pixmap
    xcb_pixmap_t bpixid = xcb_generate_id(_pconn);
    xcb_create_pixmap (_pconn, 32, bpixid, _window, width, height);
    xcb_create_gc (_pconn, _xgc = xcb_generate_id(_pconn), bpixid, 0, NULL);
    xcb_render_create_picture (_pconn, _bpict = xcb_generate_id(_pconn), bpixid, _xrfmt[rfmt_Pixmap], 0, NULL);
    xcb_free_pixmap (_pconn, bpixid);	// henceforth accessed only through _bpict
    xcb_render_create_picture (_pconn, _wpict = xcb_generate_id(_pconn), _window, _xrfmt[rfmt_Default], 0, NULL);
    // Set window title
    xcb_change_property (_pconn, XCB_PROP_MODE_REPLACE, _window, _atoms[xa_WM_NAME], _atoms[xa_STRING], 8, strlen(title), title);
    // Enable WM close message
    xcb_change_property (_pconn, XCB_PROP_MODE_REPLACE, _window, _atoms[xa_WM_PROTOCOLS], _atoms[xa_ATOM], 32, 1, &_atoms[xa_WM_DELETE_WINDOW]);
    // Set the fullscreen flag on the window
    xcb_change_property (_pconn, XCB_PROP_MODE_REPLACE, _window, _atoms[xa_NET_WM_STATE], _atoms[xa_ATOM], 32, 1, &_atoms[xa_NET_WM_STATE_FULLSCREEN]);
    // And put it on the screen
    xcb_map_window (_pconn, _window);
}

inline void CXApp::OnMap (void)
{
    LoadFont();
}

inline void CXApp::OnResize (const void* e)
{
    _winWidth = ((const xcb_configure_notify_event_t*)e)->width;
    _winHeight = ((const xcb_configure_notify_event_t*)e)->height;
    // Setup RENDER scaling of the backbuffer
    xcb_render_transform_t tr;
    memset (&tr, 0, sizeof(tr));
    tr.matrix11 = (_width<<16)/_winWidth;
    tr.matrix22 = (_height<<16)/_winHeight;
    tr.matrix33 = (1<<16);
    xcb_render_set_picture_transform (_pconn, _bpict, tr);
}

inline void CXApp::OnClientMessage (const void* e)
{
    const xcb_client_message_event_t* msg = (const xcb_client_message_event_t*) e;
    // This happens when the user clicks the close button on the window
    if (msg->window == _window && msg->type == _atoms[xa_WM_PROTOCOLS] && msg->data.data32[0] == _atoms[xa_WM_DELETE_WINDOW])
	Quit();
}

inline wchar_t CXApp::TranslateKeycode (const void* event) const
{
    const xcb_key_press_event_t* kp = (const xcb_key_press_event_t*)event;
    return (_ksyms[(kp->detail-_minKeycode)*_keysymsPerKeycode]);
}

CXApp::SImage CXApp::LoadImage (const char* const* p)
{
    SImage img;
    uint32_t d;
    sscanf (*p++, "%hu %hu %u", &img.w, &img.h, &d);
    uint32_t pal[128];
    for (uint32_t i = 0; i < d; ++i) {
	uint8_t ci; uint32_t cv = 0xff000000;
	sscanf (*p++, "%c c #%X", &ci, &cv);
	pal[ci] = cv ^ 0xff000000;
    }
    vector<uint32_t> pixels (img.w*img.h);
    for (uint32_t y = 0; y < img.h; ++y, ++p)
	for (uint32_t x = 0; x < img.w; ++x)
	    pixels[y*img.w+x] = pal[uint8_t((*p)[x])];

    xcb_pixmap_t pixid = xcb_generate_id(_pconn);
    xcb_create_pixmap (_pconn, 32, pixid, _window, img.w, img.h);
    xcb_put_image (_pconn, XCB_IMAGE_FORMAT_Z_PIXMAP, pixid, _xgc, img.w, img.h, 0, 0, 0, 32, pixels.size()*4, (const uint8_t*)&pixels[0]);
    xcb_render_create_picture (_pconn, img.id = xcb_generate_id(_pconn), pixid, _xrfmt[rfmt_Pixmap], 0, NULL);
    xcb_free_pixmap (_pconn, pixid);
    return (img);
}

void CXApp::DrawImageTile (const SImage& img, const SImageTile& tile, int x, int y)
{
    xcb_render_composite (_pconn, XCB_RENDER_PICT_OP_OVER, img.id, XCB_NONE, _bpict, tile.x, tile.y, 0, 0, x, y, tile.w, tile.h);
}

void CXApp::LoadFont (void)
{
    xcb_render_create_glyph_set (_pconn, _glyphset = xcb_generate_id(_pconn), _xrfmt[rfmt_Font]);
    const xcb_render_glyphinfo_t glyphi[] = {
	{ 4, 6, 0, 0, 4, 0 },
	{ 4, 6, 0, 0, 4, 0 },
    };
    uint32_t glid[2];
    uint8_t lbuf [glyphi[0].width*glyphi[0].height*2];
    fill_n (lbuf, sizeof(lbuf), 0);
    // The font bitmap has 128 glyphs on a 16x8 grid, each glyph line taking up 4 bits
    for (int row = 0; row < 8; ++row) {
	for (int col = 0; col < 8; ++col) {
	    uint8_t* l1d = &lbuf[0];			// So we do 2 at once
	    uint8_t* l2d = &lbuf[sizeof(lbuf)/2];
	    for (int y = 0; y < 5; ++y) {
		uint8_t v=font3x5_bits[row*8*6+col+y*8];
		for (int x = 0; x < 4; ++x, v>>=1)
		    *l1d++ = !(v&1)-1;
		for (int x = 0; x < 4; ++x, v>>=1)
		    *l2d++ = !(v&1)-1;
	    }
	    glid[0] = row*16+col*2;
	    glid[1] = row*16+col*2+1;
	    xcb_render_add_glyphs (_pconn, _glyphset, 2, glid, glyphi, sizeof(lbuf), lbuf);
	}
    }
    uint32_t repeatOn = 1, pixid = xcb_generate_id(_pconn);
    xcb_create_pixmap (_pconn, 32, pixid, _window, 1, 1);
    xcb_render_create_picture (_pconn, _glyphpen = xcb_generate_id(_pconn), pixid, _xrfmt[rfmt_Pixmap], XCB_RENDER_CP_REPEAT, &repeatOn);
    xcb_free_pixmap (_pconn, pixid);
    static const xcb_rectangle_t r = { 0, 0, 1, 1 };
    static const xcb_render_color_t rc = { 0, 0, 0, 0xffff };
    xcb_render_fill_rectangles (_pconn, XCB_RENDER_PICT_OP_SRC, _glyphpen, rc, 1, &r);
    _pencolor = 0;
}

void CXApp::DrawText (int x, int y, const char* s, uint32_t color)
{
    if (color != _pencolor) {
	xcb_render_color_t rc;
	rc.red = (color>>8)&0xff00;		// RENDER uses 16 bits per channel for colors
	rc.green = color&0xff00;
	rc.blue = (color<<8)&0xff00;
	rc.alpha = ((color>>24)&0xff00)^0xff00;	// ... and thinks that "alpha" means "opacity" instead of "transparency"
	static const xcb_rectangle_t r = { 0, 0, 1, 1 };
	xcb_render_fill_rectangles (_pconn, XCB_RENDER_PICT_OP_SRC, _glyphpen, rc, 1, &r);
	_pencolor = color;
    }
    struct TextElement {	// For some reason, render_glyphs call takes a list of these instead of a plain text string
	uint8_t		len;	// Number of characters in text
	uint8_t		_pad1;
	uint16_t	_pad2;
	int16_t		x, y;	// Offset from the last TextElement. The first element should have the passed in coordinates, the rest should have 0,0.
	char		text [256-8];
    };
    TextElement elt;
    uint8_t slen = min (strlen(s), sizeof(elt.text)-1);	// Maximum 248 chars; since this call does not see newlines and we are at 320x240, that's reasonable
    uint8_t eltsz = 8+Align(slen,4);			// This is the size of the header elements + the text padded to 4 byte grain
    elt.len = slen;
    elt.x = x; elt.y = y;
    memcpy (elt.text, s, slen);
    xcb_render_composite_glyphs_8 (_pconn, XCB_RENDER_PICT_OP_OVER, _glyphpen, _bpict, XCB_NONE, _glyphset, 0, 0, eltsz, (const uint8_t*) &elt);
}
