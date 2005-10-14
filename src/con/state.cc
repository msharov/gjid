// Copyright (c) 2003-2006 by Mike Sharov <msharov@users.sourceforge.net>
//
/// \file state.cc
/// \brief Console state management

#include "state.h"
#include <linux/vt.h>
#include <linux/kd.h>
#include <linux/kdev_t.h>	// MAJOR(rdev), MINOR(rdev) for tty validation.
#include <linux/major.h>	// tty validation
#include <unistd.h>
#include <signal.h>
#include <sys/ioctl.h>
#include <sys/stat.h>

namespace fbgl {

//----------------------------------------------------------------------

static void ConsoleSignalHandler (int sig)
{
    if (sig == SIGWINCH)
	CConsoleState::Instance().OnResize();
    else if (sig == SIG_VTACQ) {
	ioctl (STDIN_FILENO, VT_RELDISP, VT_ACKACQ);	// ok to switch in
	CConsoleState::Instance().Activate (true);
    } else if (sig == SIG_VTREL) {
	CConsoleState::Instance().Activate (false);
	ioctl (STDIN_FILENO, VT_RELDISP, 1);		// ok to switch out
    }
}

//----------------------------------------------------------------------

/// Returns singleton instance.
/*static*/ CConsoleState& CConsoleState::Instance (void)
{
    static CConsoleState s_State;
    return (s_State);
}

/// Default constructor.
CConsoleState::CConsoleState (void)
: m_pFramebuffer (NULL),
  m_TI (),
  m_Kb (),
  m_OrigVtMode (KD_TEXT),
  m_bActive (true)
{
}

/// Default destructor.
CConsoleState::~CConsoleState (void)
{
}

/// Reads terminal description \p name and sets up console accordingly.
void CConsoleState::SetTerm (const char* termname)
{
    if (!m_TI.Name().empty())
	return;
    m_TI.Load (termname);
    m_Kb.LoadKeymap (m_TI);
}

/// Puts the console into graphical mode.
void CConsoleState::EnterGraphicsMode (void)
{
    if (m_Kb.IsInUIMode() || !isatty(STDIN_FILENO))
	return;
    if (isatty(STDOUT_FILENO)) {
	cout << m_TI.HideCursor();
	cout.flush();
    }
    m_Kb.EnterUIMode();
    ioctl (STDIN_FILENO, KDGETMODE, &m_OrigVtMode);
    if (m_OrigVtMode != KD_GRAPHICS && !getenv("IN_DEBUGGER") && ioctl (STDIN_FILENO, KDSETMODE, KD_GRAPHICS))
	throw file_exception ("ioctl(KDSETMODE)", "stdin");
}

/// Leaves graphical mode.
void CConsoleState::LeaveGraphicsMode (void)
{
    if (m_OrigVtMode != KD_GRAPHICS)
	ioctl (STDIN_FILENO, KDSETMODE, m_OrigVtMode);
    if (!m_Kb.IsInUIMode())
	return;
    m_Kb.LeaveUIMode();
    if (isatty (STDOUT_FILENO)) {
	cout << m_TI.ShowCursor();
	cout.flush();
    }
}

void CConsoleState::RegisterFramebuffer (CFramebuffer* pFb)
{
    assert ((!m_pFramebuffer || !pFb) && "You can only register one framebuffer");
    if (!m_pFramebuffer && pFb)
	AttachToConsole();
    if (IsActive()) {
	if (m_pFramebuffer && !pFb)
	    m_pFramebuffer->OnFocus (false);
	if (pFb && m_pFramebuffer != pFb)
	    pFb->OnFocus (true);
    }
    m_pFramebuffer = pFb;
}

/// Returns the index of the current virtual terminal. -1 if not a console.
int CConsoleState::VtIndex (void) const
{
    if (!isatty (STDIN_FILENO))
	return (-1);
    struct stat ttyStat;
    if (fstat (STDIN_FILENO, &ttyStat))
	throw file_exception ("fstat", "stdin");
    return ((MAJOR(ttyStat.st_rdev) == TTY_MAJOR) ? int(MINOR(ttyStat.st_rdev)) : -1);
}

/// Assumes control of console switching.
void CConsoleState::AttachToConsole (void)
{
    if (signal (SIGWINCH, ConsoleSignalHandler) == SIG_ERR)
	throw libc_exception ("signal");

    if (VtIndex() < 0)
	return;	// If not on a real console, then can't use vt ioctls.
    if (signal (SIG_VTACQ, ConsoleSignalHandler) == SIG_ERR ||
	signal (SIG_VTREL, ConsoleSignalHandler) == SIG_ERR)
	throw libc_exception ("signal");

    // Tell the console to signal when gaining/losing focus.
    struct vt_mode vtm;
    if (ioctl (STDIN_FILENO, VT_GETMODE, &vtm))
	throw file_exception ("ioctl(VT_GETMODE)", "stdin");
    vtm.mode   = VT_PROCESS;	// Process must acknowledge switch in signal handler.
    vtm.waitv  = 0;		// Don't block writes.
    vtm.acqsig = SIG_VTACQ;	// Raise this on VT acquisition.
    vtm.relsig = SIG_VTREL;	// Raise this on VT release.
    if (ioctl (STDIN_FILENO, VT_SETMODE, &vtm))
	throw file_exception ("ioctl(VT_SETMODE)", "stdin");

    SetTerm();
}

/// Performs actions on console activation.
void CConsoleState::Activate (bool v)
{
    m_bActive = v;
    if (m_pFramebuffer)
	m_pFramebuffer->OnFocus (m_bActive);	// Restores screen.
}

/// Call when the console is resized.
void CConsoleState::OnResize (void)
{
    m_TI.Update();
}

//----------------------------------------------------------------------

} // namespace pm_console

