/**************************************************************/
/*  librairy editor: edition of component general properties  */
/**************************************************************/

#include "fctsys.h"
#include "appl_wxstruct.h"
#include "common.h"
#include "class_drawpanel.h"
#include "confirm.h"
#include "gestfich.h"

#include "program.h"
#include "general.h"
#include "protos.h"
#include "libeditframe.h"
#include "class_library.h"
#include "eeschema_id.h"


/* Dialog box to edit a libentry (a component in library) properties */

/* Creates a NoteBook dialog
 *  Edition:
 *  Doc and keys words
 *  Parts per package
 *  General properties
 * Fields are NOT edited here. There is a specific dialog box to do that
 */

#include "dialog_edit_component_in_lib.h"


void WinEDA_LibeditFrame::OnEditComponentProperties( wxCommandEvent& event )
{
    bool partLocked = GetComponent()->UnitsLocked();
    EditComponentProperties();
    if( partLocked != GetComponent()->UnitsLocked() )
    {   // g_EditPinByPinIsOn is set to the better value,
        // if m_UnitSelectionLocked has changed
        g_EditPinByPinIsOn = GetComponent()->UnitsLocked() ? true : false;
        m_HToolBar->ToggleTool( ID_LIBEDIT_EDIT_PIN_BY_PIN, g_EditPinByPinIsOn );
    }

    m_HToolBar->Refresh();
    DrawPanel->Refresh();
}


void WinEDA_LibeditFrame::EditComponentProperties()
{
    DIALOG_EDIT_COMPONENT_IN_LIBRARY dlg( this );

    if( dlg.ShowModal() == wxID_CANCEL )
        return;

    UpdateAliasSelectList();
    UpdatePartSelectList();
    DisplayLibInfos();
    DisplayCmpDoc();
    OnModify( );
}



void DIALOG_EDIT_COMPONENT_IN_LIBRARY::OnOkClick( wxCommandEvent& event )
{

    /* Update the doc, keyword and doc filename strings */
    int index;
    LIB_ALIAS* alias;
    LIB_COMPONENT* component = m_Parent->GetComponent();

    if( component == NULL )
    {
        EndModal( wxID_CANCEL );
        return;
    }

    m_Parent->SaveCopyInUndoList( component );

    alias = component->GetAlias( m_Parent->GetAliasName() );

    wxCHECK_RET( alias != NULL,
                 wxT( "Alias \"" ) + m_Parent->GetAliasName() + wxT( "\" of component \"" ) +
                 component->GetName() + wxT( "\" does not exist." ) );

    alias->SetDescription( m_DocCtrl->GetValue() );
    alias->SetKeyWords( m_KeywordsCtrl->GetValue() );
    alias->SetDocFileName( m_DocfileCtrl->GetValue() );

    component->SetAliases( m_PartAliasListCtrl->GetStrings() );

    index = m_SelNumberOfUnits->GetValue();
    ChangeNbUnitsPerPackage( index );

    if( m_AsConvertButt->GetValue() )
    {
        if( !m_Parent->GetShowDeMorgan() )
        {
            m_Parent->SetShowDeMorgan( true );
            SetUnsetConvert();
        }
    }
    else
    {
        if( m_Parent->GetShowDeMorgan() )
        {
            m_Parent->SetShowDeMorgan( false );
            SetUnsetConvert();
        }
    }

    component->SetShowPinNumbers( m_ShowPinNumButt->GetValue() );
    component->SetShowPinNames( m_ShowPinNameButt->GetValue() );

    if( m_PinsNameInsideButt->GetValue() == false )
        component->SetPinNameOffset( 0 );       // pin text outside the body (name is on the pin)
    else
    {
        component->SetPinNameOffset( m_SetSkew->GetValue() );
        // Ensure component->m_TextInside != 0, because the meaning is "text outside".
        if( component->GetPinNameOffset() == 0 )
            component->SetPinNameOffset( 20 );  // give a reasonnable value
    }

    if( m_OptionPower->GetValue() == true )
        component->SetPower();
    else
        component->SetNormal();

    /* Set the option "Units locked".
     *  Obviously, cannot be true if there is only one part */
    component->LockUnits( m_OptionPartsLocked->GetValue() );
    if( component->GetPartCount() <= 1 )
        component->LockUnits( false );

    /* Update the footprint filter list */
    component->GetFootPrints().Clear();
    component->GetFootPrints() = m_FootprintFilterListBox->GetStrings();

    EndModal( wxID_OK );
}


void DIALOG_EDIT_COMPONENT_IN_LIBRARY::CopyDocToAlias( wxCommandEvent& WXUNUSED (event) )
{
    LIB_COMPONENT* component = m_Parent->GetComponent();

    if( component == NULL )
        return;

    m_DocCtrl->SetValue( component->GetDescription() );
    m_DocfileCtrl->SetValue( component->GetDocFileName() );
    m_KeywordsCtrl->SetValue( component->GetKeyWords() );
}


void DIALOG_EDIT_COMPONENT_IN_LIBRARY::DeleteAllAliasOfPart( wxCommandEvent& WXUNUSED (event) )
{
    if( m_PartAliasListCtrl->FindString( m_Parent->GetAliasName() ) != wxNOT_FOUND )
    {
        wxString msg;
        msg.Printf( _( "Alias <%s> cannot be removed while it is being edited!" ),
                    GetChars( m_Parent->GetAliasName() ) );
        DisplayError( this, msg );
        return;
    }

    if( IsOK( this, _( "Remove all aliases from list?" ) ) )
    {
        m_PartAliasListCtrl->Clear();
        m_ButtonDeleteAllAlias->Enable( false );
        m_ButtonDeleteOneAlias->Enable( false );
    }
}


/* Add a new name to the alias list box
 *  New name cannot be the root name, and must not exists
 */
void DIALOG_EDIT_COMPONENT_IN_LIBRARY::AddAliasOfPart( wxCommandEvent& WXUNUSED (event) )
{
    wxString aliasname;
    LIB_COMPONENT* component = m_Parent->GetComponent();
    CMP_LIBRARY* library = m_Parent->GetLibrary();

    if( component == NULL )
        return;

    wxTextEntryDialog dlg( this, _( "New alias:" ), _( "Component Alias" ), aliasname );
    if( dlg.ShowModal() != wxID_OK )
        return; // cancelled by user

    aliasname = dlg.GetValue( );

    aliasname.Replace( wxT( " " ), wxT( "_" ) );
    if( aliasname.IsEmpty() )
        return;

    if( m_PartAliasListCtrl->FindString( aliasname ) != wxNOT_FOUND
        || library->FindEntry( aliasname ) != NULL )
    {
        wxString msg;
        msg.Printf( _( "Alias or component name <%s> already exists in library <%s>." ),
                    GetChars( aliasname ),
                    GetChars( library->GetName() ) );
        DisplayError( this, msg );
        return;
    }

    m_PartAliasListCtrl->Append( aliasname );
    if( m_Parent->GetAliasName().CmpNoCase( component->GetName() ) == 0 )
        m_ButtonDeleteAllAlias->Enable( true );
    m_ButtonDeleteOneAlias->Enable( true );
}


void DIALOG_EDIT_COMPONENT_IN_LIBRARY::DeleteAliasOfPart( wxCommandEvent& WXUNUSED (event) )
{
    wxString aliasname = m_PartAliasListCtrl->GetStringSelection();

    if( aliasname.IsEmpty() )
        return;
    if( aliasname.CmpNoCase( m_Parent->GetAliasName() ) == 0 )
    {
        wxString msg;
        msg.Printf( _( "Alias <%s> cannot be removed while it is being edited!" ),
                    GetChars( aliasname ) );
        DisplayError( this, msg );
        return;
    }

    m_PartAliasListCtrl->Delete( m_PartAliasListCtrl->GetSelection() );
    LIB_COMPONENT* component = m_Parent->GetComponent();
    if( component )
        component->RemoveAlias( aliasname );

    if( m_PartAliasListCtrl->IsEmpty() )
    {
        m_ButtonDeleteAllAlias->Enable( false );
        m_ButtonDeleteOneAlias->Enable( false );
    }
}


/*
 * Change the number of parts per package.
 */
bool DIALOG_EDIT_COMPONENT_IN_LIBRARY::ChangeNbUnitsPerPackage( int MaxUnit )
{
    LIB_COMPONENT* component = m_Parent->GetComponent();

    if( component == NULL || component->GetPartCount() == MaxUnit || MaxUnit < 1 )
        return false;

    if( MaxUnit < component->GetPartCount()
        && !IsOK( this, _( "Delete extra parts from component?" ) ) )
        return false;

    component->SetPartCount( MaxUnit );
    return true;
}


/*
 * Set or clear the component alternate body style ( DeMorgan ).
 */
bool DIALOG_EDIT_COMPONENT_IN_LIBRARY::SetUnsetConvert()
{
    LIB_COMPONENT* component = m_Parent->GetComponent();

    if( component == NULL
        || ( m_Parent->GetShowDeMorgan() == component->HasConversion() ) )
        return false;

    if( m_Parent->GetShowDeMorgan() )
    {
        if( !IsOK( this, _( "Add new pins for alternate body style ( DeMorgan ) to component?" ) ) )
            return false;
    }
    else if(  component->HasConversion() )
    {
        if( !IsOK( this, _( "Delete alternate body style (DeMorgan) draw items from component?" ) ) )
        {
            m_Parent->SetShowDeMorgan( true );
            return false;
        }
    }

    component->SetConversion( m_Parent->GetShowDeMorgan() );
    m_Parent->OnModify();

    return true;
}


void DIALOG_EDIT_COMPONENT_IN_LIBRARY::BrowseAndSelectDocFile( wxCommandEvent& event )
{
    wxString FullFileName, mask;
    wxString docpath, filename;

    docpath = wxGetApp().ReturnLastVisitedLibraryPath( wxT( "doc" ) );

    mask = wxT( "*" );
    FullFileName = EDA_FileSelector( _( "Doc Files" ),
                                     docpath,       /* Chemin par defaut */
                                     wxEmptyString, /* nom fichier par defaut */
                                     wxEmptyString, /* extension par defaut */
                                     mask,          /* Masque d'affichage */
                                     this,
                                     wxFD_OPEN,
                                     true
                                     );
    if( FullFileName.IsEmpty() )
        return;

    /* If the path is already in the library search paths
     * list, just add the library name to the list.  Otherwise, add
     * the library name with the full or relative path.
     * the relative path, when possible is preferable,
     * because it preserve use of default libraries paths, when the path is a sub path of
     * these default paths
     */
    wxFileName fn = FullFileName;
    wxGetApp().SaveLastVisitedLibraryPath( fn.GetPath() );

    filename = wxGetApp().ReturnFilenameWithRelativePathInLibPath( FullFileName );
    // Filenames are always stored in unix like mode, ie separator "\" is stored as "/"
    // to ensure files are identical under unices and windows
#ifdef __WINDOWS__
    filename.Replace( wxT( "\\" ), wxT( "/" ) );
#endif
    m_DocfileCtrl->SetValue( filename );
}


void DIALOG_EDIT_COMPONENT_IN_LIBRARY::DeleteAllFootprintFilter( wxCommandEvent& WXUNUSED (event) )
{
    if( IsOK( this, _( "Ok to Delete FootprintFilter LIST" ) ) )
    {
        m_FootprintFilterListBox->Clear();
        m_ButtonDeleteAllFootprintFilter->Enable( false );
        m_ButtonDeleteOneFootprintFilter->Enable( false );
    }
}


/* Add a new name to the footprint filter list box
 * Obvioulsy, cannot be void
 */
void DIALOG_EDIT_COMPONENT_IN_LIBRARY::AddFootprintFilter( wxCommandEvent& WXUNUSED (event) )
{
    wxString Line;
    LIB_COMPONENT* component = m_Parent->GetComponent();

    if( component == NULL )
        return;

    wxTextEntryDialog dlg( this, _( "Add Footprint Filter" ), _( "Footprint Filter" ), Line );
    if( dlg.ShowModal() != wxID_OK )
        return; // cancelled by user

    Line = dlg.GetValue();
    Line.Replace( wxT( " " ), wxT( "_" ) );

    if( Line.IsEmpty() )
        return;

    /* test for an existing name: */
    int index = m_FootprintFilterListBox->FindString( Line );

    if( index != wxNOT_FOUND )
    {
        wxString msg;

        msg.Printf( _( "Foot print filter <%s> is already defined." ), GetChars( Line ) );
        DisplayError( this, msg );
        return;
    }

    m_FootprintFilterListBox->Append( Line );
    m_ButtonDeleteAllFootprintFilter->Enable( true );
    m_ButtonDeleteOneFootprintFilter->Enable( true );
}


void DIALOG_EDIT_COMPONENT_IN_LIBRARY::DeleteOneFootprintFilter( wxCommandEvent& WXUNUSED (event) )
{
    LIB_COMPONENT* component = m_Parent->GetComponent();
    int ii = m_FootprintFilterListBox->GetSelection();

    m_FootprintFilterListBox->Delete( ii );

    if( !component || (m_FootprintFilterListBox->GetCount() == 0) )
    {
        m_ButtonDeleteAllFootprintFilter->Enable( false );
        m_ButtonDeleteOneFootprintFilter->Enable( false );
    }
}
