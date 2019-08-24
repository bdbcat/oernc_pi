/******************************************************************************
 *
 * Project:  oesenc_pi
 * Purpose:  oesenc_pi Plugin core
 * Author:   David Register
 *
 ***************************************************************************
 *   Copyright (C) 2016 by David S. Register   *
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
#include "wx/filename.h"

#ifdef __MSVC__
#include <windows.h>
#include <Shlobj.h>
#endif


#include "ocpn_plugin.h"


extern wxString g_server_bin;


#ifdef __OCPN__ANDROID__
void androidGetDeviceName()
{
    if(!g_deviceInfo.Length())
        g_deviceInfo = callActivityMethod_vs("getDeviceInfo");
    
    wxStringTokenizer tkz(g_deviceInfo, _T("\n"));
    while( tkz.HasMoreTokens() )
    {
        wxString s1 = tkz.GetNextToken();
        if(wxNOT_FOUND != s1.Find(_T("Device"))){
            int a = s1.Find(_T(":"));
            if(wxNOT_FOUND != a){
                wxString b = s1.Mid(a+1).Trim(true).Trim(false);
                g_systemName = b;
            }
        }
    }
    
}
#endif

bool IsDongleAvailable()
{
    wxString cmd = g_server_bin;
    cmd += _T(" -s ");                  // Available?

    wxArrayString ret_array, err_array;      
    wxExecute(cmd, ret_array, err_array );
            
    for(unsigned int i=0 ; i < ret_array.GetCount() ; i++){
        wxString line = ret_array[i];
        if(line.IsSameAs(_T("1")))
            return true;
        if(line.IsSameAs(_T("0")))
            return false;
    }

    // Show error in log
    wxLogMessage(_T("oeaserverd execution error:"));
    for(unsigned int i=0 ; i < err_array.GetCount() ; i++){
        wxString line = err_array[i];
        wxLogMessage(line);
    }

    g_server_bin.Clear();
    
    return false;
}

unsigned int GetDongleSN()
{
    unsigned int rv = 0;
    
    wxString cmd = g_server_bin;
    cmd += _T(" -t ");                  // SN

    wxArrayString ret_array;      
    wxExecute(cmd, ret_array, ret_array );
            
    for(unsigned int i=0 ; i < ret_array.GetCount() ; i++){
        wxString line = ret_array[i];
        long sn;
        line.ToLong(&sn, 10);
        rv = sn;
    }
    
    return rv;
}
    
wxString GetServerVersionString()
{
    wxString ver;
    
    wxString cmd = g_server_bin;
    cmd += _T(" -a ");                  // Version

    wxArrayString ret_array;      
    wxExecute(cmd, ret_array, ret_array );
            
    for(unsigned int i=0 ; i < ret_array.GetCount() ; i++){
        wxString line = ret_array[i];
        if(line.Length() > 2){
            ver = line;
            break;
        }
    }
    
    return ver;
}

wxString getFPR( bool bCopyToDesktop, bool &bCopyOK, bool bSGLock)
{
            
            wxString msg1;
            wxString fpr_file;
            wxString fpr_dir = *GetpPrivateApplicationDataLocation(); //GetWritableDocumentsDir();
            
#ifdef __WXMSW__
            
            //  On XP, we simply use the root directory, since any other directory may be hidden
            int major, minor;
            ::wxGetOsVersion( &major, &minor );
            if( (major == 5) && (minor == 1) )
                fpr_dir = _T("C:\\");
#endif        
            
            if( fpr_dir.Last() != wxFileName::GetPathSeparator() )
                fpr_dir += wxFileName::GetPathSeparator();
            
            wxString cmd = g_server_bin;
            if(bSGLock)
                cmd += _T(" -k ");                  // Make SGLock fingerprint
            else
                cmd += _T(" -g ");                  // Make fingerprint
            
#ifndef __WXMSW__
            cmd += _T("\"");
            cmd += fpr_dir;
            
            //cmd += _T("my fpr/");             // testing
            
            //            wxString tst_cedilla = wxString::Format(_T("my fpr copy %cCedilla/"), 0x00E7);       // testing French cedilla
            //            cmd += tst_cedilla;            // testing
            
            cmd += _T("\"");
#else
            cmd += wxString('\"'); 
            cmd += fpr_dir;
            
            //            cmd += _T("my fpr\\");            // testing spaces in path
            
            //            wxString tst_cedilla = wxString::Format(_T("my%c\\"), 0x00E7);       // testing French cedilla
            //            cmd += tst_cedilla;            // testing
#endif            
            wxLogMessage(_T("Create FPR command: ") + cmd);
            
            ::wxBeginBusyCursor();
            
            wxArrayString ret_array;      
            wxExecute(cmd, ret_array, ret_array );
            
            ::wxEndBusyCursor();
            
            bool berr = false;
            for(unsigned int i=0 ; i < ret_array.GetCount() ; i++){
                wxString line = ret_array[i];
                wxLogMessage(line);
                if(line.Upper().Find(_T("ERROR")) != wxNOT_FOUND){
                    berr = true;
                    break;
                }
                if(line.Upper().Find(_T("FPR")) != wxNOT_FOUND){
                    fpr_file = line.AfterFirst(':');
                }
                
            }
            if(!berr){
                if(fpr_file.IsEmpty()){                 // Probably dongle not present
                    fpr_file = _T("DONGLE_NOT_PRESENT");
                    return fpr_file;
                }
            }
            
            
            bool berror = false;
            
            if( bCopyToDesktop && !berr && fpr_file.Length()){
                
                bool bcopy = false;
                wxString sdesktop_path;
                
#ifdef __WXMSW__
                TCHAR desktop_path[MAX_PATH*2] = { 0 };
                bool bpathGood = false;
                HRESULT  hr;
                HANDLE ProcToken = NULL;
                OpenProcessToken( GetCurrentProcess(), TOKEN_READ, &ProcToken );
                
                hr = SHGetFolderPath( NULL,  CSIDL_DESKTOPDIRECTORY, ProcToken, 0, desktop_path);
                if (SUCCEEDED(hr))    
                    bpathGood = true;
                
                CloseHandle( ProcToken );
                
                //                wchar_t *desktop_path = 0;
                //                bool bpathGood = false;
                
                //               if( (major == 5) && (minor == 1) ){             //XP
                //                    if(S_OK == SHGetFolderPath( (HWND)0,  CSIDL_DESKTOPDIRECTORY, NULL, SHGFP_TYPE_CURRENT, desktop_path))
                //                        bpathGood = true;
                
                
                //                 }
                //                 else{
                    //                     if(S_OK == SHGetKnownFolderPath( FOLDERID_Desktop, 0, 0, &desktop_path))
                //                         bpathGood = true;
                //                 }
                
                
                if(bpathGood){
                    
                    char str[128];
                    wcstombs(str, desktop_path, 128);
                    wxString desktop_fpr(str, wxConvAuto());
                    
                    sdesktop_path = desktop_fpr;
                    if( desktop_fpr.Last() != wxFileName::GetPathSeparator() )
                        desktop_fpr += wxFileName::GetPathSeparator();
                    
                    wxFileName fn(fpr_file);
                    wxString desktop_fpr_file = desktop_fpr + fn.GetFullName();
                    
                    
                    wxString exe = _T("xcopy");
                    wxString parms = fpr_file.Trim() + _T(" ") + wxString('\"') + desktop_fpr + wxString('\"');
                    wxLogMessage(_T("FPR copy command: ") + exe + _T(" ") + parms);
                    
                    const wchar_t *wexe = exe.wc_str(wxConvUTF8);
                    const wchar_t *wparms = parms.wc_str(wxConvUTF8);
                    
                    if( (major == 5) && (minor == 1) ){             //XP
                        // For some reason, this does not work...
                        //8:43:13 PM: Error: Failed to copy the file 'C:\oc01W_1481247791.fpr' to '"C:\Documents and Settings\dsr\Desktop\oc01W_1481247791.fpr"'
                        //                (error 123: the filename, directory name, or volume label syntax is incorrect.)
                        //8:43:15 PM: oesenc fpr file created as: C:\oc01W_1481247791.fpr
                        
                        bcopy = wxCopyFile(fpr_file.Trim(false), _T("\"") + desktop_fpr_file + _T("\""));
                    }
                    else{
                        ::wxBeginBusyCursor();
                        
                        // Launch oeserverd as admin
                        SHELLEXECUTEINFO sei = { sizeof(sei) };
                        sei.lpVerb = L"runas";
                        sei.lpFile = wexe;
                        sei.hwnd = NULL;
                        sei.lpParameters = wparms;
                        sei.nShow = SW_SHOWMINIMIZED;
                        sei.fMask = SEE_MASK_NOASYNC;
                        
                        if (!ShellExecuteEx(&sei))
                        {
                            DWORD dwError = GetLastError();
                            if (dwError == ERROR_CANCELLED)
                            {
                                // The user refused to allow privileges elevation.
                                OCPNMessageBox_PlugIn(NULL, _("Administrator priveleges are required to copy fpr.\n  Please try again...."), _("oeRNC_pi Message"), wxOK);
                                berror = true;
                            }
                        }
                        else
                            bcopy = true;
                        
                        ::wxEndBusyCursor();
                        
                    }  
                }
#endif            // MSW

#ifdef __WXOSX__
                wxFileName fn(fpr_file);
                wxString desktop_fpr_path = ::wxGetHomeDir() + wxFileName::GetPathSeparator() +
                _T("Desktop") + wxFileName::GetPathSeparator() + fn.GetFullName();
                
                bcopy =  ::wxCopyFile(fpr_file.Trim(false), desktop_fpr_path);
                sdesktop_path = desktop_fpr_path;
                msg1 += _T("\n\n OSX ");
#endif
                
                
                wxLogMessage(_T("oeRNC fpr file created as: ") + fpr_file);
                if(bCopyToDesktop && bcopy)
                    wxLogMessage(_T("oeRNC fpr file created in desktop folder: ") + sdesktop_path);
                
                if(bcopy)
                    bCopyOK = true;
        }
        else if(berr){
            wxLogMessage(_T("oernc_pi: oeaserverd results:"));
            for(unsigned int i=0 ; i < ret_array.GetCount() ; i++){
                wxString line = ret_array[i];
                wxLogMessage( line );
            }
            berror = true;
        }
        
        if(berror)
            return _T("");
        else
            return fpr_file;
        
}

