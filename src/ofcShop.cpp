/******************************************************************************
 *
 * Project:  oesenc_pi
 * Purpose:  oesenc_pi Plugin core
 * Author:   David Register
 *
 ***************************************************************************
 *   Copyright (C) 2018 by David S. Register                               *
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


#include "wx/wxprec.h"

#ifndef  WX_PRECOMP
  #include "wx/wx.h"
#endif //precompiled headers

#include "wx/artprov.h"
#include <wx/fileconf.h>
#include <wx/uri.h>
#include "wx/tokenzr.h"
#include <wx/dir.h>
#include "ofcShop.h"
#include "ocpn_plugin.h"
#include <tinyxml.h>
#include "wx/wfstream.h"
#include <wx/zipstrm.h>
#include <memory>
#include "base64.h"
#include "oernc_inStream.h"
#include "oernc_pi.h"
#include "version.h"

#ifdef __OCPN_USE_CURL__
    #include "wxcurl/wx/curl/http.h"
    #include "wxcurl/wx/curl/thread.h"
#endif    

#ifdef __OCPN__ANDROID__
#include "androidSupport.h"
#include "qdebug.h"
#include <QtWidgets/QScroller>
#endif

#include <wx/arrimpl.cpp> 
WX_DEFINE_OBJARRAY(ArrayOfCharts);
WX_DEFINE_OBJARRAY(ArrayOfChartPanels);
WX_DEFINE_OBJARRAY(ArrayOfchartMetaInfo);

WX_DECLARE_STRING_HASH_MAP( int, ProdSKUIndexHash );


//  Static variables
extern int g_debugLevel;
ArrayOfCharts g_ChartArray;

int g_timeout_secs = 10;

wxArrayString g_systemNameChoiceArray;
wxArrayString g_systemNameServerArray;

extern int g_admin;
extern wxString g_loginUser;
extern wxString g_PrivateDataDir;
wxString  g_loginPass;
wxString g_dlStatPrefix;
extern wxString  g_versionString;

shopPanel *g_shopPanel;

#ifdef __OCPN_USE_CURL__
OESENC_CURL_EvtHandler *g_CurlEventHandler;
wxCurlDownloadThread *g_curlDownloadThread;
#endif

wxFFileOutputStream *downloadOutStream;
bool g_chartListUpdatedOK;
wxString g_statusOverride;
wxString g_lastInstallDir;

int g_downloadChainIdentifier;
itemChart* g_chartProcessing;

double dl_now;
double dl_total;
time_t g_progressTicks;

long g_FileDownloadHandle;

#define ID_CMD_BUTTON_INSTALL 7783
#define ID_CMD_BUTTON_INSTALL_CHAIN 7784
#define ID_CMD_BUTTON_INDEX_CHAIN 7785
#define ID_CMD_BUTTON_DOWNLOADLIST_CHAIN 7786
#define ID_CMD_BUTTON_DOWNLOADLIST_PROC 7787

#ifdef __OCPN__ANDROID__

QString qtStyleSheet = "QScrollBar:horizontal {\
border: 0px solid grey;\
background-color: rgb(240, 240, 240);\
height: 50px;\
margin: 0px 1px 0 1px;\
}\
QScrollBar::handle:horizontal {\
background-color: rgb(200, 200, 200);\
min-width: 20px;\
border-radius: 10px;\
}\
QScrollBar::add-line:horizontal {\
border: 0px solid grey;\
background: #32CC99;\
width: 0px;\
subcontrol-position: right;\
subcontrol-origin: margin;\
}\
QScrollBar::sub-line:horizontal {\
border: 0px solid grey;\
background: #32CC99;\
width: 0px;\
subcontrol-position: left;\
subcontrol-origin: margin;\
}\
QScrollBar:vertical {\
border: 0px solid grey;\
background-color: rgb(240, 240, 240);\
width: 50px;\
margin: 1px 0px 1px 0px;\
}\
QScrollBar::handle:vertical {\
background-color: rgb(200, 200, 200);\
min-height: 50px;\
border-radius: 10px;\
}\
QScrollBar::add-line:vertical {\
border: 0px solid grey;\
background: #32CC99;\
height: 0px;\
subcontrol-position: top;\
subcontrol-origin: margin;\
}\
QScrollBar::sub-line:vertical {\
border: 0px solid grey;\
background: #32CC99;\
height: 0px;\
subcontrol-position: bottom;\
subcontrol-origin: margin;\
}\
QCheckBox {\
spacing: 25px;\
}\
QCheckBox::indicator {\
width: 30px;\
height: 30px;\
}\
";

#endif

#define ANDROID_DIALOG_BACKGROUND_COLOR    wxColour(_T("#7cb0e9"))
#define ANDROID_DIALOG_BODY_COLOR         wxColour(192, 192, 192)


// Private class implementations

class  OFCMessageDialog: public wxDialog
{
    
public:
    OFCMessageDialog(wxWindow *parent, const wxString& message,
                      const wxString& caption = wxMessageBoxCaptionStr,
                      long style = wxOK|wxCENTRE);
    
    void OnYes(wxCommandEvent& event);
    void OnNo(wxCommandEvent& event);
    void OnCancel(wxCommandEvent& event);
    void OnClose( wxCloseEvent& event );
    
private:
    int m_style;
    DECLARE_EVENT_TABLE()
};

BEGIN_EVENT_TABLE(OFCMessageDialog, wxDialog)
EVT_BUTTON(wxID_YES, OFCMessageDialog::OnYes)
EVT_BUTTON(wxID_NO, OFCMessageDialog::OnNo)
EVT_BUTTON(wxID_CANCEL, OFCMessageDialog::OnCancel)
EVT_CLOSE(OFCMessageDialog::OnClose)
END_EVENT_TABLE()


OFCMessageDialog::OFCMessageDialog( wxWindow *parent,
                                      const wxString& message,
                                      const wxString& caption,
                                      long style)
: wxDialog( parent, wxID_ANY, caption, wxDefaultPosition, wxDefaultSize, wxDEFAULT_DIALOG_STYLE | wxSTAY_ON_TOP )
{
    SetBackgroundColour(ANDROID_DIALOG_BACKGROUND_COLOR);
    
    wxBoxSizer *topsizer = new wxBoxSizer( wxVERTICAL );
    SetSizer( topsizer );
    
    wxStaticBox* itemStaticBoxSizer4Static = new wxStaticBox( this, wxID_ANY, caption );
    
    wxStaticBoxSizer* itemStaticBoxSizer4 = new wxStaticBoxSizer( itemStaticBoxSizer4Static, wxVERTICAL );
    topsizer->Add( itemStaticBoxSizer4, 0, wxEXPAND | wxALL, 5 );
    
    itemStaticBoxSizer4->AddSpacer(10);
    
    wxStaticLine *staticLine121 = new wxStaticLine(this, wxID_ANY, wxDefaultPosition, wxDLG_UNIT(this, wxSize(-1,-1)), wxLI_HORIZONTAL);
    itemStaticBoxSizer4->Add(staticLine121, 0, wxALL|wxEXPAND, WXC_FROM_DIP(5));
    
    
    
    
    wxPanel *messagePanel = new wxPanel(this, wxID_ANY, wxDefaultPosition, wxDLG_UNIT(this, wxSize(-1,-1)), wxBG_STYLE_ERASE );
    itemStaticBoxSizer4->Add(messagePanel, 0, wxALL|wxEXPAND, WXC_FROM_DIP(5));
    messagePanel->SetForegroundColour(wxColour(200, 200, 200));
    
    wxBoxSizer *boxSizercPanel = new wxBoxSizer(wxVERTICAL);
    messagePanel->SetSizer(boxSizercPanel);
    
    messagePanel->SetBackgroundColour(ANDROID_DIALOG_BODY_COLOR);

    
    m_style = style;
    wxFont *qFont = GetOCPNScaledFont_PlugIn(_("Dialog"));
    SetFont( *qFont );
    
    
    wxBoxSizer *icon_text = new wxBoxSizer( wxHORIZONTAL );
    boxSizercPanel->Add( icon_text, 1, wxCENTER | wxLEFT|wxRIGHT|wxTOP, 10 );
    
    #if wxUSE_STATBMP
    // 1) icon
    if (style & wxICON_MASK)
    {
        wxBitmap bitmap;
        switch ( style & wxICON_MASK )
        {
            default:
                wxFAIL_MSG(_T("incorrect log style"));
                // fall through
                
            case wxICON_ERROR:
                bitmap = wxArtProvider::GetIcon(wxART_ERROR, wxART_MESSAGE_BOX);
                break;
                
            case wxICON_INFORMATION:
                bitmap = wxArtProvider::GetIcon(wxART_INFORMATION, wxART_MESSAGE_BOX);
                break;
                
            case wxICON_WARNING:
                bitmap = wxArtProvider::GetIcon(wxART_WARNING, wxART_MESSAGE_BOX);
                break;
                
            case wxICON_QUESTION:
                bitmap = wxArtProvider::GetIcon(wxART_QUESTION, wxART_MESSAGE_BOX);
                break;
        }
        wxStaticBitmap *icon = new wxStaticBitmap(this, wxID_ANY, bitmap);
        icon_text->Add( icon, 0, wxCENTER );
    }
    #endif // wxUSE_STATBMP
    
    
    wxStaticText *textMessage = new wxStaticText( messagePanel, wxID_ANY, message );
    textMessage->Wrap(-1);
    icon_text->Add( textMessage, 0, wxALIGN_CENTER | wxLEFT, 10 );
    
   
    // 3) buttons
    int AllButtonSizerFlags = wxOK|wxCANCEL|wxYES|wxNO|wxHELP|wxNO_DEFAULT;
    int center_flag = wxEXPAND;
    if (style & wxYES_NO)
        center_flag = wxALIGN_CENTRE;
    wxSizer *sizerBtn = CreateSeparatedButtonSizer(style & AllButtonSizerFlags);
    if ( sizerBtn )
        topsizer->Add(sizerBtn, 0, center_flag | wxALL, 10 );
    
    SetAutoLayout( true );
    
    topsizer->SetSizeHints( this );
    topsizer->Fit( this );
    Centre( wxBOTH | wxCENTER_FRAME);
}

void OFCMessageDialog::OnYes(wxCommandEvent& WXUNUSED(event))
{
    SetReturnCode(wxID_YES);
    EndModal( wxID_YES );
}

void OFCMessageDialog::OnNo(wxCommandEvent& WXUNUSED(event))
{
    SetReturnCode(wxID_NO);
    EndModal( wxID_NO );
}

void OFCMessageDialog::OnCancel(wxCommandEvent& WXUNUSED(event))
{
    // Allow cancellation via ESC/Close button except if
    // only YES and NO are specified.
    if ( (m_style & wxYES_NO) != wxYES_NO || (m_style & wxCANCEL) )
    {
        SetReturnCode(wxID_CANCEL);
        EndModal( wxID_CANCEL );
    }
}

void OFCMessageDialog::OnClose( wxCloseEvent& event )
{
    SetReturnCode(wxID_CANCEL);
    EndModal( wxID_CANCEL );
}


int ShowOFCMessageDialog(wxWindow *parent, const wxString& message,  const wxString& caption, long style = wxOK)
{
#ifdef __OCPN__ANDROID__
    OFCMessageDialog dlg( parent, message, caption, style);
    return dlg.ShowModal();
#else
    return OCPNMessageBox_PlugIn(parent, message, caption, style);
#endif    
}





#ifdef __OCPN_USE_CURL__

size_t wxcurl_string_write_UTF8(char* ptr, size_t size, size_t nmemb, void* pcharbuf)
{
    size_t iRealSize = size * nmemb;
    wxCharBuffer* pStr = (wxCharBuffer*) pcharbuf;
    
    
    if(pStr)
    {
#ifdef __WXMSW__        
        wxString str1a = wxString(*pStr);
        wxString str2 = wxString((const char*)ptr, wxConvUTF8, iRealSize);
        *pStr = (str1a + str2).mb_str();
#else        
        wxString str = wxString(*pStr, wxConvUTF8) + wxString((const char*)ptr, wxConvUTF8, iRealSize);
        *pStr = str.mb_str(wxConvUTF8);
        
/* arm testing       
        wxString str1a = wxString(*pStr, wxConvUTF8);
        wxString str2 = wxString((const char*)ptr, wxConvUTF8, iRealSize);
        *pStr = (str1a + str2).mb_str(wxConvUTF8);
        
//        char *v = pStr->data();
//        printf("concat(new): %s\n\n\n", (char *)v);
        
//        printf("LOGGING...\n\n\n\n");
//        wxLogMessage(_T("str1a: ") + str1a);
//        wxLogMessage(_T("str2: ") + str2);
*/        
#endif        
    }
    
    
    return iRealSize;
}

class wxCurlHTTPNoZIP : public wxCurlHTTP
{
public:
    wxCurlHTTPNoZIP(const wxString& szURL = wxEmptyString,
               const wxString& szUserName = wxEmptyString,
               const wxString& szPassword = wxEmptyString,
               wxEvtHandler* pEvtHandler = NULL, int id = wxID_ANY,
               long flags = wxCURL_DEFAULT_FLAGS);
    
   ~wxCurlHTTPNoZIP();
    
   bool Post(wxInputStream& buffer, const wxString& szRemoteFile /*= wxEmptyString*/);
   bool Post(const char* buffer, size_t size, const wxString& szRemoteFile /*= wxEmptyString*/);
   std::string GetResponseBody() const;
   
   bool Get(const wxString& szFilePath, const wxString& szRemoteFile /*= wxEmptyString*/);
   bool Get(wxOutputStream& buffer, const wxString& szRemoteFile /*=wxEmptyString*/);
   
protected:
    void SetCurlHandleToDefaults(const wxString& relativeURL);
    
};

wxCurlHTTPNoZIP::wxCurlHTTPNoZIP(const wxString& szURL /*= wxEmptyString*/, 
                       const wxString& szUserName /*= wxEmptyString*/, 
                       const wxString& szPassword /*= wxEmptyString*/, 
                       wxEvtHandler* pEvtHandler /*= NULL*/, 
                       int id /*= wxID_ANY*/,
                       long flags /*= wxCURL_DEFAULT_FLAGS*/)
: wxCurlHTTP(szURL, szUserName, szPassword, pEvtHandler, id, flags)

{
}

wxCurlHTTPNoZIP::~wxCurlHTTPNoZIP()
{
    ResetPostData();
}

void wxCurlHTTPNoZIP::SetCurlHandleToDefaults(const wxString& relativeURL)
{
    wxCurlBase::SetCurlHandleToDefaults(relativeURL);
    
    SetOpt(CURLOPT_ENCODING, "identity");               // No encoding, plain ASCII
    
    if(m_bUseCookies)
    {
        SetStringOpt(CURLOPT_COOKIEJAR, m_szCookieFile);
    }
}

bool wxCurlHTTPNoZIP::Post(const char* buffer, size_t size, const wxString& szRemoteFile /*= wxEmptyString*/)
{
    wxMemoryInputStream inStream(buffer, size);
    
    return Post(inStream, szRemoteFile);
}

bool wxCurlHTTPNoZIP::Post(wxInputStream& buffer, const wxString& szRemoteFile /*= wxEmptyString*/)
{
    curl_off_t iSize = 0;
    
    if(m_pCURL && buffer.IsOk())
    {
        SetCurlHandleToDefaults(szRemoteFile);
        
        curl_easy_setopt(m_pCURL, CURLOPT_SSL_VERIFYPEER, FALSE);
        
        SetHeaders();
        iSize = buffer.GetSize();
        
        if(iSize == (~(ssize_t)0))      // wxCurlHTTP does not know how to upload unknown length streams.
            return false;
        
        SetOpt(CURLOPT_POST, TRUE);
        SetOpt(CURLOPT_POSTFIELDSIZE_LARGE, iSize);
        SetStreamReadFunction(buffer);
        
        //  Use a private data write trap function to handle UTF8 content
        //SetStringWriteFunction(m_szResponseBody);
        SetOpt(CURLOPT_WRITEFUNCTION, wxcurl_string_write_UTF8);         // private function
        SetOpt(CURLOPT_WRITEDATA, (void*)&m_szResponseBody);
        
        if(Perform())
        {
            ResetHeaders();
            return IsResponseOk();
        }
    }
    
    return false;
}

std::string wxCurlHTTPNoZIP::GetResponseBody() const
{
#ifndef ARMHF
     wxString s = wxString((const char *)m_szResponseBody, wxConvLibc);
     return std::string(s.mb_str());

#else    
    return std::string((const char *)m_szResponseBody);
#endif
    
}

bool wxCurlHTTPNoZIP::Get(const wxString& szFilePath, const wxString& szRemoteFile /*= wxEmptyString*/)
{
    wxFFileOutputStream outStream(szFilePath);
    
    return Get(outStream, szRemoteFile);
}

bool wxCurlHTTPNoZIP::Get(wxOutputStream& buffer, const wxString& szRemoteFile /*=wxEmptyString*/)
{
    if(m_pCURL && buffer.IsOk())
    {
        SetCurlHandleToDefaults(szRemoteFile);
        
        curl_easy_setopt(m_pCURL, CURLOPT_SSL_VERIFYPEER, FALSE);
        
        SetHeaders();
        SetOpt(CURLOPT_HTTPGET, TRUE);
        SetStreamWriteFunction(buffer);
        
        if(Perform())
        {
            ResetHeaders();
            return IsResponseOk();
        }
    }
    
    return false;
}

#endif

// itemChart
//------------------------------------------------------------------------------------------

itemChart::itemChart()
{
    m_downloading = false;
    m_bEnabled = true;
    bActivated = false;
    device_ok = false;
    pendingUpdateFlag = false;
    m_thumbRetry = 0;
}

itemChart::itemChart( wxString &product_sku, int index) {
    productSKU = product_sku;
    indexSKU = index; 
    m_status = STAT_UNKNOWN;
    pendingUpdateFlag = false;
    m_thumbRetry = 0;
}

itemChart::itemChart( wxString &product_sku) {
    productSKU = product_sku;
    m_status = STAT_UNKNOWN;
    m_downloading = false;
    m_bEnabled = true;
    bActivated = false;
    device_ok = false;
    pendingUpdateFlag = false;
    m_thumbRetry = 0;
    indexSKU = 0;
}


bool itemChart::isMatch(itemChart *thatItemChart)
{
    return ( (productSKU == thatItemChart->productSKU) && (indexSKU == thatItemChart->indexSKU) );
}


// void itemChart::setDownloadPath(int slot, wxString path) {
//     if (slot == 0)
//         fileDownloadPath0 = path;
//     else if (slot == 1)
//         fileDownloadPath1 = path;
// }

// wxString itemChart::getDownloadPath(int slot) {
//     if (slot == 0)
//         return fileDownloadPath0;
//     else if (slot == 1)
//         return fileDownloadPath1;
//     else
//         return _T("");
// }

bool itemChart::isChartsetAssignedToMe(wxString systemName){
    return device_ok;
}


bool itemChart::isChartsetFullyAssigned() {
    return bActivated;
}

bool itemChart::isChartsetExpired() {
    
    bool bExp = false;
//     if (statusID0.IsSameAs("expired") || statusID1.IsSameAs("expired")) {
//         bExp = true;
//     }
    return bExp;
}

bool itemChart::isChartsetDontShow()
{
    if(isChartsetFullyAssigned() && !isChartsetAssignedToMe(wxEmptyString))
        return true;
    
    else if(isChartsetExpired() && !isChartsetAssignedToMe(wxEmptyString))
        return true;
    
    else
        return false;
}
    
    
//  Current status can be one of:
/*
 *      1.  Available for Installation.
 *      2.  Installed, Up-to-date.
 *      3.  Installed, Update available.
 *      4.  Expired.
 */        

int itemChart::getChartStatus()
{
    if(!g_chartListUpdatedOK){
        m_status = STAT_NEED_REFRESH;
        return m_status;
    }

    if(isChartsetExpired()){
        m_status = STAT_EXPIRED;
        return m_status;
    }
    
//     if(!isChartsetAssignedToMe( g_systemName )){
//         m_status = STAT_PURCHASED;
//         return m_status;
//     }

    if(!bActivated){
        m_status = STAT_REQUESTABLE;
        return m_status;
    }
    else if(pendingUpdateFlag){
        m_status = STAT_STALE;
    }
    else if(installLocation.Length()){
        m_status = STAT_CURRENT;
    }
    else{
        m_status = STAT_READY_DOWNLOAD;
        return m_status;
    }

#if 0    
    
    // We know that chart is assigned to me, so one of the sysIDx fields will match
    wxString cStat = statusID0;
    if(sysID1.IsSameAs(g_systemName))
        cStat = statusID1;
        
    if(cStat.IsSameAs(_T("requestable"))){
        m_status = STAT_REQUESTABLE;
        return m_status;
    }

    if(cStat.IsSameAs(_T("processing"))){
        m_status = STAT_PREPARING;
        return m_status;
    }

    if(cStat.IsSameAs(_T("download"))){
        m_status = STAT_READY_DOWNLOAD;
        
        if(sysID0.IsSameAs(g_systemName)){
            if(  (installLocation0.Length() > 0) && (installedFileDownloadPath0.Length() > 0) ){
                m_status = STAT_CURRENT;
                if(!installedEdition0.IsSameAs(currentChartEdition)){
                    m_status = STAT_STALE;
                }
            }
        }
        else if(sysID1.IsSameAs(g_systemName)){
            if(  (installLocation1.Length() > 0) && (installedFileDownloadPath1.Length() > 0) ){
                m_status = STAT_CURRENT;
                if(!installedEdition1.IsSameAs(currentChartEdition)){
                    m_status = STAT_STALE;
                }
                
            }
        }
    }
#endif
     
    return m_status;
    
}
wxString itemChart::getStatusString()
{
    getChartStatus();
    
    wxString sret;

#ifndef __OCPN__ANDROID__    
    switch(m_status){
        
        case STAT_UNKNOWN:
            break;
            
        case STAT_PURCHASED:
            sret = _("Available.");
            break;
            
        case STAT_CURRENT:
            sret = _("Installed, Up-to-date.");
            break;
            
        case STAT_STALE:
            sret = _("Installed, Update available.");
            break;
            
        case STAT_EXPIRED:
        case STAT_EXPIRED_MINE:
            sret = _("Expired.");
            break;
            
        case STAT_READY_DOWNLOAD:
            sret = _("Ready for download/install.");
            break;
            
        case STAT_NEED_REFRESH:
            sret = _("Please update Chart List.");
            break;

        case STAT_REQUESTABLE:
            sret = _("Ready for Activation Request.");
            break;
            
        default:
            break;
    }
#else
    switch(m_status){
        
        case STAT_UNKNOWN:
            break;
            
        case STAT_PURCHASED:
            sret = _("Available.");
            break;
            
        case STAT_CURRENT:
            sret = _("Installed");
            break;
            
        case STAT_STALE:
            sret = _("Update available.");
            break;
            
        case STAT_EXPIRED:
        case STAT_EXPIRED_MINE:
            sret = _("Expired.");
            break;
            
        case STAT_READY_DOWNLOAD:
            sret = _("Ready to install.");
            break;
            
        case STAT_NEED_REFRESH:
            sret = _("Please update Chart List.");
            break;

        case STAT_REQUESTABLE:
            sret = _("Ready for Activation");
            break;
            
        default:
            break;
    }
#endif
    
    return sret;
    

}

bool itemChart::downloadThumbnail()
{
    
    wxString fileKey = _T("ChartImage-");
    fileKey += productSKU;
    fileKey += _T(".png");
    
    int iResponseCode = 99;
    
    wxString localFile = g_PrivateDataDir + fileKey;

    wxLogMessage(_T("download thumbnail: ") + localFile);
    
    if(::wxFileExists(localFile)){
        return true;
    }
    
    thumbnailURL = _T("http://o-charts.org/ofc/");
    thumbnailURL += productSKU + _T(".png");
            
    if(g_chartListUpdatedOK && thumbnailURL.Length() && (m_thumbRetry < 2)){  // Do not access network until after first "getList"


                wxLogMessage(_T("download thumbnail: ") + thumbnailURL);
                
#ifndef __OCPN__ANDROID__
                wxCurlHTTPNoZIP get;
                
                ::wxBeginBusyCursor();
                bool getResult = get.Get(localFile, thumbnailURL);
                ::wxEndBusyCursor();
                
                // get the response code of the server
                get.GetInfo(CURLINFO_RESPONSE_CODE, &iResponseCode);
                
#else
                wxString file_URI = _T("file://") + localFile;
                    
                _OCPN_DLStatus ret = OCPN_downloadFile( thumbnailURL, file_URI, _T(""), _T(""), wxNullBitmap, g_shopPanel, OCPN_DLDS_DEFAULT_STYLE, 5);

//OCPN_DLDS_ELAPSED_TIME|OCPN_DLDS_ESTIMATED_TIME|OCPN_DLDS_REMAINING_TIME|OCPN_DLDS_SPEED|OCPN_DLDS_SIZE|OCPN_DLDS_URL|OCPN_DLDS_CAN_PAUSE|OCPN_DLDS_CAN_ABORT|OCPN_DLDS_AUTO_CLOSE,

                wxLogMessage(_T("DLRET"));
                
                if(OCPN_DL_NO_ERROR == ret)
                    iResponseCode = 200;
                else{
                    iResponseCode = ret;
                    m_thumbRetry++;
                }
#endif
    }
            
            wxString msg;
            msg.Printf(_T("thumbnail file response: %d"), iResponseCode);
            wxLogMessage(msg);

            return(iResponseCode == 200);
}
            
            



wxBitmap& itemChart::GetChartThumbnail(int size)
{
    wxString fileKey = _T("ChartImage-");
    fileKey += productSKU;
    fileKey += _T(".png");
    
    wxString localFile = g_PrivateDataDir + fileKey;
 
        
    if(!m_ChartImage.IsOk()){
        // Look for cached copy
 
        if(::wxFileExists(localFile)){
            m_ChartImage = wxImage( localFile, wxBITMAP_TYPE_ANY);
        }
    }
    
    if(m_ChartImage.IsOk()){
        int scaledHeight = size;
        int scaledWidth = m_ChartImage.GetWidth() * scaledHeight / m_ChartImage.GetHeight();
        wxImage scaledImage = m_ChartImage.Rescale(scaledWidth, scaledHeight);
        m_bm = wxBitmap(scaledImage);
        
        return m_bm;
    }
    else{
        if(::wxFileExists(localFile))
            wxRemoveFile(localFile);                   // must be bad file download
        
        wxImage img(size, size);
        unsigned char *data = img.GetData();
        for(int i=0 ; i < size * size * 3 ; i++)
            data[i] = 200;
        
        m_bm = wxBitmap(img);   // Grey bitmap
        return m_bm;
    }
    
}




//Utility Functions

//  Search g_ChartArray for chart having specified parameters
int findOrderRefChartId(wxString &SKU, int index)
{
    for(unsigned int i = 0 ; i < g_ChartArray.GetCount() ; i++){
        if(g_ChartArray.Item(i)->productSKU.IsSameAs(SKU)
            && (g_ChartArray.Item(i)->indexSKU == index) ){
                return (i);
            }
    }
    return -1;
}

int findChart(wxString &SKU, bool device_ok, bool activated)
{
    for(unsigned int i = 0 ; i < g_ChartArray.GetCount() ; i++){
        itemChart *pchart = g_ChartArray.Item(i);
        if(pchart->productSKU.IsSameAs(SKU) && (pchart->bActivated == activated) && (pchart->device_ok == device_ok)){
            return (i);
            }
    }
    return -1;
    
}


bool scrubChartlist()
{
    for(unsigned int i = 0 ; i < g_ChartArray.GetCount() ; i++)
        g_ChartArray.Item(i)->bRemove = false;
        
    for(unsigned int i = 0 ; i < g_ChartArray.GetCount() ; i++){
        itemChart *pchartA = g_ChartArray.Item(i);
        
        for(unsigned int j = 0 ; j < g_ChartArray.GetCount() ; j++){
            itemChart *pchartB = g_ChartArray.Item(j);
            
            if(i == j)
                continue;
            
            if(pchartA->bRemove || pchartB->bRemove)
                continue;

            // Looking for duplicates
            if(pchartA->productSKU.IsSameAs(pchartB->productSKU) || pchartA->productName.IsSameAs(pchartB->productName)){
                
                // Which one to keep?
                
                // Both un-activated, choose either
                if(!pchartA->bActivated && !pchartB->bActivated){
                    pchartB->bRemove = true;
                    continue;
                }
 
                // Both activated for me, keep one if it has an install directory set
                if(pchartA->bActivated && pchartA->device_ok && pchartB->bActivated && pchartB->device_ok){
                    if(pchartA->installLocation.Len() && !pchartB->installLocation.Len() ){
                        pchartB->bRemove = true;
                        continue;
                    }
                    else if(pchartB->installLocation.Len() && !pchartA->installLocation.Len()){
                        pchartA->bRemove = true;
                        continue;
                    }
                }
                    
                // One activated for me, keep it for sure
                if(pchartA->bActivated && pchartA->device_ok){
                    pchartB->bRemove = true;
                    continue;
                }
                if(pchartB->bActivated && pchartB->device_ok){
                    pchartA->bRemove = true;
                    continue;
                }
                
                //  One activated, and the other not, decide
                // If activated one is for me, keep it.  Otherwise remove it and keep the other.    
                 if(pchartA->bActivated && !pchartB->bActivated){
                     if(pchartA->device_ok){
                         pchartB->bRemove = true;
                         continue;
                     }
                     else{
                         pchartA->bRemove = true;
                         continue;
                     }
                 }
                        
                 if(pchartB->bActivated && !pchartA->bActivated){
                     if(pchartB->device_ok){
                         pchartA->bRemove = true;
                         continue;
                     }
                     else{
                         pchartB->bRemove = true;
                         continue;
                     }
                 }
                    
            }
        }
    }
    
    bool retval = false;
    //    List is marked, so remove the offenders...
    for(unsigned int i = 0 ; i < g_ChartArray.GetCount() ; i++){
        if(g_ChartArray.Item(i)->bRemove){
            g_ChartArray.RemoveAt(i);
            retval = true;
        }
    }
        
    return retval;    
}


void loadShopConfig()
{
    //    Get a pointer to the opencpn configuration object
    wxFileConfig *pConf = GetOCPNConfigObject();
    
    if( pConf ) {
        pConf->SetPath( _T("/PlugIns/ofc_pi") );
        
        pConf->Read( _T("loginUser"), &g_loginUser);
        pConf->Read( _T("loginPass"), &g_loginPass);
        pConf->Read( _T("lastInstallDir"), &g_lastInstallDir);
        
        pConf->Read( _T("ADMIN"), &g_admin);
                
        pConf->SetPath ( _T ( "/PlugIns/ofc_pi/charts" ) );
        wxString strk;
        wxString kval;
        long dummyval;
        bool bContk = pConf->GetFirstEntry( strk, dummyval );
        while( bContk ) {
            pConf->Read( strk, &kval );
            
            // Parse the key
            wxString SKU = strk;
            
            // Remove the last two characters (the SKU index)
//            wxString SKU = strk.Mid(0, strk.Length() - 2);
//            wxString sindex = strk.Right(1);
//            long lidx = 0;
//            sindex.ToLong(&lidx);
            
            itemChart *pItem = new itemChart(SKU);
            g_ChartArray.Add(pItem);

            // Parse the value
            wxStringTokenizer tkz( kval, _T(";") );
            wxString name = tkz.GetNextToken();
            wxString installDir = tkz.GetNextToken();
            
            pItem->productName = name;
            pItem->chartInstallLocnFull = installDir;
            pItem->bActivated = true;
            pItem->device_ok = true;
            
            //  Extract the parent of the full location
            wxFileName fn(installDir);
            pItem->installLocation = fn.GetPath();
           
            bContk = pConf->GetNextEntry( strk, dummyval );
        }
    }
    
    scrubChartlist();
}

void saveShopConfig()
{
    wxFileConfig *pConf = GetOCPNConfigObject();
        
   if( pConf ) {
      pConf->SetPath( _T("/PlugIns/ofc_pi") );
            
      if(g_admin){
          pConf->Write( _T("loginUser"), g_loginUser);
          pConf->Write( _T("loginPass"), g_loginPass);
      }
      
      pConf->Write( _T("lastInstallDir"), g_lastInstallDir);
      
      pConf->DeleteGroup( _T("/PlugIns/ofc_pi/charts") );
      pConf->SetPath( _T("/PlugIns/ofc_pi/charts") );
      
      for(unsigned int i = 0 ; i < g_ChartArray.GetCount() ; i++){
          itemChart *chart = g_ChartArray.Item(i);
          if(chart->bActivated && chart->device_ok){            // Mine...
//            wxString idx;
//            idx.Printf(_T("%d"), chart->indexSKU);
            wxString key = chart->productSKU; // + _T("-") + idx;
            
            wxString val = chart->productName + _T(";");
            val += chart->chartInstallLocnFull + _T(";");
            pConf->Write( key, val );
          }
      }
   }
}

            
int checkResult(wxString &result, bool bShowErrorDialog = true)
{
    if(g_shopPanel){
        g_shopPanel->getInProcessGuage()->Stop();
    }
    
    long dresult;
    if(result.ToLong(&dresult)){
        if(dresult != 200){
            if(bShowErrorDialog){
                wxString msg = _("Fugawi server error: ");
                wxString msg1;
                msg1.Printf(_T("{%ld}\n\n"), dresult);
                msg += msg1;
                switch(dresult){
                    case 53:
                    case 54:
                    case 55:
                    case 56:
                    case 57:
                        msg += _("internet communications error.");
                        msg += _T("\n");
                        msg += _("Check your internet configuration.");
                        break;
                    case 400:
                        msg += _("Invalid parameters.");
                        break;
                    case 403:    
                        msg += _("Invalid email name or password.");
                        break;
                    default:    
                        msg += _("Check your configuration and try again.");
                        break;
                }
                
                ShowOFCMessageDialog(NULL, msg, _("ofc_pi Message"), wxOK);
            }
            return dresult;
        }
        else
            return 0;
    }
    else
        return 0;               // OK, default 200
}

int checkResponseCode(int iResponseCode)
{
    if(iResponseCode != 200){
        wxString msg = _("internet communications error code: ");
        wxString msg1;
        msg1.Printf(_T("{%d}\n "), iResponseCode);
        msg += msg1;
        msg += _("Check your connection and try again.");
        ShowOFCMessageDialog(NULL, msg, _("ofc_pi Message"), wxOK);
    }
    
    // The wxCURL library returns "0" as response code,
    // even when it should probably return 404.
    // We will use "99" as a special code here.
    
    if(iResponseCode < 100)
        return 99;
    else
        return iResponseCode;
        
}

bool doLogin()
{
    xtr1Login login(NULL);
    login.m_UserNameCtl->SetValue(g_loginUser);
    login.m_PasswordCtl->SetValue(g_loginPass);

    login.ShowModal();
    if(!login.GetReturnCode() == 0){
        g_shopPanel->setStatusText( _("Invalid Login."));
        wxYield();
        return false;
    }
    
    g_loginUser = login.m_UserNameCtl->GetValue().Trim( true).Trim( false );
    g_loginPass = login.m_PasswordCtl->GetValue().Trim( true).Trim( false );
    
    return true;
}



void processBody(itemChart *chart){
    // Decode the productBody from MIME64 block in GetAccount.XML response
    if(!chart->productBody.Len())
        return;
    
    size_t buf_size = chart->productBody.Len() * 2;
    unsigned char *decodedBody = (unsigned char *)malloc( buf_size);
    
    chart->productBody.Replace(_T(" "), _T(""));
    wxCharBuffer Str = chart->productBody.ToUTF8();
    
    size_t olen;
    int result = mbedtls_base64_decode( decodedBody, buf_size, &olen,  (const unsigned char *)Str.data(), chart->productBody.Len() );
    
    //printf("%s\n", decodedBody);
    //wxLogMessage(_T("decodedBody:"));               
    //wxString db = wxString( decodedBody, wxConvUTF8 );
    //wxLogMessage(db);
    
    // Parse the xml
    
    TiXmlDocument * doc = new TiXmlDocument();
    doc->Parse( (const char *)decodedBody );
    free(decodedBody);
    
    TiXmlElement * root = doc->RootElement();
    if(!root)
        return;
   
    wxString code;
    wxString compilation_date;
    wxString name;
    wxString basedir;
    wxString indexFileURL;
    
    wxString rootName = wxString::FromUTF8( root->Value() );
    TiXmlNode *child;
    for ( child = root->FirstChild(); child != 0; child = child->NextSibling()){

        if(!strcmp(child->Value(), "maplib")){
            TiXmlElement *product = child->ToElement();
            code = wxString( product->Attribute( "code" ), wxConvUTF8 );
            compilation_date = wxString( product->Attribute( "compilation_date" ), wxConvUTF8 );
            name = wxString( product->Attribute( "name" ), wxConvUTF8 );
            basedir = wxString( product->Attribute( "basedir" ), wxConvUTF8 );
            indexFileURL = wxString( product->Attribute( "index" ), wxConvUTF8 );
        }
    }

    if(!indexFileURL.Len())
        return;
    
    if(!basedir.Len())
        return;
    

    // Sometimes the baseDir has trailing "/", sometimes not...
        //  Let us be sure that it does.    
    if(basedir.Last() != '/')    
        basedir += _T("/");
        
    chart->indexBaseDir = basedir;
    chart->shortSetName = code;
    chart->indexFileURL = indexFileURL;
}


wxString ProcessResponse(std::string body)
{
        TiXmlDocument * doc = new TiXmlDocument();
        doc->Parse( body.c_str());
    
        //doc->Print();
        
        wxString queryResult;
        wxString chartOrder;
        wxString chartPurchase;
        wxString chartExpiration;
        wxString chartID;
        wxString chartEdition;
        wxString chartPublication;
        wxString chartName;
        wxString chartQuantityID;
        wxString chartSlot;
        wxString chartAssignedSystemName;
        wxString chartLastRequested;
        wxString chartState;
        wxString chartLink;
        wxString chartSize;
        wxString chartThumbURL;
        
        wxString product_name;
        wxString product_sku;
        wxString product_type;
        wxString expiry;
        wxString purchase_url;
        wxString activated;
        wxString app_id_ok;
        wxString device_id_ok;
        wxString product_key;
        wxString product_body;
        
        ProdSKUIndexHash psi;           // Product SKU keys, instance counter value
        
            TiXmlElement * root = doc->RootElement();
            if(!root){
                return _T("50");                              // undetermined error??
            }
            
            wxString rootName = wxString::FromUTF8( root->Value() );
            TiXmlNode *child;
            for ( child = root->FirstChild(); child != 0; child = child->NextSibling()){
                
                 wxString s = wxString::FromUTF8(child->Value());
                
                if(!strcmp(child->Value(), "status")){
                    TiXmlNode *childResult = child->FirstChild();
                    //queryResult =  wxString::FromUTF8(childResult->Value());
                    TiXmlElement *stat = child->ToElement();
                    if(stat)
                        queryResult = wxString( stat->Attribute( "status-code" ), wxConvUTF8 );
                    
                }
                

                else if(!strcmp(child->Value(), "account")){
                    TiXmlNode *childacct = child->FirstChild();
                    for ( childacct = child->FirstChild(); childacct!= 0; childacct = childacct->NextSibling()){
                        
                        if(!strcmp(childacct->Value(), "product")){
                            TiXmlElement *product = childacct->ToElement();
 
                            product_body.Clear();
                            
                            TiXmlNode *productBody64 = childacct->FirstChild();
                            if(productBody64)
                                product_body = productBody64->Value();

#ifdef __WXMSW__
                            product_name = wxString( product->Attribute( "product_name" ) ); // already converted....
#else
                            product_name = wxString( product->Attribute( "product_name" ), wxConvUTF8 );
#endif                            
                            
                            product_sku = wxString( product->Attribute( "product_sku" ), wxConvUTF8 );
                            product_type = wxString( product->Attribute( "product_type" ), wxConvUTF8 );
                            expiry = wxString( product->Attribute( "expiry" ), wxConvUTF8 );
                            purchase_url = wxString( product->Attribute( "purchase_url" ), wxConvUTF8 );
                            activated = wxString( product->Attribute( "activated" ), wxConvUTF8 );
                            app_id_ok = wxString( product->Attribute( "app_id_ok" ), wxConvUTF8 );
                            device_id_ok = wxString( product->Attribute( "device_id_ok" ), wxConvUTF8 );
                            product_key = wxString( product->Attribute( "product_key" ), wxConvUTF8 );

                            itemChart *pItem = NULL;
                            
#if 1
                            bool device_ok = device_id_ok.IsSameAs(_T("TRUE"), false);
                            bool bActivated = activated.IsSameAs(_T("TRUE"), false);
                            
                            bool bSkip = false;
                            bool bUpdate = false;
                            
                            // Is the exact same chartset SKU, with same conditions, already in the array?
                            // If so, just update the info
                            int dupIndex = findChart(product_sku, device_ok, bActivated);
                            if(dupIndex >= 0){                                   // found
                                 bUpdate = true;
                            }
                            
                            //  If this set is not activated, and I already have an activated copy for me, skip it 
                            if(!bActivated){
                                int ck1Index = findChart(product_sku, true, true);
                                if(ck1Index >= 0){
                                    bSkip = true;
                                    bUpdate = true;
                                }
                            }

                            // If this set is activated, but not for me, then skip it
                            if(bActivated && !device_ok){
                                
                                bSkip = true;
                            }
                            
                            if(!bSkip){
                                
                                if(bUpdate){
                                    pItem = g_ChartArray.Item(dupIndex);
                                }
                                else{
                                    pItem = new itemChart(product_sku);
                                    g_ChartArray.Add(pItem);
                                }
                            }
                                
                                
                            
                                
                            
#else                            
                            int index = 0;
                            //  Has this product sku been seen yet?
                            if(psi.find( product_sku ) == psi.end()){           // first one
                                psi[product_sku] = 1;
                                index = 1;
                            }
                            else{
                                index = psi[product_sku];
                                index++;
                                psi[product_sku] = index;
                            }
                        
                            // Process this chart node
//                             itemChart *pItem;
//                             pItem = new itemChart(product_sku, index);
//                             g_ChartArray.Add(pItem);
                            
                            // As identified uniquely by Sku and index....
                            // Does this chart already exist in the table?
                             int indexChart = findOrderRefChartId(product_sku, index);
                             if(indexChart < 0){
                                 pItem = new itemChart(product_sku, index);
                                 g_ChartArray.Add(pItem);
                             }
                             else
                                 pItem = g_ChartArray.Item(indexChart);
#endif
                                 
                            // Populate in the rest of "item"
                            if(pItem){     
                                pItem->productName = product_name;
                                pItem->expDate = expiry;
                                pItem->fileDownloadURL = purchase_url;
                                pItem->productType = product_type;
                                pItem->productKey = product_key;
                                pItem->productBody = product_body;
                                pItem->device_ok = device_id_ok.IsSameAs(_T("TRUE"), false);
                                pItem->bActivated = activated.IsSameAs(_T("TRUE"), false);
                            
                                processBody(pItem);
                            }
                        }
                    }
                    
                
                }
            }
        
        scrubChartlist();
        
        return queryResult;
}

    

int getChartList( bool bShowErrorDialogs = true){
    
     validate_server();
     
     oernc_inStream GK;
     wxString kk = GK.getHK();
     
     if(!kk.Len())
         return 2;

     
    // We query the server for the list of charts associated with our account
    wxString url = _T("https://fugawi.com/GetAccount_v2.xml");
    
    wxString loginParms;
    loginParms = _T("email=");
    loginParms += g_loginUser;
    
    loginParms += _T("&password=");
    loginParms += g_loginPass;
    
    loginParms += _T("&api_key=cb437274b425c92e00ed2ea802959e11d0e2048a");
    loginParms += _T("&app_id=30000");
    loginParms += _T("&device_id=");
    loginParms += kk;

    wxString parms = loginParms;
    int idd = parms.Find(_T("password"));
    if(idd != wxNOT_FOUND){
        idd += 9;
        for(unsigned int i=0 ; i < g_loginPass.Length() ; i++)
            parms[idd + i] = 'X';
    }
        
    wxLogMessage(_T("getChartList Login Parms: ") + parms);
    
#ifndef __OCPN__ANDROID__    
    wxCurlHTTPNoZIP post;
    //wxCurlHTTP post;
    post.SetOpt(CURLOPT_TIMEOUT, g_timeout_secs);
    
    /*size_t res = */post.Post( loginParms.ToAscii(), loginParms.Len(), url );
    
    // get the response code of the server
    int iResponseCode;
    post.GetInfo(CURLINFO_RESPONSE_CODE, &iResponseCode);
    
    std::string a = post.GetDetailedErrorString();
    std::string b = post.GetErrorString();
    std::string c = post.GetResponseBody();
    const char *d = c.c_str();
    
    //printf("Code %d\n", iResponseCode);
    
    //printf("%s\n", a.c_str());
    //printf("%s\n", b.c_str());
    //printf("%s\n", c.c_str());
    //printf("%s\n", d);
    
    //printf("%s", post.GetResponseBody().c_str());
    
     wxString tt(post.GetResponseBody().data(), wxConvUTF8);
     wxLogMessage( _T("Response: \n") + tt);
    
     if(iResponseCode == 200){
         wxString result = ProcessResponse(post.GetResponseBody());
         
         return checkResult( result, bShowErrorDialogs );
     }
     else{
         return iResponseCode; //checkResponseCode(iResponseCode);
     }
#else

    wxString postresult;
    _OCPN_DLStatus stat = OCPN_postDataHttp( url, loginParms, postresult, 5 );

    qDebug() << "Post Stat: " << stat;
    
    if(stat != OCPN_DL_FAILED){
        wxCharBuffer buf = postresult.ToUTF8();
        std::string response(buf.data());
        
        qDebug() << response.c_str();
        wxString result = ProcessResponse(response);
        return checkResult( result, bShowErrorDialogs );
    }
    else{
        wxString result;
        switch(stat){
            case OCPN_DL_FAILED:
                result = _T("53");
                break;
            case OCPN_DL_ABORTED:
                result = _T("54");
                break;
            case OCPN_DL_UNKNOWN:
                result = _T("55");
                break;
            case OCPN_DL_USER_TIMEOUT:
                result = _T("56");
                break;
            default:
                result - _T("57");
                break;
        }
                
        return checkResult( result, bShowErrorDialogs ); // Generic comm error
    }
        
        
#endif    
}


int doActivate(itemChart *chart, bool bShowErrorDialogs = true)
{
    validate_server();
    
    oernc_inStream GK;
    wxString kk = GK.getHK();
    
    if(!kk.Len())
        return 1;
    
    wxString msg = _("This action will PERMANENTLY assign the chartset:");
    msg += _T("\n        ");
    msg += chart->productSKU; //productName;
    msg += _T("\n\n");
    msg += _("to this system.");
    msg += _T("  ");
    msg += _("Proceed?");
    
    int ret = ShowOFCMessageDialog(NULL, msg, _("ofc_pi Message"), wxYES_NO);
    if((ret != wxID_YES) && (ret != wxID_OK)){
         return 2;
    }

    
    // Activate this chart

    
    wxString url = _T("https://fugawi.com/ActivateProduct.xml");
    
    wxString loginParms;
    loginParms = _T("email=");
    loginParms += g_loginUser;
    
    loginParms += _T("&password=");
    loginParms += g_loginPass;
    
    loginParms += _T("&api_key=cb437274b425c92e00ed2ea802959e11d0e2048a");
    loginParms += _T("&app_id=30000");
    loginParms += _T("&device_id=");
    loginParms += kk;
 
    loginParms += _T("&product_sku=");
    loginParms += chart->productSKU;
    
#ifndef __OCPN__ANDROID__ 
    
    wxString parms = loginParms;
    int idd = parms.Find(_T("password"));
    if(idd != wxNOT_FOUND){
        idd += 9;
        for(unsigned int i=0 ; i < g_loginPass.Length() ; i++)
            parms[idd + i] = 'X';
    }
    
    wxLogMessage(_T("doActivate Login Parms: ") + parms);
    
    wxCurlHTTPNoZIP post;
    post.SetOpt(CURLOPT_TIMEOUT, g_timeout_secs);
    
    post.Post( loginParms.ToAscii(), loginParms.Len(), url );
    
    // get the response code of the server
    int iResponseCode;
    post.GetInfo(CURLINFO_RESPONSE_CODE, &iResponseCode);
    
    
    wxString tt(post.GetResponseBody().data(), wxConvUTF8);
    wxLogMessage(tt);
    
    if(iResponseCode == 200){
        wxString result = ProcessResponse(post.GetResponseBody());
        
        int iRet = checkResult( result, bShowErrorDialogs );
        return iRet;

    }
    else
        return iResponseCode; //checkResponseCode(iResponseCode);
#else


    wxString postresult;
    _OCPN_DLStatus stat = OCPN_postDataHttp( url, loginParms, postresult, 5 );

    qDebug() << "Activate Post Stat: " << stat;

    if(stat != OCPN_DL_FAILED){
        wxCharBuffer buf = postresult.ToUTF8();
        std::string response(buf.data());
        
        qDebug() << response.c_str();
        wxString result = ProcessResponse(response);
        return checkResult( result, bShowErrorDialogs );
    }
    else{
        wxString result;
        switch(stat){
            case OCPN_DL_FAILED:
                result = _T("53");
                break;
            case OCPN_DL_ABORTED:
                result = _T("54");
                break;
            case OCPN_DL_UNKNOWN:
                result = _T("55");
                break;
            case OCPN_DL_USER_TIMEOUT:
                result = _T("56");
                break;
            default:
                result - _T("57");
                break;
        }
        
        return checkResult( result, bShowErrorDialogs ); // Generic comm error
    }
    
#endif

}

extern wxString getFPR( bool bCopyToDesktop, bool &bCopyOK);

int doUploadXFPR()
{
    return 0;
}


int doPrepare(oeSencChartPanel *chartPrepare, int slot)
{
    return 0;
}

int doDownload(oeSencChartPanel *chartDownload)
{
#ifndef __OCPN__ANDROID__    
    itemChart *chart = chartDownload->m_pChart;

    //  Create a destination file name for the download.
    wxURI uri;
    
    wxString downloadURL = chart->fileDownloadURL;

    uri.Create(downloadURL);
    
    wxString serverFilename = uri.GetPath();
    wxString b = uri.GetServer();
    
    wxFileName fn(serverFilename);
    
    wxString downloadFile = g_PrivateDataDir + fn.GetFullName();
    chart->downloadingFile = downloadFile;
    
    downloadOutStream = new wxFFileOutputStream(downloadFile);
    
    g_curlDownloadThread = new wxCurlDownloadThread(g_CurlEventHandler);
    g_curlDownloadThread->SetURL(downloadURL);
    g_curlDownloadThread->SetOutputStream(downloadOutStream);
    g_curlDownloadThread->Download();
#endif

    return 0;
}

bool ExtractZipFiles( const wxString& aZipFile, const wxString& aTargetDir, bool aStripPath, wxDateTime aMTime, bool aRemoveZip )
{
    bool ret = true;
    
    std::auto_ptr<wxZipEntry> entry(new wxZipEntry());
    
    do
    {
        //wxLogError(_T("chartdldr_pi: Going to extract '")+aZipFile+_T("'."));
        wxFileInputStream in(aZipFile);
        
        if( !in )
        {
            wxLogError(_T("Can not open file '")+aZipFile+_T("'."));
            ret = false;
            break;
        }
        wxZipInputStream zip(in);
        ret = false;
        
        while( entry.reset(zip.GetNextEntry()), entry.get() != NULL )
        {
            // access meta-data
            wxString name = entry->GetName();
            if( aStripPath )
            {
                wxFileName fn(name);
                /* We can completly replace the entry path */
                //fn.SetPath(aTargetDir);
                //name = fn.GetFullPath();
                /* Or only remove the first dir (eg. ENC_ROOT) */
                if (fn.GetDirCount() > 0)
                    fn.RemoveDir(0);
                name = aTargetDir + wxFileName::GetPathSeparator() + fn.GetFullPath();
            }
            else
            {
                name = aTargetDir + wxFileName::GetPathSeparator() + name;
            }
            
            // read 'zip' to access the entry's data
            if( entry->IsDir() )
            {
                int perm = entry->GetMode();
                if( !wxFileName::Mkdir(name, perm, wxPATH_MKDIR_FULL) )
                {
                    wxLogError(_T("Can not create directory '") + name + _T("'."));
                    ret = false;
                    break;
                }
            }
            else
            {
                if( !zip.OpenEntry(*entry.get()) )
                {
                    wxLogError(_T("Can not open zip entry '") + entry->GetName() + _T("'."));
                    ret = false;
                    break;
                }
                if( !zip.CanRead() )
                {
                    wxLogError(_T("Can not read zip entry '") + entry->GetName() + _T("'."));
                    ret = false;
                    break;
                }
                
                wxFileName fn(name);
                if( !fn.DirExists() )
                {
                    if( !wxFileName::Mkdir(fn.GetPath()) )
                    {
                        wxLogError(_T("Can not create directory '") + fn.GetPath() + _T("'."));
                        ret = false;
                        break;
                    }
                }
                
                wxFileOutputStream file(name);
                
                g_shopPanel->setStatusText( _("Unzipping chart files...") + fn.GetFullName());
                wxYield();
                
                if( !file )
                {
                    wxLogError(_T("Can not create file '")+name+_T("'."));
                    ret = false;
                    break;
                }
                zip.Read(file);
                //fn.SetTimes(&aMTime, &aMTime, &aMTime);
                ret = true;
            }
            
        }
        
    }
    while(false);
    
    if( aRemoveZip )
        wxRemoveFile(aZipFile);
    
    return ret;
}


int doShop(){
    
    loadShopConfig();
   
    //  Do we need an initial login to get the persistent key?
    doLogin();
    saveShopConfig();
    
    getChartList();
    
    return 0;
}


class MyStaticTextCtrl : public wxStaticText {
public:
    MyStaticTextCtrl(wxWindow* parent,
                                       wxWindowID id,
                                       const wxString& label,
                                       const wxPoint& pos,
                                       const wxSize& size = wxDefaultSize,
                                       long style = 0,
                                       const wxString& name= "staticText" ):
                                       wxStaticText(parent,id,label,pos,size,style,name){};
                                       void OnEraseBackGround(wxEraseEvent& event) {};
                                       DECLARE_EVENT_TABLE()
};

BEGIN_EVENT_TABLE(MyStaticTextCtrl,wxStaticText)
EVT_ERASE_BACKGROUND(MyStaticTextCtrl::OnEraseBackGround)
END_EVENT_TABLE()


BEGIN_EVENT_TABLE(oeSencChartPanel, wxPanel)
EVT_PAINT ( oeSencChartPanel::OnPaint )
EVT_ERASE_BACKGROUND(oeSencChartPanel::OnEraseBackground)
END_EVENT_TABLE()

oeSencChartPanel::oeSencChartPanel(wxWindow *parent, wxWindowID id, const wxPoint &pos, const wxSize &size, itemChart *p_itemChart, shopPanel *pContainer)
:wxPanel(parent, id, pos, size, wxBORDER_NONE)
{
    m_pContainer = pContainer;
    m_pChart = p_itemChart;
    m_bSelected = false;

    int refHeight = GetCharHeight();
    SetMinSize(wxSize(-1, 5 * refHeight));
    m_unselectedHeight = 5 * refHeight;
    
//     wxBoxSizer* itemBoxSizer01 = new wxBoxSizer(wxHORIZONTAL);
//     SetSizer(itemBoxSizer01);
     Connect(wxEVT_LEFT_DOWN, wxMouseEventHandler(oeSencChartPanel::OnChartSelected), NULL, this);
//     
    
}

oeSencChartPanel::~oeSencChartPanel()
{
}

void oeSencChartPanel::OnChartSelected( wxMouseEvent &event )
{
    // Do not allow de-selection by mouse if this chart is busy, i.e. being prepared, or being downloaded 
    if(m_pChart){
       if(g_statusOverride.Length())
           return;
    }
           
    if(!m_bSelected){
        SetSelected( true );
        m_pContainer->SelectChart( this );
    }
    else{
        SetSelected( false );
        m_pContainer->SelectChart( NULL );
    }
}

void oeSencChartPanel::SetSelected( bool selected )
{
    m_bSelected = selected;
    wxColour colour;
    int refHeight = GetCharHeight();
    
    if (selected)
    {
        GetGlobalColor(_T("UIBCK"), &colour);
        m_boxColour = colour;
#ifdef __OCPN__ANDROID__
        SetMinSize(wxSize(-1, 5 * refHeight));
#else
        SetMinSize(wxSize(-1, 9 * refHeight));
#endif        
    }
    else
    {
        GetGlobalColor(_T("DILG0"), &colour);
        m_boxColour = colour;
        SetMinSize(wxSize(-1, 5 * refHeight));
    }
    
    Refresh( true );
    
}


extern "C"  DECL_EXP bool GetGlobalColor(wxString colorName, wxColour *pcolour);

void oeSencChartPanel::OnEraseBackground( wxEraseEvent &event )
{
}

wxArrayString splitLine(wxString &line, wxDC &dc, int widthMax)
{
    // Split into multiple lines...
    int lenCheck;
    
    wxArrayString retArray;
    wxArrayString wordArray;
    
    wxStringTokenizer tks(line, _T(" ") );
    while( tks.HasMoreTokens() ){
        wordArray.Add(tks.GetNextToken());
    }

    wxString okString;
    wxString testString;
    unsigned int iword = 0;
    bool done = false;
    while( !done && (iword < wordArray.GetCount() )){
        testString.Empty();
  
        bool tooLong = false;
        while(!tooLong && (iword < wordArray.GetCount() )){
            okString = testString;
            testString += wordArray.Item(iword) + _T(" ");
            dc.GetTextExtent(testString, &lenCheck, NULL);
            if(lenCheck > widthMax){
                tooLong = true;
                break;
            }
            iword++;
        }
        
        if(okString.IsEmpty())
            break;
        
        if(tooLong)
            retArray.Add(okString);
        else
            retArray.Add(testString);
    }
        
   
    return retArray;
}


void oeSencChartPanel::OnPaint( wxPaintEvent &event )
{
    int width, height;
    GetSize( &width, &height );
    wxPaintDC dc( this );
 
    //dc.SetBackground(*wxLIGHT_GREY);
    
    dc.SetPen(*wxTRANSPARENT_PEN);
    dc.SetBrush(wxBrush(GetBackgroundColour()));
    dc.DrawRectangle(GetVirtualSize());
    
    wxColour c;
    
    wxString nameString = m_pChart->productName;
//     wxString idxs;
//     idxs.Printf(_T("%d"),  m_pChart->indexSKU );
//     nameString += _T(" (") + idxs + _T(")");
    
    if(m_bSelected){
        dc.SetBrush( wxBrush( m_boxColour ) );
        
        GetGlobalColor( _T ( "UITX1" ), &c );
        dc.SetPen( wxPen( wxColor(0xCE, 0xD5, 0xD6), 3 ));
        
        dc.DrawRoundedRectangle( 0, 0, width-1, height-1, height / 10);
        
        int base_offset = height / 10;
        
        // Draw the thumbnail
        int scaledWidth = height;
        
        int scaledHeight = (height - (2 * base_offset)) * 95 / 100;
        wxBitmap &bm = m_pChart->GetChartThumbnail( scaledHeight );
        
        if(bm.IsOk()){
            dc.DrawBitmap(bm, base_offset + 3, base_offset + 3);
        }
        
        wxFont *dFont = GetOCPNScaledFont_PlugIn(_("Dialog"));
#ifdef __OCPN__ANDROID__
        double font_size = dFont->GetPointSize();
        wxFont *qFont = wxTheFontList->FindOrCreateFont( font_size, dFont->GetFamily(), dFont->GetStyle(),  dFont->GetWeight());
#else        
        double font_size = dFont->GetPointSize() * 4/3;
        wxFont *qFont = wxTheFontList->FindOrCreateFont( font_size, dFont->GetFamily(), dFont->GetStyle(),  wxFONTWEIGHT_BOLD);
#endif        
        dc.SetFont( *qFont );
        dc.SetTextForeground(wxColour(0,0,0));
        
        int text_x = scaledWidth * 12 / 10;
        int y_line0 = height * 5 / 100;
        
        // Split into lines...
        int lenAvail = width - text_x;

        wxArrayString array = splitLine(nameString, dc, lenAvail);

#ifdef __OCPN__ANDROID__        
        if(array.GetCount() > 3){
            wxFont *rdFont = wxTheFontList->FindOrCreateFont( font_size * 8 / 10, dFont->GetFamily(), dFont->GetStyle(),  dFont->GetWeight());
            dc.SetFont( *rdFont );
        }
#endif        
        
        int hTitle = dc.GetCharHeight();
            
        for(unsigned int i=0 ; i < array.GetCount() ; i++){
            dc.DrawText(array.Item(i), text_x, y_line0);
            y_line0 += hTitle;
        }
        
        
        
        int y_line = y_line0 + hTitle;
        dc.DrawLine( text_x, y_line, width - base_offset, y_line);
        

        
        wxFont *lFont = wxTheFontList->FindOrCreateFont( dFont->GetPointSize(), dFont->GetFamily(), dFont->GetStyle(),  dFont->GetWeight());
        dc.SetFont( *lFont );
        
        int yPitch = GetCharHeight();
        int yPos = y_line + 4;
        wxString tx;
        
        int text_x_val = scaledWidth + ((width - scaledWidth) * 2 / 10);
        
        // Create and populate the current chart information
//         tx = _("Chart Edition:");
//         dc.DrawText( tx, text_x, yPos);
//         tx = m_pChart->currentChartEdition;
//         dc.DrawText( tx, text_x_val, yPos);
        yPos += yPitch;
        
/*        tx = _("Order Reference:");
        dc.DrawText( tx, text_x, yPos);
        tx = m_pChart->orderRef;
        dc.DrawText( tx, text_x_val, yPos);
 */       yPos += yPitch;
        
//         tx = _("Purchase date:");
//         dc.DrawText( tx, text_x, yPos);
//         tx = m_pChart->purchaseDate;
//         dc.DrawText( tx, text_x_val, yPos);
        yPos += yPitch;
        
//         tx = _("Expiration date:");
//         dc.DrawText( tx, text_x, yPos);
//         tx = m_pChart->expDate;
//         dc.DrawText( tx, text_x_val, yPos);
        yPos += yPitch;

        wxFont *sFont = wxTheFontList->FindOrCreateFont( dFont->GetPointSize() * 8 / 10, dFont->GetFamily(), dFont->GetStyle(), dFont->GetWeight());
        dc.SetFont( *sFont );
        
        tx = _T(""); //_("Status:");
        if(g_statusOverride.Len())
            tx += g_statusOverride;
        else
            tx += m_pChart->getStatusString();
        dc.DrawText( tx, text_x, height - GetCharHeight() - 4);
        
        dc.SetFont( *dFont );           // Restore default font
        
    }
    else{
        dc.SetBrush( wxBrush( m_boxColour ) );
    
        GetGlobalColor( _T ( "UITX1" ), &c );
        dc.SetPen( wxPen( c, 1 ) );
    
        int offset = height / 10;
        dc.DrawRectangle( offset, offset, width - (2 * offset), height - (2 * offset));
    
        // Draw the thumbnail
        int scaledHeight = (height - (2 * offset)) * 95 / 100;
        wxBitmap &bm = m_pChart->GetChartThumbnail( scaledHeight );
        
        if(bm.IsOk()){
            dc.DrawBitmap(bm, offset + 3, offset + 3);
        }
        
        int scaledWidth = bm.GetWidth() * scaledHeight / bm.GetHeight();
        int text_x = scaledWidth * 15 / 10;
        int text_y_name = height * 15 / 100;
        int lenAvail = width - text_x - offset;
        dc.SetTextForeground(wxColour(28, 28, 28));
        wxFont *dFont = GetOCPNScaledFont_PlugIn(_("Dialog"));
        double dialog_font_size = dFont->GetPointSize();
        
        //  Count the lines, to adjust font size
        double fontTestSize = dFont->GetPointSize() * 8/4;
#ifdef __OCPN__ANDROID__
        fontTestSize = dFont->GetPointSize();
#endif        
        bool ok = false;
        while( !ok){
            wxFont *qFont = wxTheFontList->FindOrCreateFont( fontTestSize, dFont->GetFamily(), dFont->GetStyle(), dFont->GetWeight());
            dc.SetFont( *qFont );
            wxArrayString array = splitLine(nameString, dc, lenAvail);
            if(array.GetCount() > 2){
                fontTestSize *= 0.8;
            }
            else{
                ok = true;
            }
            if(fontTestSize < dialog_font_size * 0.8){
                fontTestSize *= 1.1;
                ok = true;
            }
        }

#ifdef __OCPN__ANDROID__
        fontTestSize = wxMax(fontTestSize, dialog_font_size / 2);       // On compact display, don't allow the font to get too small
#else
        fontTestSize = wxMin(fontTestSize, dialog_font_size * 5 / 4);   //  Or too big on conventional displays
#endif
        
        wxFont *qFont = wxTheFontList->FindOrCreateFont( fontTestSize, dFont->GetFamily(), dFont->GetStyle(), dFont->GetWeight());
        dc.SetFont( *qFont );
        
        int hTitle = dc.GetCharHeight();
        
        // Split into lines...
        wxArrayString array = splitLine(nameString, dc, lenAvail);
        
        for(unsigned int i=0 ; i < array.GetCount() ; i++){
            dc.DrawText(array.Item(i), text_x, text_y_name);
            text_y_name += hTitle;
        }
        
        dc.SetFont( *dFont );

        wxFont *sFont = wxTheFontList->FindOrCreateFont( dFont->GetPointSize() * 8 / 10, dFont->GetFamily(), dFont->GetStyle(), dFont->GetWeight());
        dc.SetFont( *sFont );
        
        wxString tx = /*_("Status: ") + */ m_pChart->getStatusString();
        dc.DrawText( tx, text_x, height - GetCharHeight() - offset - 2);
        
        dc.SetFont( *qFont );
        
    }
    
    
}


BEGIN_EVENT_TABLE( chartScroller, wxScrolledWindow )
//EVT_PAINT(chartScroller::OnPaint)
EVT_ERASE_BACKGROUND(chartScroller::OnEraseBackground)
END_EVENT_TABLE()

chartScroller::chartScroller(wxWindow* parent, wxWindowID id, const wxPoint& pos, const wxSize& size, long style)
: wxScrolledWindow(parent, id, pos, size, style)
{
}

void chartScroller::OnEraseBackground(wxEraseEvent& event)
{
    wxASSERT_MSG
    (
        GetBackgroundStyle() == wxBG_STYLE_ERASE,
     "shouldn't be called unless background style is \"erase\""
    );
    
    wxDC& dc = *event.GetDC();
    dc.SetPen(*wxGREEN_PEN);
    
    // clear any junk currently displayed
    dc.Clear();
    
    PrepareDC( dc );
    
    const wxSize size = GetVirtualSize();
    for ( int x = 0; x < size.x; x += 15 )
    {
        dc.DrawLine(x, 0, x, size.y);
    }
    
    for ( int y = 0; y < size.y; y += 15 )
    {
        dc.DrawLine(0, y, size.x, y);
    }
    
    dc.SetTextForeground(*wxRED);
    dc.SetBackgroundMode(wxSOLID);
    dc.DrawText("This text is drawn from OnEraseBackground", 60, 160);
    
}

void chartScroller::DoPaint(wxDC& dc)
{
    PrepareDC(dc);
    
//    if ( m_eraseBgInPaint )
    {
        dc.SetBackground(*wxRED_BRUSH);
        
        // Erase the entire virtual area, not just the client area.
        dc.SetPen(*wxTRANSPARENT_PEN);
        dc.SetBrush(GetBackgroundColour());
        dc.DrawRectangle(GetVirtualSize());
        
        dc.DrawText("Background erased in OnPaint", 65, 110);
    }
//     else if ( GetBackgroundStyle() == wxBG_STYLE_PAINT )
//     {
//         dc.SetTextForeground(*wxRED);
//         dc.DrawText("You must enable erasing background in OnPaint to avoid "
//         "display corruption", 65, 110);
//     }
//     
//     dc.DrawBitmap( m_bitmap, 20, 20, true );
//     
//     dc.SetTextForeground(*wxRED);
//     dc.DrawText("This text is drawn from OnPaint", 65, 65);
}

void chartScroller::OnPaint( wxPaintEvent &WXUNUSED(event) )
{
//     if ( m_useBuffer )
//     {
//         wxAutoBufferedPaintDC dc(this);
//         DoPaint(dc);
//     }
//     else
    {
        wxPaintDC dc(this);
        DoPaint(dc);
    }
}


BEGIN_EVENT_TABLE( shopPanel, wxPanel )
EVT_BUTTON( ID_CMD_BUTTON_INSTALL, shopPanel::OnButtonInstall )
EVT_BUTTON( ID_CMD_BUTTON_DOWNLOADLIST_CHAIN, shopPanel::OnDownloadListChain )
EVT_BUTTON( ID_CMD_BUTTON_DOWNLOADLIST_PROC, shopPanel::OnDownloadListProc )
END_EVENT_TABLE()











shopPanel::shopPanel(wxWindow* parent, wxWindowID id, const wxPoint& pos, const wxSize& size, long style)
: wxPanel(parent, id, pos, size, style)
{
    loadShopConfig();
    
#ifdef __OCPN_USE_CURL__    
    g_CurlEventHandler = new OESENC_CURL_EvtHandler;
#endif    
    
    g_shopPanel = this;
    m_bcompleteChain = false;
    m_bAbortingDownload = false;
    m_bconnected = false;
    
    m_ChartSelected = NULL;
    m_choiceSystemName = NULL;
    int ref_len = GetCharHeight();
    
    wxBoxSizer* boxSizerTop = new wxBoxSizer(wxVERTICAL);
    this->SetSizer(boxSizerTop);

#ifndef __OCPN__ANDROID__    
    wxString titleString = _T("OFC Plugin");
    titleString += _T(" [ Version: ") + g_versionString + _T(" ]");
    
    wxBoxSizer* boxSizerTitle = new wxBoxSizer(wxVERTICAL);
    boxSizerTop->Add(boxSizerTitle, 0, wxEXPAND );
    
    wxStaticText *title = new wxStaticText(this, wxID_ANY,   titleString );
    boxSizerTitle->Add(title, 0, wxALIGN_CENTER);
#endif    
    
    wxStaticBoxSizer* staticBoxSizerChartList = new wxStaticBoxSizer( new wxStaticBox(this, wxID_ANY, _("My Charts")), wxVERTICAL);
    boxSizerTop->Add(staticBoxSizerChartList, 0, wxALL|wxEXPAND, WXC_FROM_DIP(5));

    m_buttonUpdate = new wxButton(this, wxID_ANY, _("Refresh Chart List"), wxDefaultPosition, wxDLG_UNIT(this, wxSize(-1,-1)), 0);
    m_buttonUpdate->Connect(wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler(shopPanel::OnButtonRefresh), NULL, this);
    staticBoxSizerChartList->Add(m_buttonUpdate, 1, wxBOTTOM | wxRIGHT | wxALIGN_RIGHT, WXC_FROM_DIP(5));
    
    wxPanel *cPanel = new wxPanel(this, wxID_ANY, wxDefaultPosition, wxDLG_UNIT(this, wxSize(-1,-1)), wxBG_STYLE_ERASE );
    staticBoxSizerChartList->Add(cPanel, 0, wxALL|wxEXPAND, WXC_FROM_DIP(5));
    wxBoxSizer *boxSizercPanel = new wxBoxSizer(wxVERTICAL);
    cPanel->SetSizer(boxSizercPanel);
    
    //m_scrollWinChartList = new wxScrolledWindow(cPanel, wxID_ANY, wxDefaultPosition, wxDLG_UNIT(this, wxSize(-1,-1)), wxBORDER_RAISED | wxVSCROLL | wxBG_STYLE_ERASE );
    m_scrollWinChartList = new wxScrolledWindow(cPanel, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxBORDER_RAISED | wxVSCROLL | wxBG_STYLE_ERASE );
    m_scrollWinChartList->SetScrollRate(1, 1);
    boxSizercPanel->Add(m_scrollWinChartList, 0, wxALL|wxEXPAND, WXC_FROM_DIP(5));
    
    boxSizerCharts = new wxBoxSizer(wxVERTICAL);
    m_scrollWinChartList->SetSizer(boxSizerCharts);
 
#ifdef __OCPN__ANDROID__ 
    int displayHeight, displayWidth;
    ::wxDisplaySize(&displayWidth, &displayHeight);
    int ref_height = displayHeight * 4 / 10;
    m_scrollWinChartList->SetMinSize(wxSize(-1,ref_height));
    staticBoxSizerChartList->SetMinSize(wxSize(-1,ref_height + GetCharHeight()));
#else
    m_scrollWinChartList->SetMinSize(wxSize(-1,15 * GetCharHeight()));
    staticBoxSizerChartList->SetMinSize(wxSize(-1,16 * GetCharHeight()));
#endif    
    
    
    wxString actionString = _("Actions");
        
    wxStaticBoxSizer* staticBoxSizerAction = new wxStaticBoxSizer( new wxStaticBox(this, wxID_ANY, actionString), wxVERTICAL);
    boxSizerTop->Add(staticBoxSizerAction, 0, wxALL|wxEXPAND, WXC_FROM_DIP(2));

//    m_staticLine121 = new wxStaticLine(this, wxID_ANY, wxDefaultPosition, wxDLG_UNIT(this, wxSize(-1,-1)), wxLI_HORIZONTAL);
//    staticBoxSizerAction->Add(m_staticLine121, 1, wxTOP|wxEXPAND, WXC_FROM_DIP(2));
    
    ///Buttons
    wxGridSizer* gridSizerActionButtons = new wxFlexGridSizer(1, 2, 0, 0);
    staticBoxSizerAction->Add(gridSizerActionButtons, 1, wxEXPAND, WXC_FROM_DIP(1));
    
    m_buttonInstall = new wxButton(this, ID_CMD_BUTTON_INSTALL, _("Install Selected Chart"), wxDefaultPosition, wxDLG_UNIT(this, wxSize(-1,-1)), 0);
    gridSizerActionButtons->Add(m_buttonInstall, 1, wxTOP | wxBOTTOM, WXC_FROM_DIP(1));
    
    m_buttonCancelOp = new wxButton(this, wxID_ANY, _("Cancel Operation"), wxDefaultPosition, wxDLG_UNIT(this, wxSize(-1,-1)), 0);
    m_buttonCancelOp->Connect(wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler(shopPanel::OnButtonCancelOp), NULL, this);
    gridSizerActionButtons->Add(m_buttonCancelOp, 1, wxTOP | wxBOTTOM, WXC_FROM_DIP(1));

//    wxStaticLine* sLine1 = new wxStaticLine(this, wxID_ANY, wxDefaultPosition, wxDLG_UNIT(this, wxSize(-1,-1)), wxLI_HORIZONTAL);
//    staticBoxSizerAction->Add(sLine1, 1, wxEXPAND, WXC_FROM_DIP(2));
    
    
    ///Status
    m_staticTextStatus = new wxStaticText(this, wxID_ANY, _("Status: Chart List Refresh required."), wxDefaultPosition, wxDLG_UNIT(this, wxSize(-1,-1)), 0);
    m_staticTextStatus->Wrap(-1);
    staticBoxSizerAction->Add(m_staticTextStatus, 1, wxALL|wxALIGN_LEFT|wxEXPAND, WXC_FROM_DIP(1));

    m_ipGauge = new InProgressIndicator(this, wxID_ANY, 100, wxDefaultPosition, wxSize(ref_len * 12, ref_len));
    staticBoxSizerAction->Add(m_ipGauge, 0, wxALL|wxALIGN_CENTER_HORIZONTAL, WXC_FROM_DIP(1));

  
    SetName(wxT("shopPanel"));
    Layout();
    if (GetSizer()) {
        GetSizer()->Fit(this);
    }
    
#ifdef __OCPN__ANDROID__
//    m_scrollWinChartList->GetHandle()->setStyleSheet( qtStyleSheet);
//    QScroller::ungrabGesture(m_scrollWinChartList->GetHandle());
#endif    
    
    //  Turn off all buttons initially.
    ///m_buttonInstall->Hide();
    ///m_buttonCancelOp->Hide();
    
    UpdateChartList();
    
}

shopPanel::~shopPanel()
{
    Disconnect(wxEVT_DOWNLOAD_EVENT, (wxObjectEventFunction)(wxEventFunction)&shopPanel::onDLEvent);
    m_bconnected = false;
    
}

void shopPanel::SelectChart( oeSencChartPanel *chart )
{
    if (m_ChartSelected == chart)
        return;
    
    if (m_ChartSelected)
        m_ChartSelected->SetSelected(false);
    
    m_ChartSelected = chart;
    if (m_ChartSelected)
        m_ChartSelected->SetSelected(true);
    
#ifdef __OCPN__ANDROID__    
    m_scrollWinChartList->ClearBackground();
    m_scrollWinChartList->FitInside();
    m_scrollWinChartList->GetSizer()->Layout();
#else    
    m_scrollWinChartList->GetSizer()->Layout();
#endif
    
    MakeChartVisible(m_ChartSelected);
    
    UpdateActionControls();
    
    Layout();
    
    Refresh( true );
}

void shopPanel::SelectChartByID( wxString& sku, int index)
{
    bool bfound = false;
    for(unsigned int i = 0 ; i < m_panelArray.GetCount() ; i++){
        itemChart *chart = m_panelArray.Item(i)->m_pChart;
        if(sku.IsSameAs(chart->productSKU) && (index == chart->indexSKU)){
            SelectChart(m_panelArray.Item(i));
            MakeChartVisible(m_ChartSelected);
            bfound = true;
            break;
        }
    }
    
    //  It is possible that the index value is wrong, and so nothing was found.
    //  In this case, try again with just the SKU
    if(!bfound){
        for(unsigned int i = 0 ; i < m_panelArray.GetCount() ; i++){
            itemChart *chart = m_panelArray.Item(i)->m_pChart;
            if(sku.IsSameAs(chart->productSKU)){
                SelectChart(m_panelArray.Item(i));
                MakeChartVisible(m_ChartSelected);
                bfound = true;
                break;
            }
        }
    }
}

void shopPanel::MakeChartVisible(oeSencChartPanel *chart)
{
    if(!chart)
        return;
    
    itemChart *vchart = chart->m_pChart;
    
    for(unsigned int i = 0 ; i < m_panelArray.GetCount() ; i++){
        itemChart *lchart = m_panelArray.Item(i)->m_pChart;
        
        if(vchart->isMatch(lchart)){
                
            int offset = i * chart->GetUnselectedHeight();
            
///            m_scrollWinChartList->Scroll(-1, offset / 5);
        }
    }
    

}


void shopPanel::OnButtonRefresh( wxCommandEvent& event )
{
    if(!doLogin())
        return;
    
    setStatusText( _("Contacting Fugawi Charts server..."));
    m_ipGauge->Start();
    wxYield();

    ::wxBeginBusyCursor();
    int err_code = getChartList();
    ::wxEndBusyCursor();
    
    if(err_code != 0){                  // Some error
        wxString ec;
        ec.Printf(_T(" { %d }"), err_code);
        setStatusText( _("Status: Communications error.") + ec);
        m_ipGauge->Stop();
        wxYield();
        return;
    }
    g_chartListUpdatedOK = true;

    setStatusText( _("Status: Checking for updates."));
    m_ipGauge->Stop();
    
    int rv = checkUpdateStatus();
    if(rv != 0){                  // Some error
        wxString ec;
        ec.Printf(_T(" { %d }"), err_code);
        setStatusText( _("Status: Chartset index download failed.") + ec);
        return;
    }
    
    setStatusText( _("Status: Ready"));
    
    UpdateChartList( true );            // Try to download/verify chartset thubmnails
    
    saveShopConfig();
}

void shopPanel::downloadList(itemChart *chart, wxArrayString &targetURLArray)
{
    
    // Fill the URL array with desired file downloads
    chart->urlArray.Clear();
    
    for(unsigned int i=0 ; i < targetURLArray.GetCount() ; i++){
        chart->urlArray.Add( targetURLArray.Item(i) );
    }
        
    
    //  OK, the URL Array is populated
    // Now start the download chain
    chart->indexFileArrayIndex = 0;
    chart->installLocation.Clear();  // Mark as not installed
    
    g_chartProcessing = chart;
    
    wxCommandEvent event_next(wxEVT_COMMAND_BUTTON_CLICKED);
    event_next.SetId( ID_CMD_BUTTON_DOWNLOADLIST_PROC );
    GetEventHandler()->AddPendingEvent(event_next);
    
    
    
    return;
    
}





void shopPanel::doFullSetDownload(itemChart *chart)
{
    if(!chart)
        return;
    
    //  Get the full list of download URLs from the index file

    if(!chart->chartElementArray.GetCount()){           //nead to download the index file, and create the URL list, etc
     
        if(chart->indexFileURL.Len()){
            bool bUpdate = false;    
            
            //  Create a destination file name for the download.
            wxURI uri;
            uri.Create(chart->indexFileURL);
            wxString serverFilename = uri.GetPath();
            wxFileName fn(serverFilename);
            
            wxString downloadIndexDir = g_PrivateDataDir + _T("tmp") + wxFileName::GetPathSeparator();
            if(!::wxDirExists(downloadIndexDir)){
                ::wxMkdir(downloadIndexDir);
            }
            
            downloadIndexDir += chart->shortSetName + wxFileName::GetPathSeparator();
            if(!::wxDirExists(downloadIndexDir)){
                ::wxMkdir(downloadIndexDir);
            }
            
            wxString downloadIndexFile = downloadIndexDir + fn.GetFullName();
            
            #ifdef __OCPN__ANDROID__
            wxString file_URI = _T("file://") + downloadIndexFile;
            #else
            wxString file_URI = downloadIndexFile;
            #endif    
            
            
            _OCPN_DLStatus ret = OCPN_downloadFile( chart->indexFileURL, file_URI, _("Downloading index file"), _("Reading Index: "), wxNullBitmap, this,
                                                    OCPN_DLDS_ELAPSED_TIME|OCPN_DLDS_ESTIMATED_TIME|OCPN_DLDS_REMAINING_TIME|OCPN_DLDS_SPEED|OCPN_DLDS_SIZE|OCPN_DLDS_URL|OCPN_DLDS_CAN_PAUSE|OCPN_DLDS_CAN_ABORT|OCPN_DLDS_AUTO_CLOSE,
                                                    10);
            
            
            if(OCPN_DL_NO_ERROR == ret){
                // save a reference to the downloaded index file, will be relocated and cleared later
                chart->indexFileTmp = downloadIndexFile;
                
                // We parse the index file, building an array of useful information as chartMetaInfo ptrs.
                chart->chartElementArray.Clear();
                
                unsigned char *readBuffer = NULL;
                wxFFile indexFile(downloadIndexFile.mb_str());
                if(indexFile.IsOpened()){
                    wxFileOffset lenIndex = indexFile.Length();
                    unsigned int flen = wx_truncate_cast(unsigned int, lenIndex);
                    if(( flen > 0 )  && (flen < 1e7 ) ){                      // Place 10 Mb upper bound on index size 
                        readBuffer = (unsigned char *)malloc( 2 * flen);     // be conservative
                        
                        size_t nRead = indexFile.Read(readBuffer, flen);
                        if(nRead == flen){
                            indexFile.Close();
                            
                            // Good Read, so parse the XML 
                            TiXmlDocument * doc = new TiXmlDocument();
                            doc->Parse( (const char *)readBuffer );
                            
                            TiXmlElement * root = doc->RootElement();
                            if(root){
                                
                                TiXmlNode *child;
                                for ( child = root->FirstChild(); child != 0; child = child->NextSibling()){
                                    const char * t = child->Value();
                                    
                                    chartMetaInfo *pinfo = new chartMetaInfo();
                                    
                                    if(!strcmp(child->Value(), "chart")){
                                        TiXmlNode *chartx;
                                        for ( chartx = child->FirstChild(); chartx != 0; chartx = chartx->NextSibling()){
                                            const char * s = chartx->Value();
                                            
                                            if(!strcmp(s, "raster_edition")){
                                                TiXmlNode *re = chartx->FirstChild();
                                                if(re)
                                                    pinfo->raster_edition =  wxString::FromUTF8(re->Value());
                                                else
                                                    pinfo->raster_edition =  _("01");
                                            }
                                            if(!strcmp(s, "title")){
                                                TiXmlNode *title = chartx->FirstChild();
                                                pinfo->title =  wxString::FromUTF8(title->Value());
                                                
                                            }
                                            if(!strcmp(s, "x_fugawi_bzb_name")){
                                                TiXmlNode *bzbName = chartx->FirstChild();
                                                pinfo->bzb_name = wxString::FromUTF8(bzbName->Value());
                                            }
                                            
                                        }
                                        
                                        // Got everything we need, so add the element
                                        chart->chartElementArray.Add(pinfo);
                                        
                                    }
                                }
                            }
                        }
                    }
                }
                free(readBuffer);
                readBuffer = NULL;
            }
        }
    }
    
    chart->bSkipDuplicates = true;
    chart->installLocation.Clear();  // Mark as not installed
    
    // Fill a target URL array with desired file downloads,
    // Which in this case is everything gleaned from the index file
    wxArrayString fullArray;
    
    for(unsigned int i=0 ; i < chart->chartElementArray.GetCount() ; i++){
        chartMetaInfo *p_info = chart->chartElementArray.Item(i);
        wxString fullBZBUrl = chart->indexBaseDir + p_info->bzb_name;
        fullArray.Add( fullBZBUrl );
    }
    
    
    downloadList(chart, fullArray);
}







void shopPanel::OnDownloadListProc( wxCommandEvent& event )
{
    //itemChart *chart = m_ChartSelected->m_pChart;
    itemChart *chart = g_chartProcessing;
    if(!chart)
        return;
    
    // Download the BZB files listed in the array
    
    m_bcompleteChain = true;
    m_bAbortingDownload = false;
    
    // the target url
    if(chart->indexFileArrayIndex >= chart->urlArray.GetCount() ){               // some counting error
        setStatusText( _("Status: Error parsing index.xml."));
        m_buttonCancelOp->Hide();
        GetButtonUpdate()->Enable();
        g_statusOverride.Clear();
        UpdateChartList();
        return;                              // undetermined error??
    }
    
    // Advance to the next required download
    
    if(chart->bSkipDuplicates)
        advanceToNextChart(chart);

    return chainToNextChart(chart);

}

void shopPanel::advanceToNextChart(itemChart *chart)
{
    // Advance to the next required download
    while(chart->indexFileArrayIndex < chart->urlArray.GetCount()){               
        wxString tUrl = chart->urlArray.Item(chart->indexFileArrayIndex);
        wxURI uri;
        uri.Create(tUrl);
        wxString serverFilename = uri.GetPath();
        wxFileName fn(serverFilename);
        
        // Is the next BAP file in place already, and current?
        wxString fileName = fn.GetName();
        fileName.Replace("%27", "'");
        fileName.Replace("%28", "(");
        fileName.Replace("%29", ")");
        
        wxString BAPfile = chart->installLocationTentative + wxFileName::GetPathSeparator();
        BAPfile += chart->shortSetName + wxFileName::GetPathSeparator() + fileName + _T(".BAP");
        if(::wxFileExists( BAPfile )){
            chart->indexFileArrayIndex++;               // Skip to the next target
        }
        else{
            break;   // the while
        }
    }
}

void shopPanel::OnDownloadListChain( wxCommandEvent& event )
{
    //itemChart *chart = m_ChartSelected->m_pChart;
    itemChart *chart = g_chartProcessing;
    if(!chart)
        return;

    // Chained through from download end event
    if(m_bcompleteChain){
        
        setStatusText( _("Processing chart...") );
        
#ifndef __OCPN__ANDROID__        
        wxFileName fn(chart->downloadingFile);
#else        
        wxFileName fnn(chart->downloadingFile);
        wxString tempZip = _T("/data/data/org.opencpn.opencpn/files/") + fnn.GetName() + _T(".zip");
        wxFileName fn(tempZip);
#endif        
        
        
        wxString installDir = chart->installLocationTentative + wxFileName::GetPathSeparator() + chart->shortSetName;
        
        m_bcompleteChain = false;
            
        if(m_bAbortingDownload){
            m_bAbortingDownload = false;
            ShowOFCMessageDialog(NULL, _("Chart download cancelled."), _("ofc_pi Message"), wxOK);
            
            m_buttonInstall->Enable();
            
            g_statusOverride.Clear();
            UpdateChartList();
            
            return;
        }
        
        bool bSuccess = true;
        if(!wxFileExists( chart->downloadingFile )){
            bSuccess = false;
        }
        
        // BZB File is ready
        
        if(bSuccess){
            if(!validate_server()){
                setStatusText( _("Status: Server unavailable."));
                m_buttonCancelOp->Hide();
                GetButtonUpdate()->Enable();
                g_statusOverride.Clear();
                UpdateChartList();
                return;
            }
            
        }
        
        //qDebug() << "bSuccess After download: " << bSuccess;
        
        if(bSuccess){
            // location for decrypted BZB file
            
            fn.SetExt(_T("zip"));
            
            oernc_inStream decoder;
            bool result = decoder.decryptBZB(chart->downloadingFile, fn.GetFullPath());
            
            if(!result){
                bSuccess = false;
            }
        }
 
        //qDebug() << "bSuccess After decrypt: " << bSuccess;
        
        if(!bSuccess){
            setStatusText( _("BZB Decrypt failed.") );
            m_buttonCancelOp->Hide();
            GetButtonUpdate()->Enable();
            g_statusOverride.Clear();
            UpdateChartList();
            return;
        }
 
        if(bSuccess){
            // Unzip and extract the .BAP file to target location
#ifndef __OCPN__ANDROID__
            bool zipret = ExtractZipFiles( fn.GetFullPath(), installDir, false, wxDateTime::Now(), false);
#else
            bool zipret = AndroidUnzip( fn.GetFullPath(), installDir, false, false);
#endif            
            
            if(!zipret){
                bSuccess = false;
            }
        }

        //qDebug() << "bSuccess After unzip: " << bSuccess;
        
        // clean up
        //::wxRemoveFile(fn.GetFullPath());               // the decrypted zip file
        //::wxRemoveFile(chart->downloadingFile);         // the downloaded BZB file
 
//         if( 1 ){
//             setStatusText( _("OK Early stop 1.") );
//             m_buttonCancelOp->Hide();
//             GetButtonUpdate()->Enable();
//             g_statusOverride.Clear();
//             UpdateChartList();
//             return;
//         }
        
            // Success for this file...
        if(bSuccess) {   
            chart->indexFileArrayIndex++;               // move to the next file

            // Advance to the next required download
            if(chart->bSkipDuplicates)
                advanceToNextChart(chart);
                
            return chainToNextChart(chart);
        }       // bSuccess
        else {                  // on no success with this file, we might try again
            dlTryCount++;
            if(dlTryCount > N_RETRY){
                setStatusText( _("Status: BZB download FAILED after retry.") + _T("  [") + chart->downloadingFile + _T("]") );
                m_buttonCancelOp->Hide();
                GetButtonUpdate()->Enable();
                g_statusOverride.Clear();
                UpdateChartList();
                return;
            }
            else{
                chainToNextChart(chart, dlTryCount);            // Retry the same chart
            }
        }        
            
        
    }
}


void shopPanel::chainToNextChart(itemChart *chart, int ntry)
{
    bool bContinue = chart->indexFileArrayIndex < chart->urlArray.GetCount();
    
    //bool bContinue = chart->indexFileArrayIndex < 1;        /// testing
    
    if(!bContinue){
        
        // Record the full install location
        chart->installLocation = chart->installLocationTentative;
        wxString installDir = chart->installLocation + wxFileName::GetPathSeparator() + chart->shortSetName;
        chart->chartInstallLocnFull = installDir;
 
        
        //  Create the chartInfo file
        wxString infoDirTemp = g_PrivateDataDir + _T("tmp") + wxFileName::GetPathSeparator();
        infoDirTemp += chart->shortSetName + wxFileName::GetPathSeparator();
        wxString infoFileTemp = infoDirTemp + _T("chartInfo.txt");
        if(wxFileExists( infoFileTemp ))
            ::wxRemoveFile(infoFileTemp);
        
        
        wxTextFile info;
        info.Create(infoFileTemp);
        info.AddLine(_T("productSKU=") + chart->productSKU);
        info.AddLine(_T("productKey=") + chart->productKey);
        info.Write();
        info.Close();

        // Save a copy of the chartInfo file
        wxString infoFile = installDir + wxFileName::GetPathSeparator() + _T("chartInfo.txt");
#ifndef __OCPN__ANDROID__            
        ::wxCopyFile(infoFileTemp, infoFile);
#else
        bool bCopyResult1 = AndroidSecureCopyFile(infoFileTemp, infoFile);
        qDebug() << "bCopyResult1: " << bCopyResult1;
            
#endif
        if(wxFileExists( infoFileTemp ))
            ::wxRemoveFile(infoFileTemp);
        
        // Save a copy of the index file
        if(chart->indexFileTmp.Len()){
            wxString indexFile = installDir + wxFileName::GetPathSeparator() + _T("index.xml");
#ifndef __OCPN__ANDROID__            
            ::wxCopyFile(chart->indexFileTmp, indexFile);
#else
            bool bCopyResult2 = AndroidSecureCopyFile(chart->indexFileTmp, indexFile);
            qDebug() << "bCopyResult2: " << bCopyResult2;
            
#endif            
            
        }
        ::wxRemoveFile(chart->indexFileTmp);
        
        // We can always celar the updatePending flag if we got this far
        chart->pendingUpdateFlag = false;
        
        //  Add the target install directory to core dir list if necessary
        //  If the currect core chart directories do not cover this new directory, then add it
        bool covered = false;
        for( size_t i = 0; i < GetChartDBDirArrayString().GetCount(); i++ )
        {
            if( installDir.StartsWith((GetChartDBDirArrayString().Item(i))) )
            {
                covered = true;
                break;
            }
        }
        if( !covered )
        {
            AddChartDirectory( installDir );
        }
        
        // Update the chart dB on options close
        ForceChartDBUpdate();
        
        // Clean up the UI
        g_dlStatPrefix.Clear();
        setStatusText( _("Status: Ready"));
        getInProcessGuage()->Reset();
        getInProcessGuage()->Stop();
        
        ShowOFCMessageDialog(NULL, _("Chart installation complete."), _("ofc_pi Message"), wxOK);
        
        GetButtonUpdate()->Enable();
        m_buttonCancelOp->Hide();
        g_statusOverride.Clear();
        UpdateChartList();
        
        saveShopConfig();
        
        g_chartProcessing = NULL;
        
        return;
    }
    else{                           // not done yet, carry on with the list
        m_bcompleteChain = true;
        m_bAbortingDownload = false;
                
                // the target url
        wxString tUrl = chart->urlArray.Item(chart->indexFileArrayIndex);
                
                //  Create a destination file name for the download.
        wxURI uri;
        uri.Create(tUrl);
                
        wxString serverFilename = uri.GetPath();
        wxFileName fn(serverFilename);
        
        wxString downloadTmpDir = g_PrivateDataDir + _T("tmp") + wxFileName::GetPathSeparator();
        if(!::wxDirExists(downloadTmpDir)){
              ::wxMkdir(downloadTmpDir);
        }
                
        wxString downloadBZBFile = downloadTmpDir + fn.GetName() + _T(".BZB");
                
        chart->downloadingFile = downloadBZBFile;
        dlTryCount = ntry;
                
                
        wxString statIncremental =_("Downloading chart:") + _T(" ") + fn.GetName() + _T(" ");
        wxString i1;  i1.Printf(_T("(%d/%d) "), chart->indexFileArrayIndex + 1, chart->urlArray.GetCount());
        g_dlStatPrefix = statIncremental + i1;
                
        m_buttonCancelOp->Show();

#ifndef __OCPN__ANDROID__        
        downloadOutStream = new wxFFileOutputStream(downloadBZBFile);
        
        g_downloadChainIdentifier = ID_CMD_BUTTON_DOWNLOADLIST_CHAIN;
        g_curlDownloadThread = new wxCurlDownloadThread(g_CurlEventHandler);
        g_curlDownloadThread->SetURL(tUrl);
        g_curlDownloadThread->SetOutputStream(downloadOutStream);
        g_curlDownloadThread->Download();
#else
        if(!m_bconnected){
            Connect(wxEVT_DOWNLOAD_EVENT, (wxObjectEventFunction)(wxEventFunction)&shopPanel::onDLEvent);
            m_bconnected = true;
        }
 
        g_downloadChainIdentifier = ID_CMD_BUTTON_DOWNLOADLIST_CHAIN;
        
        OCPN_downloadFileBackground( tUrl, downloadBZBFile, this, &g_FileDownloadHandle);
        
#endif        
                
        return;
    }
}






void shopPanel::OnButtonUpdate( wxCommandEvent& event )
{

    itemChart *chart = m_ChartSelected->m_pChart;
    if(!chart)
        return;
    
    // First, we delete any charts indicated by the proper array
    for(unsigned int i=0 ; i < chart->deletedChartsNameArray.GetCount() ; i++){
        wxFileName fn(chart->deletedChartsNameArray.Item(i));
        
        wxString delTarget = chart->chartInstallLocnFull + wxFileName::GetPathSeparator() + fn.GetName() + _T(".BAP");
        ::wxRemoveFile(delTarget);
        delTarget = chart->chartInstallLocnFull + wxFileName::GetPathSeparator() +  fn.GetName() + _T(".BSB");
        ::wxRemoveFile(delTarget);
    }
        
    
    
    m_startedDownload = false;
    chart->bSkipDuplicates = false;     // Force overwrite
    
    
    // Fill a target URL array with desired file downloads,
    // Which in this case is the update list
    wxArrayString targetURLArray;
    
    for(unsigned int i=0 ; i < chart->updatedChartsURLArray.GetCount() ; i++){
        wxString fullBZBUrl = chart->indexBaseDir + chart->updatedChartsURLArray.Item(i);
        targetURLArray.Add( fullBZBUrl );
    }
    
    // Use existing install location, if not empty
    if(chart->installLocation.Len()){
        chart->installLocationTentative = chart->installLocation;
    }
    else{               // Otherwise, prompt for a new install directory

        wxString installLocn = g_PrivateDataDir;
        if(g_lastInstallDir.Length())
            installLocn = g_lastInstallDir;
            
        wxDirDialog dirSelector( NULL, _("Choose chart install location."), installLocn, wxDD_DEFAULT_STYLE  );
        int result = dirSelector.ShowModal();
            
        if(result == wxID_OK){
            wxString chosenInstallDir = dirSelector.GetPath();
            chart->installLocationTentative = chosenInstallDir;
            g_lastInstallDir = chosenInstallDir;
        }
        else{
            setStatusText( _("Status:  Cancelled."));
            m_buttonCancelOp->Hide();
            GetButtonUpdate()->Enable();
                
            g_statusOverride.Clear();
            UpdateChartList();
                
            return;
        }
    }
    
    return downloadList(chart, targetURLArray);
    
}


void shopPanel::OnButtonInstall( wxCommandEvent& event )
{
    m_buttonInstall->Disable();
    m_buttonCancelOp->Show();
    
    itemChart *chart = m_ChartSelected->m_pChart;
    if(!chart)
        return;

    //  If an update is pending, branch here.
    if(chart->pendingUpdateFlag)
        return OnButtonUpdate(event);
    
    
   
    // Is chart already in "download" state for me?
    if((chart->getChartStatus() == STAT_READY_DOWNLOAD) || (chart->getChartStatus() == STAT_CURRENT)) {   
        
            // Get the target install directory
            wxString installDir = chart->installLocation;
            wxString chosenInstallDir;
            
            if(1/*!installDir.Length()*/){
                
                wxString installLocn = g_PrivateDataDir;
                if(installDir.Length())
                    installLocn = installDir;
                else if(g_lastInstallDir.Length())
                    installLocn = g_lastInstallDir;
                
                //wxDirDialog dirSelector( NULL, _("Choose chart install location."), installLocn, wxDD_DEFAULT_STYLE  );
                //int result = dirSelector.ShowModal();
                
                wxString dir_spec;
                int result = PlatformDirSelectorDialog( NULL, &dir_spec, _("Choose chart install location."), installLocn);
                
//                 if( response == wxID_OK ) {
//                     m_tcChartDirectory->SetValue(dir_spec);
//                 }
                
                
                
                
                if(result == wxID_OK){
                    chosenInstallDir = dir_spec; //dirSelector.GetPath();
                    chart->installLocationTentative = chosenInstallDir;
                    g_lastInstallDir = chosenInstallDir;
                }
                else{
                    setStatusText( _("Status:  Cancelled."));
                    m_buttonCancelOp->Hide();
                    GetButtonUpdate()->Enable();
                    
                    g_statusOverride.Clear();
                    UpdateChartList();
                    
                    return;
                }
            }
        
        m_startedDownload = false;
        return doFullSetDownload(chart);
    }
    
    // Otherwise, do the activate step
    int activateResult;
    activateResult = doActivate(chart);
    if(activateResult != 0){
        if(activateResult == 2)
            setStatusText( _("Status:  Activation Cancelled."));
        else
            setStatusText( _("Status:  Activation FAILED."));
        
        m_buttonCancelOp->Hide();
        GetButtonUpdate()->Enable();
        
        g_statusOverride.Clear();
        UpdateChartList();
        m_buttonInstall->Enable();
        return;
    }
    
    // Activation appears successful
    
    setStatusText( _("Contacting Fugawi Charts server..."));
    m_ipGauge->Start();
    wxYield();
    
    ::wxBeginBusyCursor();
    int err_code = getChartList();
    ::wxEndBusyCursor();
    
    if(err_code != 0){                  // Some error
        wxString ec;
        ec.Printf(_T(" { %d }"), err_code);
        setStatusText( _("Status: Communications error.") + ec);
        m_ipGauge->Stop();
        wxYield();
        return;
    }
    g_chartListUpdatedOK = true;
    
    setStatusText( _("Status: Ready"));
    m_buttonCancelOp->Hide();
    
    m_ipGauge->Stop();
    
    UpdateChartList();
    
    {              // Success
        wxString msg = _("Activation succeeded.\n");
        msg += _("You may now proceed to install the chartset");
        ShowOFCMessageDialog(NULL, msg, _("ofc_pi Message"), wxOK);
        
    }
    
    return;
    
}






int shopPanel::doDownloadGui()
{
    setStatusText( _("Status: Downloading..."));
    //m_staticTextStatusProgress->Show();
    m_buttonCancelOp->Show();
    GetButtonUpdate()->Disable();
    
    g_statusOverride = _("Downloading...");
    UpdateChartList();
    
    wxYield();
    
    m_bcompleteChain = true;
    m_bAbortingDownload = false;
    
    doDownload(m_ChartSelected);
    
    return 0;
}

void shopPanel::OnButtonCancelOp( wxCommandEvent& event )
{
#ifndef __OCPN__ANDROID__    
    if(g_curlDownloadThread){
        m_bAbortingDownload = true;
        g_curlDownloadThread->Abort();
        g_curlDownloadThread = NULL;
        
    }
#else
    OCPN_cancelDownloadFileBackground( g_FileDownloadHandle );
#endif    

    m_ipGauge->SetValue(0);
    setStatusTextProgress(_T(""));
    m_bcompleteChain = true;
    
    setStatusText( _("Status: OK"));
    m_buttonCancelOp->Hide();
    
    g_statusOverride.Clear();
    m_buttonInstall->Enable();
    
    UpdateChartList();
    
}


void shopPanel::StopAllDownloads()
{
#ifndef __OCPN__ANDROID__    
    if(g_curlDownloadThread){
        m_bAbortingDownload = true;
        
        m_ChartSelected = NULL;                 // stop chart update
        g_downloadChainIdentifier = 0;          // stop chain-through
        
        g_curlDownloadThread->Abort();
        g_curlDownloadThread = NULL;
        
    }
    
    m_ChartSelected = NULL;                 // clear list selection
    setStatusText( _("Status: OK"));
    m_buttonCancelOp->Hide();
    
    g_statusOverride.Clear();
    m_buttonInstall->Enable();
    
    UpdateChartList();
#endif
    
}


void shopPanel::UpdateChartList( bool bDownloadThumbs )
{
    // Capture the state of any selected chart
    m_ChartSelectedSKU.Clear();
    if(m_ChartSelected){
        itemChart *chart = m_ChartSelected->m_pChart;
        if(chart){
            m_ChartSelectedSKU = chart->productSKU;           // save a copy of the selected chart
            m_ChartSelectedIndex = chart->indexSKU;
        }
    }
    
    m_scrollWinChartList->ClearBackground();
    
    // Clear any existing panels
    for(unsigned int i = 0 ; i < m_panelArray.GetCount() ; i++){
        delete m_panelArray.Item(i);
    }
    m_panelArray.Clear();
    m_ChartSelected = NULL;

    
    // Add new panels
    // Clear all flags
    for(unsigned int i=0 ; i < g_ChartArray.GetCount() ; i++){ g_ChartArray.Item(i)->display_flags= 0; }
    
    // Add the charts relevant to this device
    for(unsigned int i=0 ; i < g_ChartArray.GetCount() ; i++){
        itemChart *c1 = g_ChartArray.Item(i);
        if(!g_chartListUpdatedOK || !c1->isChartsetDontShow()){
            c1->display_flags= 1;
        }
    }
    
    // Remove duplicates by finding them, and selecting the most useful chart for the list
    for(unsigned int i=0 ; i < g_ChartArray.GetCount() ; i++){
        itemChart *c1 = g_ChartArray.Item(i);
        
        if(c1->display_flags ==0)
            continue;
        
        bool bdup = false;
        for(unsigned int j=i+1 ; j < g_ChartArray.GetCount() ; j++){
            itemChart *c2 = g_ChartArray.Item(j);

            if(c2->display_flags ==0)
                continue;
            
            if(c1->productSKU == c2->productSKU){
                // A duplicate.  Choose the best
                if(c1->bActivated && !c2->bActivated){
                    c1->display_flags += 2;     // choose activated one
                    c2->display_flags = 0;
                }
                else if(c2->bActivated && !c1->bActivated){
                    c2->display_flags += 2;     // choose activated one
                    c1->display_flags = 0;
                }
                else if(!c2->bActivated && !c1->bActivated){
                    c1->display_flags += 2;     // choose first one 
                    c2->display_flags = 0;
                }
                
                bdup = true;
            }
        }
        if(!bdup)
            c1->display_flags += 2;            
     }

    // New create the displayable list
    bool bDoThumb = true;
    for(unsigned int i=0 ; i < g_ChartArray.GetCount() ; i++){
        itemChart *c1 = g_ChartArray.Item(i);
        if(!g_chartListUpdatedOK || (c1->display_flags > 2) ){
            oeSencChartPanel *chartPanel = new oeSencChartPanel( m_scrollWinChartList, wxID_ANY, wxDefaultPosition, wxSize(-1, -1), g_ChartArray.Item(i), this);
            chartPanel->SetSelected(false);
        
            //  Try to download thumbs
            //  In the interest of snappuy UI:
            //  If a download fails, don't try any more downloads in this loop.
            //  The missing images will be captured on a later update.
            if(bDownloadThumbs && bDoThumb){
                if(!chartPanel->m_pChart->downloadThumbnail())
                    bDoThumb = false;
            }
            
            boxSizerCharts->Add( chartPanel, 0, wxEXPAND | wxLEFT | wxRIGHT, 2 );
            m_panelArray.Add( chartPanel );
        } 
    }
    
    if(m_ChartSelectedSKU.Len())
        SelectChartByID(m_ChartSelectedSKU, m_ChartSelectedIndex);

#ifdef __OCPN__ANDROID__    
    m_scrollWinChartList->ClearBackground();
    m_scrollWinChartList->FitInside();
    m_scrollWinChartList->GetSizer()->Layout();

    Layout();
    m_scrollWinChartList->ClearBackground();
    
#else
    m_scrollWinChartList->ClearBackground();
    m_scrollWinChartList->GetSizer()->Layout();
    
    Layout();
    
    m_scrollWinChartList->ClearBackground();
#endif    
    
    UpdateActionControls();
    
    //saveShopConfig();
    
    Refresh( true );
}


void shopPanel::UpdateActionControls()
{
    //  Turn off all buttons.
    m_buttonInstall->Hide();
    m_buttonCancelOp->Hide();
    
    
    if(!m_ChartSelected){                // No chart selected
        m_buttonInstall->Enable();
        return;
    }
    
    if(!g_statusOverride.Length()){
        m_buttonInstall->Enable();
    }
    
    itemChart *chart = m_ChartSelected->m_pChart;

    wxString stringInstall = _("Install Selected Chartset");
    wxString stringReinstall = _("Reinstall Selected Chartset");
    wxString stringUpdate = _("Update Selected Chartset");
    wxString stringActivate = _("Activate Selected Chartset");
    wxString stringCancel = _("Cancel Operation");

#ifdef __OCPN__ANDROID__
    stringInstall = _("Install Charts");
    stringReinstall = _("Reinstall Charts");
    stringUpdate = _("Update Charts");
    stringActivate = _("Activate Charts");
    stringCancel = _("Cancel");
#endif
    
    if(chart->getChartStatus() == STAT_PURCHASED){
        m_buttonInstall->SetLabel(stringInstall);
        m_buttonInstall->Show();
    }
    else if(chart->getChartStatus() == STAT_CURRENT){
        m_buttonInstall->SetLabel(stringReinstall);
        m_buttonInstall->Show();
    }
    else if(chart->getChartStatus() == STAT_STALE){
        m_buttonInstall->SetLabel(stringUpdate);
        m_buttonInstall->Show();
    }
    else if(chart->getChartStatus() == STAT_READY_DOWNLOAD){
        m_buttonInstall->SetLabel(stringInstall);
        m_buttonInstall->Show();       
    }
    else if(chart->getChartStatus() == STAT_REQUESTABLE){
        m_buttonInstall->SetLabel(stringActivate);
        m_buttonInstall->Show();
    }
    
    m_buttonCancelOp->SetLabel(stringCancel);
    
}


// Update management

int shopPanel::checkUpdateStatus()
{

    int ret_code = 0;
    
    // get a list of chartsets presently installed
    ArrayOfCharts installedChartList;
 
    for(unsigned int i=0 ; i < g_ChartArray.GetCount() ; i++){
        itemChart *c1 = g_ChartArray.Item(i);
        if( STAT_CURRENT == c1->getChartStatus()){
            if( (c1->installLocation.Len()) && (c1->chartInstallLocnFull.Len()) )
                installedChartList.Add(c1);
        }
    }
    
    // Presumably, the chartlist has been built once, so the URL of the index file is known
    //  For each chartset, download the index file
    
    for(unsigned int i=0 ; i < installedChartList.GetCount() ; i++){
        itemChart *c1 = g_ChartArray.Item(i);
        
        if(c1->indexFileURL.Len()){
            
            bool bUpdate = false;    
            
            //  Create a destination file name for the download.
            wxURI uri;
            uri.Create(c1->indexFileURL);
            wxString serverFilename = uri.GetPath();
            wxFileName fn(serverFilename);
            
            wxString downloadIndexDir = g_PrivateDataDir + _T("tmp") + wxFileName::GetPathSeparator();
            if(!::wxDirExists(downloadIndexDir)){
                ::wxMkdir(downloadIndexDir);
            }
            
            downloadIndexDir += c1->shortSetName + wxFileName::GetPathSeparator();
            if(!::wxDirExists(downloadIndexDir)){
                ::wxMkdir(downloadIndexDir);
            }
            
            
            wxString downloadIndexFile = downloadIndexDir + fn.GetFullName();
            
            #ifdef __OCPN__ANDROID__
            wxString file_URI = _T("file://") + downloadIndexFile;
            #else
            wxString file_URI = downloadIndexFile;
            #endif    
            
            
            _OCPN_DLStatus ret = OCPN_downloadFile( c1->indexFileURL, file_URI, _("Downloading index file"), _("Reading Index: "), wxNullBitmap, this,
                                                    OCPN_DLDS_ELAPSED_TIME|OCPN_DLDS_ESTIMATED_TIME|OCPN_DLDS_REMAINING_TIME|OCPN_DLDS_SPEED|OCPN_DLDS_SIZE|OCPN_DLDS_URL|OCPN_DLDS_CAN_PAUSE|OCPN_DLDS_CAN_ABORT|OCPN_DLDS_AUTO_CLOSE,
                                                    10);
            
             
            if(OCPN_DL_NO_ERROR == ret){
                
                // save a reference to the downloaded index file, will be relocated and cleared later
                c1->indexFileTmp = downloadIndexFile;
                
                // We parse the index file, building an array of useful information as chartMetaInfo ptrs.
                c1->chartElementArray.Clear();
                
                unsigned char *readBuffer = NULL;
                wxFile indexFile(downloadIndexFile.mb_str());
                if(indexFile.IsOpened()){
                    int unsigned flen = indexFile.Length();
                    if(( flen > 0 )  && (flen < 1e7 ) ){                      // Place 10 Mb upper bound on index size 
                        readBuffer = (unsigned char *)malloc( 2 * flen);     // be conservative
                        
                        size_t nRead = indexFile.Read(readBuffer, flen);
                        if(nRead == flen){
                            indexFile.Close();
                            
                            // Good Read, so parse the XML 
                            TiXmlDocument * doc = new TiXmlDocument();
                            doc->Parse( (const char *)readBuffer );
                            
                            TiXmlElement * root = doc->RootElement();
                            if(root){
                            
                                
                                TiXmlNode *child;
                                for ( child = root->FirstChild(); child != 0; child = child->NextSibling()){
                                    const char * t = child->Value();
                                    
                                    chartMetaInfo *pinfo = new chartMetaInfo();
                                    
                                    if(!strcmp(child->Value(), "chart")){
                                        TiXmlNode *chartx;
                                        for ( chartx = child->FirstChild(); chartx != 0; chartx = chartx->NextSibling()){
                                            const char * s = chartx->Value();
                                            if(!strcmp(s, "raster_edition")){
                                                TiXmlNode *re = chartx->FirstChild();
                                                pinfo->raster_edition =  wxString::FromUTF8(re->Value());
                                            }
                                            if(!strcmp(s, "title")){
                                                TiXmlNode *title = chartx->FirstChild();
                                                pinfo->title =  wxString::FromUTF8(title->Value());
                                            }
                                            if(!strcmp(s, "x_fugawi_bzb_name")){
                                                TiXmlNode *bzbName = chartx->FirstChild();
                                                pinfo->bzb_name = wxString::FromUTF8(bzbName->Value());
                                            }
                                            
                                        }
                                        
                                        // Got everything we need, so add the element
                                        c1->chartElementArray.Add(pinfo);
                                        
                                    }
                                }
                            }
                        }
                    }
                }
                free(readBuffer);
                readBuffer = NULL;
                
                if(c1->chartElementArray.GetCount()){
                    //  Now read the installed chartset index file...
                    
                    ArrayOfchartMetaInfo installedMetaInfo;
                    
                    wxString installedIndexName = c1->chartInstallLocnFull;
                    installedIndexName += wxFileName::GetPathSeparator();
                    installedIndexName += _T("index.xml");
                    
                    if( wxFileExists(installedIndexName)){
                        wxFile indexFile(installedIndexName.mb_str());
                        if(indexFile.IsOpened()){
                            int unsigned flen = indexFile.Length();
                            if(( flen > 0 )  && (flen < 1e7 ) ){                      // Place 10 Mb upper bound on index size 
                                readBuffer = (unsigned char *)malloc( 2 * flen);     // be conservative
                        
                                size_t nRead = indexFile.Read(readBuffer, flen);
                                if(nRead == flen){
                                    indexFile.Close();
                            
                                    // Good Read, so parse the XML 
                                    TiXmlDocument * doc = new TiXmlDocument();
                                    doc->Parse( (const char *)readBuffer );
                            
                                    TiXmlElement * root = doc->RootElement();
                                    if(root){
                                        TiXmlNode *child;
                                        for ( child = root->FirstChild(); child != 0; child = child->NextSibling()){
                                            const char * t = child->Value();
                                            
                                            chartMetaInfo *pinfo = new chartMetaInfo();
                                            
                                            if(!strcmp(child->Value(), "chart")){
                                                TiXmlNode *chartx;
                                                for ( chartx = child->FirstChild(); chartx != 0; chartx = chartx->NextSibling()){
                                                    const char * s = chartx->Value();
                                                    if(!strcmp(s, "raster_edition")){
                                                        TiXmlNode *re = chartx->FirstChild();
                                                        pinfo->raster_edition =  wxString::FromUTF8(re->Value());
                                                    }
                                                    if(!strcmp(s, "title")){
                                                        TiXmlNode *title = chartx->FirstChild();
                                                        pinfo->title =  wxString::FromUTF8(title->Value());
                                                    }
                                                    if(!strcmp(s, "x_fugawi_bzb_name")){
                                                        TiXmlNode *bzbName = chartx->FirstChild();
                                                        pinfo->bzb_name = wxString::FromUTF8(bzbName->Value());
                                                    }
                                                    
                                                }
                                                
                                                // Got everything we need, so add the element
                                                installedMetaInfo.Add(pinfo);
                                                
                                            }
                                        }
                                    }
                                }
                            }
                        }
                        free(readBuffer);
                        readBuffer = NULL;
                    }
                    
                    //  Now merge/compare the metaInfo arrays...
                    //  This will be a 2D search
                    if(installedMetaInfo.GetCount()){                   // old index present and populated?
                        c1->updatedChartsURLArray.Clear();
                        c1->deletedChartsNameArray.Clear();
                        
                        for(unsigned int i=0 ; i < c1->chartElementArray.GetCount() ; i++){
                            chartMetaInfo *pnew_info = c1->chartElementArray.Item(i);

                            wxString newTitleHash = pnew_info->title + pnew_info->bzb_name;
                            
                            bool bFound = false;
                            for(unsigned int j=0 ; j < installedMetaInfo.GetCount() ; j++){
                                chartMetaInfo *pold_info = installedMetaInfo.Item(j);

                                wxString oldTitleHash = pold_info->title + pold_info->bzb_name;
                                
                                if(newTitleHash.IsSameAs(oldTitleHash)){
                                    pold_info->flag_found = true;           // Mark the installed array item to show that it exists in the new array
                                    bFound = true;
                                    if(!pnew_info->raster_edition.IsSameAs(pold_info->raster_edition)){      // An update to "raster_edition"
                                        c1->updatedChartsURLArray.Add(pnew_info->bzb_name);
                                        bUpdate = true;    
                                    }
                                    break;
                                }
                            }
                            if(!bFound){           // New chart was not found in the old list, so we add it to update array
                                c1->updatedChartsURLArray.Add(pnew_info->bzb_name);
                                bUpdate = true;    
                            }
                        }
                        
                        // Check for deleted charts
                        // indicated by charts in the installed array that have flag_found clear, meaning not found in new array
                        
                        for(unsigned int j=0 ; j < installedMetaInfo.GetCount() ; j++){
                            chartMetaInfo *pold_info = installedMetaInfo.Item(j);
                            if(!pold_info->flag_found){
                                c1->deletedChartsNameArray.Add(pold_info->bzb_name);
                            }
                        }
                    }
                    // Post results
                    c1->pendingUpdateFlag = bUpdate;
                }
            }
            else{
                ret_code = ret;
            }
            
                                                    
        }
    }
    
    return ret_code;
        
}    

void shopPanel::onDLEvent(OCPN_downloadEvent &evt)
{
    wxDateTime now = wxDateTime::Now();
    
    switch(evt.getDLEventCondition()){
        case OCPN_DL_EVENT_TYPE_END:
        {
            m_bTransferComplete = true;
            m_bTransferSuccess = (evt.getDLEventStatus() == OCPN_DL_NO_ERROR) ? true : false;
            
            getInProcessGuage()->SetValue(100);
            setStatusTextProgress(_T(""));
            setStatusText( _("Status: OK"));
            GetButtonUpdate()->Enable();

/*            
            if(!g_shopPanel->m_bAbortingDownload){
                if(g_curlDownloadThread){
                    g_curlDownloadThread->Wait();
                    delete g_curlDownloadThread;
                    g_curlDownloadThread = NULL;
                }
            }
            
            if(g_shopPanel->m_bAbortingDownload){
                if(g_shopPanel->GetSelectedChart()){
                    itemChart *chart = g_shopPanel->GetSelectedChart()->m_pChart;
                    if(chart){
                        chart->downloadingFile.Clear();
                    }
                }
            }
*/            
            //  Send an event to chain back to "Install" button
            wxCommandEvent event(wxEVT_COMMAND_BUTTON_CLICKED);
            if(g_downloadChainIdentifier){
                event.SetId( g_downloadChainIdentifier );
                GetEventHandler()->AddPendingEvent(event);
            }
            
            break;
        }   
        case OCPN_DL_EVENT_TYPE_PROGRESS:

            dl_now = evt.getTransferred();
            dl_total = evt.getTotal();
            
            // Calculate the gauge value
            if(dl_total > 0){
                float progress = dl_now/dl_total;
                
                getInProcessGuage()->SetValue(progress * 100);
                getInProcessGuage()->Refresh();
                
            }
            
            if(now.GetTicks() != g_progressTicks){
                
                //  Set text status
                wxString tProg;
                tProg = _("Downloaded:  ");
                wxString msg;
                msg.Printf( _T("(%6.1f MiB / %4.0f MiB)    "), (float)(evt.getTransferred() / 1e6), (float)(evt.getTotal() / 1e6));
                tProg += msg;
                
                if(g_dlStatPrefix.Len())
                    tProg = g_dlStatPrefix + msg;
                
                setStatusTextProgress( tProg );
                
                g_progressTicks = now.GetTicks();
            }
            
            break;
            
        case OCPN_DL_EVENT_TYPE_START:
        case OCPN_DL_EVENT_TYPE_UNKNOWN:    
        default:
            break;
    }
}


 //------------------------------------------------------------------
 
 
 BEGIN_EVENT_TABLE( InProgressIndicator, wxGauge )
 EVT_TIMER( 4356, InProgressIndicator::OnTimer )
 END_EVENT_TABLE()
 
 InProgressIndicator::InProgressIndicator()
 {
 }
 
 InProgressIndicator::InProgressIndicator(wxWindow* parent, wxWindowID id, int range,
                     const wxPoint& pos, const wxSize& size,
                     long style, const wxValidator& validator, const wxString& name)
{
    wxGauge::Create(parent, id, range, pos, size, style, validator, name);
    
//    m_timer.Connect(wxEVT_TIMER, wxTimerEventHandler( InProgressIndicator::OnTimer ), NULL, this);
    m_timer.SetOwner( this, 4356 );
    m_timer.Start( 50 );
    
    m_bAlive = false;
    
}

InProgressIndicator::~InProgressIndicator()
{
}
 
void InProgressIndicator::OnTimer(wxTimerEvent &evt)
{
    if(m_bAlive)
        Pulse();
}
 
 
void InProgressIndicator::Start() 
{
     m_bAlive = true;
}
 
void InProgressIndicator::Stop() 
{
     m_bAlive = false;
     SetValue(0);
}

void InProgressIndicator::Reset() 
{
    SetValue(0);
}

#ifdef __OCPN_USE_CURL__

//-------------------------------------------------------------------------------------------
OESENC_CURL_EvtHandler::OESENC_CURL_EvtHandler()
{
    Connect(wxCURL_BEGIN_PERFORM_EVENT, (wxObjectEventFunction)(wxEventFunction)&OESENC_CURL_EvtHandler::onBeginEvent);
    Connect(wxCURL_END_PERFORM_EVENT, (wxObjectEventFunction)(wxEventFunction)&OESENC_CURL_EvtHandler::onEndEvent);
    Connect(wxCURL_DOWNLOAD_EVENT, (wxObjectEventFunction)(wxEventFunction)&OESENC_CURL_EvtHandler::onProgressEvent);
    
}

OESENC_CURL_EvtHandler::~OESENC_CURL_EvtHandler()
{
}

void OESENC_CURL_EvtHandler::onBeginEvent(wxCurlBeginPerformEvent &evt)
{
    g_shopPanel->m_startedDownload = true;
}

void OESENC_CURL_EvtHandler::onEndEvent(wxCurlEndPerformEvent &evt)
{
    
    g_shopPanel->getInProcessGuage()->SetValue(100);
    g_shopPanel->setStatusTextProgress(_T(""));
    g_shopPanel->setStatusText( _("Status: OK"));
    //g_shopPanel->m_buttonCancelOp->Hide();
    //g_shopPanel->GetButtonDownload()->Hide();
    g_shopPanel->GetButtonUpdate()->Enable();
    
    if(downloadOutStream){
        downloadOutStream->Close();
        delete downloadOutStream;
        downloadOutStream = NULL;
    }
    
    if(!g_shopPanel->m_bAbortingDownload){
        if(g_curlDownloadThread){
            g_curlDownloadThread->Wait();
            delete g_curlDownloadThread;
            g_curlDownloadThread = NULL;
        }
    }

    if(g_shopPanel->m_bAbortingDownload){
        if(g_shopPanel->GetSelectedChart()){
            itemChart *chart = g_shopPanel->GetSelectedChart()->m_pChart;
            if(chart){
                chart->downloadingFile.Clear();
            }
        }
    }
    //  Send an event to chain back to "Install" button
    wxCommandEvent event(wxEVT_COMMAND_BUTTON_CLICKED);
    if(g_downloadChainIdentifier){
        event.SetId( g_downloadChainIdentifier );
        g_shopPanel->GetEventHandler()->AddPendingEvent(event);
    }
    
}


void OESENC_CURL_EvtHandler::onProgressEvent(wxCurlDownloadEvent &evt)
{
    dl_now = evt.GetDownloadedBytes();
    dl_total = evt.GetTotalBytes();
    
    // Calculate the gauge value
    if(evt.GetTotalBytes() > 0){
        float progress = evt.GetDownloadedBytes()/evt.GetTotalBytes();
        g_shopPanel->getInProcessGuage()->SetValue(progress * 100);
    }
    
    wxDateTime now = wxDateTime::Now();
    if(now.GetTicks() != g_progressTicks){
        std::string speedString = evt.GetHumanReadableSpeed(" ", 0);
    
    //  Set text status
        wxString tProg;
        tProg = _("Downloaded:  ");
        wxString msg;
        msg.Printf( _T("(%6.1f MiB / %4.0f MiB)    "), (float)(evt.GetDownloadedBytes() / 1e6), (float)(evt.GetTotalBytes() / 1e6));
        msg += wxString( speedString.c_str(), wxConvUTF8);
        tProg += msg;
        
        if(g_dlStatPrefix.Len())
            tProg = g_dlStatPrefix + msg;
            
        g_shopPanel->setStatusTextProgress( tProg );
        
        g_progressTicks = now.GetTicks();
    }
    
}

#endif


IMPLEMENT_DYNAMIC_CLASS( xtr1Login, wxDialog )
BEGIN_EVENT_TABLE( xtr1Login, wxDialog )
EVT_BUTTON( ID_GETIP_CANCEL, xtr1Login::OnCancelClick )
EVT_BUTTON( ID_GETIP_OK, xtr1Login::OnOkClick )
END_EVENT_TABLE()


xtr1Login::xtr1Login()
{
}

xtr1Login::xtr1Login( wxWindow* parent, wxWindowID id, const wxString& caption,
                                          const wxPoint& pos, const wxSize& size, long style )
{
    
    long wstyle = wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER;

    m_bCompact = false;
    wxSize sz = ::wxGetDisplaySize();
    if((sz.x < 500) | (sz.y < 500))
        m_bCompact = true;
    
    wxFont *qFont = GetOCPNScaledFont_PlugIn(_("Dialog"));
    
#ifndef __OCPN__ANDROID__
    SetFont( *qFont );
#else
    if(m_bCompact){
        SetFont( *qFont );
    }
    else{
        double font_size = qFont->GetPointSize() * 1.20;
        wxFont *dFont = wxTheFontList->FindOrCreateFont( font_size, qFont->GetFamily(), qFont->GetStyle(),  qFont->GetWeight());
        SetFont( *dFont );
    }
#endif
    
    wxDialog::Create( parent, id, caption, pos, size, wstyle );
    
    CreateControls();
    GetSizer()->SetSizeHints( this );
    Centre();
    Move(-1, 2 * GetCharHeight());
    
}

xtr1Login::~xtr1Login()
{
}

/*!
 * oeSENCLogin creator
 */

bool xtr1Login::Create( wxWindow* parent, wxWindowID id, const wxString& caption,
                                  const wxPoint& pos, const wxSize& size, long style )
{
    SetExtraStyle( GetExtraStyle() | wxWS_EX_BLOCK_EVENTS );
    
    long wstyle = style;
    #ifdef __WXMAC__
    wstyle |= wxSTAY_ON_TOP;
    #endif
    wxDialog::Create( parent, id, caption, pos, size, wstyle );
    
//     wxFont *qFont = GetOCPNScaledFont_PlugIn(_("Dialog"));
//     SetFont( *qFont );

//     wxFont *qFont = GetOCPNScaledFont_PlugIn(_("Dialog"));
//     double font_size = qFont->GetPointSize() * 3 / 2;
//     wxFont *dFont = wxTheFontList->FindOrCreateFont( font_size, qFont->GetFamily(), qFont->GetStyle(),  qFont->GetWeight());
//     SetFont( *dFont );
    
    
    CreateControls(  );
    Centre();

    return TRUE;
}


void xtr1Login::CreateControls(  )
{
    int ref_len = GetCharHeight();
    
    xtr1Login* itemDialog1 = this;

#ifndef __OCPN__ANDROID__    
    
    wxBoxSizer* itemBoxSizer2 = new wxBoxSizer( wxVERTICAL );
    itemDialog1->SetSizer( itemBoxSizer2 );
    
    wxStaticBox* itemStaticBoxSizer4Static = new wxStaticBox( itemDialog1, wxID_ANY, _("Login to Fugawi.com shop") );
    
    wxStaticBoxSizer* itemStaticBoxSizer4 = new wxStaticBoxSizer( itemStaticBoxSizer4Static, wxVERTICAL );
    itemBoxSizer2->Add( itemStaticBoxSizer4, 0, wxEXPAND | wxALL, 5 );
    
    itemStaticBoxSizer4->AddSpacer(10);
    
    wxStaticLine *staticLine121 = new wxStaticLine(this, wxID_ANY, wxDefaultPosition, wxDLG_UNIT(this, wxSize(-1,-1)), wxLI_HORIZONTAL);
    itemStaticBoxSizer4->Add(staticLine121, 0, wxALL|wxEXPAND, WXC_FROM_DIP(5));
    
    wxFlexGridSizer* flexGridSizerActionStatus = new wxFlexGridSizer(0, 2, 0, 0);
    flexGridSizerActionStatus->SetFlexibleDirection( wxBOTH );
    flexGridSizerActionStatus->SetNonFlexibleGrowMode( wxFLEX_GROWMODE_SPECIFIED );
    flexGridSizerActionStatus->AddGrowableCol(0);
    
    itemStaticBoxSizer4->Add(flexGridSizerActionStatus, 1, wxALL|wxEXPAND, WXC_FROM_DIP(5));
    
    wxStaticText* itemStaticText5 = new wxStaticText( itemDialog1, wxID_STATIC, _("email address:"), wxDefaultPosition, wxDefaultSize, 0 );
    flexGridSizerActionStatus->Add( itemStaticText5, 0, wxALIGN_LEFT | wxLEFT | wxRIGHT | wxTOP, 5 );
    
    m_UserNameCtl = new wxTextCtrl( itemDialog1, ID_GETIP_IP, _T(""), wxDefaultPosition, wxSize( ref_len * 10, -1 ), 0 );
    flexGridSizerActionStatus->Add( m_UserNameCtl, 0,  wxALIGN_CENTER | wxLEFT | wxRIGHT | wxBOTTOM , 5 );
    
 
    wxStaticText* itemStaticText6 = new wxStaticText( itemDialog1, wxID_STATIC, _("Password:"), wxDefaultPosition, wxDefaultSize, 0 );
    flexGridSizerActionStatus->Add( itemStaticText6, 0, wxALIGN_LEFT | wxLEFT | wxRIGHT | wxTOP, 5 );
    
    m_PasswordCtl = new wxTextCtrl( itemDialog1, ID_GETIP_IP, _T(""), wxDefaultPosition, wxSize( ref_len * 10, -1 ), wxTE_PASSWORD );
    flexGridSizerActionStatus->Add( m_PasswordCtl, 0,  wxALIGN_CENTER | wxLEFT | wxRIGHT | wxBOTTOM , 5 );
    
    
    wxBoxSizer* itemBoxSizer16 = new wxBoxSizer( wxHORIZONTAL );
    itemBoxSizer2->Add( itemBoxSizer16, 0, wxALIGN_RIGHT | wxALL, 5 );
    
    m_CancelButton = new wxButton( itemDialog1, ID_GETIP_CANCEL, _("Cancel"), wxDefaultPosition, wxDefaultSize, 0 );
    itemBoxSizer16->Add( m_CancelButton, 0, wxALIGN_CENTER_VERTICAL | wxALL, 5 );
    
    m_OKButton = new wxButton( itemDialog1, ID_GETIP_OK, _("OK"), wxDefaultPosition, wxDefaultSize, 0 );
    m_OKButton->SetDefault();
    
    itemBoxSizer16->Add( m_OKButton, 0, wxALIGN_CENTER_VERTICAL | wxALL, 5 );
#else
    
    SetBackgroundColour(ANDROID_DIALOG_BACKGROUND_COLOR);
    SetForegroundColour(wxColour(200, 200, 200));
    
    wxBoxSizer* itemBoxSizer2 = new wxBoxSizer( wxVERTICAL );
    itemDialog1->SetSizer( itemBoxSizer2 );
    
    wxStaticBox* itemStaticBoxSizer4Static = new wxStaticBox( itemDialog1, wxID_ANY, _("Login to Fugawi.com shop") );
    
    wxStaticBoxSizer* itemStaticBoxSizer4 = new wxStaticBoxSizer( itemStaticBoxSizer4Static, wxVERTICAL );
    itemBoxSizer2->Add( itemStaticBoxSizer4, 0, wxEXPAND | wxALL, 5 );
    
    itemStaticBoxSizer4->AddSpacer(10);
    
    wxStaticLine *staticLine121 = new wxStaticLine(this, wxID_ANY, wxDefaultPosition, wxDLG_UNIT(this, wxSize(-1,-1)), wxLI_HORIZONTAL);
    itemStaticBoxSizer4->Add(staticLine121, 0, wxALL|wxEXPAND, WXC_FROM_DIP(5));
    
    
    
        
    wxPanel *loginPanel = new wxPanel(this, wxID_ANY, wxDefaultPosition, wxDLG_UNIT(this, wxSize(-1,-1)), wxBG_STYLE_ERASE );
    itemStaticBoxSizer4->Add(loginPanel, 0, wxALL|wxEXPAND, WXC_FROM_DIP(5));
    loginPanel->SetForegroundColour(wxColour(200, 200, 200));
    
    wxBoxSizer *boxSizercPanel = new wxBoxSizer(wxVERTICAL);
    loginPanel->SetSizer(boxSizercPanel);

    loginPanel->SetBackgroundColour(ANDROID_DIALOG_BODY_COLOR);
    
    wxFlexGridSizer* flexGridSizerActionStatus = new wxFlexGridSizer(0, 2, 0, 0);
    flexGridSizerActionStatus->SetFlexibleDirection( wxBOTH );
    flexGridSizerActionStatus->SetNonFlexibleGrowMode( wxFLEX_GROWMODE_SPECIFIED );
    flexGridSizerActionStatus->AddGrowableCol(0);
    
    boxSizercPanel->Add(flexGridSizerActionStatus, 1, wxALL|wxEXPAND, WXC_FROM_DIP(5));
    
    int item_space = 50;
    if(m_bCompact)
        item_space = 10;
    
    wxStaticText* itemStaticText5 = new wxStaticText( loginPanel, wxID_STATIC, _("email:"), wxDefaultPosition, wxDefaultSize, 0 );
    flexGridSizerActionStatus->Add( itemStaticText5, 0, wxALIGN_LEFT | wxLEFT | wxRIGHT | wxTOP | wxADJUST_MINSIZE, item_space );
    
    m_UserNameCtl = new wxTextCtrl( loginPanel, ID_GETIP_IP, _T(""), wxDefaultPosition, wxSize( ref_len * 10, -1 ), 0 );
    flexGridSizerActionStatus->Add( m_UserNameCtl, 0,  wxALIGN_CENTER | wxLEFT | wxRIGHT | wxTOP , item_space );
    
    wxStaticText* itemStaticText6 = new wxStaticText( loginPanel, wxID_STATIC, _("pass:"), wxDefaultPosition, wxDefaultSize, 0 );
    flexGridSizerActionStatus->Add( itemStaticText6, 0, wxALIGN_LEFT | wxLEFT | wxRIGHT | wxTOP | wxADJUST_MINSIZE, item_space );
    
    m_PasswordCtl = new wxTextCtrl( loginPanel, ID_GETIP_IP, _T(""), wxDefaultPosition, wxSize( ref_len * 10, -1 ), wxTE_PASSWORD );
    m_PasswordCtl->SetBackgroundColour(wxColour(0, 192, 192));
    flexGridSizerActionStatus->Add( m_PasswordCtl, 0,  wxALIGN_CENTER | wxLEFT | wxRIGHT | wxTOP , item_space );
    
 
    int button_space = 100;
    if(m_bCompact)
        button_space = 20;
    
    wxBoxSizer* itemBoxSizer16 = new wxBoxSizer( wxHORIZONTAL );
    boxSizercPanel->Add( itemBoxSizer16, 0, wxALIGN_RIGHT | wxALL, 5 );
    
    m_CancelButton = new wxButton( loginPanel, ID_GETIP_CANCEL, _("Cancel"), wxDefaultPosition, wxDefaultSize, 0 );
    itemBoxSizer16->Add( m_CancelButton, 0, wxALIGN_CENTER_VERTICAL | wxALL, button_space );
    
    m_OKButton = new wxButton( loginPanel, ID_GETIP_OK, _("OK"), wxDefaultPosition, wxDefaultSize, 0 );
    m_OKButton->SetDefault();
    
    itemBoxSizer16->Add( m_OKButton, 0, wxALIGN_CENTER_VERTICAL | wxALL, button_space );
#endif    
    
}


bool xtr1Login::ShowToolTips()
{
    return TRUE;
}



void xtr1Login::OnCancelClick( wxCommandEvent& event )
{
    EndModal(2);
}

void xtr1Login::OnOkClick( wxCommandEvent& event )
{
    if( (m_UserNameCtl->GetValue().Length() == 0 ) || (m_PasswordCtl->GetValue().Length() == 0 ) ){
        SetReturnCode(1);
        EndModal(1);
    }
    else {
        SetReturnCode(0);
        EndModal(0);
    }
}

void xtr1Login::OnClose( wxCloseEvent& event )
{
    SetReturnCode(2);
}

