/*
 * OpenClonk, http://www.openclonk.org
 *
 * Copyright (c) 2009-2015, The OpenClonk Team and contributors
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

#include <C4Include.h>
#include <C4Application.h>
#include <C4Viewport.h>
#include <C4ViewportWindow.h>
#include <C4FullScreen.h>
#include <C4Landscape.h>

#include <epoxy/gl.h>

#import "C4WindowController.h"
#import "C4DrawGLMac.h"
#import "C4EditorWindowController.h"
#import "C4AppDelegate.h"
#import "AppKit/NSOpenGL.h"

static SInt32 osVersion()
{
	SInt32 ver;
	Gestalt(gestaltSystemVersion, &ver);
	return ver;
}

bool lionAndBeyond() {return osVersion() >= 0x1070;}

#ifdef USE_COCOA

// Turns out, it is necessary to derive a NSWindow class after all - or else the screen-filling window won't be able to become the key window

@interface ClonkScreenfillingWindow: NSWindow
{}
- (BOOL) canBecomeKeyWindow;
@end

@implementation ClonkScreenfillingWindow
- (BOOL) canBecomeKeyWindow;
{
	return YES; // a resounding one
}
@end

@implementation C4WindowController

@synthesize stdWindow, openGLView, scrollView;

- (void) awakeFromNib
{
	[super awakeFromNib];
	if (!Application.isEditor)
		C4AppDelegate.instance.gameWindowController = self;
}

- (void) fadeOut:(CGDisplayFadeReservationToken*)token
{
	if (CGAcquireDisplayFadeReservation(15, token) == 0)
		CGDisplayFade(*token, 0.2, 0.0, 1.0, 0, 0, 0, YES);
}

- (void) fadeIn:(CGDisplayFadeReservationToken)token
{
	CGDisplayFade(token, 0.2, 1.0, 0.0, 0, 0, 0, YES);
	CGReleaseDisplayFadeReservation(token);
}

- (BOOL) isFullScreen {
	return fullscreenWindow != nil;
}

- (BOOL) isFullScreenConsideringLionFullScreen
{
	return
		[self isFullScreen] ||
		(lionAndBeyond() && (self.window.styleMask & NSFullScreenWindowMask) == NSFullScreenWindowMask);
}

- (void) setFullscreen:(BOOL)fullscreen
{
	if (fullscreen != [self isFullScreen])
	{
		// fade out
#ifndef _DEBUG
		CGDisplayFadeReservationToken token;
		[self fadeOut:&token];
#endif
		if (![self isFullScreen])
		{
			NSRect fullscreenRect = NSScreen.mainScreen.frame;
			fullscreenWindow = [[ClonkScreenfillingWindow alloc] initWithContentRect:fullscreenRect styleMask:NSBorderlessWindowMask backing:NSBackingStoreBuffered defer:YES];
			[fullscreenWindow setLevel:NSMainMenuWindowLevel+1];
			[fullscreenWindow setOpaque:YES];
			[fullscreenWindow setHidesOnDeactivate:YES];
			[fullscreenWindow setContentView:openGLView];
			[fullscreenWindow setReleasedWhenClosed:YES];
			[fullscreenWindow setDelegate:self];
			[self.window orderOut:self];
			[fullscreenWindow setInitialFirstResponder:openGLView];
			[fullscreenWindow makeKeyAndOrderFront:self];
			//[openGLView setContextSurfaceBackingSizeToOwnDimensions];
			[openGLView enableEvents];
			// hide cursor completely
			[NSCursor hide];
		}
		else
		{
			// unhide and rely on cursor rects again
			[NSCursor unhide];
		
			[fullscreenWindow close];
			fullscreenWindow = nil;
			[self.window setContentView:openGLView];
			[self.window orderFront:self];
			[openGLView setFrame:[self.window.contentView frame]];
			[openGLView enableEvents];
			[self.window makeKeyAndOrderFront:self];
		}
		[openGLView display];
#ifndef _DEBUG
		// fade in again
		[self fadeIn:token];
#endif
	}
}

- (BOOL) windowShouldClose:(id)sender
{
	if (sender == self.window && self.stdWindow)
	{
		[self.openGLView showCursor];
		self.stdWindow->Close();
	}
	return YES;
}

- (C4Viewport*) viewport
{
	for (C4Viewport* v = ::Viewports.GetFirstViewport(); v; v = v->GetNext())
		if (v->GetWindow() == stdWindow || (!v->GetWindow() && stdWindow == &FullScreen))
			return v;
	return NULL;
}

- (IBAction) scroll:(id)sender
{
	C4Viewport* viewport = self.viewport;
	if (viewport)
	{
		viewport->ViewPositionByScrollBars();
		viewport->Execute();
	}
}

- (void) windowDidResize:(NSNotification *)notification
{
	if (Application.isEditor)
	{
		C4Viewport* viewport = self.viewport;
		if (viewport && Application.isEditor)
		{
			viewport->ScrollBarsByViewPosition();
		}
	}
}

- (BOOL) isLiveResizing
{
	return self.window.inLiveResize;
}

- (NSRect) windowWillUseStandardFrame:(NSWindow*) window defaultFrame:(NSRect) newFrame
{
	return NSMakeRect(newFrame.origin.x, newFrame.origin.y, preferredContentSize.width, preferredContentSize.height);
}

- (NSSize)window:(NSWindow *)window willUseFullScreenContentSize:(NSSize)proposedSize
{
	if (stdWindow == &::FullScreen)
		return NSMakeSize(
			CGDisplayPixelsWide(C4OpenGLView.displayID),
			CGDisplayPixelsHigh(C4OpenGLView.displayID)
		);
	else
		return proposedSize;
}

- (void) windowWillExitFullScreen:(NSNotification *)notification
{
	if (!Application.isEditor)
	{
		CGAssociateMouseAndMouseCursorPosition(TRUE);
	}
}

- (void) setContentSize:(NSSize)size
{
	[self.window setContentSize:size];
	preferredContentSize = size;
}

@end

@interface NSScroller (ClonkZoom)
- (void) setToLandscapeCoordinate:(float)lc
	size:(int) s
	viewportSize:(int) vs
	zoom: (float) z;
- (float) landscapeCoordinateForSize:(int) s
	viewportSize:(int) vs
	zoom:(float) z;
@end
@implementation NSScroller (ClonkZoom)
- (void) setToLandscapeCoordinate:(float)lc
	size:(int) s
	viewportSize:(int) vs
	zoom: (float) z
{
	self.doubleValue = (double)lc/((double)s - (double)vs/z);
	self.knobProportion = (CGFloat)std::min(1.0, (double)vs/(s*z));
}

- (float) landscapeCoordinateForSize:(int) s
	viewportSize:(int) vs
	zoom:(float) z
{	
	return self.doubleValue * ((double)s - (double)vs/z);
}
@end

// C4ViewportWindow

bool C4Viewport::ScrollBarsByViewPosition()
{
	if (PlayerLock) return false;
	NSScrollView* scrollView = pWindow->objectiveCObject<C4WindowController>().scrollView;
	[scrollView.horizontalScroller setToLandscapeCoordinate:GetViewX() size:GBackWdt viewportSize:ViewWdt zoom:GetZoom()];
	[scrollView.verticalScroller setToLandscapeCoordinate:GetViewY() size:GBackHgt viewportSize:ViewHgt zoom:GetZoom()];
	return true;
}

bool C4Viewport::ViewPositionByScrollBars()
{
	NSScrollView* scrollView = pWindow->objectiveCObject<C4WindowController>().scrollView;
	SetViewX([scrollView.horizontalScroller landscapeCoordinateForSize:GBackWdt viewportSize:ViewWdt zoom:GetZoom()]);
	SetViewY([scrollView.verticalScroller landscapeCoordinateForSize:GBackHgt viewportSize:ViewHgt zoom:GetZoom()]);
	return true;
}

bool C4Viewport::TogglePlayerLock()
{
	NSScrollView* scrollView = pWindow->objectiveCObject<C4WindowController>().scrollView;
	if (PlayerLock)
	{
		PlayerLock = false;
		if (scrollView)
		{
			[scrollView.verticalScroller setEnabled:YES];
			[scrollView.horizontalScroller setEnabled:YES];
			[scrollView setAutohidesScrollers:NO];
		}
		ScrollBarsByViewPosition();
	}
	else
	{
		PlayerLock = true;
		if (scrollView)
		{
			[scrollView.verticalScroller setEnabled:NO];
			[scrollView.horizontalScroller setEnabled:NO];
			[scrollView setAutohidesScrollers:YES];
		}
	}
	return true;
}

#endif
