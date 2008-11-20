#ifndef UPPMAINWINDOW_CALLBACK_H
#define UPPMAINWINDOW_CALLBACK_H

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif
#include <wx/wx.h>


#include <wx/config.h>
#include <wx/confbase.h>

#if wxCHECK_VERSION(2,7,1) //about dialog only implemented from wxWidgets v2.7.1
#include <wx/aboutdlg.h>
#else
#warning About dialog not implemented, use a newer wxWidgets version!
#endif
//#include <wx/utils.h>
#include <iostream>

using namespace std;

#include "uppmainwindow.h"
#include "hardware.h"
#include "pictype.h"
#include "read_hexfile.h"
#include "preferences.h"

#ifdef __WXMAC__
#define EVENT_FIX 
#else //__WXMAC__
#define EVENT_FIX event.Skip();
#endif //__WXMAC__



class UppMainWindowCallBack: public UppMainWindow
{
	public:
		 
		UppMainWindowCallBack(wxWindow* parent, wxWindowID id = wxID_ANY, const wxString& title = wxT("usbpicprog"), const wxPoint& pos = wxDefaultPosition, const wxSize& size = wxSize( 634,361 ), long style = wxDEFAULT_FRAME_STYLE|wxTAB_TRAVERSAL  ) ;
		~UppMainWindowCallBack();
		void updateProgress(int value);
		void printHexFile();
		void on_new( wxCommandEvent& event ){upp_new(); EVENT_FIX}
		void on_open( wxCommandEvent& event ){upp_open(); EVENT_FIX};
		void on_refresh( wxCommandEvent& event ){upp_refresh(); EVENT_FIX};
		void on_save( wxCommandEvent& event ){upp_save(); EVENT_FIX};
		void on_save_as( wxCommandEvent& event ){upp_save_as(); EVENT_FIX};
		void on_exit( wxCommandEvent& event ){upp_exit(); EVENT_FIX};
		void on_copy( wxCommandEvent& event ){upp_copy(); EVENT_FIX};
		void on_selectall( wxCommandEvent& event ){upp_selectall(); EVENT_FIX};
		void on_program( wxCommandEvent& event ){upp_program(); EVENT_FIX};
		void on_read( wxCommandEvent& event ){upp_read(); EVENT_FIX};
		void on_verify( wxCommandEvent& event ){upp_verify(); EVENT_FIX};
		void on_erase( wxCommandEvent& event ){upp_erase(); EVENT_FIX};
		void on_blankcheck( wxCommandEvent& event ){upp_blankcheck(); EVENT_FIX};
		void on_autodetect( wxCommandEvent& event ){upp_autodetect(); EVENT_FIX};
		void on_connect( wxCommandEvent& event){upp_connect(); EVENT_FIX};
		void on_connect_boot( wxCommandEvent& event){upp_connect_boot(); EVENT_FIX};
		void on_disconnect( wxCommandEvent& event ){upp_disconnect(); EVENT_FIX};
		void on_preferences( wxCommandEvent& event ){upp_preferences(); EVENT_FIX};		
		void on_help( wxCommandEvent& event ){upp_help(); EVENT_FIX};
		void on_about( wxCommandEvent& event ){upp_about(); EVENT_FIX};
		void on_combo_changed( wxCommandEvent& event ){upp_combo_changed(); EVENT_FIX};
		void upp_open_file(wxString path);
		void upp_new();
	private:
		void upp_open();
		void upp_refresh();
		void upp_save();
		void upp_save_as();
		void upp_exit();
		void upp_copy();
		void upp_selectall();        		
		void upp_program();
		void upp_read();
		void upp_verify();
		void upp_erase();
		void upp_blankcheck();
		bool upp_autodetect();
		bool upp_connect();
		bool upp_connect_boot();
		void upp_disconnect();
		void upp_preferences();		
		void upp_help();
		void upp_about();
		void upp_combo_changed();
		void upp_update_hardware_type();
		ReadHexFile* readHexFile;
		PicType* picType;
		Hardware* hardware;
		bool fileOpened;
		void OnSize(wxSizeEvent& event);
		wxConfig* uppConfig;
		wxString defaultPath;
	    ConfigFields configFields;
		
};
#endif