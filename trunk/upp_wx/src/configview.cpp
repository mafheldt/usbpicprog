/***************************************************************************
*   Copyright (C) 2008-2009 by Frans Schreuder, Francesco Montorsi        *
*   usbpicprog.sourceforge.net                                            *
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
***************************************************************************/

// NOTE: to avoid lots of warnings with MSVC 2008 about deprecated CRT functions
//       it's important to include wx/defs.h before STL headers
#include <wx/defs.h>

#include <wx/panel.h>
#include <wx/sizer.h>
#include <wx/stattext.h>
#include <wx/choice.h>

#include "configview.h"
#include "uppmainwindow.h"


// ----------------------------------------------------------------------------
// UppConfigViewBook
// ----------------------------------------------------------------------------

UppConfigViewBook::UppConfigViewBook(wxWindow* parent, wxWindowID id)
    : wxListbook( parent, id, wxDefaultPosition, wxDefaultSize, wxLB_DEFAULT )
{
}

UppConfigViewBook::~UppConfigViewBook()
{
}

void UppConfigViewBook::SetHexFile(HexFile* hex, const Pic& pic)
{
    m_pic = pic;
    m_hexFile = hex;

    // reset current contents
    DeleteAllPages();

    // add new GUI selectors for the config flags of this PIC
    for (unsigned int i=0; i<m_pic.Config.size(); i++)
    {
        const ConfigWord& block = m_pic.Config[i];
        if (block.Masks.size() == 0)
            continue;       // skip this block

        wxPanel *panel = new wxPanel(this, wxID_ANY);
        wxFlexGridSizer *sz = new wxFlexGridSizer(2 /* num of columns */,
                                                  10, 10 /* h and vgap */);

        for (unsigned int j=0; j<block.Masks.size(); j++)
        {
            const ConfigMask& mask = block.Masks[j];

            sz->Add(new wxStaticText(panel, wxID_ANY, mask.Name + ":"),
                    0, wxLEFT|wxALIGN_CENTER, 5);

            // NOTE: we give each wxChoice we build the name of the mask it controls;
            //       in this way from OnChange() we can easily find out which object
            //       is sending the notification
            wxChoice *choice =
                new wxChoice(panel, wxID_ANY, wxDefaultPosition, wxDefaultSize,
                             mask.GetStringValues(), 0, wxDefaultValidator, mask.Name);

            // set the 
            
            unsigned int ConfigWord=0, ConfigWordMask=0;
            if (m_pic.is16Bit())
            {
                if ((i+1)<=(hex->getConfigMemory().size()))
                {
                    ConfigWord=hex->getConfigMemory()[i];
                    cout<<"ConfigWord: "<<std::hex<<ConfigWord<<endl;
                }
            }
            else
            {
                if ((2*i+1)<=(hex->getConfigMemory().size()))
                {
                    ConfigWord=((hex->getConfigMemory()[i*2])|(hex->getConfigMemory()[i*2+1]<<8));
                    cout<<"ConfigWord: "<<std::hex<<ConfigWord<<endl;
                }
            }

            for (unsigned int k=0;k<mask.Values.size();k++)
            {
                ConfigWordMask|=mask.Values[k].Value;
            }

            choice->SetSelection(0);
            for (unsigned int k=0; k<mask.Values.size(); k++)
            {
                if((ConfigWord&ConfigWordMask)==mask.Values[k].Value)
                {
                    choice->SetSelection(k);
                }
            }
            
            choice->Connect(wxEVT_COMMAND_CHOICE_SELECTED, 
                            wxCommandEventHandler(UppConfigViewBook::OnChange), 
                            NULL, this);

            sz->Add(choice, 0, wxRIGHT|wxALIGN_CENTER|wxEXPAND, 5);
        }

        sz->AddGrowableCol(1, 1);
        panel->SetSizerAndFit(sz);

        AddPage(panel, block.Name.empty() ? wxString::Format("Word %d", i) : block.Name);
    }
}

void UppConfigViewBook::OnChange(wxCommandEvent& event)
{
    wxChoice *choice = dynamic_cast<wxChoice*>(event.GetEventObject());
    wxASSERT(choice);

    // get the block the user is currently viewing/editing
    const ConfigWord& block = m_pic.Config[GetSelection()];

    // find the config mask which was changed
    const ConfigMask* mask = NULL;
    unsigned int SelectedMask;
    for (unsigned int i=0; i<block.Masks.size(); i++)
    {
        if (block.Masks[i].Name == choice->GetName())
        {
            mask = &block.Masks[i];
            //SelectedMask=i;
            break;
        }
    }
    SelectedMask=GetSelection();
    wxASSERT(mask);
    int newConfigValue = mask->Values[choice->GetSelection()].Value;
    int ConfigWord = 0; 
    if(m_pic.is16Bit())
    {
        if((SelectedMask+1)<=(m_hexFile->getConfigMemory().size()))
        {
            ConfigWord=m_hexFile->getConfigMemory()[SelectedMask];
        
        }
    }
    else
    {
        if((2*SelectedMask+1)<=(m_hexFile->getConfigMemory().size()))
        {
            ConfigWord=((m_hexFile->getConfigMemory()[SelectedMask*2])|
                        (m_hexFile->getConfigMemory()[SelectedMask*2+1]<<8));
        
        }
    }

    for (unsigned int i=0; i<mask->Values.size();i++)
    {
        ConfigWord &= ~mask->Values[i].Value;
    }
    ConfigWord |= newConfigValue;
    cout<<"newConfigValue for byte"<<SelectedMask<<": "<<hex<<ConfigWord<<endl;
    if(m_pic.is16Bit())
    {
        m_hexFile->putConfigMemory(SelectedMask,ConfigWord&0xFF);
    }
    else
    {
        m_hexFile->putConfigMemory(SelectedMask*2,ConfigWord&0xFF);
        m_hexFile->putConfigMemory(SelectedMask*2+1,(ConfigWord&0xFF00)>>8);
    }
    // notify the main window about this change
    UppMainWindow* main = dynamic_cast<UppMainWindow*>(wxTheApp->GetTopWindow());
    wxASSERT(main);
    main->upp_hex_changed();
}
