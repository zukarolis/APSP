/* ============================================================================
 *  LPushButton_win32.h
 * ============================================================================

 *  Author:         (c) 2002 Andrea Ribichini
 *  License:        See the end of this file for license information
 *  Created:        May 19, 2002
 *  Module:         LL

 *  Last changed:   $Date: 2003/05/15 15:21:00 $
 *  Changed by:     $Author: demetres $
 *  Revision:       $Revision: 1.1.1.1 $
*/

#ifndef __LPushButton_win32__
#define __LPushButton_win32__

#if __LSL_OS_GUI__ == __LSL_Win32__

struct tagLPushButton {
    HWND mHwnd;
    void (*mHandler) ();
    ui2 mID;
    LWindow* mParentWindow;
};

#endif
#endif

/* Copyright (C) 2002 Andrea Ribichini

 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.

 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.

 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/

