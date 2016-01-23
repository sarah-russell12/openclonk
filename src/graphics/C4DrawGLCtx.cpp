/*
 * OpenClonk, http://www.openclonk.org
 *
 * Copyright (c) 2001-2009, RedWolf Design GmbH, http://www.clonk.de/
 * Copyright (c) 2009-2013, The OpenClonk Team and contributors
 *
 * Distributed under the terms of the ISC license; see accompanying file
 * "COPYING" for details.
 *
 * "Clonk" is a registered trademark of Matthes Bender, used with permission.
 * See accompanying file "TRADEMARK" for details.
 *
 * To redistribute this file separately, substitute the full license texts
 * for the above references.
 */

/* OpenGL implementation of NewGfx, the context */

#include "C4Include.h"
#include <C4DrawGL.h>

#include <C4Window.h>
#include <C4App.h>

#ifndef USE_CONSOLE

static const int REQUESTED_GL_CTX_MAJOR = 3;
static const int REQUESTED_GL_CTX_MINOR = 2;

std::list<CStdGLCtx*> CStdGLCtx::contexts;

void CStdGLCtx::SelectCommon()
{
	pGL->pCurrCtx = this;
	// set some default states
	glDisable(GL_DEPTH_TEST);
	glDepthFunc(GL_LESS);
	glDisable(GL_CULL_FACE);
	glEnable(GL_BLEND);
	// Delete pending VAOs
	std::vector<GLuint> toBeDeleted;
	if (!VAOsToBeDeleted.empty())
	{
		for (unsigned int i = 0; i < VAOsToBeDeleted.size(); ++i)
		{
			if (VAOsToBeDeleted[i] < hVAOs.size() && hVAOs[VAOsToBeDeleted[i]] != 0)
			{
				toBeDeleted.push_back(hVAOs[VAOsToBeDeleted[i]]);
				hVAOs[VAOsToBeDeleted[i]] = 0;
			}
		}

		glDeleteVertexArrays(toBeDeleted.size(), &toBeDeleted[0]);
		VAOsToBeDeleted.clear();
	}
}

void CStdGLCtx::Reinitialize()
{
	assert(!pGL->pCurrCtx);
#ifdef USE_WIN32_WINDOWS
	if (hrc)
		wglDeleteContext(hrc);
	hrc = 0;
#endif
}


#if defined(USE_WIN32_WINDOWS) || (defined(_WIN32) && defined(USE_GTK))

#include <epoxy/wgl.h>

decltype(CStdGLCtx::hrc) CStdGLCtx::hrc = 0;
static PIXELFORMATDESCRIPTOR pfd;  // desired pixel format

// Enumerate available pixel formats. Choose the best pixel format in
// terms of color and depth buffer bits and then return all formats with
// different multisampling settings. If there are more then one, then choose
// the one with highest depth buffer size and lowest stencil and auxiliary
// buffer sizes since we don't use them in Clonk.
static std::vector<int> EnumeratePixelFormats(HDC hdc)
{
	std::vector<int> result;
	if(!wglGetPixelFormatAttribivARB) return result;

	int n_formats;
	int attributes = WGL_NUMBER_PIXEL_FORMATS_ARB;
	if(!wglGetPixelFormatAttribivARB(hdc, 0, 0, 1, &attributes, &n_formats)) return result;

	for(int i = 1; i < n_formats+1; ++i)
	{
		int new_attributes[] = { WGL_DRAW_TO_WINDOW_ARB, WGL_SUPPORT_OPENGL_ARB, WGL_DOUBLE_BUFFER_ARB, WGL_COLOR_BITS_ARB, WGL_DEPTH_BITS_ARB, WGL_STENCIL_BITS_ARB, WGL_AUX_BUFFERS_ARB, WGL_SAMPLE_BUFFERS_ARB, WGL_SAMPLES_ARB };
		const unsigned int nnew_attributes = sizeof(new_attributes)/sizeof(int);

		int new_results[nnew_attributes];
		if(!wglGetPixelFormatAttribivARB(hdc, i, 0, nnew_attributes, new_attributes, new_results)) continue;
		if(!new_results[0] || !new_results[1] || !new_results[2]) continue;
		if(new_results[3] < 16 || new_results[4] < 16) continue; // require at least 16 bits per pixel in color and depth

		// For no MS we do find a pixel format with depth 32 on my (ck's) computer,
		// however, when choosing it then texturing does not work anymore. I am not
		// exactly sure what the cause of that is, so let's not choose that one for now.
		if(new_results[4] > 24) continue;

		// Multisampling with just one sample is equivalent to no-multisampling,
		// so don't include that in the result.
		if(new_results[7] == 1 && new_results[8] == 1)
			continue;

		if(result.empty())
		{
			result.push_back(i);
		}
		else
		{
			int old_attributes[] = { WGL_COLOR_BITS_ARB };
			const unsigned int nold_attributes = sizeof(old_attributes)/sizeof(int);
			int old_results[nold_attributes];

			if(!wglGetPixelFormatAttribivARB(hdc, result[0], 0, nold_attributes, old_attributes, old_results)) continue;

			if(new_results[3] > old_results[0])
			{
				result.clear();
				result.push_back(i);
			}
			else if(new_results[3] == old_results[0])
			{
				unsigned int j;
				for(j = 0; j < result.size(); ++j)
				{
					int equiv_attributes[] = { WGL_DEPTH_BITS_ARB, WGL_STENCIL_BITS_ARB, WGL_AUX_BUFFERS_ARB, WGL_SAMPLE_BUFFERS_ARB, WGL_SAMPLES_ARB };
					const unsigned int nequiv_attributes = sizeof(equiv_attributes)/sizeof(int);
					int equiv_results[nequiv_attributes];
					if(!wglGetPixelFormatAttribivARB(hdc, result[j], 0, nequiv_attributes, equiv_attributes, equiv_results)) continue;

					if(new_results[7] == equiv_results[3] && new_results[8] == equiv_results[4])
					{
						if(new_results[4] > equiv_results[0] || (new_results[4] == equiv_results[0] && (new_results[5] < equiv_results[1] || (new_results[5] == equiv_results[1] && new_results[6] < equiv_results[2]))))
							result[j] = i;
						break;
					}
				}

				if(j == result.size()) result.push_back(i);
			}
		}
	}

	return result;
}

static int GetPixelFormatForMS(HDC hDC, int samples)
{
	std::vector<int> vec = EnumeratePixelFormats(hDC);
	for(unsigned int i = 0; i < vec.size(); ++i)
	{
		int attributes[] = { WGL_SAMPLE_BUFFERS_ARB, WGL_SAMPLES_ARB };
		const unsigned int n_attributes = 2;
		int results[2];
		if(!wglGetPixelFormatAttribivARB(hDC, vec[i], 0, n_attributes, attributes, results)) continue;

		if( (samples == 0 && results[0] == 0) ||
		    (samples > 0 && results[0] == 1 && results[1] == samples))
		{
			return vec[i];
		}
	}

	return 0;
}

CStdGLCtx::CStdGLCtx(): pWindow(0), hDC(0), this_context(contexts.end()) { }

void CStdGLCtx::Clear()
{
	Deselect();
	if (hDC)
	{
		ReleaseDC(pWindow ? pWindow->renderwnd : hWindow, hDC);
		hDC=0;
	}
	pWindow = 0; hWindow = NULL;

	if (this_context != contexts.end())
	{
		contexts.erase(this_context);
		this_context = contexts.end();
	}
}

bool CStdGLCtx::Init(C4Window * pWindow, C4AbstractApp *pApp)
{
	// safety
	if (!pGL) return false;

	// store window
	this->pWindow = pWindow;
	hWindow = pWindow->renderwnd;

	// get DC
	hDC = GetDC(hWindow);
	if(!hDC)
	{
		pGL->Error("  gl: Error getting DC");
		return false;
	}
	if (hrc)
	{
		SetPixelFormat(hDC, pGL->iPixelFormat, &pfd);
	}
	else
	{
		// Choose a good pixel format.
		int pixel_format;
		if((pixel_format = GetPixelFormatForMS(hDC, Config.Graphics.MultiSampling)) == 0)
			if((pixel_format = GetPixelFormatForMS(hDC, 0)) != 0)
				Config.Graphics.MultiSampling = 0;

		if (!pixel_format)
		{
			pGL->Error("  gl: Error choosing pixel format");
		}
		else
		{
			ZeroMemory(&pfd, sizeof(pfd)); pfd.nSize = sizeof(pfd);
			if(!DescribePixelFormat(hDC, pixel_format, sizeof(pfd), &pfd))
			{
				pGL->Error("  gl: Error describing chosen pixel format");
			}
			else if(!SetPixelFormat(hDC, pixel_format, &pfd))
			{
				pGL->Error("  gl: Error setting chosen pixel format");
			}
			else
			{
				// create context
				if (wglCreateContextAttribsARB)
				{
					const int attribs[] = {
						WGL_CONTEXT_FLAGS_ARB, Config.Graphics.DebugOpenGL ? WGL_CONTEXT_DEBUG_BIT_ARB : 0,
						WGL_CONTEXT_MAJOR_VERSION_ARB, REQUESTED_GL_CTX_MAJOR,
						WGL_CONTEXT_MINOR_VERSION_ARB, REQUESTED_GL_CTX_MINOR,
						WGL_CONTEXT_PROFILE_MASK_ARB, WGL_CONTEXT_CORE_PROFILE_BIT_ARB,
						0
					};

					if (Config.Graphics.DebugOpenGL)
						DebugLog("  gl: Creating debug context.");
					hrc = wglCreateContextAttribsARB(hDC, 0, attribs);
				}
				else
				{
					DebugLog("  gl: wglCreateContextAttribsARB not available; creating default context.");
					hrc = wglCreateContext(hDC);
				}

				if(!hrc)
				{
					pGL->Error("  gl: Error creating gl context");
				}

				pGL->iPixelFormat = pixel_format;
			}
		}
	}
	if (hrc)
	{
		Select();

		this_context = contexts.insert(contexts.end(), this);
		return true;
	}

	ReleaseDC(hWindow, hDC); hDC = NULL;
	return false;
}

std::vector<int> CStdGLCtx::EnumerateMultiSamples() const
{
	std::vector<int> result;
	std::vector<int> vec = EnumeratePixelFormats(hDC);
	for(unsigned int i = 0; i < vec.size(); ++i)
	{
		int attributes[] = { WGL_SAMPLE_BUFFERS_ARB, WGL_SAMPLES_ARB };
		const unsigned int n_attributes = 2;
		int results[2];
		if(!wglGetPixelFormatAttribivARB(hDC, vec[i], 0, n_attributes, attributes, results)) continue;

		if(results[0] == 1) result.push_back(results[1]);
	}

	return result;
}

bool CStdGLCtx::Select(bool verbose)
{
	// safety
	if (!pGL || !hrc) return false;
	// make context current
	if (!wglMakeCurrent (hDC, hrc))
	{
		pGL->Error("Unable to select context.");
		return false;
	}
	SelectCommon();
	// update clipper - might have been done by UpdateSize
	// however, the wrong size might have been assumed
	if (!pGL->UpdateClipper()) return false;
	// success
	return true;
}

void CStdGLCtx::Deselect()
{
	if (pGL && pGL->pCurrCtx == this)
	{
		wglMakeCurrent(NULL, NULL);
		pGL->pCurrCtx=NULL;
		pGL->RenderTarget=NULL;
	}
}

bool CStdGLCtx::PageFlip()
{
	// flush GL buffer
	glFlush();
	SwapBuffers(hDC);
	return true;
}

#elif defined(USE_GTK)
#include <epoxy/glx.h>
#include <gtk/gtk.h>
#include <gdk/gdkx.h>

namespace {
void InitGLXPointers()
{
	glXGetVisualFromFBConfig = (PFNGLXGETVISUALFROMFBCONFIGPROC)(glXGetProcAddress((const GLubyte*)"glXGetVisualFromFBConfig"));
	glXChooseFBConfig = (PFNGLXCHOOSEFBCONFIGPROC)(glXGetProcAddress((const GLubyte*)"glXChooseFBConfig"));
	glXCreateNewContext = (PFNGLXCREATENEWCONTEXTPROC)(glXGetProcAddress((const GLubyte*)"glXCreateNewContext"));
}
}

CStdGLCtx::CStdGLCtx(): pWindow(0), ctx(0), this_context(contexts.end()) { }

void CStdGLCtx::Clear()
{
	Deselect();
	if (ctx)
	{
		Display * const dpy = gdk_x11_display_get_xdisplay(gdk_display_get_default());
		glXDestroyContext(dpy, (GLXContext)ctx);
		ctx = 0;
	}
	pWindow = 0;

	if (this_context != contexts.end())
	{
		contexts.erase(this_context);
		this_context = contexts.end();
	}
}

bool CStdGLCtx::Init(C4Window * pWindow, C4AbstractApp *)
{
	// safety
	if (!pGL) return false;
	// store window
	this->pWindow = pWindow;
	Display * const dpy = gdk_x11_display_get_xdisplay(gdk_display_get_default());
	InitGLXPointers();
	if (!glXGetVisualFromFBConfig || !glXChooseFBConfig || !glXCreateNewContext)
	{
		return pGL->Error("  gl: Unable to retrieve GLX 1.4 entry points");
	}

	// Create Context with sharing (if this is the main context, our ctx will be 0, so no sharing)
	const int attribs[] = {
		GLX_CONTEXT_MAJOR_VERSION_ARB, REQUESTED_GL_CTX_MAJOR,
		GLX_CONTEXT_MINOR_VERSION_ARB, REQUESTED_GL_CTX_MINOR,
		GLX_CONTEXT_FLAGS_ARB, (Config.Graphics.DebugOpenGL ? GLX_CONTEXT_DEBUG_BIT_ARB : 0),
		GLX_CONTEXT_PROFILE_MASK_ARB, GLX_CONTEXT_CORE_PROFILE_BIT_ARB,
		None
	};
	GLXContext share_context = (pGL->pMainCtx != this) ? static_cast<GLXContext>(pGL->pMainCtx->ctx) : 0;

	if (glXCreateContextAttribsARB)
	{
		DebugLogF("  gl: Creating %s context", Config.Graphics.DebugOpenGL ? "debug" : "standard");
		ctx = glXCreateContextAttribsARB(dpy, pWindow->Info, share_context, True, attribs);
	}
	else
	{
		DebugLog("  gl: glXCreateContextAttribsARB not supported; falling back to attribute-less context creation");
		ctx = glXCreateNewContext(dpy, pWindow->Info, GLX_RGBA_TYPE, share_context, True);
	}

	// No luck?
	if (!ctx) return pGL->Error("  gl: Unable to create context");
	if (!Select(true)) return pGL->Error("  gl: Unable to select context");

	this_context = contexts.insert(contexts.end(), this);
	return true;
}

bool CStdGLCtx::Select(bool verbose)
{
	// safety
	if (!pGL || !ctx)
	{
		if (verbose) pGL->Error("  gl: pGL is zero");
		return false;
	}
	Display * const dpy = gdk_x11_display_get_xdisplay(gdk_display_get_default());
	// make context current
	if (!pWindow->renderwnd || !glXMakeCurrent(dpy, pWindow->renderwnd, (GLXContext)ctx))
	{
		if (verbose) pGL->Error("  gl: glXMakeCurrent failed");
		return false;
	}
	SelectCommon();
	// update clipper - might have been done by UpdateSize
	// however, the wrong size might have been assumed
	if (!pGL->UpdateClipper())
	{
		if (verbose) pGL->Error("  gl: UpdateClipper failed");
		return false;
	}
	// success
	return true;
}

void CStdGLCtx::Deselect()
{
	if (pGL && pGL->pCurrCtx == this)
	{
		Display * const dpy = gdk_x11_display_get_xdisplay(gdk_display_get_default());
		glXMakeCurrent(dpy, None, NULL);
		pGL->pCurrCtx = 0;
		pGL->RenderTarget = 0;
	}
}

bool CStdGLCtx::PageFlip()
{
	// flush GL buffer
	glFlush();
	if (!pWindow || !pWindow->renderwnd) return false;
	Display * const dpy = gdk_x11_display_get_xdisplay(gdk_display_get_default());
	glXSwapBuffers(dpy, pWindow->renderwnd);
	return true;
}

#elif defined(USE_SDL_MAINLOOP)

CStdGLCtx::CStdGLCtx(): pWindow(0), this_context(contexts.end()) { }

void CStdGLCtx::Clear()
{
	pWindow = 0;

	if (this_context != contexts.end())
	{
		contexts.erase(this_context);
		this_context = contexts.end();
	}
}

bool CStdGLCtx::Init(C4Window * pWindow, C4AbstractApp *)
{
	// safety
	if (!pGL) return false;
	// store window
	this->pWindow = pWindow;
	// No luck at all?
	if (!Select(true)) return pGL->Error("  gl: Unable to select context");

	this_context = contexts.insert(contexts.end(), this);
	return true;
}

bool CStdGLCtx::Select(bool verbose)
{
	SelectCommon();
	// update clipper - might have been done by UpdateSize
	// however, the wrong size might have been assumed
	if (!pGL->UpdateClipper())
	{
		if (verbose) pGL->Error("  gl: UpdateClipper failed");
		return false;
	}
	// success
	return true;
}

void CStdGLCtx::Deselect()
{
	if (pGL && pGL->pCurrCtx == this)
	{
		pGL->pCurrCtx = 0;
		pGL->RenderTarget = 0;
	}
}

bool CStdGLCtx::PageFlip()
{
	// flush GL buffer
	glFlush();
	if (!pWindow) return false;
	SDL_GL_SwapBuffers();
	return true;
}

#endif //USE_GTK/USE_SDL_MAINLOOP

#endif // USE_CONSOLE
