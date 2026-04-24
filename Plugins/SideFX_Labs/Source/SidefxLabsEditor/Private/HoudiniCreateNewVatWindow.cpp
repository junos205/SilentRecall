/*
* Copyright (c) 2025 Side Effects Software Inc.  All rights reserved.
*
* Redistribution and use of in source and binary forms, with or without
* modification, are permitted provided that the following conditions are met:
*
* 1. Redistributions of source code must retain the above copyright notice,
* this list of conditions and the following disclaimer.
*
* 2. The name of Side Effects Software may not be used to endorse or
* promote products derived from this software without specific prior
* written permission.
*
* THIS SOFTWARE IS PROVIDED BY SIDE EFFECTS SOFTWARE `AS IS' AND ANY EXPRESS
* OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
* OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN
* NO EVENT SHALL SIDE EFFECTS SOFTWARE BE LIABLE FOR ANY DIRECT, INDIRECT,
* INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
* LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA,
* OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
* LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
* NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
* EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#include "HoudiniCreateNewVatWindow.h"
#include "HoudiniCreateNewVatWindowParameters.h"
#include "SidefxLabsEditorUtils.h"
#include "VatImporter/HoudiniVatImporter.h"

#include "PropertyEditorModule.h"
#include "IDetailsView.h"
#include "Modules/ModuleManager.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Docking/SDockTab.h"
#include "Framework/Docking/TabManager.h"

#define LOCTEXT_NAMESPACE "FHoudiniCreateNewVatWindow"

DEFINE_LOG_CATEGORY(LogSidefxLabsEditor);

void FHoudiniCreateNewVatWindow::OpenPropertyEditorWindow()
{
	FGlobalTabmanager::Get()->TryInvokeTab(FName("CreateNewVATTab"));
}

TSharedRef<IDetailCustomization> FHoudiniCreateNewVatWindow::MakeInstance()
{
	return MakeShareable(new FHoudiniCreateNewVatWindow());
}

TSharedRef<SDockTab> FHoudiniCreateNewVatWindow::CreatePropertyEditorTab(const FSpawnTabArgs& Args)
{
	UCreateNewVatProperties* VatProperties = NewObject<UCreateNewVatProperties>();
	if (!VatProperties)
	{
		UE_LOG(LogSidefxLabsEditor, Error, TEXT("Failed to create VAT properties object"));
		return SNew(SDockTab).Label(LOCTEXT("CreateNewVATEditorTitle", "Create New VAT"));
	}

	UHoudiniVatImporter* VatImporter = NewObject<UHoudiniVatImporter>();
	if (!VatImporter)
	{
		UE_LOG(LogSidefxLabsEditor, Error, TEXT("Failed to create VAT importer object"));
		return SNew(SDockTab).Label(LOCTEXT("CreateNewVATEditorTitle", "Create New VAT"));
	}

	VatImporter->SetProperties(VatProperties);

	TSharedRef<IDetailsView> DetailsView = FHoudiniCreateNewVatWindow::CreateDetailsView(VatProperties);
	TSharedRef<SDockTab> NewTab = SNew(SDockTab).Label(LOCTEXT("CreateNewVATEditorTitle", "Create New VAT"));

	TWeakPtr<SDockTab> TabPtr = NewTab;

	NewTab->SetContent(FHoudiniCreateNewVatWindow::CreateTabContent(DetailsView, VatImporter, VatProperties, TabPtr));

	return NewTab;
}

void FHoudiniCreateNewVatWindow::CustomizeDetails(IDetailLayoutBuilder& DetailBuilder)
{
}

TSharedRef<IDetailsView> FHoudiniCreateNewVatWindow::CreateDetailsView(UCreateNewVatProperties* VatProperties)
{
	FPropertyEditorModule& PropertyEditorModule = FModuleManager::LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");

	FDetailsViewArgs DetailsViewArgs;
	DetailsViewArgs.bShowOptions = false;
	DetailsViewArgs.bAllowSearch = false;
	DetailsViewArgs.bHideSelectionTip = true;
	DetailsViewArgs.bShowObjectLabel = false;

	TSharedRef<IDetailsView> DetailsView = PropertyEditorModule.CreateDetailView(DetailsViewArgs);
	DetailsView->SetObject(VatProperties);

	return DetailsView;
}

TSharedRef<SWidget> FHoudiniCreateNewVatWindow::CreateTabContent(
	TSharedRef<IDetailsView> DetailsView,
	UHoudiniVatImporter* VatImporter, 
	UCreateNewVatProperties* VatProperties, 
	TWeakPtr<SDockTab> TabPtr)
{
	return SNew(SVerticalBox)
		+ SVerticalBox::Slot()
		.FillHeight(1.0f)
		.Padding(5)
		[
			DetailsView
		]
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(5)
		[
			CreateVatButton(VatImporter, VatProperties, TabPtr)
		];
}

TSharedRef<SWidget> FHoudiniCreateNewVatWindow::CreateVatButton(
	UHoudiniVatImporter* VatImporter,
	UCreateNewVatProperties* VatProperties,
	TWeakPtr<SDockTab> TabPtr)
{
	return SNew(SBox)
        .HeightOverride(35)
		[
			SNew(SButton)
			.Text(LOCTEXT("CreateNewVatButton", "Create New VAT"))
			.HAlign(HAlign_Center)
			.VAlign(VAlign_Center)
			.OnClicked_Static(&FHoudiniCreateNewVatWindow::OnCreateVatClicked, VatImporter, VatProperties, TabPtr)
		];
}

FReply FHoudiniCreateNewVatWindow::OnCreateVatClicked(
	UHoudiniVatImporter* VatImporter, 
	UCreateNewVatProperties* VatProperties, 
	TWeakPtr<SDockTab> TabPtr)
{
    if (!IsValid(VatImporter) || !IsValid(VatProperties))
	{
		UE_LOG(LogSidefxLabsEditor, Error, TEXT("FHoudiniCreateNewVatWindow::OnCreateVatClicked: VatImporter or VatProperties are invalid"));
		return FReply::Handled();
	}

	UE_LOG(LogSidefxLabsEditor, Log, TEXT("Creating VAT"));

    VatImporter->ImportFiles();

    if (VatImporter->bCanceled)
    {
        if (TSharedPtr<SDockTab> Tab = TabPtr.Pin())
        {
            Tab->RequestCloseTab();
        }
        return FReply::Handled();
    }

	VatImporter->CreateVatMaterial();

    if (!VatImporter->Material.IsValid())
    {
        UE_LOG(LogSidefxLabsEditor, Error, TEXT("FHoudiniCreateNewVatWindow::OnCreateVatClicked: Material creation failed"));
        if (TSharedPtr<SDockTab> Tab = TabPtr.Pin())
        {
            Tab->RequestCloseTab();
        }
        return FReply::Handled();
    }

	VatImporter->CreateVatMaterialInstance();

    if (!VatImporter->MaterialInstance.IsValid())
    {
        UE_LOG(LogSidefxLabsEditor, Error, TEXT("FHoudiniCreateNewVatWindow::OnCreateVatClicked: Material Instance creation failed."));
        if (TSharedPtr<SDockTab> Tab = TabPtr.Pin())
        {
            Tab->RequestCloseTab();
        }
        return FReply::Handled();
    }
	
	if (VatProperties->bCreateVatBlueprint)
	{
		VatImporter->CreateVatBlueprint();
	}

	VatImporter->RecompileVatMaterial();

	if (TSharedPtr<SDockTab> Tab = TabPtr.Pin())
	{
		Tab->RequestCloseTab();
	}
	return FReply::Handled();
}

#undef LOCTEXT_NAMESPACE
