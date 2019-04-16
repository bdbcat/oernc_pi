/******************************************************************************
 *
 * Project:  OpenCPN
 * Purpose:  OFC Plugin
 * Author:   David Register
 *
 ***************************************************************************
 *   Copyright (C) 2018 by David S. Register                               *
 *   $EMAIL$                                                               *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, write to the                         *
 *   Free Software Foundation, Inc.,                                       *
 *   59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.             *
 ***************************************************************************
 */

#ifndef _OERNCPI_H_
#define _OERNCPI_H_

#include "wx/wxprec.h"

#ifndef  WX_PRECOMP
  #include "wx/wx.h"
#endif //precompiled headers

#include "version.h"

//#define     PLUGIN_VERSION_MAJOR    0
//#define     PLUGIN_VERSION_MINOR    1

#define     MY_API_VERSION_MAJOR    1
#define     MY_API_VERSION_MINOR    9

#include <ocpn_plugin.h>


//----------------------------------------------------------------------------------------------------------
//    Utility definitions
//----------------------------------------------------------------------------------------------------------

class shopPanel;

bool validate_server(void);
bool shutdown_server(void);


//----------------------------------------------------------------------------------------------------------
//    The PlugIn Class Definition
//----------------------------------------------------------------------------------------------------------

class oernc_pi : public opencpn_plugin_19
{
public:
      oernc_pi(void *ppimgr);
      ~oernc_pi();

//    The required PlugIn Methods
      int Init(void);
      bool DeInit(void);

      int GetAPIVersionMajor();
      int GetAPIVersionMinor();
      int GetPlugInVersionMajor();
      int GetPlugInVersionMinor();
      wxBitmap *GetPlugInBitmap();
      wxString GetCommonName();
      wxString GetShortDescription();
      wxString GetLongDescription();

      wxArrayString GetDynamicChartClassNameArray();

//       bool RenderOverlay(wxDC &dc, PlugIn_ViewPort *vp);
//       void SetPluginMessage(wxString &message_id, wxString &message_body);
//
//       bool RenderGLOverlay(wxGLContext *pcontext, PlugIn_ViewPort *vp){ return false; }

      void OnSetupOptions( void );
      void OnCloseToolboxPanel(int page_sel, int ok_apply_cancel);
      

      wxArrayString     m_class_name_array;

private:

      wxBitmap          *m_pplugin_icon;

      wxScrolledWindow  *m_pOptionsPage;
      shopPanel         *m_shoppanel;

};




#endif



