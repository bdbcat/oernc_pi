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
#include <wx/listctrl.h>
#include <wx/notebook.h>
#include <wx/html/htmlwin.h>

#include "version.h"

//#define     PLUGIN_VERSION_MAJOR    0
//#define     PLUGIN_VERSION_MINOR    1

#define     MY_API_VERSION_MAJOR    1
#define     MY_API_VERSION_MINOR    16

#include <ocpn_plugin.h>

#define ID_DIALOG 10001
#define SYMBOL_ABOUT_TITLE _("oeRNC_PI Information")
#define xID_OK          10009
#define xID_CANCEL      10010
#define ID_NOTEBOOK_HELP 10002

//----------------------------------------------------------------------------------------------------------
//    Utility definitions
//----------------------------------------------------------------------------------------------------------

class shopPanel;

bool validate_server(void);
bool shutdown_server(void);
void saveShopConfig(void);
wxString getFPR( bool bCopyToDesktop, bool &bCopyOK, bool bSGLock);

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

      void OnSetupOptions( void );
      void OnCloseToolboxPanel(int page_sel, int ok_apply_cancel);
      void ShowPreferencesDialog( wxWindow* parent );
      void Set_FPR();


      wxArrayString     m_class_name_array;
      shopPanel         *m_shoppanel;
      wxScrolledWindow  *m_pOptionsPage;

private:

      wxBitmap          *m_pplugin_icon;


};


class oerncPrefsDialog : public wxDialog 
{
private:
    
protected:
    wxStdDialogButtonSizer* m_sdbSizer1;
    wxButton* m_sdbSizer1OK;
    wxButton* m_sdbSizer1Cancel;
    
public:
    
    
    oerncPrefsDialog( wxWindow* parent, wxWindowID id = wxID_ANY, const wxString& title = _("oeRNC_PI Preferences"), const wxPoint& pos = wxDefaultPosition, const wxSize& size = wxDefaultSize, long style = wxCAPTION|wxDEFAULT_DIALOG_STYLE ); 
    ~oerncPrefsDialog();
    void OnPrefsOkClick(wxCommandEvent& event);
    
    wxButton *m_buttonNewFPR, *m_buttonNewDFPR;
    wxButton *m_buttonShowFPR;
    wxButton *m_buttonClearSystemName;
    wxButton *m_buttonClearCreds;
    wxStaticText *m_fpr_text;
    wxStaticText *m_nameTextBox;
    wxButton *m_buttonShowEULA;
    wxButton *m_buttonClearDownloadCache;
    DECLARE_EVENT_TABLE()
    
    
    
};


// An Event handler class to catch events from UI dialog
class oernc_pi_event_handler : public wxEvtHandler
{
public:
    
    oernc_pi_event_handler(oernc_pi *parent);
    ~oernc_pi_event_handler();
    
    void OnNewFPRClick( wxCommandEvent &event );
    void OnNewDFPRClick( wxCommandEvent &event );
    void OnShowFPRClick( wxCommandEvent &event );
    void onTimerEvent(wxTimerEvent &event);
    void OnGetHWIDClick( wxCommandEvent &event );
    void OnManageShopClick( wxCommandEvent &event );
    void OnClearSystemName( wxCommandEvent &event );
    void OnShowEULA( wxCommandEvent &event );
    void OnClearCredentials( wxCommandEvent &event );
    void OnClearDownloadCache( wxCommandEvent &event );

private:
    void processArbResult( wxString result );
    
    oernc_pi  *m_parent;
    
    wxTimer     m_eventTimer;
    int         m_timerAction;
    
    DECLARE_EVENT_TABLE()
    
};

class oeRNC_pi_about: public wxDialog
{
    DECLARE_DYNAMIC_CLASS( about )
    DECLARE_EVENT_TABLE()
    
public:
    explicit oeRNC_pi_about( );
    explicit oeRNC_pi_about( wxWindow* parent, 
                    wxWindowID id = ID_DIALOG,
                    const wxString& caption = SYMBOL_ABOUT_TITLE,
                    const wxPoint& pos = wxDefaultPosition,
                    const wxSize& size = wxSize(500, 500),
                    long style = wxCAPTION|wxRESIZE_BORDER|wxSYSTEM_MENU|wxCLOSE_BOX );

    explicit oeRNC_pi_about( wxWindow* parent, 
                              wxString fileName,
                              wxWindowID id = ID_DIALOG,
                              const wxString& caption = SYMBOL_ABOUT_TITLE,
                              const wxPoint& pos = wxDefaultPosition,
                              const wxSize& size = wxSize(500, 500),
                              long style = wxCAPTION|wxRESIZE_BORDER|wxSYSTEM_MENU|wxCLOSE_BOX );
    
    bool Create( wxWindow* parent,
                 wxWindowID id = ID_DIALOG,
                 const wxString& caption = SYMBOL_ABOUT_TITLE,
                 const wxPoint& pos = wxDefaultPosition,
                 const wxSize& size = wxSize(500, 500),
                 long style = wxCAPTION|wxRESIZE_BORDER|wxSYSTEM_MENU|wxCLOSE_BOX );
    
    void RecalculateSize( void );
    void SetOKMode();
    
private:
    void CreateControls( void );
    void Populate( void );
    void OnXidOkClick( wxCommandEvent& event );
    void OnXidRejectClick( wxCommandEvent& event );
    void OnPageChange(wxNotebookEvent& event);
    void OnClose( wxCloseEvent& event );
    
    wxWindow *m_parent;
    bool m_btips_loaded;
    
    wxPanel* itemPanelAbout;
    wxPanel* itemPanelAuthors;
    wxPanel* itemPanelLicense;
    wxPanel* itemPanelTips;
    
    wxTextCtrl *pAuthorTextCtl;
    wxTextCtrl *pLicenseTextCtl;
    wxNotebook *pNotebook;
    wxHtmlWindow *pAboutHTMLCtl;
    wxHtmlWindow *pLicenseHTMLCtl;
    wxHtmlWindow *pAuthorHTMLCtl;
    
    wxString m_fileName;
    wxButton* closeButton;
    wxButton* rejectButton;
    
    //wxSize m_displaySize;
    
};


#endif



