// fb.h
//

#ifndef FB_H_03AB4F7E09259B61062C6ACD0AB3CC92
#define FB_H_03AB4F7E09259B61062C6ACD0AB3CC92

#include "../fb.h"
#include <X11/Xlib.h>

namespace fbgl {

/// \class CXlibFramebuffer xfb fbgl.h
///
/// \brief Xlib backend.
///
class CXlibFramebuffer : public CFramebuffer {
public:
    static CXlibFramebuffer&	Instance (void);
    virtual void		Open (void);
    virtual void		Close (void);
    virtual void		SetMode (CMode m, size_t depth);
    virtual void		CheckEvents (CEventProcessor* evp);
    virtual void		Flush (void);
    void			OnIOError (void);
protected:
				CXlibFramebuffer (void);
    virtual		       ~CXlibFramebuffer (void);
    virtual void		LoadModes (modevec_t& mv);
private:
    void			DecodeMotion (CEventProcessor* pep, const XMotionEvent& e);
    void			DecodeButton (CEventProcessor* pep, const XButtonEvent& e);
    void			DecodeKey (CEventProcessor* pep, const XKeyEvent& e);
    void			WaitForEvents (void);
    void			SetFullscreenMode (bool v = true);
    template <typename PixelType>
    void			InitColormap (PixelType* cmap) const;
    template <typename PixelType>
    void			CopyGCToImage (void);
private:
    typedef ::GC		XGC;
private:
    Display*			m_pDisplay;
    Visual*			m_pVisual;
    XGC				m_XGC;
    memblock			m_ImageData;
    XImage*			m_pImage;
    Window			m_Window;
};

} // namespace fbgl

#endif

