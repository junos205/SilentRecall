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
#include "Types/SlateEnums.h"

class IDetailLayoutBuilder;
class IDetailCategoryBuilder;
class UCreateNewVatProperties;
struct FSlateColor;

/**
 * Custom detail panel layout for VAT (Vertex Animation Texture) properties.
 * Provides custom file selection widgets for FBX and texture file paths.
 */
class FHoudiniVatPropertiesCustomization : public IDetailCustomization
{
public:
	/** Creates a new instance of this detail customization. */
	static TSharedRef<IDetailCustomization> MakeInstance();

	// IDetailCustomization interface
	virtual void CustomizeDetails(IDetailLayoutBuilder& DetailBuilder) override;

private:
	/** Called when the FBX browse button is clicked. */
	FReply OnSelectFbxClicked();

	/** Returns the text to display for FBX file selection status. */
	FText GetFbxSelectionText() const;

	/** Returns the color for FBX selection text based on whether a file is selected. */
	FSlateColor GetFbxTextColor() const;

	/** Called when the texture browse button is clicked. */
	FReply OnSelectTexturesClicked();

	/** Returns the text to display for texture file selection status. */
	FText GetTextureSelectionText() const;

	/** Returns the color for texture selection text based on whether files are selected. */
	FSlateColor GetTextureTextColor() const;

	/** Adds the custom FBX file selection widget. */
	void AddFbxFileSelectionWidget(IDetailLayoutBuilder& DetailBuilder, IDetailCategoryBuilder& ImportCategory);

	/** Adds the custom texture file selection widget. */
	void AddTextureFileSelectionWidget(IDetailLayoutBuilder& DetailBuilder, IDetailCategoryBuilder& ImportCategory);

	/** Gets the parent window handle for file dialogs. */
	void* GetParentWindowHandle() const;

	/** Gets a default path for the file dialog. */
	FString GetDefaultPathForFileDialog(const FString& CurrentFilePath) const;

	/** Forces the details panel to refresh. */
	void RefreshDetailsPanel() const;

	/** Sorts texture files based on known texture types. */
	void SortTextureFilesByType(TArray<FString>& Files) const;

	/** Returns a label for a texture file based on its name. */
	FString GetTextureTypeLabel(const FString& FileName) const;

	/** Weak pointer to the properties object being customized. */
	TWeakObjectPtr<UCreateNewVatProperties> VatPropertiesPtr;
};
