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

#include <iostream>
#include <fstream>
#include <wx/fileconf.h>
#include <wx/uri.h>
#include "wx/tokenzr.h"
#include <wx/dir.h>
#include "ochartShop.h"
#include "ocpn_plugin.h"
#include "wxcurl/wx/curl/http.h"
#include "wxcurl/wx/curl/thread.h"
#include <tinyxml.h>
#include "wx/wfstream.h"
#include <wx/zipstrm.h>
#include <memory>
#include "fpr.h"
#include "sha256.h"

#include <wx/arrimpl.cpp> 
WX_DEFINE_OBJARRAY(ArrayOfCharts);
WX_DEFINE_OBJARRAY(ArrayOfChartPanels);

//  Static variables
ArrayOfCharts g_ChartArray;
std::vector<itemChart *> ChartVector;

wxString userURL(_T("https://o-charts.org/shop/index.php"));
wxString adminURL(_T("http://test.o-charts.org/shop/index.php"));
int g_timeout_secs = 5;

wxArrayString g_systemNameChoiceArray;
wxArrayString g_systemNameServerArray;
wxArrayString g_systemNameDisabledArray;
wxString g_lastSlotUUID;

extern int g_admin;

wxString g_systemName;
wxString g_loginKey;
wxString g_loginUser;
wxString g_PrivateDataDir;
wxString g_debugShop;

shopPanel *g_shopPanel;
OESENC_CURL_EvtHandler *g_CurlEventHandler;
wxCurlDownloadThread *g_curlDownloadThread;
wxFFileOutputStream *downloadOutStream;
bool g_chartListUpdatedOK;
wxString g_statusOverride;
wxString g_lastInstallDir;
wxString g_LastErrorMessage;

unsigned int    g_dongleSN;
wxString        g_dongleName;

itemSlot        *gtargetSlot;
itemChart       *gtargetChart;

InProgressIndicator *g_ipGauge;

WX_DECLARE_STRING_HASH_MAP( wxString, OKeyHash );
extern OKeyHash keyMapDongle;
extern OKeyHash keyMapSystem;

#define ID_CMD_BUTTON_INSTALL 7783
#define ID_CMD_BUTTON_INSTALL_CHAIN 7784


// Private class implementations

size_t wxcurl_string_write_UTF8(void* ptr, size_t size, size_t nmemb, void* pcharbuf)
{
    size_t iRealSize = size * nmemb;
    wxCharBuffer* pStr = (wxCharBuffer*) pcharbuf;
    
//     if(pStr)
//     {
//         wxString str = wxString(*pStr, wxConvUTF8) + wxString((const char*)ptr, wxConvUTF8);
//         *pStr = str.mb_str();
//     }
 
    if(pStr)
    {
#ifdef __WXMSW__        
        wxString str1a = wxString(*pStr);
        wxString str2 = wxString((const char*)ptr, wxConvUTF8, iRealSize);
        *pStr = (str1a + str2).mb_str();
#else        
        wxString str = wxString(*pStr, wxConvUTF8) + wxString((const char*)ptr, wxConvUTF8, iRealSize);
        *pStr = str.mb_str(wxConvUTF8);
#endif        
    }
 
    return iRealSize;
}

static int xferinfo(void *p,
                    curl_off_t dltotal, curl_off_t dlnow,
                    curl_off_t ultotal, curl_off_t ulnow)
{
    if(g_ipGauge){
        g_ipGauge->Pulse();
        wxYieldIfNeeded();
    }

    return 0;
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
        
        curl_easy_setopt(m_pCURL, CURLOPT_XFERINFOFUNCTION, xferinfo);
        curl_easy_setopt(m_pCURL, CURLOPT_NOPROGRESS, 0L);
        
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
#ifndef OCPN_ARMHF
    wxString s = wxString((const char *)m_szResponseBody, wxConvLibc);
    return std::string(s.mb_str());
    
#else    
    return std::string((const char *)m_szResponseBody);
#endif
    
}


//    ChartSetData()
//------------------------------------------------------------------------------------------
ChartSetData::ChartSetData( std::string fileXML)
{
    // Open and parse the given file
    FILE *iFile = fopen(fileXML.c_str(), "rb");
   
    if (iFile <= 0)
        return;            // file error
        
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
    if(!root){
        free(iText);
        return;                              // undetermined error??
    }

    wxString rootName = wxString::FromUTF8( root->Value() );
    if(rootName.IsSameAs(_T("chartList"))){
            
        TiXmlNode *child;
        for ( child = root->FirstChild(); child != 0; child = child->NextSibling()){
            itemChartData *cdata = new itemChartData();
            chartList.push_back(cdata);
            
            wxString s = wxString::FromUTF8(child->Value());  //chart
            
            TiXmlNode *childChart = child->FirstChild();
            for ( childChart = child->FirstChild(); childChart!= 0; childChart = childChart->NextSibling()){
                const char *chartVal =  childChart->Value();
                
/*                
    <Name>Kemer Turkiz Marina</Name>
    <ID>G40-E-2014</ID>
    <SE></SE>
    <RE>01</RE>
    <ED>2019-06-03</ED>
    <Scale>17500</Scale>
*/
                            
                if(!strcmp(chartVal, "Name")){
                    TiXmlNode *childVal = childChart->FirstChild();
                    if(childVal)
                        cdata->Name = childVal->Value();
                }
                else if(!strcmp(chartVal, "ID")){
                    TiXmlNode *childVal = childChart->FirstChild();
                    if(childVal)
                        cdata->ID = childVal->Value();
                }
                else if(!strcmp(chartVal, "SE")){
                    TiXmlNode *childVal = childChart->FirstChild();
                    if(childVal)
                        cdata->SE = childVal->Value();                                
                }
                else if(!strcmp(chartVal, "RE")){
                    TiXmlNode *childVal = childChart->FirstChild();
                    if(childVal)
                        cdata->RE = childVal->Value();
                }
                else if(!strcmp(chartVal, "ED")){
                    TiXmlNode *childVal = childChart->FirstChild();
                    if(childVal)
                        cdata->ED = childVal->Value();
                }
                else if(!strcmp(chartVal, "Scale")){
                    TiXmlNode *childVal = childChart->FirstChild();
                    if(childVal)
                        cdata->Scale = childVal->Value();
                }
            }
        }
        
        free( iText );
        return;
    }           

    free( iText );
    return;

}

bool ChartSetData::RemoveChart( std::string fileNameKap )
{
    size_t nl = fileNameKap.find(".kap");
    std::string search;
    if(nl != std::string::npos)
        search = fileNameKap.substr(0, nl);
    else{
        nl = fileNameKap.find(".KAP");
        if(nl != std::string::npos)
            search = fileNameKap.substr(0, nl);
    }

        // Search for the chart

    for(unsigned int i=0 ; i < chartList.size() ; i++){
        itemChartData *pd = chartList[i];
        if(!search.compare(pd->ID)){
            chartList.erase(chartList.begin()+i);
            delete pd;
            return true;
        }
    }
    
    return false;
}

bool ChartSetData::AddChart(itemChartData *cdata){

    //Search for an existing entry, by ID

    itemChartData *target = NULL;
    for(unsigned int i=0 ; i < chartList.size() ; i++){
        itemChartData *pd = chartList[i];
        if(!pd->ID.compare(cdata->ID)){
            target = pd;
            break;
        }
    }
    
    // If no current chart item found, create one and add it to the vector
    if(!target){
        target = new itemChartData;
        chartList.push_back(target);
    }
    
    target->Name = cdata->Name;
    target->ID = cdata->ID;
    target->SE = cdata->SE;
    target->RE = cdata->RE;
    target->ED = cdata->ED;
    target->Scale = cdata->Scale;
        
    return true;
    
}

bool ChartSetData::WriteFile( std::string fileName)
{
    TiXmlDocument doc;  
    TiXmlDeclaration* decl = new TiXmlDeclaration( "1.0", "", "" );  
    doc.LinkEndChild( decl );  
 
    TiXmlElement * root = new TiXmlElement( "ChartList" );  
    doc.LinkEndChild( root );  
    root->SetAttribute("version", "1.0");
    root->SetAttribute("creator", "OpenCPN");
    root->SetAttribute("xmlns:xsi", "http://www.w3.org/2001/XMLSchema-instance");
    root->SetAttribute("xmlns:opencpn", "http://www.opencpn.org");

    for(size_t i=0 ; i < chartList.size() ;i++){
        TiXmlElement * chart = new TiXmlElement( "Chart" );  
        root->LinkEndChild( chart );  
    
        TiXmlElement *item = new TiXmlElement( "Name" );  
        item->LinkEndChild( new TiXmlText( chartList[i]->Name.c_str()) );  
        chart->LinkEndChild( item );  

        item = new TiXmlElement( "ID" );  
        item->LinkEndChild( new TiXmlText( chartList[i]->ID.c_str()) );  
        chart->LinkEndChild( item );  
        
        item = new TiXmlElement( "SE" );  
        item->LinkEndChild( new TiXmlText( chartList[i]->SE.c_str()) );  
        chart->LinkEndChild( item );  
        
        item = new TiXmlElement( "RE" );  
        item->LinkEndChild( new TiXmlText( chartList[i]->RE.c_str()) );  
        chart->LinkEndChild( item );  
        
        item = new TiXmlElement( "ED" );  
        item->LinkEndChild( new TiXmlText( chartList[i]->ED.c_str()) );  
        chart->LinkEndChild( item );  

        item = new TiXmlElement( "Scale" );  
        item->LinkEndChild( new TiXmlText( chartList[i]->Scale.c_str()) );  
        chart->LinkEndChild( item );  

    }
    
    return(doc.SaveFile( fileName.c_str() ));  
    
        
/*
<?xml version="1.0"?>
<chartList version="1.0" creator="OpenCPN" xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance" xmlns:opencpn="http://www.opencpn.org">
  <Chart>
    <Name>Chart AB 16 Treasure Cay</Name>
    <ID>AB16</ID>
    <SE>2017</SE>
    <RE>01</RE>
    <ED>2019-04-22</ED>
    <Scale>20000</Scale>
  </Chart>
*/

}



//    ChartSetKeys()
//------------------------------------------------------------------------------------------
ChartSetKeys::ChartSetKeys( std::string fileXML)
{
    m_bOK = Load( fileXML );
    
}

bool ChartSetKeys::Load( std::string fileXML)
{
    // Open and parse the given file
    FILE *iFile = fopen(fileXML.c_str(), "rb");
   
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
    if(!root){
        free(iText);
        return false;                              // undetermined error??
    }

    wxString rootName = wxString::FromUTF8( root->Value() );
    if(rootName.IsSameAs(_T("keyList"))){
            
        TiXmlNode *child;
        for ( child = root->FirstChild(); child != 0; child = child->NextSibling()){

            itemChartDataKeys *cdata = new itemChartDataKeys();
            chartList.push_back(cdata);
           
            TiXmlNode *childChart = child->FirstChild();
            for ( childChart = child->FirstChild(); childChart!= 0; childChart = childChart->NextSibling()){
                const char *chartVal =  childChart->Value();
  
/*
    <Name>Chart AB 6 Walkers &amp; Grand Cays</Name>
    <FileName>AB6</FileName>
    <ID>AB6</ID>
    <RInstallKey>AF2A6D76</RInstallKey>
*/
                if(!strcmp(chartVal, "RInstallKey")){
                    TiXmlNode *childVal = childChart->FirstChild();
                    if(childVal)
                        cdata->RIK = childVal->Value();
                }
                if(!strcmp(chartVal, "FileName")){
                    TiXmlNode *childVal = childChart->FirstChild();
                    if(childVal)
                        cdata->fileName = childVal->Value();
                }
                if(!strcmp(chartVal, "Name")){
                    TiXmlNode *childVal = childChart->FirstChild();
                    if(childVal)
                        cdata->Name = childVal->Value();
                }
                if(!strcmp(chartVal, "ID")){
                    TiXmlNode *childVal = childChart->FirstChild();
                    if(childVal)
                        cdata->ID = childVal->Value();
                }

            }
        }
        
        free( iText );
        m_bOK = true;
        return true;
    }           

    free( iText );
    m_bOK = true;
    return true;

}

bool ChartSetKeys::RemoveKey( std::string fileNameKap )
{
    size_t nl = fileNameKap.find(".kap");
    std::string search;
    if(nl != std::string::npos)
        search = fileNameKap.substr(0, nl);
    else{
        nl = fileNameKap.find(".KAP");
        if(nl != std::string::npos)
            search = fileNameKap.substr(0, nl);
    }

        // Search for the chart

    for(unsigned int i=0 ; i < chartList.size() ; i++){
        itemChartDataKeys *pd = chartList[i];
        if(!search.compare(pd->ID)){
            chartList.erase(chartList.begin()+i);
            delete pd;
            return true;
        }
    }
    
    return false;
}

bool ChartSetKeys::AddKey(itemChartDataKeys *kdata)
{
    //Search for an existing entry, by ID

    itemChartDataKeys *target = NULL;
    for(unsigned int i=0 ; i < chartList.size() ; i++){
        itemChartDataKeys *pd = chartList[i];
        if(!pd->ID.compare(kdata->ID)){
            target = pd;
            break;
        }
    }
    
    // If no current chart item found, create one and add it to the vector
    if(!target){
        target = new itemChartDataKeys;
        chartList.push_back(target);
    }
    
    target->Name = kdata->Name;
    target->ID = kdata->ID;
    target->fileName = kdata->fileName;
    target->RIK = kdata->RIK;
        
    return true;

}

bool ChartSetKeys::WriteFile( std::string fileName)
{
    TiXmlDocument doc;  
    TiXmlDeclaration* decl = new TiXmlDeclaration( "1.0", "", "" );  
    doc.LinkEndChild( decl );  
 
    TiXmlElement * root = new TiXmlElement( "keyList" );  
    doc.LinkEndChild( root );  
    root->SetAttribute("version", "1.0");
    root->SetAttribute("creator", "OpenCPN");
    root->SetAttribute("xmlns:xsi", "http://www.w3.org/2001/XMLSchema-instance");
    root->SetAttribute("xmlns:opencpn", "http://www.opencpn.org");

    for(size_t i=0 ; i < chartList.size() ;i++){
        TiXmlElement * chart = new TiXmlElement( "Chart" );  
        root->LinkEndChild( chart );  
    
        TiXmlElement *item = new TiXmlElement( "Name" );  
        item->LinkEndChild( new TiXmlText( chartList[i]->Name.c_str()) );  
        chart->LinkEndChild( item );  

        item = new TiXmlElement( "FileName" );  
        item->LinkEndChild( new TiXmlText( chartList[i]->fileName.c_str()) );  
        chart->LinkEndChild( item );  
        
        item = new TiXmlElement( "ID" );  
        item->LinkEndChild( new TiXmlText( chartList[i]->ID.c_str()) );  
        chart->LinkEndChild( item );  
        
        item = new TiXmlElement( "RInstallKey" );  
        item->LinkEndChild( new TiXmlText( chartList[i]->RIK.c_str()) );  
        chart->LinkEndChild( item );  
    }
    
/*
    <Chart>
    <Name>Chart AB 6 Walkers &amp; Grand Cays</Name>
    <FileName>AB6</FileName>
    <ID>AB6</ID>
    <RInstallKey>AF2A6D</RInstallKey>
    </Chart>
*/    
    return(doc.SaveFile( fileName.c_str() ));  
    
}

// itemChart
//------------------------------------------------------------------------------------------

void itemChart::Update(itemChart *other)
{
    orderRef = other->orderRef;
    purchaseDate = other->purchaseDate;
    expDate = other->expDate;
    chartName = other->chartName;
    chartID = other->chartID;
    serverChartEdition = other->serverChartEdition;
    editionDate = other->editionDate;
    thumbLink = other->thumbLink;

    maxSlots = other->maxSlots;
    bExpired = other->bExpired;
    
    baseChartListArray.Clear();
    for(unsigned int i = 0 ; i < other->baseChartListArray.GetCount() ; i++)
        baseChartListArray.Add(other->baseChartListArray.Item(i));

    updateChartListArray.Clear();
    for(unsigned int i = 0 ; i < other->updateChartListArray.GetCount() ; i++)
        updateChartListArray.Add(other->baseChartListArray.Item(i));
    
    std::vector<itemQuantity> quantityListTemp;

    for(unsigned int i = 0 ; i < other->quantityList.size() ; i++){
        
        itemQuantity Qty;
        Qty.quantityId = other->quantityList[i].quantityId;
        
        for(unsigned int j = 0 ; j < other->quantityList[i].slotList.size() ; j++){
            itemSlot *slot = GetSlotPtr( wxString(other->quantityList[i].slotList[j]->slotUuid.c_str()) );
            if(!slot)
                slot = new itemSlot;
            
            slot->slotUuid = other->quantityList[i].slotList[j]->slotUuid;
            slot->assignedSystemName = other->quantityList[i].slotList[j]->assignedSystemName;
            if(!slot->lastRequested.size())
                slot->lastRequested = other->quantityList[i].slotList[j]->lastRequested;
            if(!slot->installLocation.size())
                slot->installLocation = other->quantityList[i].slotList[j]->installLocation;
            
            Qty.slotList.push_back(slot);
        }
        
        quantityListTemp.push_back(Qty);
    }

    
    quantityList.clear();
    for(unsigned int i = 0 ; i < quantityListTemp.size() ; i++){
        itemQuantity Qty = quantityListTemp[i];
        quantityList.push_back(Qty);
    }
        

    
}


wxBitmap& itemChart::GetChartThumbnail(int size)
{
    if(!m_ChartImage.IsOk()){
        // Look for cached copy
        wxString fileKey = _T("ChartImage-");
        fileKey += chartID;
        fileKey += _T(".jpg");
 
        wxString file = g_PrivateDataDir + fileKey;
        if(::wxFileExists(file)){
            m_ChartImage = wxImage( file, wxBITMAP_TYPE_ANY);
        }
        else{
            if(g_chartListUpdatedOK && thumbLink.length()){  // Do not access network until after first "getList"
                wxCurlHTTP get;
                get.SetOpt(CURLOPT_TIMEOUT, g_timeout_secs);
                bool getResult = get.Get(file, wxString(thumbLink));

            // get the response code of the server
                int iResponseCode;
                get.GetInfo(CURLINFO_RESPONSE_CODE, &iResponseCode);
            
                if(iResponseCode == 200){
                    if(::wxFileExists(file)){
                        m_ChartImage = wxImage( file, wxBITMAP_TYPE_ANY);
                    }
                }
            }
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
        wxImage img(size, size);
        unsigned char *data = img.GetData();
        for(int i=0 ; i < size * size * 3 ; i++)
            data[i] = 200;
        
        m_bm = wxBitmap(img);   // Grey bitmap
        return m_bm;
    }
    
}

bool itemChart::isChartsetAssignedToSystemKey(wxString key)
{
    if(!key.Length())
        return false;
    for(unsigned int i=0 ; i < quantityList.size() ; i++){
        itemQuantity Qty = quantityList[i];
        for(unsigned int j = 0 ; j < Qty.slotList.size() ; j++){
            itemSlot *slot = Qty.slotList[j];
            if(!strcmp(key.mb_str(), slot->assignedSystemName.c_str())){
                return true;
            }
        }
    }
    
    return false;
    
}


int itemChart::getChartAssignmentCount()
{
    int rv = 0;
    for(unsigned int i=0 ; i < quantityList.size() ; i++){
        itemQuantity Qty = quantityList[i];
        for(unsigned int j = 0 ; j < Qty.slotList.size() ; j++){
            itemSlot *slot = Qty.slotList[j];
            if(strlen(slot->slotUuid.c_str())){
                rv++;
            }
        }
    }
    return rv;
}

bool itemChart::isUUIDAssigned( wxString UUID)
{
    for(unsigned int i=0 ; i < quantityList.size() ; i++){
        itemQuantity Qty = quantityList[i];
        for(unsigned int j = 0 ; j < Qty.slotList.size() ; j++){
            itemSlot *slot = Qty.slotList[j];
            if(!strcmp(slot->slotUuid.c_str(), UUID.mb_str())){
                return true;
            }
        }
    }
    return false;
}

itemSlot *itemChart::GetSlotPtr( wxString UUID )
{
    for(unsigned int i=0 ; i < quantityList.size() ; i++){
        itemQuantity Qty = quantityList[i];
        for(unsigned int j = 0 ; j < Qty.slotList.size() ; j++){
            itemSlot *slot = Qty.slotList[j];
            if(!strcmp(slot->slotUuid.c_str(), UUID.mb_str())){
                return slot;
            }
        }
    }
    return NULL;
 
}


bool itemChart::isChartsetFullyAssigned()
{
    
/*    if (statusID0.IsSameAs("unassigned") || !statusID0.Len())
        return false;
    
    if (statusID1.IsSameAs("unassigned") || !statusID1.Len())
        return false;
 */   
    return false;
}

bool itemChart::isChartsetExpired()
{
    
    return bExpired;
}

bool itemChart::isChartsetAssignedToAnyDongle() {
    
    int tmpQ;
    if(GetSlotAssignedToInstalledDongle( tmpQ ) >= 0)
        return true;
    return false;
}

   
  

bool itemChart::isChartsetDontShow()
{
    if(isChartsetFullyAssigned() && !isChartsetAssignedToSystemKey(g_systemName))
        return true;
    
    else if(isChartsetExpired() && !isChartsetAssignedToSystemKey(g_systemName))
        return true;
    
    else
        return false;
}
    
bool itemChart::isChartsetShow()
{
    return true;
#if 0    
    if(isChartsetAssignedToMe(g_systemName))
        return true;
    
    if(!isChartsetFullyAssigned())
        return true;

    if(isChartsetAssignedToAnyDongle())
        return true;

    return false;
#endif    
}

int itemChart::GetSlotAssignedToInstalledDongle( int &qId )
{
    if(!g_dongleName.Length())
        return (-1);
    
    for(unsigned int i=0 ; i < quantityList.size() ; i++){
        itemQuantity Qty = quantityList[i];
        for(unsigned int j = 0 ; j < Qty.slotList.size() ; j++){
            itemSlot *slot = Qty.slotList[j];
            if(!strcmp(g_dongleName.mb_str(), slot->assignedSystemName.c_str())){
                qId = Qty.quantityId;
                return j;
            }
        }
    }
    
    return (-1);
}
            
int itemChart::GetSlotAssignedToSystem( int &qId )
{
    for(unsigned int i=0 ; i < quantityList.size() ; i++){
        itemQuantity Qty = quantityList[i];
        for(unsigned int j = 0 ; j < Qty.slotList.size() ; j++){
            itemSlot *slot = Qty.slotList[j];
            if(!strcmp(g_systemName.mb_str(), slot->assignedSystemName.c_str())){
                qId = Qty.quantityId;
                return j;
            }
        }
    }
    
    return (-1);
}
            
int itemChart::FindQuantityIndex( int nqtyID){
    for(unsigned int i=0 ; i < quantityList.size() ; i++){
        itemQuantity Qty = quantityList[i];
        if(Qty.quantityId == nqtyID)
            return i;
    }
    
    return -1;
}

itemSlot *itemChart::GetActiveSlot(){
    itemSlot *rv = NULL;
    if((m_activeQtyID < 0) || (m_assignedSlotIndex < 0))
        return rv;
    
    int qtyIndex = FindQuantityIndex( m_activeQtyID );
    return quantityList[qtyIndex].slotList[m_assignedSlotIndex];
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
    
    if(!isChartsetAssignedToSystemKey( g_systemName )){
        if(!g_dongleName.Len()){
            if(!isChartsetAssignedToAnyDongle()){
                m_status = STAT_PURCHASED;
                return m_status;
            }
        }
        else{
            if(!isChartsetAssignedToAnyDongle()){
                m_status = STAT_PURCHASED;
                return m_status;
            }
        }
    }
    
    // We know that chart is assigned to me, so identify the slot
    m_assignedSlotIndex = -1;
    int tmpQtyID = -1;
    
    int slotIndex = GetSlotAssignedToInstalledDongle( tmpQtyID );
    if(slotIndex >= 0){
        m_assignedSlotIndex = slotIndex;
        m_activeQtyID = tmpQtyID;
    }
    else{
        slotIndex = GetSlotAssignedToSystem( tmpQtyID );
        if(slotIndex >= 0){
           m_assignedSlotIndex = slotIndex;
           m_activeQtyID = tmpQtyID;
        }
    }
    
    if(m_assignedSlotIndex < 0)
        return m_status;

    // From here, chart may be:
    // a.  Requestable
    // b.  Preparing
    // c.  Ready for download
    // d.  Downloading now.
    
    m_status = STAT_REQUESTABLE;

    
    //  Now check for installation state

    // Check for update
    if(installedChartEdition.size()){
        if(GetServerEditionInt() > GetInstalledEditionInt())
            m_status = STAT_STALE;
        else
            m_status = STAT_CURRENT;
    }
    
    return m_status;

#if 0    
    
    wxString cStat = statusID0;
    int slot = 0;
    if(isChartsetAssignedToAnyDongle()){
        if(isSlotAssignedToMyDongle( 1 )){
            cStat = statusID1;
            slot = 1;
        }
    }
    
    else{
        if(sysID1.IsSameAs(g_systemName)){
            cStat = statusID1;
            slot = 1;
        }
    }
        
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
        
        if(slot == 0){
            if(  (installLocation0.Length() > 0) && (installedFileDownloadPath0.Length() > 0) ){
                m_status = STAT_CURRENT;
                if(!installedEdition0.IsSameAs(currentChartEdition)){
                    m_status = STAT_STALE;
                }
            }
        }
        else if(slot == 1){
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
            
        case STAT_PREPARING:
            sret = _("Preparing your chartset.");
            break;
            
        case STAT_READY_DOWNLOAD:
            sret = _("Ready for download.");
            break;
            
        case STAT_NEED_REFRESH:
            sret = _("Please update Chart List.");
            break;

        case STAT_REQUESTABLE:
            sret = _("Ready for Download Request.");
            break;
            
        default:
            break;
    }
    
    return sret;
    

}

wxString itemChart::getKeytypeString( std::string slotUUID ){
    
    // Find the slot
    for(unsigned int i=0 ; i < quantityList.size() ; i++){
        itemQuantity Qty = quantityList[i];
        for(unsigned int j = 0 ; j < Qty.slotList.size() ; j++){
            itemSlot *slot = Qty.slotList[j];
            if(!strcmp(slotUUID.c_str(), slot->slotUuid.c_str())){
                wxString akey = wxString(slot->assignedSystemName.c_str());
                if(akey.StartsWith("sgl"))
                    return _("USB Key Dongle");
                else
                    return _("System Key");
            }
        }
    }
 
    return _T("");
}


int GetEditionInt(std::string edition)
{
    if(!edition.size())
        return 0;
    
    wxString sed(edition.c_str());
    wxString smaj = sed.BeforeFirst('-');
    wxString smin = sed.AfterFirst('-');

        
    long major = 0;
    smaj.ToLong(&major);
    long minor = 0;
    smin.ToLong(&minor);
    
    return (major * 100) + minor;
}

int itemChart::GetInstalledEditionInt()
{
    return GetEditionInt(installedChartEdition);
}

int itemChart::GetServerEditionInt()
{
    return GetEditionInt(serverChartEdition);
}



// oitemChart
//------------------------------------------------------------------------------------------

oitemChart::oitemChart( wxString &order_ref, wxString &chartid, wxString &quantity) {
    //ident = id;
    orderRef = order_ref;
    chartID = chartid;
    quantityId = quantity;
    m_status = STAT_UNKNOWN;
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

bool oitemChart::isChartsetAssignedToMe(wxString systemName){
    
    // is this chartset assigned to this system?
        if (sysID0.IsSameAs(systemName)) {
            return true;
        }
    
        if (sysID1.IsSameAs(systemName)) {
            return true;
        }
    
    return false;
    
}


bool oitemChart::isChartsetFullyAssigned() {
    
    if (statusID0.IsSameAs("unassigned") || !statusID0.Len())
        return false;
    
    if (statusID1.IsSameAs("unassigned") || !statusID1.Len())
        return false;
    
    return true;
}

bool oitemChart::isChartsetExpired() {
    
    bool bExp = false;
    if (statusID0.IsSameAs("expired") || statusID1.IsSameAs("expired")) {
        bExp = true;
    }
    return bExp;
}

bool oitemChart::isChartsetAssignedToAnyDongle() {
    
    if(isSlotAssignedToAnyDongle(0))
        return true;
    if(isSlotAssignedToAnyDongle(1))
        return true;
    return false;
}

bool oitemChart::isSlotAssignedToAnyDongle( int slot ) {
    long tl;
    if( slot == 0 ){
        if (sysID0.StartsWith("sgl")){
            if(sysID0.Mid(4).ToLong(&tl, 16))
                return true;
        }
    }
    else{
        if (sysID1.StartsWith("sgl")){
            if(sysID1.Mid(4).ToLong(&tl, 16))
                return true;
        }
    }
    return false;
}   
    
bool oitemChart::isSlotAssignedToMyDongle( int slot ) {
    long tl;
    if( slot == 0 ){
        if (sysID0.StartsWith("sgl")){
            if(sysID0.Mid(4).ToLong(&tl, 16)){
                if(tl == g_dongleSN)
                    return true;
            }
        }
    }
    else{
        if (sysID1.StartsWith("sgl")){
            if(sysID1.Mid(4).ToLong(&tl, 16)){
                if(tl == g_dongleSN)
                    return true;
            }
        }
    }
    return false;
}   
    

bool oitemChart::isChartsetDontShow()
{
    if(isChartsetFullyAssigned() && !isChartsetAssignedToMe(g_systemName))
        return true;
    
    else if(isChartsetExpired() && !isChartsetAssignedToMe(g_systemName))
        return true;
    
    else
        return false;
}
    
bool oitemChart::isChartsetShow()
{
    if(isChartsetAssignedToMe(g_systemName))
        return true;
    
    if(!isChartsetFullyAssigned())
        return true;

    if(isChartsetAssignedToAnyDongle())
        return true;

    return false;
}

    
//  Current status can be one of:
/*
 *      1.  Available for Installation.
 *      2.  Installed, Up-to-date.
 *      3.  Installed, Update available.
 *      4.  Expired.
 */        

int oitemChart::getChartStatus()
{
    if(!g_chartListUpdatedOK){
        m_status = STAT_NEED_REFRESH;
        return m_status;
    }

    if(isChartsetExpired()){
        m_status = STAT_EXPIRED;
        return m_status;
    }
    
    if(!isChartsetAssignedToMe( g_systemName )){
        if(!g_dongleName.Len()){
            if(!isChartsetAssignedToAnyDongle()){
                m_status = STAT_PURCHASED;
                return m_status;
            }
        }
        else{
            if(!isChartsetAssignedToAnyDongle()){
                m_status = STAT_PURCHASED;
                return m_status;
            }
        }
    }
    
    // We know that chart is assigned to me, so one of the sysIDx fields will match
    wxString cStat = statusID0;
    int slot = 0;
    if(isChartsetAssignedToAnyDongle()){
        if(isSlotAssignedToMyDongle( 1 )){
            cStat = statusID1;
            slot = 1;
        }
    }
    
    else{
        if(sysID1.IsSameAs(g_systemName)){
            cStat = statusID1;
            slot = 1;
        }
    }
        
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
        
        if(slot == 0){
            if(  (installLocation0.Length() > 0) && (installedFileDownloadPath0.Length() > 0) ){
                m_status = STAT_CURRENT;
                if(!installedEdition0.IsSameAs(currentChartEdition)){
                    m_status = STAT_STALE;
                }
            }
        }
        else if(slot == 1){
            if(  (installLocation1.Length() > 0) && (installedFileDownloadPath1.Length() > 0) ){
                m_status = STAT_CURRENT;
                if(!installedEdition1.IsSameAs(currentChartEdition)){
                    m_status = STAT_STALE;
                }
                
            }
        }
    }

     
    return m_status;
    
}
wxString oitemChart::getStatusString()
{
    getChartStatus();
    
    wxString sret;
    
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
            
        case STAT_PREPARING:
            sret = _("Preparing your chartset.");
            break;
            
        case STAT_READY_DOWNLOAD:
            sret = _("Ready for download.");
            break;
            
        case STAT_NEED_REFRESH:
            sret = _("Please update Chart List.");
            break;

        case STAT_REQUESTABLE:
            sret = _("Ready for Download Request.");
            break;
            
        default:
            break;
    }
    
    return sret;
    

}

wxString oitemChart::getKeytypeString(){
 
    if(isChartsetAssignedToAnyDongle())
        return _("USB Dongle Key");
    else if(isChartsetAssignedToMe(g_systemName))
        return _("System Key");
    else
        return _T("");
}

wxBitmap& oitemChart::GetChartThumbnail(int size)
{
    if(!m_ChartImage.IsOk()){
        // Look for cached copy
        wxString fileKey = _T("ChartImage-");
        fileKey += chartID;
        fileKey += _T(".jpg");
 
        wxString file = g_PrivateDataDir + fileKey;
        if(::wxFileExists(file)){
            m_ChartImage = wxImage( file, wxBITMAP_TYPE_ANY);
        }
        else{
            if(g_chartListUpdatedOK && thumbnailURL.Length()){  // Do not access network until after first "getList"
                wxCurlHTTP get;
                get.SetOpt(CURLOPT_TIMEOUT, g_timeout_secs);
                bool getResult = get.Get(file, thumbnailURL);

            // get the response code of the server
                int iResponseCode;
                get.GetInfo(CURLINFO_RESPONSE_CODE, &iResponseCode);
            
                if(iResponseCode == 200){
                    if(::wxFileExists(file)){
                        m_ChartImage = wxImage( file, wxBITMAP_TYPE_ANY);
                    }
                }
            }
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
int findOrderRefChartId(wxString &orderRef, wxString &chartId, wxString &quantity)
{
    for(unsigned int i = 0 ; i < g_ChartArray.GetCount() ; i++){
        if(g_ChartArray.Item(i)->orderRef.IsSameAs(orderRef)
            && g_ChartArray.Item(i)->chartID.IsSameAs(chartId) &&
            g_ChartArray.Item(i)->quantityId.IsSameAs(quantity) ){
                return (i);
            }
    }
    return -1;
}

int findOrderRefChartId( std::string orderRef, std::string chartId)
{
    for(unsigned int i = 0 ; i < ChartVector.size() ; i++){
        if(
            (!strcmp(ChartVector[i]->orderRef.c_str(), orderRef.c_str()))
            && (!strcmp(ChartVector[i]->chartID.c_str(), chartId.c_str()))){
            return (i);
        }
    }
    return -1;
}


void loadShopConfig()
{
    //    Get a pointer to the opencpn configuration object
    wxFileConfig *pConf = GetOCPNConfigObject();
    
    if( pConf ) {
        pConf->SetPath( _T("/PlugIns/oernc") );
        
        pConf->Read( _T("systemName"), &g_systemName);
        pConf->Read( _T("loginUser"), &g_loginUser);
        pConf->Read( _T("loginKey"), &g_loginKey);
        pConf->Read( _T("lastInstllDir"), &g_lastInstallDir);
        
        pConf->Read( _T("ADMIN"), &g_admin);
        pConf->Read( _T("DEBUG_SHOP"), &g_debugShop);
        
        // Get the list of charts
        wxArrayString chartIDArray;
        
        pConf->SetPath ( _T ( "/PlugIns/oernc/charts" ) );
        wxString strk;
        wxString kval;
        long dummyval;
        bool bContk = pConf->GetFirstEntry( strk, dummyval );
        while( bContk ) {
            pConf->Read( strk, &kval );
            chartIDArray.Add(kval);
            bContk = pConf->GetNextEntry( strk, dummyval );

        }
            
        for(unsigned int i=0 ; i < chartIDArray.GetCount() ; i++){
            wxString chartID = chartIDArray[i];
            pConf->SetPath ( _T ( "/PlugIns/oernc/charts/" ) + chartID );
            
            wxString orderRef, chartName, installedChartEdition, overrideChartEdition;
            pConf->Read( _T("orderRef"), &orderRef);
            pConf->Read( _T("chartName"), &chartName);
            pConf->Read( _T("installedChartEdition"), &installedChartEdition);
            pConf->Read( _T("overrideChartEdition"), &overrideChartEdition);


            // Add a chart if necessary
            int index = findOrderRefChartId((const char *)orderRef.mb_str(), (const char *)chartID.mb_str());
            itemChart *chart;
            if(index < 0){
                chart = new itemChart;
                chart->orderRef = orderRef.mb_str();
                chart->chartID = chartID.mb_str();
                chart->chartName = chartName.mb_str();
                chart->installedChartEdition = installedChartEdition.mb_str();
                chart->overrideChartEdition = overrideChartEdition.mb_str();
                ChartVector.push_back(chart);
            }
            else
                chart = ChartVector[index];
            
            // Process Slots
            
            pConf->SetPath ( _T ( "/PlugIns/oernc/charts/" ) + chartID + _T("/Slots" ));

            bContk = pConf->GetFirstEntry( strk, dummyval );
            while( bContk ) {
                pConf->Read( strk, &kval );
                if(strk.StartsWith("Slot")){
                    wxStringTokenizer tkz( kval, _T(";") );
                    while(tkz.HasMoreTokens()){
                        wxString sqid = tkz.GetNextToken(); 
                        wxString slotUUID = tkz.GetNextToken();
                        wxString assignedSystemName = tkz.GetNextToken();
                        wxString installLocation = tkz.GetNextToken();
                        
                        // Make a new "quantity" clause if necessary
                        long nqid = -1;
                        
                        if(sqid.ToLong(&nqid)){
                            int qtyIndex = chart->FindQuantityIndex(nqid);
                            if(qtyIndex < 0){
                                itemQuantity new_qty;
                                new_qty.quantityId = nqid;
                                itemSlot *slot = new itemSlot;
                                slot->installLocation = std::string(installLocation.mb_str());
                                slot->assignedSystemName = std::string(assignedSystemName.mb_str());
                                slot->slotUuid = std::string(slotUUID.mb_str());

                                new_qty.slotList.push_back(slot);
                                chart->quantityList.push_back(new_qty);
                            }
                            else{
                                itemQuantity *exist_qty = &(chart->quantityList[qtyIndex]);
                                itemSlot *slot = chart->GetSlotPtr( slotUUID );
                                if(!slot)
                                    slot = new itemSlot;
                                
                                slot->installLocation = std::string(installLocation.mb_str());
                                slot->assignedSystemName = std::string(assignedSystemName.mb_str());
                                slot->slotUuid = std::string(slotUUID.mb_str());

                                exist_qty->slotList.push_back(slot);
                            }
                        }
                    }


                        
                    int yyp = 4;
                    
                }
                bContk = pConf->GetNextEntry( strk, dummyval );

            }
        }
            
            
    }
}

void saveShopConfig()
{
    wxFileConfig *pConf = GetOCPNConfigObject();
        
   if( pConf ) {
      pConf->SetPath( _T("/PlugIns/oernc") );
            
      pConf->Write( _T("systemName"), g_systemName);
      pConf->Write( _T("loginUser"), g_loginUser);
      pConf->Write( _T("loginKey"), g_loginKey);
      pConf->Write( _T("lastInstllDir"), g_lastInstallDir);
      
      pConf->DeleteGroup( _T("/PlugIns/oernc/charts") );
      pConf->SetPath( _T("/PlugIns/oernc/charts") );

     for(unsigned int i = 0 ; i < ChartVector.size() ; i++){
          itemChart *chart = ChartVector[i];
          wxString keyChart;
          keyChart.Printf(_T("Chart%d"), i);
          pConf->Write(keyChart, wxString(chart->chartID));
     }
      
     for(unsigned int i = 0 ; i < ChartVector.size() ; i++){
          itemChart *chart = ChartVector[i];
          wxString chartPath = _T("/PlugIns/oernc/charts/");
          chartPath += wxString(chart->chartID);
          pConf->DeleteGroup( chartPath );
          pConf->SetPath( chartPath );
           
          pConf->Write( _T("chartName"), wxString(chart->chartName) );
          pConf->Write( _T("orderRef"), wxString(chart->orderRef) );
          pConf->Write( _T("installedChartEdition"), wxString(chart->installedChartEdition) );
          if(chart->overrideChartEdition.size())
            pConf->Write( _T("overrideChartEdition"), wxString(chart->overrideChartEdition) );

          wxString slotsPath = chartPath + _T("/Slots");
          pConf->DeleteGroup( slotsPath );
          pConf->SetPath( slotsPath );
         
          for( unsigned int j = 0 ; j < chart->quantityList.size() ; j++){
              int qID = chart->quantityList[j].quantityId;
              if(qID < 0)
                  continue;
              wxString sqid;
              sqid.Printf(_T("%d"), qID);
              
              wxString key = _T("Slot");
              key += sqid;
              itemSlot *slot;
              wxString val;

              for( unsigned int k = 0 ; k < chart->quantityList[j].slotList.size() ; k++){
                  slot = chart->quantityList[j].slotList[k];
                  if( slot->assignedSystemName.size()){
                    val += sqid + _T(";");
                    val += slot->slotUuid + _T(";");
                    val += slot->assignedSystemName + _T(";");
                    val += slot->installLocation + _T(";");
                  }
              }
              if(val.Length())
                  pConf->Write( key, val );
          }
      }
   }
}

            
int checkResult(wxString &result, bool bShowErrorDialog = true)
{
    if(g_shopPanel){
        g_ipGauge->Stop();
    }
    
    long dresult;
    if(result.ToLong(&dresult)){
        if(dresult == 1){
            g_LastErrorMessage.Clear();
            return 0;
        }
        else{
            if(bShowErrorDialog){
                wxString msg = _("o-charts API error code: ");
                wxString msg1;
                msg1.Printf(_T("{%ld}\n\n"), dresult);
                msg += msg1;
                switch(dresult){
                    case 4:
                    case 5:
                        msg += _("Invalid user/email name or password.");
                        break;
                    default:    
                        msg += _("Check your configuration and try again.");
                        break;
                }
                
                OCPNMessageBox_PlugIn(NULL, msg, _("oeSENC_pi Message"), wxOK);
            }
            return dresult;
        }
    }
    else{
        OCPNMessageBox_PlugIn(NULL, result, _("oeRNC_pi Message"), wxOK);
    }
     
    g_LastErrorMessage = result;
     
    return 98;
}

int checkResponseCode(int iResponseCode)
{
    if(iResponseCode != 200){
        wxString msg = _("internet communications error code: ");
        wxString msg1;
        msg1.Printf(_T("{%d}\n "), iResponseCode);
        msg += msg1;
        msg += _("Check your connection and try again.");
        OCPNMessageBox_PlugIn(NULL, msg, _("oeSENC_pi Message"), wxOK);
    }
    
    // The wxCURL library returns "0" as response code,
    // even when it should probably return 404.
    // We will use "99" as a special code here.
    
    if(iResponseCode < 100)
        return 99;
    else
        return iResponseCode;
        
}

int doLogin()
{
    oeSENCLogin login(g_shopPanel);
    login.ShowModal();
    if(!login.GetReturnCode() == 0){
        g_shopPanel->setStatusText( _("Invalid Login."));
        wxYield();
        return 55;
    }
    
    g_loginUser = login.m_UserNameCtl->GetValue();
    wxString pass = login.m_PasswordCtl->GetValue();
    
    wxString url = userURL;
    if(g_admin)
        url = adminURL;
    
    url +=_T("?fc=module&module=occharts&controller=apioernc");
    
    wxString loginParms;
    loginParms += _T("taskId=login");
    loginParms += _T("&username=") + g_loginUser;
    loginParms += _T("&password=") + pass;
    if(g_debugShop.Len())
        loginParms += _T("&debug=") + g_debugShop;
    
    wxCurlHTTPNoZIP post;
    post.SetOpt(CURLOPT_TIMEOUT, g_timeout_secs);
    size_t res = post.Post( loginParms.ToAscii(), loginParms.Len(), url );
    
    // get the response code of the server
    int iResponseCode;
    post.GetInfo(CURLINFO_RESPONSE_CODE, &iResponseCode);
    
    if(iResponseCode == 200){
        TiXmlDocument * doc = new TiXmlDocument();
        const char *rr = doc->Parse( post.GetResponseBody().c_str());
        
        wxString p = wxString(post.GetResponseBody().c_str(), wxConvUTF8);
        wxLogMessage(_T("doLogin results:"));
        wxLogMessage(p);
        
        wxString queryResult;
        wxString loginKey;
        
        if( res )
        {
            TiXmlElement * root = doc->RootElement();
            if(!root){
                wxString r = _T("50");
                checkResult(r);                              // undetermined error??
                return false;
            }
            
            wxString rootName = wxString::FromUTF8( root->Value() );
            TiXmlNode *child;
            for ( child = root->FirstChild(); child != 0; child = child->NextSibling()){
                wxString s = wxString::FromUTF8(child->Value());
                
                if(!strcmp(child->Value(), "result")){
                    TiXmlNode *childResult = child->FirstChild();
                    queryResult =  wxString::FromUTF8(childResult->Value());
                }
                else if(!strcmp(child->Value(), "key")){
                    TiXmlNode *childResult = child->FirstChild();
                    loginKey =  wxString::FromUTF8(childResult->Value());
                }
            }
        }
        
        if(queryResult == _T("1"))
            g_loginKey = loginKey;
        else
            checkResult(queryResult, true);
        
        long dresult;
        if(queryResult.ToLong(&dresult)){
            return dresult;
        }
        else{
            return 53;
        }
    }
    else
        return 54;
    
}

itemChart *FindChartForSlotUUID(wxString UUID)
{
    itemChart *rv = NULL;
    for(unsigned int i=0 ; i < ChartVector.size() ; i++){
        itemChart *chart = ChartVector[i];
        if(chart->isUUIDAssigned(UUID))
            return chart;
    }
 
    return rv;
}

wxString ProcessResponse(std::string body, bool bsubAmpersand)
{
//    std::string db("<?xml version=\"1.0\" encoding=\"utf-8\"?><response><result>1</result><chart> <quantity> <quantityId>1</quantityId> <slot> <slotUuid>123e4567-e89b-12d3-a456-426655440000</slotUuid> <assignedSystemName>mylaptop</assignedSystemName> <lastRequested>2-1</lastRequested>     </slot> <slot> </slot> </quantity> </chart></response>");
//    body = db;
        TiXmlDocument * doc = new TiXmlDocument();
        if(bsubAmpersand){
            std::string oldStr("&");
            std::string newStr("&amp;");
            std::string::size_type pos = 0u;
            while((pos = body.find(oldStr, pos)) != std::string::npos){
                body.replace(pos, oldStr.length(), newStr);
                pos += newStr.length();
            }
        }

        const char *rr = doc->Parse( body.c_str());
    
        //doc->Print();
        
        itemChart *pChart = NULL;

        wxString queryResult;
        wxString chartOrder;
        wxString chartPurchase;
        wxString chartExpiration;
        wxString chartID;
        wxString edition;
        wxString editionDate;
        wxString maxSlots;
        wxString expired;
        wxString chartName;
        wxString chartQuantityID;
        wxString chartSlot;
        wxString chartAssignedSystemName;
        wxString chartLastRequested;
        wxString chartState;
        wxString chartLink;
        wxString chartSize;
        wxString chartThumbURL;
        itemSlot *activeSlot = NULL;

         wxString p = wxString(body.c_str(), wxConvUTF8);
         //  wxMSW does not like trying to format this string containing "%" characters
#ifdef __WXGTK__         
         wxLogMessage(_T("ProcessResponse results:"));
         wxLogMessage(p);
#endif
        
            TiXmlElement * root = doc->RootElement();
            if(!root){
                return _T("57");                              // undetermined error??
            }
            
            wxString rootName = wxString::FromUTF8( root->Value() );
            TiXmlNode *child;
            for ( child = root->FirstChild(); child != 0; child = child->NextSibling()){
                pChart = NULL;
                wxString s = wxString::FromUTF8(child->Value());
                
                if(!strcmp(child->Value(), "result")){
                    TiXmlNode *childResult = child->FirstChild();
                    queryResult =  wxString::FromUTF8(childResult->Value());
                }
                
                else if(!strcmp(child->Value(), "systemName")){
                    TiXmlNode *childsystemName = child->FirstChild();
                    wxString sName =  wxString::FromUTF8(childsystemName->Value());
                    if(g_systemNameChoiceArray.Index(sName) == wxNOT_FOUND)
                        g_systemNameChoiceArray.Add(sName);
                    
                    //  Maintain a separate list of systemNames known to the server
                    if(g_systemNameServerArray.Index(sName) == wxNOT_FOUND)
                        g_systemNameServerArray.Add(sName);
                        
                }
                
                else if(!strcmp(child->Value(), "disabledSystemName")){
                    TiXmlNode *childsystemNameDisabled = child->FirstChild();
                    wxString sName =  wxString::FromUTF8(childsystemNameDisabled->Value());
                    if(g_systemNameDisabledArray.Index(sName) == wxNOT_FOUND)
                        g_systemNameDisabledArray.Add(sName);
                    
                }
                else if(!strcmp(child->Value(), "slotUuid")){
                    TiXmlNode *childslotuuid = child->FirstChild();
                    g_lastSlotUUID =  wxString::FromUTF8(childslotuuid->Value());
 
                    // This is a response from ChartRequest
                    // Find the slot, and clear out the array of itemTaskFileInfo
                    itemChart *pChart = FindChartForSlotUUID(g_lastSlotUUID);
                    if(pChart){
                        itemSlot *pslot = pChart->GetSlotPtr(g_lastSlotUUID);
                        if(pslot){
                            pslot->taskFileList.clear();
                        }
                    }
                }
                
                else if(!strcmp(child->Value(), "file")){
                    // Get a pointer to slot referenced by last UUID seen
                    itemChart *pChart = FindChartForSlotUUID(g_lastSlotUUID);
                    if(pChart){
                        itemSlot *pslot = pChart->GetSlotPtr(g_lastSlotUUID);
                        activeSlot = pslot;
                        if(activeSlot){
                            
                            // Create a new set of task files definitions
                            itemTaskFileInfo *ptfi = new itemTaskFileInfo;
                            activeSlot->taskFileList.push_back(ptfi);
                        
                            TiXmlNode *childFile = child->FirstChild();
                            for ( childFile = child->FirstChild(); childFile!= 0; childFile = childFile->NextSibling()){
                                const char *fileVal =  childFile->Value();
                            
                                if(!strcmp(fileVal, "link")){
                                    TiXmlNode *childVal = childFile->FirstChild();
                                    if(childVal) 
                                        ptfi->link = childVal->Value();
                                }
                                
                                else if(!strcmp(fileVal, "size")){
                                    TiXmlNode *childVal = childFile->FirstChild();
                                    if(childVal) 
                                        ptfi->size = childVal->Value();
                                }

                                else if(!strcmp(fileVal, "sha256")){
                                    TiXmlNode *childVal = childFile->FirstChild();
                                    if(childVal) 
                                        ptfi->sha256 = childVal->Value();
                                }

                                else if(!strcmp(fileVal, "chartKeysLink")){
                                    TiXmlNode *childVal = childFile->FirstChild();
                                    if(childVal) 
                                        ptfi->linkKeys = childVal->Value();
                                }
                                
                                else if(!strcmp(fileVal, "chartKeysSha256")){
                                    TiXmlNode *childVal = childFile->FirstChild();
                                    if(childVal) 
                                        ptfi->sha256Keys = childVal->Value();
                                }

                                else if(!strcmp(fileVal, "editionTarget")){
                                    TiXmlNode *childVal = childFile->FirstChild();
                                    if(childVal) 
                                        ptfi->target = childVal->Value();
                                }

                                else if(!strcmp(fileVal, "editionResult")){
                                    TiXmlNode *childVal = childFile->FirstChild();
                                    if(childVal) 
                                        ptfi->result = childVal->Value();
                                }
                            }
                        }
                    }
                }
                    
                else if(!strcmp(child->Value(), "chart")){
                                    
                    pChart = new itemChart();
                    
                    
                    TiXmlNode *childChart = child->FirstChild();
                    for ( childChart = child->FirstChild(); childChart!= 0; childChart = childChart->NextSibling()){
                        const char *chartVal =  childChart->Value();
  
                        /*
                                    b.0. order: order reference.
                                    b.1. purchase: purchase date YYYY-MM-DD.
                                    b.2. expiration: expiration date YYYY-MM-DD.
                                    b.3. expired: 0 > not expired, 1 > expired.
                                    b.4. thumbLink: link to thumbimage in shop.
                                    b.5. edition: current chart edition on shop. Format: N-N > edition-update.
                                    b.6. editionDate: publication date of current edition YYYY-MM-DD.
                                    b.7. chartName: Name of chartset in user language.
                                    b.8. maxSlots: maximum slots allowed for this chart.
                                b.9. baseChartList (list): list of links to all base ChartList.xml files of the current edition.
                                b.10. updateChartList (list): list of links to all update ChartList.xml files of the current edition.
                                    b.11. chartid: unique chart id on shop.
                        */
                        if(!strcmp(chartVal, "order")){
                            TiXmlNode *childVal = childChart->FirstChild();
                            if(childVal) pChart->orderRef = childVal->Value();
                        }
                        else if(!strcmp(chartVal, "purchase")){
                            TiXmlNode *childVal = childChart->FirstChild();
                            if(childVal) pChart->purchaseDate = childVal->Value();
                        }
                        else if(!strcmp(chartVal, "expiration")){
                            TiXmlNode *childVal = childChart->FirstChild();
                            if(childVal) pChart->expDate = childVal->Value();
                        }
                        else if(!strcmp(chartVal, "chartId")){
                            TiXmlNode *childVal = childChart->FirstChild();
                            if(childVal) pChart->chartID = childVal->Value();
                        }                        
                        else if(!strcmp(chartVal, "thumbLink")){
                            TiXmlNode *childVal = childChart->FirstChild();
                            if(childVal) pChart->thumbLink = childVal->Value();
                        }
                        else if(!strcmp(chartVal, "edition")){
                            TiXmlNode *childVal = childChart->FirstChild();
                            if(childVal) pChart->serverChartEdition = childVal->Value();
                        }
                        else if(!strcmp(chartVal, "editionDate")){
                            TiXmlNode *childVal = childChart->FirstChild();
                            if(childVal) pChart->editionDate = childVal->Value();
                        }
                        else if(!strcmp(chartVal, "chartName")){
                            TiXmlNode *childVal = childChart->FirstChild();
                            if(childVal) pChart->chartName = childVal->Value();
                        }
                        else if(!strcmp(chartVal, "expired")){
                            TiXmlNode *childVal = childChart->FirstChild();
                            if(childVal){
                                if(!strcmp(childVal->Value(), "1"))
                                    pChart->bExpired = true;
                                else
                                    pChart->bExpired =false;
                            }
                        }
                        else if(!strcmp(chartVal, "maxSlots")){
                            TiXmlNode *childVal = childChart->FirstChild();
                            if(childVal){
                                wxString mxSlots = wxString::FromUTF8(childVal->Value());
                                long slots;
                                mxSlots.ToLong(&slots);
                                pChart->maxSlots = slots;
                            }
                        }

                        else if(!strcmp(chartVal, "baseChartList")){
                            wxString URL;
                            TiXmlNode *childVal = childChart->FirstChild();
                            if(childVal){
                                URL = wxString::FromUTF8(childVal->Value());
                                pChart->baseChartListArray.Add( URL );
                            }
                        }
                        else if(!strcmp(chartVal, "updateChartList")){
                            wxString URL;
                            TiXmlNode *childVal = childChart->FirstChild();
                            if(childVal){
                                URL = wxString::FromUTF8(childVal->Value());
                                pChart->updateChartListArray.Add( URL );
                            }
                        }

                        else if(!strcmp(chartVal, "quantity")){
                            itemQuantity Qty;
                            TiXmlNode *childQuantityChild;
                            for ( childQuantityChild = childChart->FirstChild(); childQuantityChild!= 0; childQuantityChild = childQuantityChild->NextSibling()){
                                const char *quantityVal =  childQuantityChild->Value();

                                if(!strcmp(quantityVal, "quantityId")){
                                    TiXmlNode *quantityNode = childChart->FirstChild();
                                    if(quantityNode){
                                        wxString qid = wxString::FromUTF8(quantityNode->FirstChild()->Value());
                                        long sv;
                                        qid.ToLong(&sv);
                                        Qty.quantityId = sv;
                                    }
                                }
                                else if(!strcmp(quantityVal, "slot")){
                                     itemSlot *Slot = new itemSlot;
                                     TiXmlNode *childSlotChild;
                                     for ( childSlotChild = childQuantityChild->FirstChild(); childSlotChild!= 0; childSlotChild = childSlotChild->NextSibling()){
                                         const char *slotVal =  childSlotChild->Value();
 
                                         if(!strcmp(slotVal, "slotUuid")){
                                             TiXmlNode *childVal = childSlotChild->FirstChild();
                                             if(childVal) Slot->slotUuid = childVal->Value();
                                         }
                                         else if(!strcmp(slotVal, "assignedSystemName")){
                                             TiXmlNode *childVal = childSlotChild->FirstChild();
                                             if(childVal) Slot->assignedSystemName = childVal->Value();
                                         }
                                         else if(!strcmp(slotVal, "lastRequested")){
                                             TiXmlNode *childVal = childSlotChild->FirstChild();
                                             if(childVal) Slot->lastRequested = childVal->Value();
                                         }
                                     }
                                     Qty.slotList.push_back(Slot);
                                 }
                            }
                            pChart->quantityList.push_back(Qty);
                        }
                    }
                    
                    if( pChart == NULL )
                        continue;
                        
                    // Process this chart node
                    
                    // As identified uniquely by order, and chartid....
                    // Does this chart already exist in the table?
                    int index = findOrderRefChartId(pChart->orderRef, pChart->chartID);
                    if(index < 0){
                        ChartVector.push_back(pChart);
                    }
                    else{
                        ChartVector[index]->Update(pChart);
                        delete pChart;
                    }
                    
                }
            }
        
        
        return queryResult;
}

    

int getChartList( bool bShowErrorDialogs = true){
    
    // We query the server for the list of charts associated with our account
    wxString url = userURL;
    if(g_admin)
        url = adminURL;
    
    url +=_T("?fc=module&module=occharts&controller=apioernc");
    
    wxString loginParms;
    loginParms += _T("taskId=getlist");
    loginParms += _T("&username=") + g_loginUser;
    loginParms += _T("&key=") + g_loginKey;
    if(g_debugShop.Len())
        loginParms += _T("&debug=") + g_debugShop;

    
    wxCurlHTTPNoZIP post;
    post.SetOpt(CURLOPT_TIMEOUT, g_timeout_secs);
    
    size_t res = post.Post( loginParms.ToAscii(), loginParms.Len(), url );
    
    // get the response code of the server
    int iResponseCode;
    post.GetInfo(CURLINFO_RESPONSE_CODE, &iResponseCode);
    
    std::string a = post.GetDetailedErrorString();
    std::string b = post.GetErrorString();
    std::string c = post.GetResponseBody();
    
    //printf("%s", post.GetResponseBody().c_str());
    
    wxString tt(post.GetResponseBody().data(), wxConvUTF8);
    //wxLogMessage(tt);
    
    if(iResponseCode == 200){
        wxString result = ProcessResponse(post.GetResponseBody());
        
        return checkResult( result, bShowErrorDialogs );
    }
    else
        return checkResponseCode(iResponseCode);
}


int doAssign(itemChart *chart, int qtyIndex, wxString systemName)
{
    wxString msg = _("This action will PERMANENTLY assign the chart:");
    msg += _T("\n        ");
    msg += chart->chartName;
    msg += _T("\n\n");
    msg += _("to this systemName:");
    msg += _T("\n        ");
    msg += systemName;
    if(systemName.StartsWith("sgl")){
        msg += _T(" (") + _("USB Key Dongle") + _T(")");
    }
    
    msg += _T("\n\n");
    msg += _("Proceed?");
    
    int ret = OCPNMessageBox_PlugIn(NULL, msg, _("oeRNC_PI Message"), wxYES_NO);
    
    if(ret != wxID_YES){
        return 1;
    }
        
    // Assign a chart to this system name
    wxString url = userURL;
    if(g_admin)
        url = adminURL;
    
    url +=_T("?fc=module&module=occharts&controller=apioernc");
    
    wxString loginParms;
    loginParms += _T("taskId=assign");
    loginParms += _T("&username=") + g_loginUser;
    loginParms += _T("&key=") + g_loginKey;
    if(g_debugShop.Len())
        loginParms += _T("&debug=") + g_debugShop;

    loginParms += _T("&systemName=") + systemName;
    loginParms += _T("&order=") + chart->orderRef;
    loginParms += _T("&chartid=") + chart->chartID;
    wxString sqid;
    sqid.Printf(_T("%1d"), chart->quantityList[qtyIndex].quantityId);
    loginParms += _T("&quantityId=") + sqid;
    
    wxCurlHTTPNoZIP post;
    post.SetOpt(CURLOPT_TIMEOUT, g_timeout_secs);
    size_t res = post.Post( loginParms.ToAscii(), loginParms.Len(), url );
    
    // get the response code of the server
    int iResponseCode;
    post.GetInfo(CURLINFO_RESPONSE_CODE, &iResponseCode);
    
    if(iResponseCode == 200){
        wxString result = ProcessResponse(post.GetResponseBody());

        if(result.IsSameAs(_T("1"))){                    // Good result
            // Create a new slot and record the assigned slotUUID, etc
            itemSlot *slot = new itemSlot;
            slot->assignedSystemName = systemName.mb_str();
            slot->slotUuid = g_lastSlotUUID.mb_str();
            chart->quantityList[qtyIndex].slotList.push_back(slot);
            return 0;
        }
        
        if(result.IsSameAs(_T("8"))){                    // Unknown system name
            return 0;
        }

        if(result.IsSameAs(_T("50"))){                  // General network error
            return 0;                                   // OK
        }
        
        return checkResult(result);
    }
    else
        return checkResponseCode(iResponseCode);
}


int doUploadXFPR(bool bDongle)
{
    wxString err;
    
    // Generate the FPR file
    bool b_copyOK = false;
    
    wxString fpr_file = getFPR( false, b_copyOK, bDongle);              // No copy needed
    
    fpr_file = fpr_file.Trim(false);            // Trim leading spaces...
    
    if(fpr_file.Len()){
        
        wxString stringFPR;
        
        //Read the file, convert to ASCII hex, and build a string
        if(::wxFileExists(fpr_file)){
            wxString stringFPR;
            wxFileInputStream stream(fpr_file);
            while(stream.IsOk() && !stream.Eof() ){
                unsigned char c = stream.GetC();
                if(!stream.Eof()){
                    wxString sc;
                    sc.Printf(_T("%02X"), c);
                    stringFPR += sc;
                }
            }
            
            // Prepare the upload command string
            wxString url = userURL;
            if(g_admin)
                url = adminURL;
            
            url +=_T("?fc=module&module=occharts&controller=apioernc");
            
            wxFileName fnxpr(fpr_file);
            wxString fprName = fnxpr.GetFullName();
            
            wxString loginParms;
            loginParms += _T("taskId=xfpr");
            loginParms += _T("&username=") + g_loginUser;
            loginParms += _T("&key=") + g_loginKey;
            if(g_debugShop.Len())
                loginParms += _T("&debug=") + g_debugShop;

            if(!bDongle)
                loginParms += _T("&systemName=") + g_systemName;
            else
                loginParms += _T("&systemName=") + g_dongleName;
                
            loginParms += _T("&xfpr=") + stringFPR;
            loginParms += _T("&xfprName=") + fprName;
            
            wxLogMessage(loginParms);
            
            wxCurlHTTPNoZIP post;
            post.SetOpt(CURLOPT_TIMEOUT, g_timeout_secs);
            size_t res = post.Post( loginParms.ToAscii(), loginParms.Len(), url );
            
            // get the response code of the server
            int iResponseCode;
            post.GetInfo(CURLINFO_RESPONSE_CODE, &iResponseCode);
            
            if(iResponseCode == 200){
                wxString result = ProcessResponse(post.GetResponseBody());
                
                if(result.IsSameAs(_T("8"))){                    // Unknown system name
                    return 0;                                   // OK
                }

                if(result.IsSameAs(_T("50"))){                  // General network error
                    return 0;                                   // OK
                }
                
                int iret = checkResult(result);
                
                return iret;
            }
            else
                return checkResponseCode(iResponseCode);
            
        }
        else if(fpr_file.IsSameAs(_T("DONGLE_NOT_PRESENT")))
            err = _("  {USB Dongle not found.}");
            
        else
            err = _("  {fpr file not found.}");
    }
    else{
        err = _("  {fpr file not created.}");
    }
    
    if(err.Len()){
        wxString msg = _("ERROR Creating Fingerprint file") + _T("\n");
        msg += _("Check OpenCPN log file.") + _T("\n"); 
        msg += err;
        OCPNMessageBox_PlugIn(NULL, msg, _("oeRNC_pi Message"), wxOK);
        return 1;
    }
        
    return 0;
}


int doPrepare(oeXChartPanel *chartPrepare, itemSlot *slot)
{
    // Request a chart preparation
    wxString url = userURL;
    if(g_admin)
        url = adminURL;
    
    url +=_T("?fc=module&module=occharts&controller=apioernc");
    
   
    itemChart *chart = chartPrepare->m_pChart;
    
    
/*
                        a. taskId: request
                        b. username
                        c. key
                        d. assignedSystemName: The current systemName assigned to the machine or connected dongle.
                        e. slotUuid: got after request 4
                        f. requestedFile: base|update (all files or just updated files from previous versions)
                        g. requestedEdition: E-U
                        h. currentEdition: E-U (it could be void if requestedFile = base but it can not if requestedFile = update)
                        i. debug
*/       
    wxString loginParms;
    loginParms += _T("taskId=request");
    loginParms += _T("&username=") + g_loginUser;
    loginParms += _T("&key=") + g_loginKey;
    if(g_debugShop.Len())
        loginParms += _T("&debug=") + g_debugShop;

    loginParms += _T("&assignedSystemName=") + wxString(slot->assignedSystemName.c_str());
    loginParms += _T("&slotUuid=") + wxString(slot->slotUuid.c_str());
    loginParms += _T("&requestedFile=") + chart->taskRequestedFile;
    loginParms += _T("&requestedEdition=") + chart->taskRequestedEdition;
    loginParms += _T("&currentEdition=") + chart->taskCurrentEdition;
    
    wxLogMessage(loginParms);
    
    wxCurlHTTPNoZIP post;
    post.SetOpt(CURLOPT_TIMEOUT, g_timeout_secs);
    size_t res = post.Post( loginParms.ToAscii(), loginParms.Len(), url );
    
    // get the response code of the server
    int iResponseCode;
    post.GetInfo(CURLINFO_RESPONSE_CODE, &iResponseCode);
    
    if(iResponseCode == 200){
        // Expecting complex links with embedded entities, so process the "&" correctly
        wxString result = ProcessResponse(post.GetResponseBody(), true);
        
        if(result.IsSameAs(_T("50"))){                  // General network error
            return 0;                                   // OK
        }
        
        return checkResult(result);
    }
    else
        return checkResponseCode(iResponseCode);
    

    return 0;
}

int doDownload(itemChart *targetChart, itemSlot *targetSlot)
{
    //  Create the download queue for all files necessary.

    targetSlot->dlQueue.clear();
    
    for(unsigned int i=0 ; i < targetSlot->taskFileList.size() ; i++){
    
        // First, the shorter Key file
        itemDLTask task1;
        wxURI uri;
        wxString downloadURL = wxString(targetSlot->taskFileList[i]->linkKeys.c_str());
        uri.Create(downloadURL);
        wxString serverFilename = uri.GetPath();
        wxFileName fn(serverFilename);
        wxString fileTarget = fn.GetFullName();

        task1.url = downloadURL;
        task1.localFile = wxString(g_PrivateDataDir + _T("DownloadCache") + wxFileName::GetPathSeparator() + fileTarget).mb_str();
        task1.SHA256 = targetSlot->taskFileList[i]->sha256Keys;
        targetSlot->taskFileList[i]->cacheKeysLocn = task1.localFile;
        targetSlot->dlQueue.push_back(task1);
        
        // Next, the chart payload file
        itemDLTask task2;
        wxURI uri2;
        downloadURL = wxString(targetSlot->taskFileList[i]->link.c_str());
        uri2.Create(downloadURL);
        serverFilename = uri2.GetPath();
        wxFileName fn2(serverFilename);
        fileTarget = fn2.GetFullName();

        task2.url = downloadURL;
        task2.localFile = wxString(g_PrivateDataDir + _T("DownloadCache") + wxFileName::GetPathSeparator() + fileTarget).mb_str();
        task2.SHA256 = targetSlot->taskFileList[i]->sha256;
        targetSlot->taskFileList[i]->cacheLinkLocn = task2.localFile;
        targetSlot->dlQueue.push_back(task2);
    }
    
    // Store the targetSlot pointer globally for general access
    gtargetSlot = targetSlot;
    gtargetSlot->idlQueue = 0;
    gtargetChart = targetChart;
    
    //Send an event to kick off the download chain
    wxCommandEvent event(wxEVT_COMMAND_BUTTON_CLICKED);
    event.SetId( ID_CMD_BUTTON_INSTALL_CHAIN );
    g_shopPanel->GetEventHandler()->AddPendingEvent(event);

    return 0;
}

bool ExtractZipFiles( const wxString& aZipFile, const wxString& aTargetDir, bool aStripPath, wxDateTime aMTime, bool aRemoveZip )
{
    bool ret = true;
    
    std::unique_ptr<wxZipEntry> entry(new wxZipEntry());
    
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
                
                if(g_ipGauge)
                    g_ipGauge->Pulse();
                wxYield();
                
                if( !file )
                {
                    wxLogError(_T("Can not create file '")+name+_T("'."));
                    ret = false;
                    break;
                }
                zip.Read(file);
                fn.SetTimes(&aMTime, &aMTime, &aMTime);
                ret = true;
            }
            
        }
        
    }
    while(false);
    
    if( aRemoveZip )
        wxRemoveFile(aZipFile);
    
    return ret;
}


int doUnzip(itemSlot *slot)
{
    if(!slot)
        return 1;
    
    wxString downloadFile;
    
    // Is there an install directory known for this chart, say from config file?
    wxString installDir;
    
    
    wxString chosenInstallDir;
        
    wxString installLocn = g_PrivateDataDir;
    if(installDir.Length())
        installLocn = installDir;
    else if(g_lastInstallDir.Length())
        installLocn = g_lastInstallDir;
        
    wxDirDialog dirSelector( NULL, _("Choose chart install location."), installLocn, wxDD_DEFAULT_STYLE  );
    int result = dirSelector.ShowModal();
        
    if(result == wxID_OK){
        chosenInstallDir = dirSelector.GetPath();
    }
    else{
        return 1;
    }

    g_shopPanel->setStatusText( _("Ready for unzipping chart files."));
    g_shopPanel->Refresh(true);
    wxYield();
    
    // Ready for unzip
    // Walk the download queue, unzipping any ZIP files
    for(unsigned int i = 0 ; i < slot->dlQueue.size() ; i++){
        downloadFile = wxString(slot->dlQueue[i].localFile);
        if( downloadFile.Lower().EndsWith(_T("zip")) ){ //Zip compressed
            g_shopPanel->setStatusText( _("Unzipping chart files..."));
            wxYield();
        
            ::wxBeginBusyCursor();
            bool ret = ExtractZipFiles( downloadFile, chosenInstallDir, false, wxDateTime::Now(), false);
            ::wxEndBusyCursor();
        
            if(!ret){
                wxLogError(_T("oernc_pi: Unable to extract: ") + downloadFile );
                OCPNMessageBox_PlugIn(NULL, _("Error extracting zip file"), _("oeRNC_pi Message"), wxOK);
                return 2;
            }
        }
    }
    
    //  Any extra, unzipped files present in the queue (e.g. keyList.XML) should be simply copied to the root
    // directory of the zip set, or, in other words, to the directory containing the .oernc files.
    // Find that directory...
    wxFileName fnz(downloadFile);
    wxString containerDir = fnz.GetName();
    
     // Walk the download queueagain, copying any plain files to containerDir
    for(unsigned int i = 0 ; i < slot->dlQueue.size() ; i++){
        downloadFile = wxString(slot->dlQueue[i].localFile);
        if( downloadFile.Lower().EndsWith(_T("zip")) ){ //Zip compressed
            continue;
        }
        else{                   // Not a ZIP file, just needs copy
            wxFileName fn(downloadFile);
            if(!::wxCopyFile( downloadFile, chosenInstallDir +  wxFileName::GetPathSeparator() + containerDir + wxFileName::GetPathSeparator() + fn.GetFullName())){
                wxLogError(_T("oernc_pi: Unable to copy: ") + downloadFile );
                OCPNMessageBox_PlugIn(NULL, _("Error copying file"), _("oeRNC_pi Message"), wxOK);
                return 3;
            }
        }
    }
   
    
    //  We know that the unzip process puts all charts in a subdir whose name is the "downloadFile", without extension
    //  This is the dir that we want to add to database.
    wxFileName fn(downloadFile);
    wxString chartDir = fn.GetName();
    wxString targetAddDir = chosenInstallDir + wxFileName::GetPathSeparator() + chartDir;
    
    //  If the currect core chart directories do not cover this new directory, then add it
    bool covered = false;
    for( size_t i = 0; i < GetChartDBDirArrayString().GetCount(); i++ )
    {
        if( targetAddDir.StartsWith((GetChartDBDirArrayString().Item(i))) )
        {
            covered = true;
            break;
        }
    }
    if( !covered )
    {
        AddChartDirectory( targetAddDir );
    }

#if 0
    
    //  Is this an update?
    wxString lastInstalledZip;
    bool b_update = false;
    
    if(slot == 0){
        if(!chart->installedEdition0.IsSameAs(chart->lastRequestEdition0)){
            b_update = true;
            lastInstalledZip = chart->installedFileDownloadPath0;
        }
    }
    else if(slot == 1){
        if(!chart->installedEdition1.IsSameAs(chart->lastRequestEdition1)){
            b_update = true;
            lastInstalledZip = chart->installedFileDownloadPath1;
        }
    }
    
    if(b_update){
        
        // It would be nice here to remove the now obsolete chart dir from the OCPN core set.
        //  Not possible with current API, 
        //  So, best we can do is to rename it, so that it will disappear from the scanning.
 
        wxString installParent;
        if(slot == 0)
            installParent = chart->installLocation0;
        else if(slot == 1)
            installParent = chart->installLocation1;
 
        if(installParent.Len() && lastInstalledZip.Len()){
            wxFileName fn(lastInstalledZip);
            wxString lastInstall = installParent + wxFileName::GetPathSeparator() + fn.GetName();
                
            if(!lastInstall.IsSameAs(targetAddDir)){
                if(::wxDirExists(lastInstall)){
                    
                    //const wxString obsDir = lastInstall + _T(".OBSOLETE");
                    //bool success = ::wxRenameFile(lastInstall, obsDir);
                    
                    // Delete all the files in this directory
                    wxArrayString files;
                    wxDir::GetAllFiles(lastInstall, &files);
                    for(unsigned int i = 0 ; i < files.GetCount() ; i++){
                        ::wxRemoveFile(files[i]);
                    }
                    ::wxRmdir(lastInstall);
                    
                }
            }
        }
    }
#endif    
    
    // Update the config persistence
//     if(slot == 0){
//         chart->installLocation0 = chosenInstallDir;
//         chart->installedEdition0 = chart->lastRequestEdition0;
//         chart->installedFileDownloadPath0 = chart->fileDownloadPath0;
//     }
//     else if(slot == 1){
//         chart->installLocation1 = chosenInstallDir;
//         chart->installedEdition1 = chart->lastRequestEdition1;
//         chart->installedFileDownloadPath1 = chart->fileDownloadPath1;
//     }
    
    g_lastInstallDir = chosenInstallDir;
    
    ForceChartDBUpdate();
    
    saveShopConfig();
    
    
    return 0;
}

    

int doShop(){
    
    loadShopConfig();
   
    // Check the dongle
    g_dongleName.Clear();
    if(IsDongleAvailable()){
        g_dongleSN = GetDongleSN();
        char sName[20];
        snprintf(sName, 19, "sgl%08X", g_dongleSN);

        g_dongleName = wxString(sName);
    }
 
    if(g_shopPanel)
        g_shopPanel->RefreshSystemName();

    //  Do we need an initial login to get the persistent key?
    if(g_loginKey.Len() == 0){
        doLogin();
        saveShopConfig();
    }
    
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

oeSencChartPanel::oeSencChartPanel(wxWindow *parent, wxWindowID id, const wxPoint &pos, const wxSize &size, oitemChart *p_itemChart, shopPanel *pContainer)
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
        //m_pContainer->SelectChart( this );
        
    }
    else{
        SetSelected( false );
        //m_pContainer->SelectChart( (oeSencChartPanel *)NULL );
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
        SetMinSize(wxSize(-1, 10 * refHeight));
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
    
    wxString nameString = m_pChart->chartName;
    if(!m_pChart->quantityId.IsSameAs(_T("1")))
        nameString += _T(" (") + m_pChart->quantityId + _T(")");
    
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
        double font_size = dFont->GetPointSize() * 3/2;
        wxFont *qFont = wxTheFontList->FindOrCreateFont( font_size, dFont->GetFamily(), dFont->GetStyle(), dFont->GetWeight());
        
        int text_x = scaledWidth * 12 / 10;
        dc.SetFont( *qFont );
        dc.SetTextForeground(wxColour(0,0,0));
        dc.DrawText(nameString, text_x, height * 5 / 100);
        
        int hTitle = dc.GetCharHeight();
        int y_line = (height * 5 / 100) + hTitle;
        dc.DrawLine( text_x, y_line, width - base_offset, y_line);
        
        
        dc.SetFont( *dFont );           // Restore default font
        int offset = GetCharHeight();
        
        int yPitch = GetCharHeight();
        int yPos = y_line + 4;
        wxString tx;
        
        int text_x_val = scaledWidth + ((width - scaledWidth) * 4 / 10);
        
        // Create and populate the current chart information
        tx = _("Chart Edition:");
        dc.DrawText( tx, text_x, yPos);
        tx = m_pChart->currentChartEdition;
        dc.DrawText( tx, text_x_val, yPos);
        yPos += yPitch;
        
        tx = _("Order Reference:");
        dc.DrawText( tx, text_x, yPos);
        tx = m_pChart->orderRef;
        dc.DrawText( tx, text_x_val, yPos);
        yPos += yPitch;
        
        tx = _("Purchase date:");
        dc.DrawText( tx, text_x, yPos);
        tx = m_pChart->purchaseDate;
        dc.DrawText( tx, text_x_val, yPos);
        yPos += yPitch;
        
        tx = _("Expiration date:");
        dc.DrawText( tx, text_x, yPos);
        tx = m_pChart->expDate;
        dc.DrawText( tx, text_x_val, yPos);
        yPos += yPitch;
        
        tx = _("Status:");
        dc.DrawText( tx, text_x, yPos);
        tx = m_pChart->getStatusString();
        if(g_statusOverride.Len())
            tx = g_statusOverride;
        dc.DrawText( tx, text_x_val, yPos);
        yPos += yPitch;

        tx = m_pChart->getKeytypeString();
        if(tx.Len()){
            tx = _("Key type:");
            dc.DrawText( tx, text_x, yPos);
            tx = m_pChart->getKeytypeString();
            dc.DrawText( tx, text_x_val, yPos);
            yPos += yPitch;
        }


#if 0        
        dc.SetBrush( wxBrush( m_boxColour ) );
        
        GetGlobalColor( _T ( "UITX1" ), &c );
        dc.SetPen( wxPen( c, 3 ));
        
        dc.DrawRoundedRectangle( 0, 0, width-1, height-1, height / 10);
         
        wxFont *dFont = GetOCPNScaledFont_PlugIn(_("Dialog"));
        double font_size = dFont->GetPointSize() * 4/3;
        wxFont *qFont = wxTheFontList->FindOrCreateFont( font_size, dFont->GetFamily(), dFont->GetStyle(), dFont->GetWeight());

        dc.SetFont( *qFont );
        dc.SetTextForeground(wxColour(0,0,0));
        dc.DrawText(m_pChart->chartName, 5, 5);
        
        dc.SetFont( *dFont );
        
        int offset = GetCharHeight();
        
        int yPitch = GetCharHeight();
        int yPos = yPitch * 2;
        int xcolumn = GetCharHeight();
        wxString tx; 
        // Create and populate the current chart information
/*        
        <order>OAUMRVVRP</order>
        <purchase>2017-07-06 19:27:41</purchase>
        <expiration>2018-07-06 19:27:41</expiration>
        <chartid>10</chartid>
        <chartEdition>2018-2</chartEdition>
        <chartPublication>1522533600</chartPublication>
        <chartName>Netherlands and Belgium 2017</chartName>
        <quantityId>1</quantityId>
        
        <slot>1</slot>
        <assignedSystemName />
        <lastRequested />
        <state>unassigned</state>
        <link />
  */      
        tx = _("Chart Edition: ") + m_pChart->currentChartEdition;
        dc.DrawText( tx, xcolumn, yPos);
        yPos += yPitch;

        tx = _("Order Reference: ") + m_pChart->orderRef;
        dc.DrawText( tx, xcolumn, yPos);
        yPos += yPitch;

        yPos = yPitch * 2;
        xcolumn = width / 2;
        
        tx = _("Purchase date: ") + m_pChart->purchaseDate;
        dc.DrawText( tx, xcolumn, yPos);
        yPos += yPitch;
        
        tx = _("Expiration date: ") + m_pChart->expDate;
        dc.DrawText( tx, xcolumn, yPos);
        yPos += yPitch;
        
        dc.DrawLine( offset, yPos + 3, width - offset, yPos + 3);
        yPos += 6;
        
        
        //  The two assignment slots
        xcolumn = GetCharHeight();
        int yTable = yPos;
        wxString adjStatus;
        
        tx = _("Assigment 1");
        dc.DrawText( tx, xcolumn, yPos);
        yPos += yPitch;
        
        tx = _("System: ") + m_pChart->sysID0;
        dc.DrawText( tx, xcolumn + yPitch, yPos);
        yPos += yPitch;
        
        tx = _("Installed edition: ") + m_pChart->lastRequestEdition0;
        dc.DrawText( tx, xcolumn + yPitch, yPos);
        yPos += yPitch;
        
        adjStatus = m_pChart->statusID0;
//         if(adjStatus.IsSameAs(_T("requestable")))
//             adjStatus = _("ok");
        
        tx = _("Status: ") + adjStatus;
        dc.DrawText( tx, xcolumn + yPitch, yPos);
        yPos += yPitch;
 
        yPos = yTable;
        xcolumn = width / 2;
        
        tx = _("Assigment 2");
        dc.DrawText( tx, xcolumn, yPos);
        yPos += yPitch;
        
        tx = _("System: ") + m_pChart->sysID1;
        dc.DrawText( tx, xcolumn + yPitch, yPos);
        yPos += yPitch;
        
        tx = _("Installed edition: ") + m_pChart->lastRequestEdition1;
        dc.DrawText( tx, xcolumn + yPitch, yPos);
        yPos += yPitch;
        
        adjStatus = m_pChart->statusID1;
//        if(adjStatus.IsSameAs(_T("requestable")))
//            adjStatus = _("ok");
        
        tx = _("Status: ") + adjStatus;
        dc.DrawText( tx, xcolumn + yPitch, yPos);
        yPos += yPitch;
#endif
        
        
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
        
        
        wxFont *dFont = GetOCPNScaledFont_PlugIn(_("Dialog"));
        double font_size = dFont->GetPointSize() * 3/2;
        wxFont *qFont = wxTheFontList->FindOrCreateFont( font_size, dFont->GetFamily(), dFont->GetStyle(), dFont->GetWeight());

        dc.SetFont( *qFont );
        dc.SetTextForeground(wxColour(128, 128, 128));
        
        if(m_pContainer->GetSelectedChart())
            dc.SetTextForeground(wxColour(220,220,220));
        
        dc.DrawText(nameString, scaledWidth * 15 / 10, height * 35 / 100);
        
    }
    
    
}


BEGIN_EVENT_TABLE(oeXChartPanel, wxPanel)
EVT_PAINT ( oeXChartPanel::OnPaint )
EVT_ERASE_BACKGROUND(oeXChartPanel::OnEraseBackground)
END_EVENT_TABLE()

oeXChartPanel::oeXChartPanel(wxWindow *parent, wxWindowID id, const wxPoint &pos, const wxSize &size, itemChart *p_itemChart, shopPanel *pContainer)
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
     Connect(wxEVT_LEFT_DOWN, wxMouseEventHandler(oeXChartPanel::OnChartSelected), NULL, this);
//     
    
}

oeXChartPanel::~oeXChartPanel()
{
}

void oeXChartPanel::OnChartSelected( wxMouseEvent &event )
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
        m_pContainer->SelectChart( (oeXChartPanel*)NULL );
    }
}

void oeXChartPanel::SetSelected( bool selected )
{
    m_bSelected = selected;
    wxColour colour;
    int refHeight = GetCharHeight();
    
    if (selected)
    {
        GetGlobalColor(_T("UIBCK"), &colour);
        m_boxColour = colour;

        // Calculate minimum size required
        if(m_pChart){
        int nAssign = 0;
            for(unsigned int i=0 ; i < m_pChart->quantityList.size() ; i++){
                itemQuantity Qty = m_pChart->quantityList[i];
                nAssign += Qty.slotList.size();
            }
            SetMinSize(wxSize(-1, (9 + nAssign) * refHeight));
        }
        else
            SetMinSize(wxSize(-1, 5 * refHeight));
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

void oeXChartPanel::OnEraseBackground( wxEraseEvent &event )
{
}

void oeXChartPanel::OnPaint( wxPaintEvent &event )
{
    int width, height;
    GetSize( &width, &height );
    wxPaintDC dc( this );
 
    //dc.SetBackground(*wxLIGHT_GREY);
    
    dc.SetPen(*wxTRANSPARENT_PEN);
    dc.SetBrush(wxBrush(GetBackgroundColour()));
    dc.DrawRectangle(GetVirtualSize());
    
    wxColour c;
    
    wxString nameString = m_pChart->chartName;
    //if(!m_pChart->quantityId.IsSameAs(_T("1")))
      //  nameString += _T(" (") + m_pChart->quantityId + _T(")");
    
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
        double font_size = dFont->GetPointSize() * 3/2;
        wxFont *qFont = wxTheFontList->FindOrCreateFont( font_size, dFont->GetFamily(), dFont->GetStyle(), dFont->GetWeight());
        
        int text_x = scaledWidth * 12 / 10;
        dc.SetFont( *qFont );
        dc.SetTextForeground(wxColour(0,0,0));
        dc.DrawText(nameString, text_x, height * 5 / 100);
        
        int hTitle = dc.GetCharHeight();
        int y_line = (height * 5 / 100) + hTitle;
        dc.DrawLine( text_x, y_line, width - base_offset, y_line);
        
        
        dc.SetFont( *dFont );           // Restore default font
        int offset = GetCharHeight();
        
        int yPitch = GetCharHeight();
        int yPos = y_line + 4;
        wxString tx;
        
        int text_x_val = scaledWidth + ((width - scaledWidth) * 4 / 10);
        
        // Create and populate the current chart information
        tx = _("Installed Chart Edition:");
        dc.DrawText( tx, text_x, yPos);
        tx = m_pChart->installedChartEdition;
        dc.DrawText( tx, text_x_val, yPos);
        yPos += yPitch;
        
        tx = _("Current Chart Edition:");
        dc.DrawText( tx, text_x, yPos);
        tx = m_pChart->serverChartEdition;
        dc.DrawText( tx, text_x_val, yPos);
        yPos += yPitch;
        
        tx = _("Order Reference:");
        dc.DrawText( tx, text_x, yPos);
        tx = m_pChart->orderRef;
        dc.DrawText( tx, text_x_val, yPos);
        yPos += yPitch;
        
        tx = _("Updates available through:");
        dc.DrawText( tx, text_x, yPos);
        tx = m_pChart->expDate;
        dc.DrawText( tx, text_x_val, yPos);
        yPos += yPitch;
        
        tx = _("Status:");
        dc.DrawText( tx, text_x, yPos);
        tx = m_pChart->getStatusString();
        if(g_statusOverride.Len())
            tx = g_statusOverride;
        dc.DrawText( tx, text_x_val, yPos);
        yPos += yPitch;

        tx = _("Assignments:");
        int assignedCount = m_pChart->getChartAssignmentCount();
        int availCount = m_pChart->quantityList.size() * m_pChart->maxSlots;
        wxString acv;
        acv.Printf(_T("   %d/%d"), assignedCount, availCount);
        if(assignedCount)
            tx += acv;
        else
            tx += _("    None");
        dc.DrawText( tx, text_x, yPos);
        yPos += yPitch;

        wxString id;
        int nid = 1;
        for(unsigned int i=0 ; i < m_pChart->quantityList.size() ; i++){
            itemQuantity Qty = m_pChart->quantityList[i];
            for(unsigned int j = 0 ; j < Qty.slotList.size() ; j++){
                itemSlot *slot = Qty.slotList[j];
                tx = m_pChart->getKeytypeString(slot->slotUuid);
                id.Printf(_("%d) "), nid);
                tx.Prepend(id);
                tx += _T("    ") + wxString(slot->assignedSystemName.c_str());
                //tx += _T(" ") + wxString(slot->slotUuid.c_str());
                dc.DrawText( tx, text_x_val, yPos);
                yPos += yPitch;
                nid++;
            }
        }
        


#if 0        
        dc.SetBrush( wxBrush( m_boxColour ) );
        
        GetGlobalColor( _T ( "UITX1" ), &c );
        dc.SetPen( wxPen( c, 3 ));
        
        dc.DrawRoundedRectangle( 0, 0, width-1, height-1, height / 10);
         
        wxFont *dFont = GetOCPNScaledFont_PlugIn(_("Dialog"));
        double font_size = dFont->GetPointSize() * 4/3;
        wxFont *qFont = wxTheFontList->FindOrCreateFont( font_size, dFont->GetFamily(), dFont->GetStyle(), dFont->GetWeight());

        dc.SetFont( *qFont );
        dc.SetTextForeground(wxColour(0,0,0));
        dc.DrawText(m_pChart->chartName, 5, 5);
        
        dc.SetFont( *dFont );
        
        int offset = GetCharHeight();
        
        int yPitch = GetCharHeight();
        int yPos = yPitch * 2;
        int xcolumn = GetCharHeight();
        wxString tx; 
        // Create and populate the current chart information
/*        
        <order>OAUMRVVRP</order>
        <purchase>2017-07-06 19:27:41</purchase>
        <expiration>2018-07-06 19:27:41</expiration>
        <chartid>10</chartid>
        <chartEdition>2018-2</chartEdition>
        <chartPublication>1522533600</chartPublication>
        <chartName>Netherlands and Belgium 2017</chartName>
        <quantityId>1</quantityId>
        
        <slot>1</slot>
        <assignedSystemName />
        <lastRequested />
        <state>unassigned</state>
        <link />
  */      
        tx = _("Chart Edition: ") + m_pChart->currentChartEdition;
        dc.DrawText( tx, xcolumn, yPos);
        yPos += yPitch;

        tx = _("Order Reference: ") + m_pChart->orderRef;
        dc.DrawText( tx, xcolumn, yPos);
        yPos += yPitch;

        yPos = yPitch * 2;
        xcolumn = width / 2;
        
        tx = _("Purchase date: ") + m_pChart->purchaseDate;
        dc.DrawText( tx, xcolumn, yPos);
        yPos += yPitch;
        
        tx = _("Expiration date: ") + m_pChart->expDate;
        dc.DrawText( tx, xcolumn, yPos);
        yPos += yPitch;
        
        dc.DrawLine( offset, yPos + 3, width - offset, yPos + 3);
        yPos += 6;
        
        
        //  The two assignment slots
        xcolumn = GetCharHeight();
        int yTable = yPos;
        wxString adjStatus;
        
        tx = _("Assigment 1");
        dc.DrawText( tx, xcolumn, yPos);
        yPos += yPitch;
        
        tx = _("System: ") + m_pChart->sysID0;
        dc.DrawText( tx, xcolumn + yPitch, yPos);
        yPos += yPitch;
        
        tx = _("Installed edition: ") + m_pChart->lastRequestEdition0;
        dc.DrawText( tx, xcolumn + yPitch, yPos);
        yPos += yPitch;
        
        adjStatus = m_pChart->statusID0;
//         if(adjStatus.IsSameAs(_T("requestable")))
//             adjStatus = _("ok");
        
        tx = _("Status: ") + adjStatus;
        dc.DrawText( tx, xcolumn + yPitch, yPos);
        yPos += yPitch;
 
        yPos = yTable;
        xcolumn = width / 2;
        
        tx = _("Assigment 2");
        dc.DrawText( tx, xcolumn, yPos);
        yPos += yPitch;
        
        tx = _("System: ") + m_pChart->sysID1;
        dc.DrawText( tx, xcolumn + yPitch, yPos);
        yPos += yPitch;
        
        tx = _("Installed edition: ") + m_pChart->lastRequestEdition1;
        dc.DrawText( tx, xcolumn + yPitch, yPos);
        yPos += yPitch;
        
        adjStatus = m_pChart->statusID1;
//        if(adjStatus.IsSameAs(_T("requestable")))
//            adjStatus = _("ok");
        
        tx = _("Status: ") + adjStatus;
        dc.DrawText( tx, xcolumn + yPitch, yPos);
        yPos += yPitch;
#endif
        
        
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
        
        
        wxFont *dFont = GetOCPNScaledFont_PlugIn(_("Dialog"));
        double font_size = dFont->GetPointSize() * 3/2;
        wxFont *qFont = wxTheFontList->FindOrCreateFont( font_size, dFont->GetFamily(), dFont->GetStyle(), dFont->GetWeight());

        dc.SetFont( *qFont );
        dc.SetTextForeground(wxColour(128, 128, 128));
        
        if(m_pContainer->GetSelectedChart())
            dc.SetTextForeground(wxColour(220,220,220));
        
        dc.DrawText(nameString, scaledWidth * 15 / 10, height * 35 / 100);
        
    }
    
    
}


BEGIN_EVENT_TABLE( chartScroller, wxScrolledWindow )
//EVT_PAINT(chartScroller::OnPaint)
EVT_ERASE_BACKGROUND(chartScroller::OnEraseBackground)
END_EVENT_TABLE()

chartScroller::chartScroller(wxWindow* parent, wxWindowID id, const wxPoint& pos, const wxSize& size, long style)
: wxScrolledWindow(parent, id, pos, size, style)
{
    int yyp = 3;
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
EVT_TIMER( 4357, shopPanel::OnPrepareTimer )
EVT_BUTTON( ID_CMD_BUTTON_INSTALL, shopPanel::OnButtonInstall )
EVT_BUTTON( ID_CMD_BUTTON_INSTALL_CHAIN, shopPanel::OnButtonInstallChain )
END_EVENT_TABLE()


shopPanel::shopPanel(wxWindow* parent, wxWindowID id, const wxPoint& pos, const wxSize& size, long style)
: wxPanel(parent, id, pos, size, style)
{
    loadShopConfig();
    
    g_CurlEventHandler = new OESENC_CURL_EvtHandler;
    
    g_shopPanel = this;
    m_binstallChain = false;
    m_bAbortingDownload = false;
    
    m_ChartSelected = NULL;
    m_choiceSystemName = NULL;
    int ref_len = GetCharHeight();
    
    wxBoxSizer* boxSizerTop = new wxBoxSizer(wxVERTICAL);
    this->SetSizer(boxSizerTop);
    
    wxGridSizer *sysBox = new wxGridSizer(2);
    boxSizerTop->Add(sysBox, 0, wxALL|wxEXPAND, WXC_FROM_DIP(5));
    wxString sn = _("System Name:");
    sn += _T(" ");
    sn += g_systemName;
    
    m_staticTextSystemName = new wxStaticText(this, wxID_ANY, sn, wxDefaultPosition, wxDLG_UNIT(this, wxSize(-1,-1)), 0);
    sysBox->Add(m_staticTextSystemName, 0, wxALL | wxALIGN_LEFT, WXC_FROM_DIP(5));

    m_buttonUpdate = new wxButton(this, wxID_ANY, _("Refresh Chart List"), wxDefaultPosition, wxDLG_UNIT(this, wxSize(-1,-1)), 0);
    m_buttonUpdate->Connect(wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler(shopPanel::OnButtonUpdate), NULL, this);
    sysBox->Add(m_buttonUpdate, 0, wxRIGHT | wxALIGN_RIGHT, WXC_FROM_DIP(5));
    
    wxStaticBoxSizer* staticBoxSizerChartList = new wxStaticBoxSizer( new wxStaticBox(this, wxID_ANY, _("My Chart Sets")), wxVERTICAL);
    boxSizerTop->Add(staticBoxSizerChartList, 0, wxALL|wxEXPAND, WXC_FROM_DIP(5));

    wxPanel *cPanel = new wxPanel(this, wxID_ANY, wxDefaultPosition, wxDLG_UNIT(this, wxSize(-1,-1)), wxBG_STYLE_ERASE );
    staticBoxSizerChartList->Add(cPanel, 0, wxALL|wxEXPAND, WXC_FROM_DIP(5));
    wxBoxSizer *boxSizercPanel = new wxBoxSizer(wxVERTICAL);
    cPanel->SetSizer(boxSizercPanel);
    
    m_scrollWinChartList = new wxScrolledWindow(cPanel, wxID_ANY, wxDefaultPosition, wxDLG_UNIT(this, wxSize(-1,-1)), wxBORDER_RAISED | wxVSCROLL | wxBG_STYLE_ERASE );
    m_scrollWinChartList->SetScrollRate(5, 5);
    boxSizercPanel->Add(m_scrollWinChartList, 0, wxALL|wxEXPAND, WXC_FROM_DIP(5));
    
    boxSizerCharts = new wxBoxSizer(wxVERTICAL);
    m_scrollWinChartList->SetSizer(boxSizerCharts);
 
    m_scrollWinChartList->SetMinSize(wxSize(-1,15 * GetCharHeight()));
    staticBoxSizerChartList->SetMinSize(wxSize(-1,16 * GetCharHeight()));
    
    wxStaticBoxSizer* staticBoxSizerAction = new wxStaticBoxSizer( new wxStaticBox(this, wxID_ANY, _("Actions")), wxVERTICAL);
    boxSizerTop->Add(staticBoxSizerAction, 0, wxALL|wxEXPAND, WXC_FROM_DIP(5));

    m_staticLine121 = new wxStaticLine(this, wxID_ANY, wxDefaultPosition, wxDLG_UNIT(this, wxSize(-1,-1)), wxLI_HORIZONTAL);
    staticBoxSizerAction->Add(m_staticLine121, 0, wxALL|wxEXPAND, WXC_FROM_DIP(5));
    
    ///Buttons
    wxGridSizer* gridSizerActionButtons = new wxGridSizer(1, 2, 0, 0);
    staticBoxSizerAction->Add(gridSizerActionButtons, 1, wxALL|wxEXPAND, WXC_FROM_DIP(2));
    
    m_buttonInstall = new wxButton(this, ID_CMD_BUTTON_INSTALL, _("Install Selected Chart Set"), wxDefaultPosition, wxDLG_UNIT(this, wxSize(-1,-1)), 0);
    gridSizerActionButtons->Add(m_buttonInstall, 1, wxTOP | wxBOTTOM, WXC_FROM_DIP(2));
    
    m_buttonCancelOp = new wxButton(this, wxID_ANY, _("Cancel Operation"), wxDefaultPosition, wxDLG_UNIT(this, wxSize(-1,-1)), 0);
    m_buttonCancelOp->Connect(wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler(shopPanel::OnButtonCancelOp), NULL, this);
    gridSizerActionButtons->Add(m_buttonCancelOp, 1, wxTOP | wxBOTTOM, WXC_FROM_DIP(2));

    wxStaticLine* sLine1 = new wxStaticLine(this, wxID_ANY, wxDefaultPosition, wxDLG_UNIT(this, wxSize(-1,-1)), wxLI_HORIZONTAL);
    staticBoxSizerAction->Add(sLine1, 0, wxALL|wxEXPAND, WXC_FROM_DIP(5));
    
    
    ///Status
    m_staticTextStatus = new wxStaticText(this, wxID_ANY, _("Status: Chart List Refresh required."), wxDefaultPosition, wxDLG_UNIT(this, wxSize(-1,-1)), 0);
    staticBoxSizerAction->Add(m_staticTextStatus, 0, wxALL|wxALIGN_LEFT, WXC_FROM_DIP(5));

    g_ipGauge = new InProgressIndicator(this, wxID_ANY, 100, wxDefaultPosition, wxSize(ref_len * 12, ref_len));
    staticBoxSizerAction->Add(g_ipGauge, 0, wxALL|wxALIGN_CENTER_HORIZONTAL, WXC_FROM_DIP(5));

    ///Last Error Message
    m_staticTextLEM = new wxStaticText(this, wxID_ANY, _("Last Error Message: "), wxDefaultPosition, wxDLG_UNIT(this, wxSize(-1,-1)), 0);
    staticBoxSizerAction->Add(m_staticTextLEM, 0, wxALL|wxALIGN_LEFT, WXC_FROM_DIP(5));
    
    SetName(wxT("shopPanel"));
    //SetSize(500,600);
    if (GetSizer()) {
        GetSizer()->Fit(this);
    }
    
    //  Turn off all buttons initially.
    m_buttonInstall->Hide();
    m_buttonCancelOp->Hide();
    m_staticTextLEM->Hide();
    
        // Check the dongle
    g_dongleName.Clear();
    if(IsDongleAvailable()){
        g_dongleSN = GetDongleSN();
        char sName[20];
        snprintf(sName, 19, "sgl%08X", g_dongleSN);

        g_dongleName = wxString(sName);
    }

    RefreshSystemName();
    
    UpdateChartList();
    
}

shopPanel::~shopPanel()
{
}

void shopPanel::SetErrorMessage()
{
    if(g_LastErrorMessage.Length()){
        wxString head = _("Last Error Message: ");
        head += g_LastErrorMessage;
        m_staticTextLEM->SetLabel( head );
        m_staticTextLEM->Show();
    }
    else
        m_staticTextLEM->Hide();
    
    g_statusOverride.Clear();
    setStatusText( _("Status: Ready"));
}

void shopPanel::RefreshSystemName()
{
    wxString sn;
    if(g_dongleName.Length()){
        sn = _("System Name:");
        sn += _T(" ");
        sn += g_dongleName + _T(" (") + _("USB Key Dongle") + _T(")");
        m_staticTextSystemName->SetLabel( sn );
    }
    else{
        sn = _("System Name:");
        sn += _T(" ");
        sn += g_systemName;
    }
    
    m_staticTextSystemName->SetLabel(sn);
}


void shopPanel::SelectChart( oeXChartPanel *chart )
{
    if (m_ChartSelected == chart)
        return;
    
    if (m_ChartSelected)
        m_ChartSelected->SetSelected(false);
    
    m_ChartSelected = chart;
    if (m_ChartSelected)
        m_ChartSelected->SetSelected(true);
    
    m_scrollWinChartList->GetSizer()->Layout();
    
    MakeChartVisible(m_ChartSelected);
    
    UpdateActionControls();
    
    Layout();
    
    Refresh( true );
}


void shopPanel::SelectChartByID( std::string id, std::string order)
{
    for(unsigned int i = 0 ; i < panelVector.size() ; i++){
        itemChart *chart = panelVector[i]->m_pChart;
        if(wxString(id).IsSameAs(chart->chartID) && wxString(order).IsSameAs(chart->orderRef)){
            SelectChart(panelVector[i]);
            MakeChartVisible(m_ChartSelected);
        }
    }
}


void shopPanel::MakeChartVisible(oeXChartPanel *chart)
{
    if(!chart)
        return;
    
    itemChart *vchart = chart->m_pChart;
    
    for(unsigned int i = 0 ; i < panelVector.size() ; i++){
        itemChart *lchart = panelVector[i]->m_pChart;
        if( !strcmp(vchart->chartID.c_str(), lchart->chartID.c_str()) && !strcmp(vchart->orderRef.c_str(), lchart->orderRef.c_str())){
            
            int offset = i * chart->GetUnselectedHeight();
            
            m_scrollWinChartList->Scroll(-1, offset / 5);
        }
    }
    
}


void shopPanel::OnButtonUpdate( wxCommandEvent& event )
{
    loadShopConfig();
    
    g_LastErrorMessage.Clear();
    SetErrorMessage();

    // Check the dongle
    g_dongleName.Clear();
    if(IsDongleAvailable()){
        g_dongleSN = GetDongleSN();
        char sName[20];
        snprintf(sName, 19, "sgl%08X", g_dongleSN);

        g_dongleName = wxString(sName);
    }
 
    RefreshSystemName();
    
    //  Do we need an initial login to get the persistent key?
    if(g_loginKey.Len() == 0){
        if(doLogin() != 1)
            return;
        saveShopConfig();
    }
    
     setStatusText( _("Contacting o-charts server..."));
     g_ipGauge->Start();
     wxYield();

    ::wxBeginBusyCursor();
    int err_code = getChartList( false );               // no error code dialog, we handle here
    ::wxEndBusyCursor();
 
    // Could be a change in login_key, userName, or password.
    // if so, force a full (no_key) login, and retry
    if((err_code == 4) || (err_code == 5) || (err_code == 6)){
        setStatusText( _("Status: Login error."));
        g_ipGauge->Stop();
        wxYield();
        if(doLogin() != 1)      // if the second login attempt fails, return to GUI
            return;
        saveShopConfig();
        
        // Try to get the status one more time only.
        ::wxBeginBusyCursor();
        int err_code_2 = getChartList( false );               // no error code dialog, we handle here
        ::wxEndBusyCursor();
        
        if(err_code_2 != 0){                  // Some error on second getlist() try, if so just return to GUI
         
            if((err_code_2 == 4) || (err_code_2 == 5) || (err_code_2 == 6))
                setStatusText( _("Status: Login error."));
            else{
                wxString ec;
                ec.Printf(_T(" { %d }"), err_code_2);
                setStatusText( _("Status: Communications error.") + ec);
            }
            g_ipGauge->Stop();
            wxYield();
            return;
        }
    }
    
    else if(err_code != 0){                  // Some other error
        wxString ec;
        ec.Printf(_T(" { %d }"), err_code);
        setStatusText( _("Status: Communications error.") + ec);
        g_ipGauge->Stop();
        wxYield();
        return;
    }
    g_chartListUpdatedOK = true;
    
    bool bNeedSystemName = false;
    
    // User reset system name, and removed dongle
    if(!g_systemName.Len() && !g_dongleName.Len())
        bNeedSystemName = true;
    
    
    if(bNeedSystemName ){
        GetNewSystemName();
/*
        bool sname_ok = false;
        int itry = 0;
        while(!sname_ok && itry < 4){
            bool bcont = doSystemNameWizard();
        
            if( !bcont ){                // user "Cancel"
                g_systemName.Clear();
                break;
            }
            
            if(!g_systemName.Len()){
                wxString msg = _("Invalid System Name");
                OCPNMessageBox_PlugIn(NULL, msg, _("oeSENC_pi Message"), wxOK);
                itry++;
            }
            
            else if(g_systemNameDisabledArray.Index(g_systemName) != wxNOT_FOUND){
                wxString msg = _("This System Name has been disabled\nPlease choose another SystemName");
                OCPNMessageBox_PlugIn(NULL, msg, _("oeSENC_pi Message"), wxOK);
                itry++;
            }
            else{
                sname_ok = true;
            }
                
        }
*/        
    }
 
    RefreshSystemName();
    
    setStatusText( _("Status: Ready"));
    g_ipGauge->Stop();
    
    UpdateChartList();
    
    saveShopConfig();
}

bool shopPanel::GetNewSystemName()
{
        bool sname_ok = false;
        int itry = 0;
        while(!sname_ok && itry < 4){
            bool bcont = doSystemNameWizard();
        
            if( !bcont ){                // user "Cancel"
                g_systemName.Clear();
                break;
            }
            
            if(!g_systemName.Len()){
                wxString msg = _("Invalid System Name");
                OCPNMessageBox_PlugIn(NULL, msg, _("oeSENC_pi Message"), wxOK);
                itry++;
            }
            
            else if(g_systemNameDisabledArray.Index(g_systemName) != wxNOT_FOUND){
                wxString msg = _("This System Name has been disabled\nPlease choose another SystemName");
                OCPNMessageBox_PlugIn(NULL, msg, _("oeSENC_pi Message"), wxOK);
                itry++;
            }
            else{
                sname_ok = true;
            }
                
        }

    return sname_ok;
}


int shopPanel::ComputeUpdates(itemChart *chart)
{
    int installedEdition = chart->GetInstalledEditionInt();
    int serverEdition = chart->GetServerEditionInt();
 
    // Debugging/testing?
    if(g_admin && chart->overrideChartEdition.size()){
        if(chart->overrideChartEdition.find("-0") != std::string::npos){
            chart->taskRequestedFile = _T("base");
            chart->taskRequestedEdition = chart->overrideChartEdition;
            chart->taskCurrentEdition = std::string();
            chart->taskAction = TASK_REPLACE;
        
            return 0;               // no error
        }
        else{
            chart->taskRequestedFile = _T("update");
            chart->taskRequestedEdition = chart->overrideChartEdition;
            chart->taskCurrentEdition = chart->installedChartEdition;
            chart->taskAction = TASK_UPDATE;
        
            return 0;               // no error
        }
    }
        
    // Is this a reload?
    // If so, just download and install the appropriate "base"
    if(serverEdition == installedEdition){
        chart->taskRequestedFile = _T("base");
        chart->taskRequestedEdition = chart->serverChartEdition;
        chart->taskCurrentEdition = chart->installedChartEdition;
        chart->taskAction = TASK_REPLACE;
        
        return 0;               // no error
    }
    
    // Is this a base edition update?
    //  If so, we need to simply download the latest new edition available
    if(serverEdition/100 > installedEdition / 100){
        chart->taskRequestedFile = _T("base");
        chart->taskRequestedEdition = chart->serverChartEdition;
        chart->taskCurrentEdition = chart->installedChartEdition;
        chart->taskAction = TASK_REPLACE;
        
        return 0;               // no error
    }
        
    int ypp = 4;
    
    return 1;           // General error
}

std::string GetNormalizedChartsetName( std::string rawName)
{
    //Get the basic chartset names
    
    wxFileName fn1(rawName);            // /home/xxx/.opencpn/oernc_pi/DownloadCache/oeRNC-IMR-GR-2-0-base.zip
    wxString tlDir = fn1.GetName();     // oeRNC-IMR-GR-2-0-base
    int nl = tlDir.Find( _T("-base"));
    if(nl == wxNOT_FOUND)
        nl = tlDir.Find( _T("-update"));
        
    if(nl != wxNOT_FOUND){
        nl--;
        int ndash_found = 0;
        while(nl){
            wxString ttt = tlDir.Mid(0, nl);
            if(tlDir[nl] == '-'){
                ndash_found++;
                if(ndash_found == 2)
                    break;
            }
            nl--;
        }
        return  std::string(tlDir.Mid(0, nl).mb_str());   // like "oeRNC-XXXXX"
    }
    else
        return std::string();
}
 

int shopPanel::processTask(itemSlot *slot, itemChart *chart, itemTaskFileInfo *task)
{
    //Get the basic chartset name, and store in the task for later reference
    task->chartsetNameNormalized = GetNormalizedChartsetName( task->cacheLinkLocn);
    

#if 0    
    if(chart->taskAction == TASK_REPLACE){
        
        if(!slot->installLocation.size())
            return false;

        // Check the SHA256 of both files in the task
        if(!validateSHA256(task->cacheLinkLocn, task->sha256)){
            wxLogError(_T("oernc_pi: Sha256 error on: ") + task->cacheLinkLocn );
            OCPNMessageBox_PlugIn(NULL, _("Validation error on zip file"), _("oeRNC_pi Message"), wxOK);
            return 6;
        }
        if(!validateSHA256(task->cacheKeysLocn, task->sha256Keys)){
            wxLogError(_T("oernc_pi: Sha256 error on: ") + task->cacheKeysLocn );
            OCPNMessageBox_PlugIn(NULL, _("Validation error on key file"), _("oeRNC_pi Message"), wxOK);
            return 7;
        }

            
        // Remove all .oernc files from the current installation directory
        
        if(chart->taskCurrentEdition.size()){
            // Craft the name of the previous chartset's top level directory
            wxString tlDirFull;
            wxFileName fnInstall(task->cacheLinkLocn);      // /home/dsr/.opencpn/oernc_pi/DownloadCache/oeRNC-IMR-GR-2-0-base.zip
            wxString tlDir = fnInstall.GetName();           // oeRNC-IMR-GR-2-0-base
            
            int nl = tlDir.Find( chart->taskRequestedEdition + _T("-base"));      // "2-0"
            if(nl != wxNOT_FOUND){                  // result is a base set
                wxString tlDirTest = tlDir;
                tlDirTest.Replace(chart->taskRequestedEdition + _T("-base"), chart->taskCurrentEdition +_T("-base"), false);
                tlDirTest.Prepend( wxString(slot->installLocation.c_str()) + wxFileName::GetPathSeparator());
                if(::wxDirExists(tlDirTest)){
                    tlDirFull = tlDirTest;
                }
                else{
                    wxString tlDirTest = tlDir;
                    tlDirTest.Replace(chart->taskRequestedEdition + _T("-base"), chart->taskCurrentEdition +_T("-update"), false);
                    tlDirTest.Prepend( wxString(slot->installLocation.c_str()) + wxFileName::GetPathSeparator());
                    if(::wxDirExists(tlDirTest)){
                        tlDirFull = tlDirTest;
                    }
                }
            }
            else{
                nl = tlDir.Find( chart->taskRequestedEdition + _T("-update"));      // "2-0"
                if(nl != wxNOT_FOUND){                  // result is an update set
                    wxString tlDirTest = tlDir;
                    tlDirTest.Replace(chart->taskRequestedEdition + _T("-update"), chart->taskCurrentEdition +_T("-base"), false);
                    tlDirTest.Prepend( wxString(slot->installLocation.c_str()) + wxFileName::GetPathSeparator());
                    if(::wxDirExists(tlDirTest)){
                        tlDirFull = tlDirTest;
                    }
                }
                else{
                    wxString tlDirTest = tlDir;
                    tlDirTest.Replace(chart->taskRequestedEdition + _T("-update"), chart->taskCurrentEdition +_T("-update"), false);
                    tlDirTest.Prepend( wxString(slot->installLocation.c_str()) + wxFileName::GetPathSeparator());
                    if(::wxDirExists(tlDirTest)){
                        tlDirFull = tlDirTest;
                    }
                }
            }
            
            if(tlDirFull.Length()){
                wxArrayString fileArray;
                size_t nFiles = wxDir::GetAllFiles( tlDirFull, &fileArray, _T("*.oernc"));
                for(unsigned int i=0 ; i < nFiles ;i++)
                    ::wxRemoveFile(fileArray.Item(i));
                
                // Remove the directory itself, if empty.
                wxDir oldDir(tlDirFull);
                if(!oldDir.HasFiles() && !oldDir.HasSubDirs())
                    wxRmdir(tlDirFull);

            }
        }
        
        
        int yyp = 4;
            
        
        // We can unzip the downloaded files directly to their final location
        g_shopPanel->setStatusText( _("Unzipping chart files..."));
        wxYield();
        
        ::wxBeginBusyCursor();
        bool ret = ExtractZipFiles( task->cacheLinkLocn, slot->installLocation, false, wxDateTime::Now(), false);
        ::wxEndBusyCursor();
        
        if(!ret){
            wxLogError(_T("oernc_pi: Unable to extract: ") + task->cacheLinkLocn );
            OCPNMessageBox_PlugIn(NULL, _("Error extracting zip file"), _("oeRNC_pi Message"), wxOK);
            ::wxRemoveFile(wxString(task->cacheLinkLocn.c_str()));
            return 2;
        }
    
    //  keyList.XML should be simply copied to the root
    // directory of the zip set, or, in other words, to the directory containing the .oernc files.
    // Find that directory...
        wxString containerDir;
        
        if(wxFileName::Exists(wxString(task->cacheKeysLocn.c_str()))){
            wxFile file(wxString(task->cacheKeysLocn.c_str()));
            if(file.IsOpened()){
                if(!file.Length()){
                    wxLogError(_T("oernc_pi: Found empty file: ") + task->cacheKeysLocn );
                    OCPNMessageBox_PlugIn(NULL, _("Error empty file "), _("oeRNC_pi Message"), wxOK);
                    ::wxRemoveFile(wxString(task->cacheKeysLocn.c_str()));
                    return 5;
                }
            }
            
            wxFileName fnz(task->cacheLinkLocn);
            containerDir = fnz.GetName();
    
            wxFileName fn(task->cacheKeysLocn);
            if(!::wxCopyFile( task->cacheKeysLocn, slot->installLocation +  wxFileName::GetPathSeparator() + containerDir + wxFileName::GetPathSeparator() + fn.GetFullName())){
                wxLogError(_T("oernc_pi: Unable to copy: ") + task->cacheKeysLocn );
                OCPNMessageBox_PlugIn(NULL, _("Error copying file"), _("oeRNC_pi Message"), wxOK);
                return 3;
            }
        }
        else{
            wxLogError(_T("oernc_pi: Unable to find: ") + task->cacheKeysLocn );
            OCPNMessageBox_PlugIn(NULL, _("Error finding file"), _("oeRNC_pi Message"), wxOK);
            return 4;
        }
            
        
        chart->lastInstalledtlDir = slot->installLocation +  wxFileName::GetPathSeparator() + containerDir;
        
    }
#endif    
    if(1/*chart->taskAction == TASK_UPDATE*/){
        
        if(!slot->installLocation.size())
            return 11;

        // Check the SHA256 of both files in the task
        if(!validateSHA256(task->cacheLinkLocn, task->sha256)){
            wxLogError(_T("oernc_pi: Sha256 error on: ") + task->cacheLinkLocn );
            OCPNMessageBox_PlugIn(NULL, _("Validation error on zip file"), _("oeRNC_pi Message"), wxOK);
            return 8;
        }
        if(!validateSHA256(task->cacheKeysLocn, task->sha256Keys)){
            wxLogError(_T("oernc_pi: Sha256 error on: ") + task->cacheKeysLocn );
            OCPNMessageBox_PlugIn(NULL, _("Validation error on key file"), _("oeRNC_pi Message"), wxOK);
            return 9;
        }

        // Extract the key type and name from the downloaded KeyList file
        wxFileName fn2(wxString(task->cacheKeysLocn.c_str()));
        wxString kfn = fn2.GetName();
        int nl = kfn.Find( _T("-base"));
        if(nl == wxNOT_FOUND)
            nl = kfn.Find( _T("-update"));

        wxString keySystem;
        nl++;
        if(nl != wxNOT_FOUND){
            bool dash_found = false;
            while( (size_t)nl < kfn.Length() - 1){
                if(kfn[nl++] == '-'){
                    dash_found = true;
                    break;
                }
            }
            if(dash_found)
                keySystem = kfn.Mid(nl);
        }
  
        if(!keySystem.Length()){
            wxLogError(_T("ChartKey list system name cannot be determined: ") + wxString(task->cacheKeysLocn.c_str()));
            return 16;
        }
        

        wxString pathSep(wxFileName::GetPathSeparator());  std::string ps(pathSep.mb_str());
        
        // If the task type is TASK_REPLACE, we will not get any "WithdrawnCharts" directives
        // So best to simply delete all the charts, and the keylist file from the current installation
        if(chart->taskAction == TASK_REPLACE){
            wxArrayString fileArray;
            wxString currentTLDIR = wxString((slot->installLocation +  ps + task->chartsetNameNormalized).c_str());
            if(wxDir::Exists(currentTLDIR)){
                size_t nFiles = wxDir::GetAllFiles( currentTLDIR, &fileArray);
                for(unsigned int i=0 ; i < nFiles ;i++)
                    ::wxRemoveFile(fileArray.Item(i));
            }
        }

        
        
        // Create chart list container class from currently installed base set   e.g. /home/dsr/Charts/oeRNC_GREECE/oeRNC-IMR/ChartList.XML
        std::string chartListXMLtarget = slot->installLocation +  ps + task->chartsetNameNormalized + ps + "ChartList.XML";
        ChartSetData csdata_target(chartListXMLtarget);

        // Create chart keylist container class from currently installed keylist   e.g. /home/dsr/Charts/oeRNC_GREECE/oeRNC-IMR-GR/oeRNC-IMR-GR-hp64linux.XML
        std::string chartListKeysXMLtarget = slot->installLocation +  ps + task->chartsetNameNormalized + ps + task->chartsetNameNormalized + "-" + std::string(keySystem.mb_str()) + ".XML";
        ChartSetKeys cskey_target(chartListKeysXMLtarget);
        
        // Extract the zip file to a temporary location, making the embedded files available for parsing
        wxString tmp_dir = wxFileName::CreateTempFileName( _T("") );                    // Be careful, this method actually create a file
        tmp_dir += _T("zipTemp");
        tmp_dir += wxFileName::GetPathSeparator();
        wxFileName fn(tmp_dir);
        if( !fn.DirExists() ){
            if( !wxFileName::Mkdir(fn.GetPath()) ){
                wxLogError(_T("Can not create tmp directory on TASK_UPDATE '") + fn.GetPath() + _T("'."));
                return 10;
            }
        }
  
        bool zret = ExtractZipFiles( task->cacheLinkLocn, tmp_dir, false, wxDateTime::Now(), false);

        // Find any "ChartList.XML" file
        wxString actionChartList;
        wxArrayString fileArrayXML;
        wxDir::GetAllFiles(tmp_dir, &fileArrayXML, _T("*.XML"));
        
        for(unsigned int i=0 ; i < fileArrayXML.GetCount() ; i++){
            wxString candidate = fileArrayXML.Item(i);
            wxFileName fn(candidate);
            if(fn.GetName().IsSameAs(_T("ChartList"))){
                actionChartList = candidate;
                break;
            }
        }

        // Action vectors
        std::vector < std::string> actionWithdrawn;
        std::vector < itemChartData *> actionAddUpdate;

        // If there is a ChartList.XML, read it, parse it and prepare the indicated operations
        if(actionChartList.Length()){
            
                FILE *iFile = fopen(actionChartList.mb_str(), "rb");
                if (iFile > 0){
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
                    if(root){
                        //wxString rootName = wxString::FromUTF8( root->Value() );
                        if(!strcmp(root->Value(), "chartList")){    //rootName.IsSameAs(_T("chartList"))){
            
                            TiXmlNode *child;
                            for ( child = root->FirstChild(); child != 0; child = child->NextSibling()){

                                if(!strcmp(child->Value(), "WithdrawnCharts")){
                                    TiXmlNode *childChart = child->FirstChild();
                                    for ( childChart = child->FirstChild(); childChart!= 0; childChart = childChart->NextSibling()){
                                        TiXmlNode *childChartName = childChart->FirstChild();
                                        const char *chartNameKAP =  childChartName->Value();
                                        actionWithdrawn.push_back(std::string(chartNameKAP));
                                    }
                                }
                                else if(!strcmp(child->Value(), "Chart")){
                                    TiXmlNode *childChart = child->FirstChild();
                                    itemChartData *cdata = new itemChartData;
                                    actionAddUpdate.push_back(cdata);
                                    
                                    for ( childChart = child->FirstChild(); childChart!= 0; childChart = childChart->NextSibling()){
                                        const char *chartVal = childChart->Value();
                                        if(!strcmp(chartVal, "Name")){
                                            TiXmlNode *childVal = childChart->FirstChild();
                                            if(childVal)
                                                cdata->Name = childVal->Value();
                                        }
                                        else if(!strcmp(chartVal, "ID")){
                                            TiXmlNode *childVal = childChart->FirstChild();
                                            if(childVal)
                                                cdata->ID = childVal->Value();
                                        }
                                        else if(!strcmp(chartVal, "SE")){
                                            TiXmlNode *childVal = childChart->FirstChild();
                                            if(childVal)
                                                cdata->SE = childVal->Value();                                
                                        }
                                        else if(!strcmp(chartVal, "RE")){
                                            TiXmlNode *childVal = childChart->FirstChild();
                                            if(childVal)
                                                cdata->RE = childVal->Value();
                                        }
                                        else if(!strcmp(chartVal, "ED")){
                                            TiXmlNode *childVal = childChart->FirstChild();
                                            if(childVal)
                                                cdata->ED = childVal->Value();
                                        }
                                        else if(!strcmp(chartVal, "Scale")){
                                            TiXmlNode *childVal = childChart->FirstChild();
                                            if(childVal)
                                                cdata->Scale = childVal->Value();
                                        }
                                    }
                                }
                            }
                        }
                    }
                    free( iText );
                }
        }

        ChartSetKeys workCSK;
        
        // If there is a Key list file (there had better be, even if empty), read it, parse it and prepare the indicated operations
        if(task->cacheKeysLocn.size()){
            workCSK.Load( task->cacheKeysLocn ); 
        }
        if( !workCSK.m_bOK){
            wxLogError(_T("New ChartKey list cannot be loaded: ") + wxString(task->cacheKeysLocn.c_str()));
            return 13;
        }

        //Ready to perform the actions indicated
        
        // Process withdrawn charts
        for(unsigned int i = 0 ; i < actionWithdrawn.size() ; i++){
            // Delete the oernc file
            wxFileName fn(actionWithdrawn[i].c_str());
            std::string chartFileToDelete = slot->installLocation +  ps + task->chartsetNameNormalized + ps + std::string(fn.GetName().c_str()) + ".oernc";
            if(::wxFileExists(wxString(chartFileToDelete.c_str()))){
                ::wxRemoveFile(wxString(chartFileToDelete.c_str()));
            }
            
            // Remove the entry from the working copy of ChartList.XML file
            csdata_target.RemoveChart( actionWithdrawn[i] );
 
            // Remove the entry from the working copy of keylist XML file
            cskey_target.RemoveKey( actionWithdrawn[i] );
 
        }
        
        wxDir unzipDir(tmp_dir);
        wxString chartTopLevelZip;
        if(!unzipDir.GetFirst( &chartTopLevelZip, _T("oeRNC*"), wxDIR_DIRS)){
            wxLogError(_T("Can not find oeRNC directory in zip file ") + task->cacheLinkLocn);
            return 11;
        }
 
 
        wxString destinationDir = wxString(slot->installLocation.c_str()) + wxFileName::GetPathSeparator() + task->chartsetNameNormalized + wxFileName::GetPathSeparator();
        wxFileName fndd(destinationDir);
        if( !fndd.DirExists() ){
            if( !wxFileName::Mkdir(fndd.GetPath()) ){
                wxLogError(_T("Can not create chart target directory on TASK_UPDATE '") + fndd.GetPath() );
                return 12;
            }
        }
  
        // Process added/modified charts
        for(unsigned int i = 0 ; i < actionAddUpdate.size() ; i++){
            
            wxString fileTarget = wxString( (actionAddUpdate[i]->ID).c_str()) + _T(".oernc");
            // Copy the oernc chart from the temp unzip location to the target location
            wxString source = tmp_dir + chartTopLevelZip + wxFileName::GetPathSeparator() + fileTarget;
            if(!wxFileExists(source)){
                wxLogError(_T("Can not find .oernc file referenced in ChartList: ") + source);
                continue;
            }
            
            wxString destination = destinationDir + fileTarget;
            if(!wxCopyFile( source, destination)){
                wxLogError(_T("Can not copy .oernc file referenced in ChartList...Source: ") + source + _T("   Destination: ") + destination);
                continue;
            }

            // Add the entry from the working copy of ChartList.XML file
            csdata_target.AddChart( actionAddUpdate[i] );
 
        }
        
        // Process the new Key list file, adding/editing new keys into the target set
        for(size_t i=0 ; i < workCSK.chartList.size() ; i++){
            cskey_target.AddKey(workCSK.chartList[i]);
        }
                
       
        // Write out the modified Target ChartList.XML file as the new result ChartList.XML
        wxString destinationCLXML = destinationDir + _T("ChartList.XML");
        if(! csdata_target.WriteFile( std::string(destinationCLXML.mb_str()) )){
            wxLogError(_T("Can not write target ChartList.XML on TASK_UPDATE '") + destinationCLXML );
            return 14;
        }
        

        // Write out the modified Target KeyList.XML file as the new result KeyList.XML
        wxString destinationKLXML = destinationDir  + wxString(task->chartsetNameNormalized.c_str()) + _T("-") + keySystem + _T(".XML");
        if(!cskey_target.WriteFile( std::string(destinationKLXML.mb_str()) )){
            wxLogError(_T("Can not write target KefList XML file on TASK_UPDATE '") + destinationKLXML );
            return 15;
        }
        
        chart->lastInstalledtlDir = destinationDir;

    }    
        

    return 0;
}

bool shopPanel::validateSHA256(std::string fileName, std::string shaSum)
{
    //File...
    std::string sfile = fileName;
    
    // Does the file exist?
    if(!wxFileName::Exists(wxString(sfile.c_str())))
        return false;
    
    // Has the file any content?
    wxFile file(wxString(sfile.c_str()));
    if(!file.IsOpened())
        return false;
                
    if(!file.Length())
        return false;
    
    // Calculate the SHA256 Digest

    // Check for the file presence, and openability
    FILE *rFile = fopen(sfile.c_str(), "rb");
   
    if (rFile < 0)
        return false;            // file error

    wxString previousStatus = getStatusText();
    setStatusText( _("Status: Validating download file..."));
    wxYield();

    // compute the file length    
    fseek(rFile, 0, SEEK_END);
    unsigned int rLength = ftell(rFile);
                        
    unsigned char buffer[1024 * 64];
    fseek(rFile, 0, SEEK_SET);
    
    SHA256_CTX ctx;
    unsigned char shasum[32];
    sha256_init(&ctx);
 
    size_t nread = 0;
    int ic = 0;
    while (nread < rLength){
        memset(buffer, 0, sizeof(buffer));
        int iread = fread(buffer, 1, sizeof(buffer), rFile);
        sha256_update(&ctx, buffer, iread);
        nread += iread;
        
        if((!(ic % 16)) && g_ipGauge){
            g_ipGauge->Pulse();
            wxYieldIfNeeded();
        }
        ic++;
    }

    fclose(rFile);
    
    sha256_final(&ctx, shasum);
    
    std::string ssum;
    for(int i=0 ; i < 32 ; i++){
        char t[3];
        sprintf(t, "%02x", shasum[i]);
        ssum += t;
    }
    
    int cval = ssum.compare(shaSum);
    
    setStatusText(previousStatus);
    wxYield();
    
    return (cval == 0);
    
}


void shopPanel::OnButtonInstallChain( wxCommandEvent& event )
{
    // Chained through from download end event

    if(m_bAbortingDownload){
        m_bAbortingDownload = false;
        OCPNMessageBox_PlugIn(NULL, _("Chart download cancelled."), _("oeRNC_PI Message"), wxOK);
        m_buttonInstall->Enable();
        return;
    }

    //  Is the queue done?

    if(gtargetSlot->idlQueue < gtargetSlot->dlQueue.size()){
        
        // is the required file available in the cache?
        if(wxFileExists(gtargetSlot->dlQueue[gtargetSlot->idlQueue].localFile)){
            
            // Validate the existing file using SHA256
            if( validateSHA256(gtargetSlot->dlQueue[gtargetSlot->idlQueue].localFile, gtargetSlot->dlQueue[gtargetSlot->idlQueue].SHA256)){
                //  OK, Skip to next in queue
                gtargetSlot->idlQueue++;        // next
                
                //Send an event to continue the download chain
                wxCommandEvent event(wxEVT_COMMAND_BUTTON_CLICKED);
                event.SetId( ID_CMD_BUTTON_INSTALL_CHAIN );
                g_shopPanel->GetEventHandler()->AddPendingEvent(event);
                return;
            }
        }

        // Prepare to start the next download
        //Make the temp download directory if needed
        wxFileName fn(gtargetSlot->dlQueue[gtargetSlot->idlQueue].localFile);
        if( !wxFileName::DirExists(fn.GetPath()) ){
            if( !wxFileName::Mkdir(fn.GetPath()) ){
                wxLogError(_T("Can not create directory '") + fn.GetPath() + _T("'."));
                return;
            }
        }

        g_curlDownloadThread = new wxCurlDownloadThread(g_CurlEventHandler);
        downloadOutStream = new wxFFileOutputStream(gtargetSlot->dlQueue[gtargetSlot->idlQueue].localFile);
        g_curlDownloadThread->SetURL(gtargetSlot->dlQueue[gtargetSlot->idlQueue].url);
        g_curlDownloadThread->SetOutputStream(downloadOutStream);
        //wxLogMessage(_T("Downloading: ") + gtargetSlot->dlQueue[gtargetSlot->idlQueue].url);
        g_curlDownloadThread->Download();

        gtargetSlot->idlQueue++;        // next
        
        return;
    }


    // Chained through from download end event
    if(m_binstallChain){
        m_binstallChain = false;
        
        int rv = 0;
        
        //  Download is apparently done.
        g_statusOverride.Clear();

        // Parse the task definitions, decide what to do
    
        // the simple case of inital load, or full base update
        if( (gtargetChart->taskAction == TASK_REPLACE) || (gtargetChart->taskAction == TASK_UPDATE) ){
            
            // Is there a known install directory?
            wxString installDir = gtargetSlot->installLocation;
            
            // Update, or initial load?
            if(!gtargetChart->taskCurrentEdition.Length() || !installDir.Length()){             // initial load
        
                wxString installLocn = g_PrivateDataDir;
                if(installDir.Length())
                    installLocn = installDir;
                else if(g_lastInstallDir.Length())
                    installLocn = g_lastInstallDir;
        
                wxDirDialog dirSelector( NULL, _("Choose chart install location."), installLocn, wxDD_DEFAULT_STYLE  );
                int result = dirSelector.ShowModal();
        
                if(result == wxID_OK)
                    gtargetSlot->installLocation = dirSelector.GetPath().mb_str();

            }
            
            //Presumably there is an install directory, and a current Edition, so this is an update
                
            // Process the array of itemTaskFileInfo
            for(unsigned int i=0 ; i < gtargetSlot->taskFileList.size() ; i++){
                itemTaskFileInfo *pTask = gtargetSlot->taskFileList[i];
                int rv = 0;
                rv = processTask(gtargetSlot, gtargetChart, pTask);
                if(rv){

                    g_statusOverride.Clear();
                    setStatusText( _("Status: Ready"));
                    OCPNMessageBox_PlugIn(NULL, _("Chart installation ERROR."), _("oeRNC_PI Message"), wxOK);
                    UpdateChartList();
                    m_buttonInstall->Enable();
                    return;
                }
            }

            // If no error, finalize the contract
            gtargetChart->installedChartEdition = gtargetChart->taskRequestedEdition;
        }
        
#if 0 
         // the Update case
        if(gtargetChart->taskAction == TASK_UPDATE){
            
            // Is there a known install directory?
            wxString installDir = gtargetSlot->installLocation;
            
            // Update, or initial load?
            if(!gtargetChart->taskCurrentEdition.Length() || !installDir.Length()){             // initial load
        
                wxString installLocn = g_PrivateDataDir;
                if(installDir.Length())
                    installLocn = installDir;
                else if(g_lastInstallDir.Length())
                    installLocn = g_lastInstallDir;
        
                wxDirDialog dirSelector( NULL, _("Choose chart install location."), installLocn, wxDD_DEFAULT_STYLE  );
                int result = dirSelector.ShowModal();
        
                if(result == wxID_OK)
                    gtargetSlot->installLocation = dirSelector.GetPath().mb_str();

            }
            
            //Presumably there is an install directory, and a current Edition, so this is an update
                
            // Process the array of itemTaskFileInfo
            for(unsigned int i=0 ; i < gtargetSlot->taskFileList.size() ; i++){
                itemTaskFileInfo *pTask = gtargetSlot->taskFileList[i];
                int rv = 0;
                rv = processTask(gtargetSlot, gtargetChart, pTask);
                if(rv){

                    g_statusOverride.Clear();
                    setStatusText( _("Status: Ready"));
                    OCPNMessageBox_PlugIn(NULL, _("Chart installation ERROR."), _("oeRNC_PI Message"), wxOK);
                    UpdateChartList();
                    m_buttonInstall->Enable();
                    return;
                }
            }

            // If no error, update the installed records
            gtargetChart->installedChartEdition = gtargetChart->taskRequestedEdition;
        }
#endif 

        //  We know that the unzip process puts all charts in a subdir whose name is the "downloadFile", without extension
        //  This is the dir that we want to add to database.
        wxString targetAddDir = gtargetChart->lastInstalledtlDir;
    
        //  If the currect core chart directories do not cover this new directory, then add it
        bool covered = false;
        for( size_t i = 0; i < GetChartDBDirArrayString().GetCount(); i++ ){
            if( targetAddDir.StartsWith((GetChartDBDirArrayString().Item(i))) ){
                covered = true;
                break;
            }
        }
        if( !covered ){
            AddChartDirectory( targetAddDir );
        }

        g_lastInstallDir = gtargetSlot->installLocation;
    
        // Clear out the global key hashes...
        keyMapDongle.clear();
        keyMapSystem.clear();

        ForceChartDBUpdate();
    
        saveShopConfig();
    
   
        g_statusOverride.Clear();
        setStatusText( _("Status: Ready"));
                
        OCPNMessageBox_PlugIn(NULL, _("Chart installation complete."), _("oeRNC_PI Message"), wxOK);

        UpdateChartList();

        m_buttonInstall->Enable();

        return;
    }
}

void shopPanel::OnButtonInstall( wxCommandEvent& event )
{
    itemChart *chart = m_ChartSelected->m_pChart;
    if(!chart)
        return;

    ComputeUpdates(chart);
    
    g_LastErrorMessage.Clear();
    SetErrorMessage();
    
    g_statusOverride = _T("Downloading...");
    setStatusText( _("Checking dongle..."));

    wxYield();
    
    // Check the dongle
    g_dongleName.Clear();
    if(IsDongleAvailable()){
        g_dongleSN = GetDongleSN();
        char sName[20];
        snprintf(sName, 19, "sgl%08X", g_dongleSN);

        g_dongleName = wxString(sName);
    }

    m_buttonInstall->Disable();
    m_buttonCancelOp->Show();
    


    // Is this systemName known to the server?
    // If not, need to upload XFPR first
    
    // Prefer to use the dongle, if present
    if(g_dongleName.Len()){
        if(g_systemNameServerArray.Index(g_dongleName) == wxNOT_FOUND){
            if( doUploadXFPR( true ) != 0){
                g_dongleName.Clear();
                g_statusOverride.Clear();
                setStatusText( _("Status: Dongle FPR upload error"));

                m_buttonInstall->Enable();
                m_buttonCancelOp->Hide();

                return;
            }
        }
    }
    else{
        if(!g_systemName.Length()){
            if(GetNewSystemName())
                RefreshSystemName();
        }
        
        if(!g_systemName.Length()){
                saveShopConfig();       // record blank system name.
                m_buttonInstall->Enable();
                m_buttonCancelOp->Hide();
                RefreshSystemName();
                return;
        }

        if(g_systemNameServerArray.Index(g_systemName) == wxNOT_FOUND){
            if( doUploadXFPR( false ) != 0){
                g_systemName.Clear();
                g_statusOverride.Clear();
                setStatusText( _("Status: System FPR upload error"));
                saveShopConfig();       // record blank system name.
                RefreshSystemName();
                
                m_buttonInstall->Enable();
                m_buttonCancelOp->Hide();

                return;
            }
        }
    }

 
    int qtyIndex = -1;
    
    //  Check if I am already assigned to this chart
    if(!chart->isChartsetAssignedToSystemKey(g_systemName) && !chart->isChartsetAssignedToSystemKey(g_dongleName) ){
        
        // Need assignment
        // Choose the first available qty that has an available slot
        for(unsigned int i=0 ; i < chart->quantityList.size() ; i++){
            itemQuantity Qty = chart->quantityList[i];
            if(Qty.slotList.size() < chart->maxSlots){
                qtyIndex = i;
                break;
            }
        }
        
        if(qtyIndex < 0){
            wxLogMessage(_T("oeRNC Error: No available slot found for unassigned chart."));
            m_buttonInstall->Enable();
            return;
        }
            
        // Read to assign.
        //Try to assign to dongle first..... 
        int assignResult;

        if(g_dongleName.Len())
            assignResult = doAssign(chart, qtyIndex, g_dongleName);
        else
            assignResult = doAssign(chart, qtyIndex, g_systemName);
        
        if(assignResult != 0){
            wxLogMessage(_T("oeRNC Error: Slot doAssign()."));
            m_buttonInstall->Enable();
            return;
        }
        
        // Find the active slot parameters
        chart->m_activeQtyID = chart->quantityList[qtyIndex].quantityId;
        // By definition, the new active slot index is the last one added
        chart->m_assignedSlotIndex = chart->quantityList[qtyIndex].slotList.size()-1;

    }
    
    // Known assigned to me, so maybe Ready for download
    
    // Get the slot target
    itemSlot *activeSlot = chart->GetActiveSlot();
    if(!activeSlot){
        wxLogMessage(_T("oeRNC Error: active slot not defined."));
        m_buttonInstall->Enable();
        return;
    }

        
    bool bNeedRequestWait = true;
    
    int request_return;
    if(bNeedRequestWait){

        ::wxBeginBusyCursor();
        request_return = doPrepareGUI(activeSlot);
        ::wxEndBusyCursor();

        if(request_return != 0){
//             wxString ec;
//             ec.Printf(_T(" { %d }"), request_return);     
//             setStatusText( _("Status: Communications error.") + ec);
            if(g_ipGauge)
                g_ipGauge->SetValue(0);
            m_buttonCancelOp->Hide();
            g_statusOverride.Clear();
            m_buttonInstall->Show();
            m_buttonInstall->Enable();
            
            SetErrorMessage();
            UpdateChartList();

            
            return;
        }
    }

    doDownloadGui(chart, activeSlot);
    
            

    return;
}


void shopPanel::OnButtonDownload( wxCommandEvent& event )
{
#if 0    
    
    if(!m_ChartSelected)                // No chart selected
        return;
   
    oitemChart *chart = m_ChartSelected->m_pChart;
    m_ChartSelectedID = chart->chartID;           // save a copy of the selected chart
    m_ChartSelectedOrder = chart->orderRef;
    m_ChartSelectedQty = chart->quantityId;
    
    // What slot?
    m_activeSlot = -1;
    if(chart->sysID0.IsSameAs(g_systemName))
        m_activeSlot = 0;
    else if(chart->sysID1.IsSameAs(g_systemName))
        m_activeSlot = 1;
    
    if(m_activeSlot < 0)
        return;
    
    // Is the selected chart ready for download?
    // If not, we do the "Request/Prepare" step
    bool bNeedRequestWait = false;    
    if(m_activeSlot == 0){
        if(!chart->statusID0.IsSameAs(_T("download")))
            bNeedRequestWait = true;
    }
    else if(m_activeSlot == 1){
        if(!chart->statusID1.IsSameAs(_T("download")))
            bNeedRequestWait = true;
    }

    if(bNeedRequestWait)
        int retval = doPrepareGUI();
    else
        doDownloadGui();
#endif    
}

int shopPanel::doPrepareGUI(itemSlot *targetSlot)
{
    m_buttonCancelOp->Show();
    
    setStatusText( _("Requesting License Keys..."));
    
    m_prepareTimerCount = 8;            // First status query happens in 2 seconds
    m_prepareProgress = 0;
    m_prepareTimeout = 60;
    
//    m_prepareTimer.SetOwner( this, 4357 );

    wxYield();
    
    int err_code = doPrepare(m_ChartSelected, targetSlot);
    if(err_code != 0){                  // Some error
//             wxString ec;
//             ec.Printf(_T(" { %d }"), err_code);     
//             setStatusText( _("Status: Communications error.") + ec);
            if(g_ipGauge)
                g_ipGauge->SetValue(0);
            m_buttonCancelOp->Hide();
            m_prepareTimer.Stop();
            g_statusOverride.Clear();
            m_buttonInstall->Show();

            SetErrorMessage();

            return err_code;
    }
     return 0;
}

int shopPanel::doDownloadGui(itemChart *targetChart, itemSlot* targetSlot)
{
    setStatusText( _("Status: Downloading..."));
    //m_staticTextStatusProgress->Show();
    m_buttonCancelOp->Show();
    GetButtonUpdate()->Disable();
    
    g_statusOverride = _("Downloading...");
    UpdateChartList();
    
    wxYield();
    
    m_binstallChain = true;
    m_bAbortingDownload = false;
    
    int err_code = doDownload(targetChart, targetSlot);
    return 0;
    
}

void shopPanel::OnButtonCancelOp( wxCommandEvent& event )
{
    if(m_prepareTimer.IsRunning()){
        m_prepareTimer.Stop();
        g_ipGauge->SetValue(0);
    }
    
    if(g_curlDownloadThread){
        m_bAbortingDownload = true;
        g_curlDownloadThread->Abort();
        g_ipGauge->SetValue(0);
        setStatusTextProgress(_T(""));
        m_binstallChain = true;
    }
    
    setStatusText( _("Status: OK"));
    m_buttonCancelOp->Hide();
    
    g_statusOverride.Clear();
    m_buttonInstall->Enable();
    
    SetErrorMessage();

    UpdateChartList();
    
}

void shopPanel::OnPrepareTimer(wxTimerEvent &evt)
{
#if 0
    m_prepareTimerCount++;
    m_prepareProgress++;
    
    float progress = m_prepareProgress * 100 / m_prepareTimeout;
    
    if(m_ipGauge)
        m_ipGauge->SetValue(progress);
    
    
    //  Check the status every n seconds
    
    if((m_prepareTimerCount % 10) == 0){
        int err_code = getChartList(false);     // Do not show error dialogs
        
        /*
         * Safe to ignore errors, since this is only a status loop that will time out eventually
        if(err_code != 0){                  // Some error
            wxString ec;
            ec.Printf(_T(" { %d }"), err_code);     
            setStatusText( _("Status: Communications error.") + ec);
            if(m_ipGauge)
                m_ipGauge->SetValue(0);
            m_buttonCancelOp->Hide();
            m_prepareTimer.Stop();
            
            return;
        }
        */
        
        if(!m_ChartSelected) {               // No chart selected
            setStatusText( _("Status: OK"));
            m_buttonCancelOp->Hide();
            m_prepareTimer.Stop();
            
            return;
        }
        
        oitemChart *chart = m_ChartSelected->m_pChart;
        bool bDownloadReady = false;    
        if(m_activeSlot == 0){
            if(chart->statusID0.IsSameAs(_T("download")))
                bDownloadReady = true;
        }
        else if(m_activeSlot == 1){
            if(chart->statusID1.IsSameAs(_T("download")))
                bDownloadReady = true;
        }
        
        if(1/*bDownloadReady*/){
            UpdateChartList();
            wxYield();
        }
        
        if(bDownloadReady){
            if(m_ipGauge)
                m_ipGauge->SetValue(0);
            m_buttonCancelOp->Hide();
            m_prepareTimer.Stop();
            
            doDownloadGui();
        }
            
    }
        
    if(m_prepareTimerCount >= m_prepareTimeout){
        m_prepareTimer.Stop();
        
        if(m_ipGauge)
            m_ipGauge->SetValue(100);
        
        wxString msg = _("Your chart preparation is not complete.");
        msg += _T("\n");
        msg += _("You may continue to wait, or return to this screen later to complete the download.");
        msg += _T("\n");
        msg += _("You will receive an email message when preparation for download is complete");
        msg += _T("\n\n");
        msg += _("Continue waiting?");
        msg += _T("\n\n");
        
        int ret = OCPNMessageBox_PlugIn(NULL, msg, _("oeSENC_PI Message"), wxYES_NO);
        
        if(ret == wxID_YES){
            m_prepareTimerCount = 0;
            m_prepareProgress = 0;
            m_prepareTimeout = 60;
            if(m_ipGauge)
                m_ipGauge->SetValue(0);
            
            m_prepareTimer.Start( 1000 );
        }
        else{
            if(m_ipGauge)
                m_ipGauge->SetValue(0);
            setStatusText( _("Status: OK"));
            m_buttonCancelOp->Hide();
            m_prepareTimer.Stop();
            
            
            return;
        }
    }
#endif    
}    


void shopPanel::UpdateChartList( )
{
    // Capture the state of any selected chart
     if(m_ChartSelected){
         itemChart *chart = m_ChartSelected->m_pChart;
         if(chart){
             m_ChartSelectedID = chart->chartID;           // save a copy of the selected chart
             m_ChartSelectedOrder = chart->orderRef;
         }
     }
    
    m_scrollWinChartList->ClearBackground();
    
    // Clear any existing panels
    for(unsigned int i = 0 ; i < panelVector.size() ; i++){
        delete panelVector[i];
    }
    panelVector.clear();
    m_ChartSelected = NULL;

    
    // Add new panels
    for(unsigned int i=0 ; i < ChartVector.size() ; i++){
        if(/*g_chartListUpdatedOK && */ChartVector[i]->isChartsetShow()){
            oeXChartPanel *chartPanel = new oeXChartPanel( m_scrollWinChartList, wxID_ANY, wxDefaultPosition, wxSize(-1, -1), ChartVector[i], this);
            chartPanel->SetSelected(false);
        
            boxSizerCharts->Add( chartPanel, 0, wxEXPAND|wxALL, 0 );
//            boxSizerCharts->Layout();
            panelVector.push_back( chartPanel );
        } 
    }
    
    SelectChartByID(m_ChartSelectedID, m_ChartSelectedOrder);
    
    m_scrollWinChartList->ClearBackground();
    m_scrollWinChartList->GetSizer()->Layout();

    Layout();

    m_scrollWinChartList->ClearBackground();
    
    UpdateActionControls();
    
    saveShopConfig();
    
    Refresh( true );
}


void shopPanel::UpdateActionControls()
{
    //  Turn off all buttons.
    m_buttonInstall->Hide();
    
    
    if(!m_ChartSelected){                // No chart selected
        m_buttonInstall->Enable();
        return;
    }
    
    if(!g_statusOverride.Length()){
        m_buttonInstall->Enable();
    }
    
    itemChart *chart = m_ChartSelected->m_pChart;

    if(chart->getChartStatus() == STAT_REQUESTABLE){
        m_buttonInstall->SetLabel(_("Download Selected Chart"));
        m_buttonInstall->Show();
    }

    else if(chart->getChartStatus() == STAT_PURCHASED){
        m_buttonInstall->SetLabel(_("Install Selected Chart"));
        m_buttonInstall->Show();
    }
    else if(chart->getChartStatus() == STAT_CURRENT){
        m_buttonInstall->SetLabel(_("Reinstall Selected Chart"));
        m_buttonInstall->Show();
    }
    else if(chart->getChartStatus() == STAT_STALE){
        m_buttonInstall->SetLabel(_("Update Selected Chart"));
        m_buttonInstall->Show();
    }
#if 0    
    else if(chart->getChartStatus() == STAT_READY_DOWNLOAD){
        m_buttonInstall->SetLabel(_("Download Selected Chart"));
        m_buttonInstall->Show();       
    }
    else if(chart->getChartStatus() == STAT_REQUESTABLE){
        m_buttonInstall->SetLabel(_("Download Selected Chart"));
        m_buttonInstall->Show();
    }
    else if(chart->getChartStatus() == STAT_PREPARING){
        m_buttonInstall->Hide();
    }
#endif    
    
}

    
bool shopPanel::doSystemNameWizard(  )
{
    // Make sure the system name array is current
    
    if( g_systemName.Len() && (g_systemNameChoiceArray.Index(g_systemName) == wxNOT_FOUND))
        g_systemNameChoiceArray.Insert(g_systemName, 0);
    
    oeRNCSystemNameSelector dlg( GetOCPNCanvasWindow());
    
    wxSize dialogSize(500, -1);
    
    #ifdef __OCPN__ANDROID__
    wxSize ss = ::wxGetDisplaySize();
    dialogSize.x = ss.x * 8 / 10;
    #endif         
    dlg.SetSize(dialogSize);
    dlg.Centre();
    
    
    #ifdef __OCPN__ANDROID__
    androidHideBusyIcon();
    #endif             
    int ret = dlg.ShowModal();
    
    if(ret == 0){               // OK
        wxString sName = dlg.getRBSelection();
        if(g_systemNameChoiceArray.Index(sName) == wxNOT_FOUND){
            // Is it the dongle selected?
            if(sName.Find(_T("Dongle")) != wxNOT_FOUND){
                wxString ssName = sName.Mid(0, 11);
                g_systemNameChoiceArray.Insert(ssName, 0);
                sName = ssName;
            }
            else{    
                sName = doGetNewSystemName();
                if(sName.Len())
                    g_systemNameChoiceArray.Insert(sName, 0);
                else
                    return false;
            }
        }
        if(sName.Len())
            g_systemName = sName;
    }
    else 
        return false;
    
    RefreshSystemName();
    
    saveShopConfig();
    
    return true;
}

wxString shopPanel::doGetNewSystemName( )
{
    oeRNCGETSystemName dlg( GetOCPNCanvasWindow());
    
    wxSize dialogSize(500, -1);
    
    #ifdef __OCPN__ANDROID__
    wxSize ss = ::wxGetDisplaySize();
    dialogSize.x = ss.x * 8 / 10;
    #endif         
    dlg.SetSize(dialogSize);
    dlg.Centre();
    
    
    int ret = dlg.ShowModal();
    
    wxString sName;
    if(ret == 0){               // OK
        sName = dlg.GetNewName();
        
        // Check system name rules...
        const char *s = sName.c_str();
        if( (strlen(s) < 3) || (strlen(s) > 15))
            return wxEmptyString;
        
        char *t = (char *)s;
        for(unsigned int i = 0; i < strlen(s); i++, t++){
            bool bok = false;
            if( ((*t >= 'a') && (*t <= 'z')) ||
                ((*t >= 'A') && (*t <= 'Z')) ||
                ((*t >= '0') && (*t <= '9')) ){
                
                bok = true;
            }
            else{
                sName.Clear();
                break;
            }
        }
    }
    
    
    return sName;
}

void shopPanel::OnGetNewSystemName( wxCommandEvent& event )
{
    doGetNewSystemName();
}

// void shopPanel::OnChangeSystemName( wxCommandEvent& event )
// {
//     doSystemNameWizard();
// }

   


IMPLEMENT_DYNAMIC_CLASS( oeRNCGETSystemName, wxDialog )
BEGIN_EVENT_TABLE( oeRNCGETSystemName, wxDialog )
   EVT_BUTTON( ID_GETIP_CANCEL, oeRNCGETSystemName::OnCancelClick )
   EVT_BUTTON( ID_GETIP_OK, oeRNCGETSystemName::OnOkClick )
END_EVENT_TABLE()
 
 
 oeRNCGETSystemName::oeRNCGETSystemName()
 {
 }
 
 oeRNCGETSystemName::oeRNCGETSystemName( wxWindow* parent, wxWindowID id, const wxString& caption,
                                             const wxPoint& pos, const wxSize& size, long style )
 {
     
     long wstyle = wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER;
     wxDialog::Create( parent, id, caption, pos, size, wstyle );
     
     wxFont *qFont = GetOCPNScaledFont_PlugIn(_("Dialog"));
     SetFont( *qFont );
     
     CreateControls();
     GetSizer()->SetSizeHints( this );
     Centre();
     
 }
 
 oeRNCGETSystemName::~oeRNCGETSystemName()
 {
     delete m_SystemNameCtl;
 }
 
 /*!
  * oeRNCGETSystemName creator
  */
 
 bool oeRNCGETSystemName::Create( wxWindow* parent, wxWindowID id, const wxString& caption,
                                    const wxPoint& pos, const wxSize& size, long style )
 {
     SetExtraStyle( GetExtraStyle() | wxWS_EX_BLOCK_EVENTS );
     
     long wstyle = style;
     #ifdef __WXMAC__
     wstyle |= wxSTAY_ON_TOP;
     #endif
     wxDialog::Create( parent, id, caption, pos, size, wstyle );
     
     wxFont *qFont = GetOCPNScaledFont_PlugIn(_("Dialog"));
     SetFont( *qFont );
     
     SetTitle( _("New OpenCPN oeSENC System Name"));
     
     CreateControls(  );
     Centre();
     return TRUE;
 }
 
 
 void oeRNCGETSystemName::CreateControls(  )
 {
     int ref_len = GetCharHeight();
     
     oeRNCGETSystemName* itemDialog1 = this;
     
     wxBoxSizer* itemBoxSizer2 = new wxBoxSizer( wxVERTICAL );
     itemDialog1->SetSizer( itemBoxSizer2 );
     
     wxStaticBox* itemStaticBoxSizer4Static = new wxStaticBox( itemDialog1, wxID_ANY, _("Enter New System Name") );
     
     wxStaticBoxSizer* itemStaticBoxSizer4 = new wxStaticBoxSizer( itemStaticBoxSizer4Static, wxVERTICAL );
     itemBoxSizer2->Add( itemStaticBoxSizer4, 0, wxEXPAND | wxALL, 5 );
     
     wxStaticText* itemStaticText5 = new wxStaticText( itemDialog1, wxID_STATIC, _T(""), wxDefaultPosition, wxDefaultSize, 0 );
     itemStaticBoxSizer4->Add( itemStaticText5, 0, wxALIGN_LEFT | wxLEFT | wxRIGHT | wxTOP, 5 );
     
     m_SystemNameCtl = new wxTextCtrl( itemDialog1, ID_GETIP_IP, _T(""), wxDefaultPosition, wxSize( ref_len * 10, -1 ), 0 );
     itemStaticBoxSizer4->Add( m_SystemNameCtl, 0,  wxALIGN_CENTER | wxLEFT | wxRIGHT | wxBOTTOM , 5 );
     
     wxStaticText *itemStaticTextLegend = new wxStaticText( itemDialog1, wxID_STATIC,  _("A valid System Name is 3 to 15 characters in length."));
     itemBoxSizer2->Add( itemStaticTextLegend, 0, wxALIGN_CENTER | wxLEFT | wxRIGHT | wxTOP, 5 );

//      wxStaticText *itemStaticTextLegend1 = new wxStaticText( itemDialog1, wxID_STATIC,  _("lower case letters and numbers only."));
//      itemBoxSizer2->Add( itemStaticTextLegend1, 0, wxALIGN_CENTER | wxLEFT | wxRIGHT | wxTOP, 5 );
     
     wxStaticText *itemStaticTextLegend2 = new wxStaticText( itemDialog1, wxID_STATIC,  _("No symbols or spaces are allowed."));
     itemBoxSizer2->Add( itemStaticTextLegend2, 0, wxALIGN_CENTER | wxLEFT | wxRIGHT | wxTOP, 5 );
     
     
     wxBoxSizer* itemBoxSizer16 = new wxBoxSizer( wxHORIZONTAL );
     itemBoxSizer2->Add( itemBoxSizer16, 0, wxALIGN_RIGHT | wxALL, 5 );
     
     {
         m_CancelButton = new wxButton( itemDialog1, ID_GETIP_CANCEL, _("Cancel"), wxDefaultPosition, wxDefaultSize, 0 );
         itemBoxSizer16->Add( m_CancelButton, 0, wxALIGN_CENTER_VERTICAL | wxALL, 5 );
         m_CancelButton->SetDefault();
     }
     
     m_OKButton = new wxButton( itemDialog1, ID_GETIP_OK, _("OK"), wxDefaultPosition,
                                wxDefaultSize, 0 );
     itemBoxSizer16->Add( m_OKButton, 0, wxALIGN_CENTER_VERTICAL | wxALL, 5 );
     
     
 }
 
 
 bool oeRNCGETSystemName::ShowToolTips()
 {
     return TRUE;
 }
 
 wxString oeRNCGETSystemName::GetNewName()
 {
     return m_SystemNameCtl->GetValue();
 }
 
 
 void oeRNCGETSystemName::OnCancelClick( wxCommandEvent& event )
 {
     EndModal(2);
 }
 
 void oeRNCGETSystemName::OnOkClick( wxCommandEvent& event )
 {
     if( m_SystemNameCtl->GetValue().Length() == 0 )
         EndModal(1);
     else {
         //g_systemNameChoiceArray.Insert(m_SystemNameCtl->GetValue(), 0);   // Top of the list
         
         EndModal(0);
     }
 }
 

 
 IMPLEMENT_DYNAMIC_CLASS( oeRNCSystemNameSelector, wxDialog )
 BEGIN_EVENT_TABLE( oeRNCSystemNameSelector, wxDialog )
    EVT_BUTTON( ID_GETIP_CANCEL, oeRNCSystemNameSelector::OnCancelClick )
    EVT_BUTTON( ID_GETIP_OK, oeRNCSystemNameSelector::OnOkClick )
 END_EVENT_TABLE()
 
 
 oeRNCSystemNameSelector::oeRNCSystemNameSelector()
 {
 }
 
 oeRNCSystemNameSelector::oeRNCSystemNameSelector( wxWindow* parent, wxWindowID id, const wxString& caption,
                                           const wxPoint& pos, const wxSize& size, long style )
 {
     
     long wstyle = wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER;
     wxDialog::Create( parent, id, caption, pos, size, wstyle );
     
     wxFont *qFont = GetOCPNScaledFont_PlugIn(_("Dialog"));
     SetFont( *qFont );
     
     CreateControls();
     GetSizer()->SetSizeHints( this );
     Centre();
     
 }
 
 oeRNCSystemNameSelector::~oeRNCSystemNameSelector()
 {
 }
 
  
 bool oeRNCSystemNameSelector::Create( wxWindow* parent, wxWindowID id, const wxString& caption,
                                   const wxPoint& pos, const wxSize& size, long style )
 {
     SetExtraStyle( GetExtraStyle() | wxWS_EX_BLOCK_EVENTS );
     
     long wstyle = style;
     #ifdef __WXMAC__
     wstyle |= wxSTAY_ON_TOP;
     #endif
     wxDialog::Create( parent, id, caption, pos, size, wstyle );
     
     wxFont *qFont = GetOCPNScaledFont_PlugIn(_("Dialog"));
     SetFont( *qFont );
     
     SetTitle( _("Select OpenCPN/oeSENC System Name"));
     
     CreateControls(  );
     Centre();
     return TRUE;
 }
 
 
 void oeRNCSystemNameSelector::CreateControls(  )
 {
     oeRNCSystemNameSelector* itemDialog1 = this;
     
     wxBoxSizer* itemBoxSizer2 = new wxBoxSizer( wxVERTICAL );
     itemDialog1->SetSizer( itemBoxSizer2 );
     
     
     wxStaticText* itemStaticText5 = new wxStaticText( itemDialog1, wxID_STATIC, _("Select your System Name from the following list, or "),
                                                       wxDefaultPosition, wxDefaultSize, 0 );
     
     itemBoxSizer2->Add( itemStaticText5, 0, wxALIGN_CENTER | wxLEFT | wxRIGHT | wxTOP, 5 );
 
     wxStaticText* itemStaticText6 = new wxStaticText( itemDialog1, wxID_STATIC, _(" create a new System Name for this computer."),
                                                       wxDefaultPosition, wxDefaultSize, 0 );
                                                       
     itemBoxSizer2->Add( itemStaticText6, 0, wxALIGN_CENTER | wxLEFT | wxRIGHT | wxTOP, 5 );
                                                       
     
     bool bDongleAdded = false;
     wxArrayString system_names;
     for(unsigned int i=0 ; i < g_systemNameChoiceArray.GetCount() ; i++){
         wxString candidate = g_systemNameChoiceArray.Item(i);
         if(candidate.StartsWith("sgl")){
             if(g_systemNameDisabledArray.Index(candidate) == wxNOT_FOUND){
                system_names.Add(candidate + _T(" (") + _("USB Key Dongle") + _T(")"));
                bDongleAdded = true;
             }
         }

         else if(g_systemNameDisabledArray.Index(candidate) == wxNOT_FOUND)
            system_names.Add(candidate);
     }
     
  
     // Add USB dongle if present, and not already added
     
     if(!bDongleAdded && IsDongleAvailable()){
        system_names.Add( g_dongleName  + _T(" (") + _("USB Key Dongle") + _T(")"));
     }
         
     system_names.Add(_("new..."));
     
     m_rbSystemNames = new wxRadioBox(this, wxID_ANY, _("System Names"), wxDefaultPosition, wxDefaultSize, system_names, 0, wxRA_SPECIFY_ROWS);
     
     itemBoxSizer2->Add( m_rbSystemNames, 0, wxALIGN_CENTER | wxALL, 25 );

     wxStaticLine* itemStaticLine = new wxStaticLine( this, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxLI_HORIZONTAL );
     itemBoxSizer2->Add( itemStaticLine, 0, wxEXPAND|wxALL, 0 );
     
     
     wxBoxSizer* itemBoxSizer16 = new wxBoxSizer( wxHORIZONTAL );
     itemBoxSizer2->Add( itemBoxSizer16, 0, wxALIGN_RIGHT | wxALL, 5 );
                                              
     m_CancelButton = new wxButton( itemDialog1, ID_GETIP_CANCEL, _("Cancel"), wxDefaultPosition, wxDefaultSize, 0 );
     itemBoxSizer16->Add( m_CancelButton, 0, wxALIGN_CENTER_VERTICAL | wxALL, 5 );
                                              
     m_OKButton = new wxButton( itemDialog1, ID_GETIP_OK, _("OK"), wxDefaultPosition, wxDefaultSize, 0 );
     m_OKButton->SetDefault();
     itemBoxSizer16->Add( m_OKButton, 0, wxALIGN_CENTER_VERTICAL | wxALL, 5 );
                                              
                                              
 }
 
 
 bool oeRNCSystemNameSelector::ShowToolTips()
 {
     return TRUE;
 }
 
 wxString oeRNCSystemNameSelector::getRBSelection(  )
 {
     return m_rbSystemNames->GetStringSelection();
 }
 
 void oeRNCSystemNameSelector::OnCancelClick( wxCommandEvent& event )
 {
     EndModal(2);
 }
 
 void oeRNCSystemNameSelector::OnOkClick( wxCommandEvent& event )
 {
     EndModal(0);
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
 //   OCPNMessageBox_PlugIn(NULL, _("DLSTART."), _("oeSENC_PI Message"), wxOK);
    g_shopPanel->m_startedDownload = true;
    g_shopPanel->m_buttonCancelOp->Show();

}

void OESENC_CURL_EvtHandler::onEndEvent(wxCurlEndPerformEvent &evt)
{
 //   OCPNMessageBox_PlugIn(NULL, _("DLEnd."), _("oeSENC_PI Message"), wxOK);
    
    g_ipGauge->SetValue(0);
    g_shopPanel->setStatusTextProgress(_T(""));
    g_shopPanel->setStatusText( _("Status: OK"));
    g_shopPanel->m_buttonCancelOp->Hide();
    //g_shopPanel->GetButtonDownload()->Hide();
    g_shopPanel->GetButtonUpdate()->Enable();
    
    if(downloadOutStream){
        downloadOutStream->Close();
        downloadOutStream = NULL;
    }
    
    g_curlDownloadThread = NULL;

    if(g_shopPanel->m_bAbortingDownload){
        if(g_shopPanel->GetSelectedChart()){
//             oitemChart *chart = g_shopPanel->GetSelectedChart()->m_pChart;
//             if(chart){
//                 chart->downloadingFile.Clear();
//             }
        }
    }
            
    //  Send an event to chain back to "Install" button
    wxCommandEvent event(wxEVT_COMMAND_BUTTON_CLICKED);
    event.SetId( ID_CMD_BUTTON_INSTALL_CHAIN );
    g_shopPanel->GetEventHandler()->AddPendingEvent(event);
    
}

double dl_now;
double dl_total;
time_t g_progressTicks;

void OESENC_CURL_EvtHandler::onProgressEvent(wxCurlDownloadEvent &evt)
{
    dl_now = evt.GetDownloadedBytes();
    dl_total = evt.GetTotalBytes();
    
    // Calculate the gauge value
    if(evt.GetTotalBytes() > 0){
        float progress = evt.GetDownloadedBytes()/evt.GetTotalBytes();
        g_ipGauge->SetValue(progress * 100);
    }
    
    wxDateTime now = wxDateTime::Now();
    if(now.GetTicks() != g_progressTicks){
        std::string speedString = evt.GetHumanReadableSpeed(" ", 0);
    
    //  Set text status
        wxString tProg;
        tProg = _("Downloaded:  ");
        wxString msg;
        msg.Printf( _T("%6.1f MiB / %4.0f MiB    "), (float)(evt.GetDownloadedBytes() / 1e6), (float)(evt.GetTotalBytes() / 1e6));
        msg += wxString( speedString.c_str(), wxConvUTF8);
        tProg += msg;

        g_shopPanel->setStatusTextProgress( tProg );
        
        g_progressTicks = now.GetTicks();
    }
    
}

//IMPLEMENT_DYNAMIC_CLASS( oeSENCLogin, wxDialog )
BEGIN_EVENT_TABLE( oeSENCLogin, wxDialog )
EVT_BUTTON( ID_GETIP_CANCEL, oeSENCLogin::OnCancelClick )
EVT_BUTTON( ID_GETIP_OK, oeSENCLogin::OnOkClick )
END_EVENT_TABLE()


oeSENCLogin::oeSENCLogin()
{
}

oeSENCLogin::oeSENCLogin( wxWindow* parent, wxWindowID id, const wxString& caption,
                                          const wxPoint& pos, const wxSize& size, long style )
{
    
    long wstyle = wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER;
    wxDialog::Create( parent, id, caption, pos, size, wstyle );
    
    wxFont *qFont = GetOCPNScaledFont_PlugIn(_("Dialog"));
    SetFont( *qFont );
    
    CreateControls();
    GetSizer()->SetSizeHints( this );
    Centre();
    
}

oeSENCLogin::~oeSENCLogin()
{
}

/*!
 * oeSENCLogin creator
 */

bool oeSENCLogin::Create( wxWindow* parent, wxWindowID id, const wxString& caption,
                                  const wxPoint& pos, const wxSize& size, long style )
{
    SetExtraStyle( GetExtraStyle() | wxWS_EX_BLOCK_EVENTS );
    
    long wstyle = style;
    #ifdef __WXMAC__
    wstyle |= wxSTAY_ON_TOP;
    #endif
    wxDialog::Create( parent, id, caption, pos, size, wstyle );
    
    wxFont *qFont = GetOCPNScaledFont_PlugIn(_("Dialog"));
    SetFont( *qFont );
    
    
    CreateControls(  );
    Centre();
    return TRUE;
}


void oeSENCLogin::CreateControls(  )
{
    int ref_len = GetCharHeight();
    
    oeSENCLogin* itemDialog1 = this;
    
    wxBoxSizer* itemBoxSizer2 = new wxBoxSizer( wxVERTICAL );
    itemDialog1->SetSizer( itemBoxSizer2 );
    
    wxStaticBox* itemStaticBoxSizer4Static = new wxStaticBox( itemDialog1, wxID_ANY, _("Login to o-charts.org") );
    
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
    
    m_PasswordCtl = new wxTextCtrl( itemDialog1, ID_GETIP_IP, _T(""), wxDefaultPosition, wxSize( ref_len * 10, -1 ), 0 );
    flexGridSizerActionStatus->Add( m_PasswordCtl, 0,  wxALIGN_CENTER | wxLEFT | wxRIGHT | wxBOTTOM , 5 );
    
    
    wxBoxSizer* itemBoxSizer16 = new wxBoxSizer( wxHORIZONTAL );
    itemBoxSizer2->Add( itemBoxSizer16, 0, wxALIGN_RIGHT | wxALL, 5 );
    
    m_CancelButton = new wxButton( itemDialog1, ID_GETIP_CANCEL, _("Cancel"), wxDefaultPosition, wxDefaultSize, 0 );
    itemBoxSizer16->Add( m_CancelButton, 0, wxALIGN_CENTER_VERTICAL | wxALL, 5 );
    
    m_OKButton = new wxButton( itemDialog1, ID_GETIP_OK, _("OK"), wxDefaultPosition, wxDefaultSize, 0 );
    m_OKButton->SetDefault();
    
    itemBoxSizer16->Add( m_OKButton, 0, wxALIGN_CENTER_VERTICAL | wxALL, 5 );
    
    
}


bool oeSENCLogin::ShowToolTips()
{
    return TRUE;
}



void oeSENCLogin::OnCancelClick( wxCommandEvent& event )
{
    EndModal(2);
}

void oeSENCLogin::OnOkClick( wxCommandEvent& event )
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

void oeSENCLogin::OnClose( wxCloseEvent& event )
{
    SetReturnCode(2);
}

