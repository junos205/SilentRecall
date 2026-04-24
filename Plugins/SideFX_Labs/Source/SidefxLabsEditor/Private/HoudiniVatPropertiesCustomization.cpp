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

#include "HoudiniVatPropertiesCustomization.h"
#include "HoudiniCreateNewVatWindowParameters.h"
#include "DetailLayoutBuilder.h"
#include "DetailCategoryBuilder.h"
#include "DetailWidgetRow.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SScrollBox.h"
#include "DesktopPlatformModule.h"
#include "IDesktopPlatform.h"
#include "Framework/Application/SlateApplication.h"
#include "Styling/AppStyle.h"
#include "Misc/Paths.h"
#include "UObject/UnrealType.h"

/**
 * Creates an instance of FHoudiniVatPropertiesCustomization.
 *
 * @return A shared reference to the detail customization instance.
 */
TSharedRef<IDetailCustomization> FHoudiniVatPropertiesCustomization::MakeInstance()
{
	return MakeShareable(new FHoudiniVatPropertiesCustomization);
}

/**
 * Customizes the details panel layout by hiding and replacing specific properties with custom file selection widgets.
 *
 * @param DetailBuilder Reference to the layout builder used for customization.
 */
void FHoudiniVatPropertiesCustomization::CustomizeDetails(IDetailLayoutBuilder& DetailBuilder)
{
	TArray<TWeakObjectPtr<UObject>> Objects;
	DetailBuilder.GetObjectsBeingCustomized(Objects);
	if (Objects.Num() > 0)
	{
		VatPropertiesPtr = Cast<UCreateNewVatProperties>(Objects[0].Get());
	}

	DetailBuilder.HideProperty(GET_MEMBER_NAME_CHECKED(UCreateNewVatProperties, VatFbxFilePath));
	DetailBuilder.HideProperty(GET_MEMBER_NAME_CHECKED(UCreateNewVatProperties, VatTextureFilePath));

	IDetailCategoryBuilder& ImportCategory = DetailBuilder.EditCategory("Import");

	AddFbxFileSelectionWidget(DetailBuilder, ImportCategory);

	AddTextureFileSelectionWidget(DetailBuilder, ImportCategory);

	ImportCategory.AddProperty(GET_MEMBER_NAME_CHECKED(UCreateNewVatProperties, VatAssetPath));
	ImportCategory.AddProperty(GET_MEMBER_NAME_CHECKED(UCreateNewVatProperties, bCreateVatBlueprint));
}

/**
 * Adds a custom widget for FBX file selection to the Import category.
 *
 * @param DetailBuilder The layout builder used for the customization.
 * @param ImportCategory The detail category where the widget will be added.
 */
void FHoudiniVatPropertiesCustomization::AddFbxFileSelectionWidget(
	IDetailLayoutBuilder& DetailBuilder,
	IDetailCategoryBuilder& ImportCategory)
{
	ImportCategory.AddCustomRow(FText::FromString("FBX File"))
		.NameContent()
		[
			SNew(STextBlock)
			.Text(FText::FromString("FBX File Path"))
			.Font(DetailBuilder.GetDetailFont())
			.ToolTipText(FText::FromString("The file path to the exported FBX file from the Labs Vertex Animation Textures ROP"))
		]
		.ValueContent()
		.MinDesiredWidth(250.0f)
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				.Padding(0, 0, 8, 0)
				[
					SNew(SButton)
					.Text(FText::FromString("Browse..."))
					.OnClicked(this, &FHoudiniVatPropertiesCustomization::OnSelectFbxClicked)
					.ToolTipText(FText::FromString("Select the FBX file exported from Houdini"))
				]
				+ SHorizontalBox::Slot()
				.FillWidth(1.0f)
				.VAlign(VAlign_Center)
				[
					SNew(STextBlock)
					.Text(this, &FHoudiniVatPropertiesCustomization::GetFbxSelectionText)
					.ColorAndOpacity(this, &FHoudiniVatPropertiesCustomization::GetFbxTextColor)
					.AutoWrapText(true)
				]
			]
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0, 2)
			[
				SNew(SBox)
				.Visibility_Lambda([this]()
				{
					return (VatPropertiesPtr.IsValid() && !VatPropertiesPtr->VatFbxFilePath.FilePath.IsEmpty())
						? EVisibility::Visible
						: EVisibility::Collapsed;
				})
				[
					SNew(STextBlock)
					.Text_Lambda([this]()
					{
						if (VatPropertiesPtr.IsValid() && !VatPropertiesPtr->VatFbxFilePath.FilePath.IsEmpty())
						{
							return FText::FromString(FPaths::GetCleanFilename(VatPropertiesPtr->VatFbxFilePath.FilePath));
						}
						return FText::GetEmpty();
					})
					.Font(DetailBuilder.GetDetailFontItalic())
					.ColorAndOpacity(FSlateColor::UseSubduedForeground())
				]
			]
		];
}

/**
 * Adds a custom widget for selecting texture files to the Import category.
 *
 * @param DetailBuilder The layout builder used for the customization.
 * @param ImportCategory The detail category where the widget will be added.
 */
void FHoudiniVatPropertiesCustomization::AddTextureFileSelectionWidget(
	IDetailLayoutBuilder& DetailBuilder,
	IDetailCategoryBuilder& ImportCategory)
{
	ImportCategory.AddCustomRow(FText::FromString("Texture Files"))
		.NameContent()
		[
			SNew(STextBlock)
			.Text(FText::FromString("Texture File Paths"))
			.Font(DetailBuilder.GetDetailFont())
			.ToolTipText(FText::FromString("Select texture files exported from the Labs Vertex Animation Textures ROP"))
		]
		.ValueContent()
		.MinDesiredWidth(250.0f)
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				.Padding(0, 0, 8, 0)
				[
					SNew(SButton)
					.Text(FText::FromString("Browse..."))
					.OnClicked(this, &FHoudiniVatPropertiesCustomization::OnSelectTexturesClicked)
					.ToolTipText(FText::FromString("Select multiple texture files (Hold Ctrl/Cmd to select multiple files)"))
				]
				+ SHorizontalBox::Slot()
				.FillWidth(1.0f)
				.VAlign(VAlign_Center)
				[
					SNew(STextBlock)
					.Text(this, &FHoudiniVatPropertiesCustomization::GetTextureSelectionText)
					.ColorAndOpacity(this, &FHoudiniVatPropertiesCustomization::GetTextureTextColor)
					.AutoWrapText(true)
				]
			]
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0, 4)
			[
				SNew(SBox)
				.MaxDesiredHeight(100.0f)
				.Visibility_Lambda([this]()
				{
					return (VatPropertiesPtr.IsValid() && VatPropertiesPtr->VatTextureFilePath.Num() > 0)
						? EVisibility::Visible
						: EVisibility::Collapsed;
				})
				[
					SNew(SBorder)
					.BorderImage(FAppStyle::GetBrush("ToolPanel.GroupBorder"))
					.Padding(4)
					[
						SNew(SScrollBox)
						.Orientation(Orient_Vertical)
						+ SScrollBox::Slot()
						[
							SNew(STextBlock)
							.Text_Lambda([this]()
							{
								if (!VatPropertiesPtr.IsValid())
								{
									return FText::GetEmpty();
								}

								FString FileList;
								for (int32 Index = 0; Index < VatPropertiesPtr->VatTextureFilePath.Num(); ++Index)
								{
									const FString FileName = FPaths::GetCleanFilename(VatPropertiesPtr->VatTextureFilePath[Index].FilePath);
									const FString TextureType = GetTextureTypeLabel(FileName);

									FileList += FString::Printf(TEXT("%s%s"), *FileName, *TextureType);
									if (Index < VatPropertiesPtr->VatTextureFilePath.Num() - 1)
									{
										FileList += TEXT("\n");
									}
								}
								return FText::FromString(FileList);
							})
							.Font(DetailBuilder.GetDetailFontItalic())
							.ColorAndOpacity(FSlateColor::UseSubduedForeground())
						]
					]
				]
			]
		];
}

/**
 * Handles the FBX file browse button click and opens the file dialog.
 *
 * @return FReply indicating the operation was handled.
 */
FReply FHoudiniVatPropertiesCustomization::OnSelectFbxClicked()
{
	IDesktopPlatform* DesktopPlatform = FDesktopPlatformModule::Get();
	if (!DesktopPlatform || !VatPropertiesPtr.IsValid())
	{
		return FReply::Handled();
	}

	void* ParentWindowHandle = GetParentWindowHandle();
	TArray<FString> OutFiles;
	FString DefaultPath = GetDefaultPathForFileDialog(VatPropertiesPtr->VatFbxFilePath.FilePath);

	const FString FileTypes = TEXT("FBX Files (*.fbx)|*.fbx|All Files (*.*)|*.*");

	const bool bOpened = DesktopPlatform->OpenFileDialog(
		ParentWindowHandle,
		TEXT("Select VAT FBX File"),
		DefaultPath,
		TEXT(""),
		FileTypes,
		EFileDialogFlags::None,
		OutFiles
	);

	if (bOpened && OutFiles.Num() > 0)
	{
		VatPropertiesPtr->VatFbxFilePath.FilePath = OutFiles[0];
        VatPropertiesPtr->Modify();
        VatPropertiesPtr->PostEditChange();
	}

	return FReply::Handled();
}

/**
 * Handles the texture files browse button click and opens the file dialog.
 *
 * @return FReply indicating the operation was handled.
 */
FReply FHoudiniVatPropertiesCustomization::OnSelectTexturesClicked()
{
	IDesktopPlatform* DesktopPlatform = FDesktopPlatformModule::Get();
	if (!DesktopPlatform || !VatPropertiesPtr.IsValid())
	{
		return FReply::Handled();
	}

	void* ParentWindowHandle = GetParentWindowHandle();
	TArray<FString> OutFiles;
	
	FString DefaultPath = FPaths::ProjectDir();
	if (VatPropertiesPtr->VatTextureFilePath.Num() > 0)
	{
		DefaultPath = FPaths::GetPath(VatPropertiesPtr->VatTextureFilePath[0].FilePath);
	}
	else if (!VatPropertiesPtr->VatFbxFilePath.FilePath.IsEmpty())
	{
		DefaultPath = FPaths::GetPath(VatPropertiesPtr->VatFbxFilePath.FilePath);
	}

	const FString FileTypes = TEXT("Texture Files (*.exr;*.png)|*.exr;*.png|EXR Files (*.exr)|*.exr|PNG Files (*.png)|*.png|All Files (*.*)|*.*");

	const bool bOpened = DesktopPlatform->OpenFileDialog(
		ParentWindowHandle,
		TEXT("Select VAT Texture Files (Hold Ctrl/Cmd for multiple selection)"),
		DefaultPath,
		TEXT(""),
		FileTypes,
		EFileDialogFlags::Multiple,
		OutFiles
	);

	if (bOpened && OutFiles.Num() > 0)
	{
		VatPropertiesPtr->VatTextureFilePath.Empty();

		SortTextureFilesByType(OutFiles);

		for (const FString& FilePath : OutFiles)
		{
			FFilePath NewPath;
			NewPath.FilePath = FilePath;
			VatPropertiesPtr->VatTextureFilePath.Add(NewPath);
		}

        VatPropertiesPtr->Modify();
        VatPropertiesPtr->PostEditChange();
	}

	return FReply::Handled();
}

/**
 * Retrieves the native operating system handle of the main Unreal window.
 *
 * @return A void pointer to the OS window handle, or nullptr if invalid.
 */
void* FHoudiniVatPropertiesCustomization::GetParentWindowHandle() const
{
	const TSharedPtr<SWindow> MainWindow = FSlateApplication::Get().GetActiveTopLevelWindow();
	if (MainWindow.IsValid() && MainWindow->GetNativeWindow().IsValid())
	{
		return MainWindow->GetNativeWindow()->GetOSWindowHandle();
	}
	return nullptr;
}

/**
 * Gets the default directory path to open in the file dialog.
 *
 * @param CurrentFilePath The current file path stored in the property.
 * @return The directory path to use as the default for file browsing.
 */
FString FHoudiniVatPropertiesCustomization::GetDefaultPathForFileDialog(const FString& CurrentFilePath) const
{
	if (!CurrentFilePath.IsEmpty())
	{
		return FPaths::GetPath(CurrentFilePath);
	}
	return FPaths::ProjectDir();
}

/**
 * Forces a refresh of the details panel to reflect any property changes.
 */
/*
void FHoudiniVatPropertiesCustomization::RefreshDetailsPanel() const
{
	if (DetailBuilderPtr)
	{
		DetailBuilderPtr->ForceRefreshDetails();
	}
}
*/

/**
 * Sorts texture files by common types: position, rotation, color, lookup.
 * Ensures consistent ordering in the display and logic.
 *
 * @param Files Array of file paths to sort.
 */
void FHoudiniVatPropertiesCustomization::SortTextureFilesByType(TArray<FString>& Files) const
{
	Files.Sort([](const FString& A, const FString& B)
	{
		auto GetPriority = [](const FString& FileName) -> int32
		{
			if (FileName.Contains(TEXT("pos"))) return 0;
			if (FileName.Contains(TEXT("rot"))) return 1;
			if (FileName.Contains(TEXT("col"))) return 2;
			if (FileName.Contains(TEXT("lookup"))) return 3;
			return 4;
		};

		return GetPriority(A) < GetPriority(B);
	});
}

/**
 * Extracts and returns a human-readable label based on texture type.
 *
 * @param FileName The file name to evaluate.
 * @return A string label indicating the texture type.
 */
FString FHoudiniVatPropertiesCustomization::GetTextureTypeLabel(const FString& FileName) const
{
	if (FileName.Contains(TEXT("pos")))
	{
		return TEXT(" (Position)");
	}
	if (FileName.Contains(TEXT("rot")))
	{
		return TEXT(" (Rotation)");
	}
	if (FileName.Contains(TEXT("col")))
	{
		return TEXT(" (Color)");
	}
	if (FileName.Contains(TEXT("lookup")))
	{
		return TEXT(" (Lookup)");
	}
	return TEXT("");
}

/**
 * Returns a short status message for the FBX file selection.
 *
 * @return A localized FText string indicating selection state.
 */
FText FHoudiniVatPropertiesCustomization::GetFbxSelectionText() const
{
	if (!VatPropertiesPtr.IsValid() || VatPropertiesPtr->VatFbxFilePath.FilePath.IsEmpty())
	{
		return FText::FromString("No FBX file selected");
	}

	return FText::FromString("FBX file selected");
}

/**
 * Gets the text color for the FBX selection status based on whether a file is selected.
 *
 * @return A slate color indicating subdued or normal foreground color.
 */
FSlateColor FHoudiniVatPropertiesCustomization::GetFbxTextColor() const
{
	if (!VatPropertiesPtr.IsValid() || VatPropertiesPtr->VatFbxFilePath.FilePath.IsEmpty())
	{
		return FSlateColor::UseSubduedForeground();
	}

	return FSlateColor::UseForeground();
}

/**
 * Returns a short status message for the texture file selection.
 *
 * @return A localized FText string indicating how many textures are selected.
 */
FText FHoudiniVatPropertiesCustomization::GetTextureSelectionText() const
{
	if (!VatPropertiesPtr.IsValid())
	{
		return FText::FromString("No textures selected");
	}

	const int32 NumTextures = VatPropertiesPtr->VatTextureFilePath.Num();

	if (NumTextures == 0)
	{
		return FText::FromString("No textures selected");
	}
	
	if (NumTextures == 1)
	{
		return FText::FromString("1 texture selected");
	}

	return FText::Format(FText::FromString("{0} textures selected"), NumTextures);
}

/**
 * Gets the text color for the texture selection status based on whether files are selected.
 *
 * @return A slate color indicating subdued or normal foreground color.
 */
FSlateColor FHoudiniVatPropertiesCustomization::GetTextureTextColor() const
{
	if (!VatPropertiesPtr.IsValid() || VatPropertiesPtr->VatTextureFilePath.Num() == 0)
	{
		return FSlateColor::UseSubduedForeground();
	}

	return FSlateColor::UseForeground();
}
