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

#pragma once

#include "CoreMinimal.h"
#include "IDetailCustomization.h"

class IDetailLayoutBuilder;
class UHoudiniVatImporter;
class UCreateNewVatProperties;
class SDockTab;
class SWidget;
class FSpawnTabArgs;

/**
 * Custom detail panel for creating new VAT assets.
 * Manages the VAT creation window and workflow.
 */
class FHoudiniCreateNewVatWindow : public IDetailCustomization
{
public:
	/** Opens the VAT property editor window. */
	static void OpenPropertyEditorWindow();

	/** Creates a new instance of this detail customization. */
	static TSharedRef<IDetailCustomization> MakeInstance();

	/** Creates the property editor tab. */
	static TSharedRef<SDockTab> CreatePropertyEditorTab(const FSpawnTabArgs& Args);

	// IDetailCustomization interface.
	virtual void CustomizeDetails(IDetailLayoutBuilder& DetailBuilder) override;

private:
	/** Creates the details view for VAT properties. */
	static TSharedRef<class IDetailsView> CreateDetailsView(UCreateNewVatProperties* VatProperties);

	/** Creates the tab content widget. */
	static TSharedRef<SWidget> CreateTabContent(
		TSharedRef<class IDetailsView> DetailsView,
		UHoudiniVatImporter* VatImporter, 
		UCreateNewVatProperties* VatProperties, 
		TWeakPtr<SDockTab> TabPtr);

	/** Creates the VAT creation button widget. */
	static TSharedRef<SWidget> CreateVatButton(
		UHoudiniVatImporter* VatImporter, 
		UCreateNewVatProperties* VatProperties, 
		TWeakPtr<SDockTab> TabPtr);

	/** Handles the VAT creation button click. */
	static FReply OnCreateVatClicked(
		UHoudiniVatImporter* VatImporter, 
		UCreateNewVatProperties* VatProperties, 
		TWeakPtr<SDockTab> TabPtr);
};
