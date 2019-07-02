/******************************************************************************
 * $Id: oernc_pi.cpp,v 1.8 2010/06/21 01:54:37 bdbcat Exp $
 *
 * Project:  OpenCPN
 * Purpose:  XTR1 PlugIn
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


#include "wx/wxprec.h"

#ifndef  WX_PRECOMP
  #include "wx/wx.h"
#endif //precompiled headers
#include <wx/filename.h>
#include <wx/dir.h>

#include "oernc_pi.h"
#include "chart.h"
#include "oernc_inStream.h"
#include "ochartShop.h"
#include <map>
#include <unordered_map>
#include <tinyxml.h>

#ifdef __OCPN__ANDROID__
#include "androidSupport.h"
#include "qdebug.h"
#endif

// the class factories, used to create and destroy instances of the PlugIn

extern "C" DECL_EXP opencpn_plugin* create_pi(void *ppimgr)
{
    return new oernc_pi(ppimgr);
}

extern "C" DECL_EXP void destroy_pi(opencpn_plugin* p)
{
    delete p;
}


wxString g_server_bin;
wxString g_pipeParm;
bool g_serverDebug;
long g_serverProc;
int g_debugLevel = 0;
int g_admin;

wxString  g_deviceInfo;
extern wxString  g_loginUser;
extern wxString  g_PrivateDataDir;
wxString  g_versionString;
bool g_bNoFindMessageShown;

//std::unordered_map<std::string, std::string> keyMap;
WX_DECLARE_STRING_HASH_MAP( wxString, OKeyHash );
OKeyHash keyMap;

//---------------------------------------------------------------------------------------------------------
//
//    PlugIn Implementation
//
//---------------------------------------------------------------------------------------------------------

#include "default_pi.xpm"

bool parseKeyFile( wxString kfile )
{
    FILE *iFile = fopen(kfile.mb_str(), "rb");
   
    if (iFile <= 0)
        return false;            // file error
        
    // compute the file length    
    fseek(iFile, 0, SEEK_END);
    size_t iLength = ftell(iFile);
    
    char *iText = (char *)calloc(iLength + 1, sizeof(char));
    
    // Read the file
    fseek(iFile, 0, SEEK_SET);
    size_t nread = 0;
    while (nread < iLength){
        nread += fread(iText + nread, 1, iLength - nread, iFile);
    }           
    fclose(iFile);

    
    //  Parse the XML
    TiXmlDocument * doc = new TiXmlDocument();
    const char *rr = doc->Parse( iText);
    
    TiXmlElement * root = doc->RootElement();
    if(!root)
        return false;                              // undetermined error??

    wxString RInstallKey, fileName;
    wxString rootName = wxString::FromUTF8( root->Value() );
    if(rootName.IsSameAs(_T("keyList"))){
            
        TiXmlNode *child;
        for ( child = root->FirstChild(); child != 0; child = child->NextSibling()){
            wxString s = wxString::FromUTF8(child->Value());  //chart
            
            TiXmlNode *childChart = child->FirstChild();
            for ( childChart = child->FirstChild(); childChart!= 0; childChart = childChart->NextSibling()){
                const char *chartVal =  childChart->Value();
                            
                if(!strcmp(chartVal, "RInstallKey")){
                    TiXmlNode *childVal = childChart->FirstChild();
                    if(childVal){
                        RInstallKey = childVal->Value();
                        
                    }
                }
                if(!strcmp(chartVal, "FileName")){
                    TiXmlNode *childVal = childChart->FirstChild();
                    if(childVal){
                        fileName = childVal->Value();
                        
                    }
                }

            }
            if(RInstallKey.Length() && fileName.Length()){
                OKeyHash::iterator search = keyMap.find(fileName);
                if (search == keyMap.end()) {
                    keyMap[fileName] = RInstallKey;
                }
            }
        }
        
        free( iText );
        return true;
    }           

    free( iText );
    return false;

}
    

bool loadKeyMap( wxString file )
{
    // Make a list of all XML or xml files found in the parent directory of the chart itself.
    wxFileName fn(file);

    wxArrayString xmlFiles;
    int nFiles = wxDir::GetAllFiles(fn.GetPath(), &xmlFiles, _T("*.XML"));
    nFiles += wxDir::GetAllFiles(fn.GetPath(), &xmlFiles, _T("*.xml"));
        
    //  Read and parse them all
    for(unsigned int i=0; i < xmlFiles.GetCount(); i++){
        wxString xmlFile = xmlFiles.Item(i);
        parseKeyFile(xmlFile);
    }
    
 
    return true;
}


wxString getKey(wxString file)
{
     wxFileName fn(file);
//     char buf[400];
//     strcpy( buf, (const char*)fn.GetName().mb_str(wxConvUTF8) ); // buf will now contain name, as UTF8
//     std::string sKey(buf);
    
    OKeyHash::iterator search = keyMap.find(fn.GetName());
    if (search != keyMap.end()) {
        return search->second;
    }

    loadKeyMap(file);

    search = keyMap.find(fn.GetName());
    if (search != keyMap.end()) {
        return search->second;
    }
    
    return wxString();
    
    
}



//---------------------------------------------------------------------------------------------------------
//
//          PlugIn initialization and de-init
//
//---------------------------------------------------------------------------------------------------------

oernc_pi::oernc_pi(void *ppimgr)
      :opencpn_plugin_19(ppimgr)
{
      // Create the PlugIn icons

      m_pplugin_icon = new wxBitmap(default_pi);
}

oernc_pi::~oernc_pi()
{
      delete m_pplugin_icon;
}

int oernc_pi::Init(void)
{
    wxString vs;
    vs.Printf(_T("%d.%d.%d"), PLUGIN_VERSION_MAJOR, PLUGIN_VERSION_MINOR, PLUGIN_VERSION_PATCH);
    g_versionString = vs;

    m_shoppanel = NULL;
    
    AddLocaleCatalog( _T("opencpn-oernc_pi") );

      //    Build an arraystring of dynamically loadable chart class names
      m_class_name_array.Add(_T("Chart_oeRNC"));

      // Specify the location of the xxserverd helper.
      wxFileName fn_exe(GetOCPN_ExePath());
      g_server_bin = fn_exe.GetPath( wxPATH_GET_VOLUME | wxPATH_GET_SEPARATOR) + _T("oeaserverd");
      
      
      #ifdef __WXMSW__
      g_server_bin = _T("\"") + fn_exe.GetPath( wxPATH_GET_VOLUME | wxPATH_GET_SEPARATOR) +
      _T("plugins\\oernc_pi\\oeaserverd.exe\"");
      #endif
      
      #ifdef __WXOSX__
      fn_exe.RemoveLastDir();
      g_server_bin = _T("\"") + fn_exe.GetPath( wxPATH_GET_VOLUME | wxPATH_GET_SEPARATOR) +
      _T("PlugIns/oernc_pi/oeaserverd\"");
      #endif
      
      #ifdef __OCPN__ANDROID__
      wxString piLocn = GetPlugInPath(this); //*GetpSharedDataLocation();
      wxFileName fnl(piLocn);
      g_server_bin = fnl.GetPath(wxPATH_GET_SEPARATOR) + _T("oeaserverda");
      g_serverProc = 0;
      #endif
      
      
      // Get and build if necessary a private data dir
      g_PrivateDataDir = *GetpPrivateApplicationDataLocation();
      g_PrivateDataDir += wxFileName::GetPathSeparator();
      g_PrivateDataDir += _T("oernc_pi");
      g_PrivateDataDir += wxFileName::GetPathSeparator();
      if(!::wxDirExists( g_PrivateDataDir ))
          ::wxMkdir( g_PrivateDataDir );
      
      wxLogMessage(_T("Path to serverd is: ") + g_server_bin);
      
      int flags = INSTALLS_PLUGIN_CHART;

      flags |= INSTALLS_TOOLBOX_PAGE;             // for  shop interface
      
      return flags;
      
}

bool oernc_pi::DeInit(void)
{
#if 0    
    SaveConfig();
    
    delete pinfoDlg;
    pinfoDlg = NULL;
    
    if( m_pOptionsPage )
    {
        if( DeleteOptionsPage( m_pOptionsPage ) )
            m_pOptionsPage = NULL;
    }
#endif

    m_class_name_array.Clear();
    
    shutdown_server();
    
    return true;
}

int oernc_pi::GetAPIVersionMajor()
{
      return MY_API_VERSION_MAJOR;
}

int oernc_pi::GetAPIVersionMinor()
{
      return MY_API_VERSION_MINOR;
}

int oernc_pi::GetPlugInVersionMajor()
{
      return PLUGIN_VERSION_MAJOR;
}

int oernc_pi::GetPlugInVersionMinor()
{
      return PLUGIN_VERSION_MINOR;
}

wxBitmap *oernc_pi::GetPlugInBitmap()
{
      return m_pplugin_icon;
}

wxString oernc_pi::GetCommonName()
{
    return _("oeRNC Charts");
}


wxString oernc_pi::GetShortDescription()
{
    return _("oeRNC Charts PlugIn for OpenCPN");
}


wxString oernc_pi::GetLongDescription()
{
      return _("oeRNC Charts PlugIn for OpenCPN\n\
Provides support for oeRNC raster charts.\n\n\
");

}

wxArrayString oernc_pi::GetDynamicChartClassNameArray()
{
      return m_class_name_array;
}


void oernc_pi::OnSetupOptions( void )
{
#ifdef __OCPN__ANDROID__
    m_pOptionsPage = AddOptionsPage( PI_OPTIONS_PARENT_CHARTS, _("oeRNC Charts") );
    if( ! m_pOptionsPage )
    {
        wxLogMessage( _T("Error: oernc_pi::OnSetupOptions AddOptionsPage failed!") );
        return;
    }
    wxBoxSizer *sizer = new wxBoxSizer( wxVERTICAL );
    m_pOptionsPage->SetSizer( sizer );
    
    m_oesencpanel = new oesencPanel( this, m_pOptionsPage, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxDEFAULT_DIALOG_STYLE );

    m_pOptionsPage->InvalidateBestSize();
    sizer->Add( m_oesencpanel, 1, wxALL | wxEXPAND );
    m_oesencpanel->FitInside();
#else
    m_pOptionsPage = AddOptionsPage( PI_OPTIONS_PARENT_CHARTS, _("oeRNC Charts") );
    if( ! m_pOptionsPage )
    {
        wxLogMessage( _T("Error: oernc_pi::OnSetupOptions AddOptionsPage failed!") );
        return;
    }
    wxBoxSizer *sizer = new wxBoxSizer( wxVERTICAL );
    m_pOptionsPage->SetSizer( sizer );
    
    m_shoppanel = new shopPanel( m_pOptionsPage, wxID_ANY, wxDefaultPosition, wxDefaultSize );
    
    m_pOptionsPage->InvalidateBestSize();
    sizer->Add( m_shoppanel, 1, wxALL | wxEXPAND );
    m_shoppanel->FitInside();

#endif
}
void oernc_pi::OnCloseToolboxPanel(int page_sel, int ok_apply_cancel)
{
    if(m_shoppanel){
        //m_shoppanel->StopAllDownloads();
    }
    
}


bool validate_server(void)
{
    
    
    if(g_debugLevel)printf("\n-------validate_server\n");
    //wxLogMessage(_T("validate_server"));
    
    if(1 /*g_serverProc*/){
        // Check to see if the server is already running, and available
        //qDebug() << "Check running server Proc";
        oernc_inStream testAvail;
        if(testAvail.isAvailable(wxEmptyString)){
            //wxLogMessage(_T("Available TRUE"));
            return true;
        }
        
        wxString tmsg;
        int nLoop = 1;
        while(nLoop < 2){
            tmsg.Printf(_T(" nLoop: %d"), nLoop);
            if(g_debugLevel)printf("      validate_server, retry: %d \n", nLoop);
            wxLogMessage(_T("Available FALSE, retrying...") + tmsg);
            wxMilliSleep(500);
            oernc_inStream testAvailRetry;
            if(testAvailRetry.isAvailable(wxEmptyString)){
                wxLogMessage(_T("Available TRUE on retry."));
                return true;
            }
            nLoop++;
        }
    }
    
    // Not running, so start it up...
    
    wxString bin_test = g_server_bin;
    
#ifndef __OCPN__ANDROID__    
    //Verify that serverd actually exists, and runs.
    
    if(wxNOT_FOUND != g_server_bin.Find('\"'))
        bin_test = g_server_bin.Mid(1).RemoveLast();
    
    wxString msg = _T("Checking server utility at ");
    msg += _T("{");
    msg += bin_test;
    msg += _T("}");
    wxLogMessage(_T("oernc_pi: ") + msg);
    
    
    if(!::wxFileExists(bin_test)){
        if(!g_bNoFindMessageShown){
            wxString msg = _("Cannot find the oernc_pi server utility at \n");
            msg += _T("{");
            msg += bin_test;
            msg += _T("}");
            OCPNMessageBox_PlugIn(NULL, msg, _("oernc_pi Message"),  wxOK, -1, -1);
            wxLogMessage(_T("oernc_pi: ") + msg);
        
            g_bNoFindMessageShown = true;
        }
        g_server_bin.Clear();
        return false;
    }
    
    // now start the server...
    wxString cmds = g_server_bin;
    
    
    wxString pipeParm;
    
    int flags = wxEXEC_ASYNC;
#ifdef __WXMSW__    
    flags |= wxEXEC_HIDE_CONSOLE;
    long pid = ::wxGetProcessId();
    pipeParm.Printf(_T("OCPN_PIPER%04d"), pid % 10000);
    g_pipeParm = pipeParm;
#endif
    
    if(g_pipeParm.Length())
        cmds += _T(" -p ") + g_pipeParm;
    
    if(g_serverDebug)
        cmds += _T(" -d");
    
    wxLogMessage(_T("oernc_pi: starting serverd utility: ") + cmds);
    g_serverProc = wxExecute(cmds, flags);              // exec asynchronously
    wxMilliSleep(1000);
    
    
#else           // Android
    qDebug() << "Starting oernc_pi server";
    
    //  The target binary executable
    wxString cmd = g_server_bin;
    
    //  Set up the parameter passed as the local app storage directory
    wxString dataLoc = *GetpPrivateApplicationDataLocation();
    wxFileName fn(dataLoc);
    wxString dataDir = fn.GetPath(wxPATH_GET_SEPARATOR);
    
    //  Set up the parameter passed to runtime environment as LD_LIBRARY_PATH
    // This will be {dir of g_server_bin}/lib
    wxFileName fnl(cmd);
    wxString libDir = fnl.GetPath(wxPATH_GET_SEPARATOR) + _T("lib");
    
    wxLogMessage(_T("oernc_pi: Starting: ") + cmd );
    
    wxString result = callActivityMethod_s4s("createProc", cmd, _T("-q"), dataDir, libDir);
    
    wxLogMessage(_T("oernc_pi: Start Result: ") + result);
    
    long pid;
    if(result.ToLong(&pid))
        g_serverProc = pid;
    
    wxMilliSleep(1000);
    
#endif    
    
    // Check to see if the server function is available
    if(g_serverProc){
        bool bAvail = false;
        int nLoop = 10;
        
        while(nLoop){
            oernc_inStream testAvail_One;
            if(!testAvail_One.isAvailable(_T("?")))
                wxSleep(1);
            else{
                bAvail = true;
                break;
            }
            nLoop--;
        }
        
        if(!bAvail){
            wxString msg = _T("oeaserverd utility at \n");
            msg += _T("{");
            msg += bin_test;
            msg += _T("}\n");
            msg += _T(" reports Unavailable.\n\n");
            //            OCPNMessageBox_PlugIn(NULL, msg, _("oesenc_pi Message"),  wxOK, -1, -1);
            wxLogMessage(_T("oernc_pi: ") + msg);
            
            g_server_bin.Clear();
            return false;
            
        }
        else{
            wxString nc;
            nc.Printf(_T("LoopCount: %d"), nLoop);
            
            wxLogMessage(_T("oernc_pi: serverd Check OK...") + nc);

        }
    }
    else{
        wxString msg = _("serverd utility at \n");
        msg += _T("{");
        msg += bin_test;
        msg += _T("}\n");
        msg += _(" could not be started.\n\n");
        OCPNMessageBox_PlugIn(NULL, msg, _("oernc_pi Message"),  wxOK, -1, -1);
        wxLogMessage(_T("oernc_pi: ") + msg);
        
        g_server_bin.Clear();
        return false;
    }
    
    return true;
}

bool shutdown_server( void )
{
    wxLogMessage(_T("oernc_pi: Shutdown_server"));
    
    // Check to see if the server is already running, and available
    oernc_inStream testAvail;
    if(1){
        testAvail.Shutdown();
        return true;
    }
    else{
        return false;
    }
}

#if 0
#ifdef __OCPN__ANDROID__

extern JavaVM *java_vm;         // found in androidUtil.cpp, accidentally exported....

bool CheckPendingJNIException()
{
    JNIEnv* jenv;
    
    if (java_vm->GetEnv( (void **) &jenv, JNI_VERSION_1_6) != JNI_OK) 
        return true;
    
    if( (jenv)->ExceptionCheck() == JNI_TRUE ) {
        
        // Handle exception here.
        (jenv)->ExceptionDescribe(); // writes to logcat
        (jenv)->ExceptionClear();
        
        return false;           // There was a pending exception, but cleared OK
        // interesting discussion:  http://blog.httrack.com/blog/2013/08/23/catching-posix-signals-on-android/
    }
    
    return false;
    
}


wxString callActivityMethod_s4s(const char *method, wxString parm1, wxString parm2, wxString parm3, wxString parm4)
{
    if(CheckPendingJNIException())
        return _T("NOK");
    JNIEnv* jenv;
    
    wxString return_string;
    QAndroidJniObject activity = QAndroidJniObject::callStaticObjectMethod("org/qtproject/qt5/android/QtNative",
                                                                           "activity", "()Landroid/app/Activity;");
    if(CheckPendingJNIException())
        return _T("NOK");
    
    if ( !activity.isValid() ){
        return return_string;
    }
    
    //  Need a Java environment to decode the resulting string
    if (java_vm->GetEnv( (void **) &jenv, JNI_VERSION_1_6) != JNI_OK) {
        return _T("jenv Error");
    }
    
    wxCharBuffer p1b = parm1.ToUTF8();
    jstring p1 = (jenv)->NewStringUTF(p1b.data());
    
    wxCharBuffer p2b = parm2.ToUTF8();
    jstring p2 = (jenv)->NewStringUTF(p2b.data());
    
    wxCharBuffer p3b = parm3.ToUTF8();
    jstring p3 = (jenv)->NewStringUTF(p3b.data());
    
    wxCharBuffer p4b = parm4.ToUTF8();
    jstring p4 = (jenv)->NewStringUTF(p4b.data());
    
    QAndroidJniObject data = activity.callObjectMethod(method, "(Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;)Ljava/lang/String;",
                                                       p1, p2, p3, p4);
    (jenv)->DeleteLocalRef(p1);
    (jenv)->DeleteLocalRef(p2);
    (jenv)->DeleteLocalRef(p3);
    (jenv)->DeleteLocalRef(p4);
    
    if(CheckPendingJNIException())
        return _T("NOK");
    
    //qDebug() << "Back from method_s4s";
        
        jstring s = data.object<jstring>();
        
        if( (jenv)->GetStringLength( s )){
            const char *ret_string = (jenv)->GetStringUTFChars(s, NULL);
            return_string = wxString(ret_string, wxConvUTF8);
        }
        
        return return_string;
        
}

#endif
#endif