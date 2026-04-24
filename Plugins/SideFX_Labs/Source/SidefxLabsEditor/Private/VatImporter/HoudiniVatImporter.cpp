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
* NO EVENT SHALL SIDE EFFECTS SOFTWARE BE LIABLE FOR ANY DIRECT, INDIRECT,\
* INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
* LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA,
* OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
* LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
* NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
* EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#include "HoudiniVatImporter.h"
#include "HoudiniCreateNewVatWindowParameters.h"
#include "HoudiniVatActor.h"
#include "SidefxLabsEditorUtils.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetToolsModule.h"
#include "EditorFramework/AssetImportData.h"
#include "Engine/Blueprint.h"
#include "Engine/StaticMesh.h"
#include "Engine/Texture2D.h"
#include "Factories/BlueprintFactory.h"
#include "Factories/FbxFactory.h"
#include "Factories/MaterialFactoryNew.h"
#include "Factories/MaterialInstanceConstantFactoryNew.h"
#include "Factories/TextureFactory.h"
#include "FileHelpers.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Materials/Material.h"
#include "Materials/MaterialExpressionMaterialFunctionCall.h"
#include "Materials/MaterialFunction.h"
#include "Materials/MaterialInstanceConstant.h"
#include "MaterialEditingLibrary.h"
#include "Misc/FileHelper.h"
#include "ObjectTools.h"
#include "UObject/Package.h"

/** Material function paths for different VAT types. */
namespace VatMaterialPaths
{
	static const TCHAR* SoftBody    = TEXT("/SideFX_Labs/Materials/MaterialFunctions/Houdini_VAT_SoftBodyDeformation.Houdini_VAT_SoftBodyDeformation");
	static const TCHAR* RigidBody   = TEXT("/SideFX_Labs/Materials/MaterialFunctions/Houdini_VAT_RigidBodyDynamics.Houdini_VAT_RigidBodyDynamics");
	static const TCHAR* Fluid       = TEXT("/SideFX_Labs/Materials/MaterialFunctions/Houdini_VAT_DynamicRemeshing.Houdini_VAT_DynamicRemeshing");
	static const TCHAR* Sprite      = TEXT("/SideFX_Labs/Materials/MaterialFunctions/Houdini_VAT_ParticleSprites.Houdini_VAT_ParticleSprites");
}

/**
 * Default constructor for importer.
 * Loads the default material function for initial setup.
 */
UHoudiniVatImporter::UHoudiniVatImporter()
	: bCanceled(false)
	,VatProperties(nullptr)
	, HoudiniVatMaterialFunction(nullptr)
{
static ConstructorHelpers::FObjectFinder<UMaterialFunction> MaterialFunctionFinder(TEXT("MaterialFunction'/SideFX_Labs/Materials/MaterialFunctions/Houdini_VAT_RigidBodyDynamics.Houdini_VAT_RigidBodyDynamics'"));

	if (MaterialFunctionFinder.Succeeded())
	{
		HoudiniVatMaterialFunction = MaterialFunctionFinder.Object;
	}
	else
	{
		UE_LOG(LogSidefxLabsEditor, Error, TEXT("Failed to find Houdini_VAT_RigidBodyDynamics material function"));
	}
}

/**
 * Sets the properties object for the import process and loads the corresponding material function.
 *
 * @param InProperties The UCreateNewVatProperties object containing user settings.
 */
void UHoudiniVatImporter::SetProperties(UCreateNewVatProperties* InProperties)
{
	VatProperties = InProperties;

	if (!VatProperties)
	{
		return;
	}

	const TCHAR* MaterialPath = nullptr;
	
	switch (VatProperties->VatType)
	{
		case EVatType::VatType1:
			MaterialPath = VatMaterialPaths::SoftBody;
			break;

		case EVatType::VatType2:
			MaterialPath = VatMaterialPaths::RigidBody;
			break;
			
		case EVatType::VatType3:
			MaterialPath = VatMaterialPaths::Fluid;
			break;

		case EVatType::VatType4:
			MaterialPath = VatMaterialPaths::Sprite;
			break;

		default:
			UE_LOG(LogSidefxLabsEditor, Error, TEXT("Invalid VAT type"));
			return;
	}

	if (UMaterialFunction* LoadedFunction = LoadObject<UMaterialFunction>(nullptr, MaterialPath))
	{
		HoudiniVatMaterialFunction = LoadedFunction;
        UE_LOG(LogSidefxLabsEditor, Log, TEXT("Loaded VAT material function: %s"), *LoadedFunction->GetPathName());
	}
	else
	{
		UE_LOG(LogSidefxLabsEditor, Error, TEXT("Failed to load material function: %s"), MaterialPath);
	}
}

/**
 * Imports an FBX file as a Static Mesh asset.
 *
 * @param FbxPath The full path to the FBX file.
 * @param AssetPath The content path where the asset should be created.
 *
 * @return UStaticMesh Static Mesh of imported FBX file.
 */
UStaticMesh* UHoudiniVatImporter::ImportFbx(const FString& FbxPath, const FString& AssetPath)
{
    bCanceled = false;

    if (!IFileManager::Get().FileExists(*FbxPath))
    {
        UE_LOG(LogSidefxLabsEditor, Error, TEXT("FBX file not found: %s"), *FbxPath);
        return nullptr;
    }

    if (!AssetPath.StartsWith(TEXT("/Game")))
    {
        UE_LOG(LogSidefxLabsEditor, Error, TEXT("Asset Path must start with /Game: %s"), *AssetPath);
        return nullptr;
    }

    UFbxFactory* FbxFactory = NewObject<UFbxFactory>(GetTransientPackage());

    if (!FbxFactory)
    {
        UE_LOG(LogSidefxLabsEditor, Error, TEXT("UHoudiniVatImporter::ImportFbx: Failed to create UFbxFactory"));
        return nullptr;
    }

    if (!FbxFactory->ConfigureProperties())
    {
        UE_LOG(LogSidefxLabsEditor, Warning, TEXT("FBX import canceled by user"));
        bCanceled = true;
        return nullptr;
    }

    FString BaseName = ObjectTools::SanitizeObjectName(FPaths::GetBaseFilename(FbxPath));
    const FString PackageName = FPaths::Combine(AssetPath, BaseName);

    UPackage* Package = CreatePackage(*PackageName);

    if (!Package)
    {
        UE_LOG(LogSidefxLabsEditor, Error, TEXT("UHoudiniVatImporter::ImportFbx: Failed to create package: %s"), *PackageName);
        return nullptr;
    }

    FName AssetFName = FName(*BaseName);

    if (FindObject<UObject>(Package, *BaseName))
    {
        AssetFName = MakeUniqueObjectName(Package, UStaticMesh::StaticClass(), AssetFName);
    }

    UStaticMesh* Mesh = Cast<UStaticMesh>(FbxFactory->ImportObject(
        UStaticMesh::StaticClass(),
        Package,
        AssetFName,
        RF_Public | RF_Standalone,
        *FbxPath,
        nullptr,
        bCanceled));

    if (bCanceled)
    {
        UE_LOG(LogSidefxLabsEditor, Warning, TEXT("FBX import canceled during import: %s"), *FbxPath);
        return nullptr;
    }

    if (!Mesh)
    {
        UE_LOG(LogSidefxLabsEditor, Warning, TEXT("FBX import failed: %s"), *FbxPath);
        return nullptr;
    }

    FAssetRegistryModule::AssetCreated(Mesh);
    Package->MarkPackageDirty();

    UE_LOG(LogSidefxLabsEditor, Log, TEXT("FBX imported: %s -> %s"), *FbxPath, *Mesh->GetPathName());
    return Mesh;
}

/**
 * Configures specific mesh settings required for Fluid VAT (VatType3).
 */
void UHoudiniVatImporter::ConfigureFluidVatMeshSettings()
{
    if (!StaticMesh.IsValid())
    {
        UE_LOG(LogSidefxLabsEditor, Error, TEXT("UHoudiniVatImporter::ConfigureFluidVatMeshSettings: Static Mesh is invalid for VAT type 3 (Fluid)"));
        return;
    }

    check(IsInGameThread());

    const int32 NumSrc = StaticMesh->GetNumSourceModels();

    if (NumSrc <= 0)
    {
        UE_LOG(LogSidefxLabsEditor, Error, TEXT("Static Mesh has no source models. Cannot configure UV precision safely"));
        return;
    }

    FScopedTransaction Tx(FText::FromString(TEXT("Configure Fluid VAT Mesh Settings")));
    StaticMesh->Modify();

    FStaticMeshSourceModel& SourceModel = StaticMesh->GetSourceModel(0);
    FMeshBuildSettings& BuildSettings   = SourceModel.BuildSettings;

    BuildSettings.bUseFullPrecisionUVs = true;
    BuildSettings.bUseBackwardsCompatibleF16TruncUVs = false;

    StaticMesh->Build(false);

    StaticMesh->MarkPackageDirty();
    StaticMesh->PostEditChange();

    UE_LOG(LogSidefxLabsEditor, Log, TEXT("Updated UV precision settings for fluid VAT mesh"));
}

/**
 * Imports a texture file as a UTexture2D asset.
 *
 * @param TexturePath The full path to the texture file.
 * @param AssetPath The content path where the asset should be created.
 *
 * @return UTexture2D The imported textures as a UTexture2D asset.
 */
UTexture2D* UHoudiniVatImporter::ImportTexture(const FString& TexturePath, const FString& AssetPath)
{
    if (!IFileManager::Get().FileExists(*TexturePath))
    {
        UE_LOG(LogSidefxLabsEditor, Error, TEXT("Texture file not found: %s"), *TexturePath);
        return nullptr;
    }

    if (!AssetPath.StartsWith(TEXT("/Game")))
    {
        UE_LOG(LogSidefxLabsEditor, Error, TEXT("AssetPath must start with /Game: %s"), *AssetPath);
        return nullptr;
    }

    UTextureFactory* TextureFactory = NewObject<UTextureFactory>(GetTransientPackage());

    if (!TextureFactory)
    {
        UE_LOG(LogSidefxLabsEditor, Error, TEXT("UHoudiniVatImporter::ImportTexture: Failed to create UTextureFactory."));
        return nullptr;
    }

    FString BaseName = ObjectTools::SanitizeObjectName(FPaths::GetBaseFilename(TexturePath));
    const FString PackageName = FPaths::Combine(AssetPath, BaseName);

    UPackage* Package = CreatePackage(*PackageName);

    if (!Package)
    {
        UE_LOG(LogSidefxLabsEditor, Error, TEXT("UHoudiniVatImporter::ImportTexture: Failed to create package: %s"), *PackageName);
        return nullptr;
    }

    FName AssetFName(*BaseName);

    if (FindObject<UObject>(Package, *BaseName))
    {
        AssetFName = MakeUniqueObjectName(Package, UTexture2D::StaticClass(), AssetFName);
    }

    bool bCanceledImport = false;

    UTexture2D* Texture = Cast<UTexture2D>(TextureFactory->ImportObject(
        UTexture2D::StaticClass(),
        Package,
        AssetFName,
        RF_Public | RF_Standalone,
        *TexturePath,
        nullptr,
        bCanceledImport));

    if (bCanceledImport)
    {
        UE_LOG(LogSidefxLabsEditor, Warning, TEXT("Texture import canceled: %s"), *TexturePath);
        return nullptr;
    }

    if (!Texture)
    {
        UE_LOG(LogSidefxLabsEditor, Warning, TEXT("Texture import failed: %s"), *TexturePath);
        return nullptr;
    }

    FAssetRegistryModule::AssetCreated(Texture);
    Package->MarkPackageDirty();

    UE_LOG(LogSidefxLabsEditor, Log, TEXT("Imported texture: %s -> %s"), *TexturePath, *Texture->GetPathName());
    return Texture;
}

/**
 * Configures compression and sampling settings for the imported VAT texture assets.
 * Settings are based on the file extension.
 *
 * @param Textures The array of UTexture2D assets to configure.
 */
void UHoudiniVatImporter::SetTextureParameters(TArray<UTexture2D*> Textures)
{
	for (UTexture2D* Texture2D : Textures)
	{
        if (!IsValid(Texture2D) || !Texture2D->AssetImportData)
		{
			UE_LOG(LogSidefxLabsEditor, Warning, TEXT("UHoudiniVatImporter::SetTextureParameters: Texture is null or has no AssetImportData"));
			continue;
		}

        const FAssetImportInfo& ImportInfo = Texture2D->AssetImportData->SourceData;

        FString SrcPath;

        if (ImportInfo.SourceFiles.Num() > 0)
        {
            SrcPath = ImportInfo.SourceFiles[0].RelativeFilename;
        }

        if (SrcPath.IsEmpty())
        {
            UE_LOG(LogSidefxLabsEditor, Warning, TEXT("Source path is empty for texture: %s"), *Texture2D->GetName());
            continue;
        }

        const FString Ext = FPaths::GetExtension(SrcPath).ToLower();
        bool bChanged = false;

        Texture2D->Modify();

        if (Ext == TEXT("exr"))
        {
            bChanged |= (Texture2D->Filter != TF_Nearest);                      Texture2D->Filter = TF_Nearest;
            bChanged |= (Texture2D->LODGroup != TEXTUREGROUP_16BitData);        Texture2D->LODGroup = TEXTUREGROUP_16BitData;
            bChanged |= (Texture2D->MipGenSettings != TMGS_NoMipmaps);          Texture2D->MipGenSettings = TMGS_NoMipmaps;
            bChanged |= (Texture2D->CompressionSettings != TC_HDR);             Texture2D->CompressionSettings = TC_HDR;
            bChanged |= (Texture2D->SRGB != false);                             Texture2D->SRGB = false;
        }
        else if (Ext == TEXT("png"))
        {
            bChanged |= (Texture2D->Filter != TF_Nearest);                       Texture2D->Filter = TF_Nearest;
            bChanged |= (Texture2D->LODGroup != TEXTUREGROUP_8BitData);          Texture2D->LODGroup = TEXTUREGROUP_8BitData;
            bChanged |= (Texture2D->MipGenSettings != TMGS_NoMipmaps);           Texture2D->MipGenSettings = TMGS_NoMipmaps;
            bChanged |= (Texture2D->CompressionSettings != TC_VectorDisplacementmap);
            Texture2D->CompressionSettings = TC_VectorDisplacementmap;
            bChanged |= (Texture2D->SRGB != false);                              Texture2D->SRGB = false;
        }
        else
        {
            UE_LOG(LogSidefxLabsEditor, Verbose, TEXT("Unhandled texture extension for %s (.%s)"), *Texture2D->GetName(), *Ext);
        }

        if (bChanged)
        {
            Texture2D->MarkPackageDirty();
            Texture2D->PostEditChange();
            UE_LOG(LogSidefxLabsEditor, Log, TEXT("Set parameters for texture: %s"), *Texture2D->GetName());
        }
    }
}

/**
 * Imports the specified FBX and texture files.
 */
void UHoudiniVatImporter::ImportFiles()
{
    bCanceled = false;
    TArray<UObject*> ImportedFiles;

    if (!VatProperties)
    {
        UE_LOG(LogSidefxLabsEditor, Error, TEXT("UHoudiniVatImporter::ImportFiles: Failed to load VAT Properties"));
        return;
    }

    const FString FullFbxPathLocal = FPaths::ConvertRelativePathToFull(VatProperties->VatFbxFilePath.FilePath);
    const FString DestPath = VatProperties->VatAssetPath.Path;

    UObject* ImportedFbx = ImportFbx(FullFbxPathLocal, DestPath);

    if (bCanceled || !ImportedFbx)
    {
        UE_LOG(LogSidefxLabsEditor, Warning, TEXT("FBX import canceled or failed"));
        return;
    }

    const FString MeshName = FPaths::GetBaseFilename(FullFbxPathLocal);
    const FString MeshPath = FString::Printf(TEXT("%s/%s.%s"), *DestPath, *MeshName, *MeshName);

    UStaticMesh* ImportedStaticMesh = LoadObject<UStaticMesh>(nullptr, *MeshPath);

    if (!ImportedStaticMesh)
    {
        UE_LOG(LogSidefxLabsEditor, Error, TEXT("UHoudiniVatImporter::ImportFiles: Failed to load imported Static Mesh: %s"), *MeshPath);
        return;
    }

    StaticMesh = ImportedStaticMesh;
    ImportedFiles.Add(ImportedStaticMesh);

    UE_LOG(LogSidefxLabsEditor, Log, TEXT("Imported Static Mesh: %s"), *ImportedStaticMesh->GetPathName());

    if (VatProperties->VatType == EVatType::VatType3)
    {
        ConfigureFluidVatMeshSettings();
    }

    TArray<UTexture2D*> ImportedTextures;

    for (const FFilePath& TextureFilePath : VatProperties->VatTextureFilePath)
    {
        if (bCanceled) break;

        const FString FullTexPath = FPaths::ConvertRelativePathToFull(TextureFilePath.FilePath);

        UObject* ImportedTexObj = ImportTexture(FullTexPath, DestPath);

        const FString TexName = FPaths::GetBaseFilename(FullTexPath);
        const FString TexObjPath = FString::Printf(TEXT("%s/%s.%s"), *DestPath, *TexName, *TexName);

        if (UTexture2D* ImportedTexture = LoadObject<UTexture2D>(nullptr, *TexObjPath))
        {
            ImportedTextures.Add(ImportedTexture);
            ImportedFiles.Add(ImportedTexture);

            UE_LOG(LogSidefxLabsEditor, Log, TEXT("Imported texture: %s"), *ImportedTexture->GetPathName());
        }
        else
        {
            UE_LOG(LogSidefxLabsEditor, Warning, TEXT("UHoudiniVatImporter::ImportFiles: Failed to load texture by path: %s"), *TexObjPath);
        }
    }

    SetTextureParameters(ImportedTextures);
    TSet<UPackage*> UniquePkgs;

    if (StaticMesh.IsValid() && StaticMesh->GetPackage())
    {
        UniquePkgs.Add(StaticMesh->GetPackage());
    }

    for (const UTexture2D* Tex : ImportedTextures)
    {
        if (Tex && Tex->GetPackage())
        {
            UniquePkgs.Add(Tex->GetPackage());
        }
    }

    FSidefxLabsEditorUtils::SavePackages(UniquePkgs.Array());

    UE_LOG(LogSidefxLabsEditor, Log, TEXT("Imported %d asset(s): Mesh: %d, Textures: %d"),
        ImportedFiles.Num(),
        StaticMesh.IsValid() ? 1 : 0,
        ImportedTextures.Num());
}

/**
 * Resolves and loads the material function for the given VAT type.
 *
 * @param Type The VAT type.
 *
 * @return UMaterialFunction A pointer to the Material Function, or nullptr if loading fails.
 */
UMaterialFunction* UHoudiniVatImporter::ResolveMaterialFunctionFor(EVatType Type) const
{
    const TCHAR* Path = nullptr;
    switch (Type)
    {
        case EVatType::VatType1: Path = VatMaterialPaths::SoftBody;  break;
        case EVatType::VatType2: Path = VatMaterialPaths::RigidBody; break;
        case EVatType::VatType3: Path = VatMaterialPaths::Fluid;     break;
        case EVatType::VatType4: Path = VatMaterialPaths::Sprite;    break;
        default: return nullptr;
    }

    if (UMaterialFunction* MF = LoadObject<UMaterialFunction>(nullptr, Path))
    {
        return MF;
    }

    UE_LOG(LogSidefxLabsEditor, Error, TEXT("ResolveMaterialFunctionFor: failed to load '%s' for VAT type %d"), Path ? Path : TEXT("<null>"), (int32)Type);
    return nullptr;
}

/**
 * Connects the outputs for a Soft Body VAT Material.
 */
void UHoudiniVatImporter::ConnectSoftBodyOutputs()
{
    if (!Material.IsValid())
    {
        UE_LOG(LogSidefxLabsEditor, Error, TEXT("UHoudiniVatImporter::ConnectSoftBodyOutputs: VAT Material is invalid"));
        return;
    }

    if (!VatMaterialExp.IsValid())
    {
        UE_LOG(LogSidefxLabsEditor, Error, TEXT("UHoudiniVatImporter::ConnectSoftBodyOutputs: VAT Material Expression is invalid"));
        return;
    }

    ensureMsgf(IsInGameThread(), TEXT("Material wiring must run on the game thread"));

    UMaterialEditorOnlyData* Data = Material->GetEditorOnlyData();
    if (!Data)
    {
        UE_LOG(LogSidefxLabsEditor, Error, TEXT("UHoudiniVatImporter::ConnectSoftBodyOutputs: EditorOnlyData missing"));
        return;
    }

    auto ConnectOut = [&](FExpressionInput& Input, int32 OutIdx)
    {
        if (VatMaterialExp->Outputs.IsValidIndex(OutIdx))
        {
            Input.Connect(OutIdx, VatMaterialExp.Get());
        }
    };

    auto ConnectUV = [&](int32 UVIdx, int32 OutIdx)
    {
        if (UVIdx >= 0 && UVIdx < 8 && VatMaterialExp->Outputs.IsValidIndex(OutIdx))
        {
            Data->CustomizedUVs[UVIdx].Connect(OutIdx, VatMaterialExp.Get());
            Material->NumCustomizedUVs = FMath::Max(Material->NumCustomizedUVs, UVIdx + 1);
        }
    };

    ConnectOut(Data->BaseColor, 0);
    ConnectOut(Data->Normal, 3);
    ConnectOut(Data->WorldPositionOffset, 4);

    ConnectUV(2, 19);
    ConnectUV(3, 20);
    ConnectUV(4, 21);
}

/**
 * Connects the outputs for a Rigid Body VAT Material.
 */
void UHoudiniVatImporter::ConnectRigidBodyOutputs()
{
    if (!Material.IsValid())
    {
        UE_LOG(LogSidefxLabsEditor, Error, TEXT("UHoudiniVatImporter::ConnectRigidBodyOutputs: VAT Material is invalid"));
        return;
    }

    if (!VatMaterialExp.IsValid())
    {
        UE_LOG(LogSidefxLabsEditor, Error, TEXT("UHoudiniVatImporter::ConnectRigidBodyOutputs: VAT Material Expression is invalid"));
        return;
    }

    ensureMsgf(IsInGameThread(), TEXT("Material wiring must run on the game thread"));

    UMaterialEditorOnlyData* Data = Material->GetEditorOnlyData();
    if (!Data)
    {
        UE_LOG(LogSidefxLabsEditor, Error, TEXT("UHoudiniVatImporter::ConnectRigidBodyOutputs: EditorOnlyData missing"));
        return;
    }

    auto ConnectOut = [&](FExpressionInput& Input, int32 OutIdx)
    {
        if (VatMaterialExp->Outputs.IsValidIndex(OutIdx))
        {
            Input.Connect(OutIdx, VatMaterialExp.Get());
        }
    };

    auto ConnectUV = [&](int32 UVIdx, int32 OutIdx)
    {
        if (UVIdx >= 0 && UVIdx < 8 && VatMaterialExp->Outputs.IsValidIndex(OutIdx))
        {
            Data->CustomizedUVs[UVIdx].Connect(OutIdx, VatMaterialExp.Get());
            Material->NumCustomizedUVs = FMath::Max(Material->NumCustomizedUVs, UVIdx + 1);
        }
    };

    ConnectOut(Data->BaseColor, 0);
    ConnectOut(Data->Normal, 3);
    ConnectOut(Data->WorldPositionOffset, 4);

    ConnectUV(2, 21);
    ConnectUV(3, 22);
    ConnectUV(4, 23);
}

/**
 * Connects the outputs for a Fluid VAT Material.
 */
void UHoudiniVatImporter::ConnectFluidOutputs()
{
    if (!Material.IsValid())
    {
        UE_LOG(LogSidefxLabsEditor, Error, TEXT("UHoudiniVatImporter::ConnectFluidOutputs: VAT Material is invalid"));
        return;
    }

    if (!VatMaterialExp.IsValid())
    {
        UE_LOG(LogSidefxLabsEditor, Error, TEXT("UHoudiniVatImporter::ConnectFluidOutputs: VAT Material Expression is invalid"));
        return;
    }

    ensureMsgf(IsInGameThread(), TEXT("Material wiring must run on the game thread"));

    UMaterialEditorOnlyData* Data = Material->GetEditorOnlyData();
    if (!Data)
    {
        UE_LOG(LogSidefxLabsEditor, Error, TEXT("UHoudiniVatImporter::ConnectFluidOutputs: EditorOnlyData missing"));
        return;
    }

    auto ConnectOut = [&](FExpressionInput& Input, int32 OutIdx)
    {
        if (VatMaterialExp->Outputs.IsValidIndex(OutIdx))
        {
            Input.Connect(OutIdx, VatMaterialExp.Get());
        }
    };

    auto ConnectUV = [&](int32 UVIdx, int32 OutIdx)
    {
        if (UVIdx >= 0 && UVIdx < 8 && VatMaterialExp->Outputs.IsValidIndex(OutIdx))
        {
            Data->CustomizedUVs[UVIdx].Connect(OutIdx, VatMaterialExp.Get());
            Material->NumCustomizedUVs = FMath::Max(Material->NumCustomizedUVs, UVIdx + 1);
        }
    };

    ConnectOut(Data->BaseColor, 0);
    ConnectOut(Data->Normal, 3);
    ConnectOut(Data->WorldPositionOffset, 4);

    ConnectUV(1, 19);
    ConnectUV(2, 20);
    ConnectUV(3, 21);
}

/**
 * Connects the outputs for a Particle Sprite VAT Material.
 */
void UHoudiniVatImporter::ConnectSpriteOutputs()
{
    if (!Material.IsValid())
    {
        UE_LOG(LogSidefxLabsEditor, Error, TEXT("UHoudiniVatImporter::ConnectSpriteOutputs: VAT Material is invalid"));
        return;
    }

    if (!VatMaterialExp.IsValid())
    {
        UE_LOG(LogSidefxLabsEditor, Error, TEXT("UHoudiniVatImporter::ConnectSpriteOutputs: VAT Material Expression is invalid"));
        return;
    }

    ensureMsgf(IsInGameThread(), TEXT("Material wiring must run on the game thread"));

    UMaterialEditorOnlyData* Data = Material->GetEditorOnlyData();
    if (!Data)
    {
        UE_LOG(LogSidefxLabsEditor, Error, TEXT("UHoudiniVatImporter::ConnectSpriteOutputs: EditorOnlyData missing"));
        return;
    }

    auto ConnectOut = [&](FExpressionInput& Input, int32 OutIdx)
    {
        if (VatMaterialExp->Outputs.IsValidIndex(OutIdx))
        {
            Input.Connect(OutIdx, VatMaterialExp.Get());
        }
    };

    auto ConnectUV = [&](int32 UVIdx, int32 OutIdx)
    {
        if (UVIdx >= 0 && UVIdx < 8 && VatMaterialExp->Outputs.IsValidIndex(OutIdx))
        {
            Data->CustomizedUVs[UVIdx].Connect(OutIdx, VatMaterialExp.Get());
            Material->NumCustomizedUVs = FMath::Max(Material->NumCustomizedUVs, UVIdx + 1);
        }
    };

    ConnectOut(Data->BaseColor, 0);
    ConnectOut(Data->Normal, 3);
    ConnectOut(Data->WorldPositionOffset, 4);

    ConnectUV(2, 19);
}

/**
 * Connects the outputs of the VAT Material Expression to the appropriate inputs on the base Material.
 * The connections depend on the selected VAT type.
 */
void UHoudiniVatImporter::ConnectMaterialOutputs()
{
    if (!VatProperties)
    {
        UE_LOG(LogSidefxLabsEditor, Error, TEXT("UHoudiniVatImporter::ConnectMaterialOutputs: VatProperties is null"));
        return;
    }

    if (!Material.IsValid())
    {
        UE_LOG(LogSidefxLabsEditor, Error, TEXT("UHoudiniVatImporter::ConnectMaterialOutputs: Material is invalid"));
        return;
    }

    switch (VatProperties->VatType)
    {
        case EVatType::VatType1: ConnectSoftBodyOutputs();  break;
        case EVatType::VatType2: ConnectRigidBodyOutputs(); break;
        case EVatType::VatType3: ConnectFluidOutputs();     break;
        case EVatType::VatType4: ConnectSpriteOutputs();    break;
        default:
            UE_LOG(LogSidefxLabsEditor, Error, TEXT("UHoudiniVatImporter::ConnectMaterialOutputs: Invalid VAT type %d"), (int32)VatProperties->VatType);
            break;
    }
}

/**
 * Creates the base Material asset, adds the VAT Material Function, and connects the outputs.
 */
void UHoudiniVatImporter::CreateVatMaterial()
{
	if (!VatProperties)
	{
		UE_LOG(LogSidefxLabsEditor, Error, TEXT("UHoudiniVatImporter::CreateVatMaterial: VatProperties is not set"));
		return;
	}

    UMaterialFunction* MF = ResolveMaterialFunctionFor(VatProperties->VatType);

    if (!MF)
    {
        UE_LOG(LogSidefxLabsEditor, Error, TEXT("UHoudiniVatImporter::CreateVatMaterial: Aborting material creation. No valid VAT material function for type %d"), (int32)VatProperties->VatType);
        return;
    }

    HoudiniVatMaterialFunction = MF;
    int32 NumUVs = 0;

    if (!HoudiniVatMaterialFunction)
    {
        UE_LOG(LogSidefxLabsEditor, Error, TEXT("UHoudiniVatImporter::CreateVatMaterial: HoudiniVatMaterialFunction is invalid"));
        return;
    }

    switch (VatProperties->VatType)
    {
        case EVatType::VatType1:
        case EVatType::VatType2: NumUVs = 5; break;
        case EVatType::VatType3: NumUVs = 4; break;
        case EVatType::VatType4: NumUVs = 2; break;
		default:
			UE_LOG(LogSidefxLabsEditor, Error, TEXT("UHoudiniVatImporter::CreateVatMaterial: Invalid VAT type"));
			return;
    }

    FString MaterialName = VatProperties->VatMaterialName;

	if (MaterialName.IsEmpty())
	{
		MaterialName = TEXT("M_HoudiniVAT");
	}

	if (!MaterialName.StartsWith(TEXT("M_")))
	{
		MaterialName = TEXT("M_") + MaterialName;
	}

    MaterialName = ObjectTools::SanitizeObjectName(MaterialName);

    const FString AssetPath = VatProperties->VatAssetPath.Path;

    if (!AssetPath.StartsWith(TEXT("/Game")) || !FPackageName::IsValidLongPackageName(AssetPath))
    {
        UE_LOG(LogSidefxLabsEditor, Error, TEXT("UHoudiniVatImporter::CreateVatMaterial: Invalid asset path: %s."), *AssetPath);
        return;
    }

    const FString PackageName = FPaths::Combine(AssetPath, MaterialName);

    UPackage* Package = CreatePackage(*PackageName);

    if (!Package)
    {
        UE_LOG(LogSidefxLabsEditor, Error, TEXT("UHoudiniVatImporter::CreateVatMaterial: Failed to create package at path: %s"), *PackageName);
        return;
    }

    UMaterialFactoryNew* MaterialFactory = NewObject<UMaterialFactoryNew>(GetTransientPackage());

    if (!MaterialFactory)
    {
        UE_LOG(LogSidefxLabsEditor, Error, TEXT("UHoudiniVatImporter::CreateVatMaterial: Failed to create MaterialFactory")); 
        return;
    }

    FName AssetFName(*MaterialName);

    if (FindObject<UObject>(Package, *MaterialName))
    {
        AssetFName = MakeUniqueObjectName(Package, UMaterial::StaticClass(), AssetFName);
    }

    UMaterial* NewMat = Cast<UMaterial>(MaterialFactory->FactoryCreateNew(
        UMaterial::StaticClass(),
        Package,
        AssetFName,
        RF_Public | RF_Standalone,
        nullptr,
        GWarn));

    if (!NewMat)
    {
        UE_LOG(LogSidefxLabsEditor, Error, TEXT("UHoudiniVatImporter::CreateVatMaterial: Failed to create UMaterial")); 
        return;
    }

    Material = NewMat;
    CreatedMaterialName = AssetFName.ToString();

    Material->Modify();
    Material->NumCustomizedUVs = NumUVs;
    Material->bTangentSpaceNormal = false;

    UMaterialExpression* Node = UMaterialEditingLibrary::CreateMaterialExpression(Material.Get(), UMaterialExpressionMaterialFunctionCall::StaticClass());
    UMaterialExpressionMaterialFunctionCall* FuncCall = Cast<UMaterialExpressionMaterialFunctionCall>(Node);

    if (!FuncCall)
    {
        UE_LOG(LogSidefxLabsEditor, Error, TEXT("UHoudiniVatImporter::CreateVatMaterial: Failed to create UMaterialExpressionMaterialFunctionCall"));
        return;
    }

    FuncCall->SetMaterialFunction(HoudiniVatMaterialFunction);
    VatMaterialExp = FuncCall;
    FuncCall->UpdateFromFunctionResource();

    Node->MaterialExpressionEditorX -= 700;

    ConnectMaterialOutputs();

    Material->PostEditChange();

    FSidefxLabsEditorUtils::MarkPackageDirtyAndRegister(Material.Get());
    FSidefxLabsEditorUtils::SavePackages({ Material->GetPackage() });
}

/**
 * Sets the basic scalar and static switch parameters on the Material Instance Constant.
 */
void UHoudiniVatImporter::SetBasicMaterialInstanceParameters()
{
    if (!MaterialInstance.IsValid() || !VatProperties)
    {
        return;
    }

    ensureMsgf(IsInGameThread(), TEXT("SetBasicMaterialInstanceParameters must run on game thread"));

    MaterialInstance->Modify();

    static const FName Param_FPS(TEXT("Houdini FPS"));
    static const FName Param_Loop(TEXT("Loop Animation"));
    static const FName Param_Length(TEXT("Animation Length"));
    static const FName Param_Interp(TEXT("Interframe Interpolation"));
    static const FName Param_Legacy(TEXT("Support Legacy Parameters and Instancing"));

    auto Check = [&](const TCHAR* What)
    {
        UE_LOG(LogSidefxLabsEditor, Log, TEXT("Attempted to set MIC parameter: %s"), What);
    };

    MaterialInstance->SetScalarParameterValueEditorOnly(Param_FPS, VatProperties->VatFps);
    Check(TEXT("Houdini FPS"));

    MaterialInstance->SetScalarParameterValueEditorOnly(Param_Length, VatProperties->VatAnimationLength);
    Check(TEXT("Animation Length"));

    MaterialInstance->SetStaticSwitchParameterValueEditorOnly(Param_Loop, VatProperties->bVatLoopAnimation);
    Check(TEXT("Loop Animation"));

    MaterialInstance->SetStaticSwitchParameterValueEditorOnly(Param_Interp, VatProperties->bVatInterpolate);
    Check(TEXT("Interframe Interpolation"));

    MaterialInstance->SetStaticSwitchParameterValueEditorOnly(Param_Legacy, VatProperties->bVatSupportLegacyParametersAndInstancing);
    Check(TEXT("Support Legacy Parameters and Instancing"));
}

/**
 * Determines the correct texture parameter name in the Material Instance based on the filename (e.g., 'pos' -> 'Position Texture').
 *
 * @param FileName The base filename of the texture.
 *
 * @return The corresponding FName of the texture parameter, or NAME_None if not found.
 */
FName UHoudiniVatImporter::GetTextureParameterName(const FString& FileName)
{
	if (FileName.Contains(TEXT("pos")))
	{
		return FName("Position Texture");
	}
	if (FileName.Contains(TEXT("rot")))
	{
		return FName("Rotation Texture");
	}
	if (FileName.Contains(TEXT("col")))
	{
		return FName("Color Texture");
	}
	if (FileName.Contains(TEXT("lookup")))
	{
		return FName("Lookup Table");
	}
	
	return NAME_None;
}

/**
 * Assigns the imported texture assets to their corresponding Texture Parameter slots on the Material Instance Constant.
 */
void UHoudiniVatImporter::AssignTexturesToMaterialInstance()
{
    if (!MaterialInstance.IsValid() || !VatProperties)
    {
        return;
    }

    const FString AssetPath = VatProperties->VatAssetPath.Path;

    for (const FFilePath& TextureFilePath : VatProperties->VatTextureFilePath)
    {
        const FString BaseNameFromFile = FPaths::GetBaseFilename(TextureFilePath.FilePath);

        const FString TextureObjPath = FString::Printf(TEXT("%s/%s.%s"), *AssetPath, *BaseNameFromFile, *BaseNameFromFile);

        if (UTexture2D* ImportedTexture = LoadObject<UTexture2D>(nullptr, *TextureObjPath))
        {
            const FName ParameterName = GetTextureParameterName(BaseNameFromFile);
            if (!ParameterName.IsNone())
            {
                MaterialInstance->SetTextureParameterValueEditorOnly(ParameterName, ImportedTexture);

                UTexture* CurrentTexture = nullptr;
                FHashedMaterialParameterInfo ParamInfo(ParameterName);

                if (MaterialInstance->GetTextureParameterValue(ParamInfo, CurrentTexture))
                {
                    if (CurrentTexture == ImportedTexture)
                    {
                        UE_LOG(LogSidefxLabsEditor, Log, TEXT("Set %s -> %s"), *ParameterName.ToString(), *ImportedTexture->GetPathName());
                    }
                    else
                    {
                        UE_LOG(LogSidefxLabsEditor, Warning, TEXT("Failed to set texture parameter for: %s"), *ParameterName.ToString());
                    }
                }
                else
                {
                    UE_LOG(LogSidefxLabsEditor, Warning, TEXT("Failed to get texture parameter value for: %s"), *ParameterName.ToString());
                }
            }
            else
            {
                UE_LOG(LogSidefxLabsEditor, Warning, TEXT("Failed to load texture asset by object path: %s"), *TextureObjPath);
            }
        }
    }
}

/**
 * Loads and parses a JSON data file and uses the boundary values to set parameters on the VAT Material Instance.
 *
 * @param JsonPath The full path to the JSON file.
 */
void UHoudiniVatImporter::LoadLegacyDataFromJson(const FString& JsonPath)
{
	if (!MaterialInstance.IsValid())
	{
		return;
	}

	FString JsonString;
	if (!FFileHelper::LoadFileToString(JsonString, *JsonPath))
	{
		UE_LOG(LogSidefxLabsEditor, Error, TEXT("Failed to load JSON file: %s"), *JsonPath);
		return;
	}

	struct FBoundData
	{
		FString Key;
		float Value;
		FName ParameterName;
	};

	TArray<FBoundData> BoundValues = {
		{ TEXT("\"Bound Max X\": "), 0.0f, FName("Bound Max X") },
		{ TEXT("\"Bound Max Y\": "), 0.0f, FName("Bound Max Y") },
		{ TEXT("\"Bound Max Z\": "), 0.0f, FName("Bound Max Z") },
		{ TEXT("\"Bound Min X\": "), 0.0f, FName("Bound Min X") },
		{ TEXT("\"Bound Min Y\": "), 0.0f, FName("Bound Min Y") },
		{ TEXT("\"Bound Min Z\": "), 0.0f, FName("Bound Min Z") }
	};

	for (FBoundData& BoundData : BoundValues)
	{
		const int32 KeyIndex = JsonString.Find(BoundData.Key);
		if (KeyIndex == INDEX_NONE)
		{
			UE_LOG(LogSidefxLabsEditor, Warning, TEXT("Field '%s' not found in the JSON file."), *BoundData.Key);
			continue;
		}

		const int32 ValueStartIndex = KeyIndex + BoundData.Key.Len();
		int32 ValueEndIndex = JsonString.Find(TEXT(","), ESearchCase::IgnoreCase, ESearchDir::FromStart, ValueStartIndex);
		
		if (ValueEndIndex == INDEX_NONE)
		{
			ValueEndIndex = JsonString.Find(TEXT("}"), ESearchCase::IgnoreCase, ESearchDir::FromStart, ValueStartIndex);
		}

		if (ValueEndIndex != INDEX_NONE)
		{
			const FString ValueString = JsonString.Mid(ValueStartIndex, ValueEndIndex - ValueStartIndex).TrimStartAndEnd();
			BoundData.Value = FCString::Atof(*ValueString);
			MaterialInstance->SetScalarParameterValueEditorOnly(BoundData.ParameterName, BoundData.Value);
		}
		else
		{
			UE_LOG(LogSidefxLabsEditor, Warning, TEXT("Could not find the end of the value for '%s'."), *BoundData.Key);
		}
	}
}

/**
 * Creates a Material Instance Constant (MIC) from the created base material.
 * Also sets basic scalar/static switch parameters and assigns imported textures.
 */
void UHoudiniVatImporter::CreateVatMaterialInstance()
{
    if (!VatProperties)
    {
        UE_LOG(LogSidefxLabsEditor, Error, TEXT("UHoudiniVatImporter::CreateVatMaterialInstance: VatProperties is null"));
        return;
    }

    if (CreatedMaterialName.IsEmpty())
    {
        UE_LOG(LogSidefxLabsEditor, Error, TEXT("UHoudiniVatImporter::CreateVatMaterialInstance: CreatedMaterialName is empty"));
        return;
    }

    const FString AssetPath = VatProperties->VatAssetPath.Path;

    if (!AssetPath.StartsWith(TEXT("/Game")) || !FPackageName::IsValidLongPackageName(AssetPath))
    {
        UE_LOG(LogSidefxLabsEditor, Error, TEXT("UHoudiniVatImporter::CreateVatMaterialInstance: Invalid asset path: %s"), *AssetPath);
        return;
    }

    ensureMsgf(IsInGameThread(), TEXT("CreateVatMaterialInstance must run on the game thread"));

    const FString MaterialObjPath = FString::Printf(TEXT("%s/%s.%s"), *AssetPath, *CreatedMaterialName, *CreatedMaterialName);
	
    UMaterial* CreatedMaterial = LoadObject<UMaterial>(nullptr, *MaterialObjPath);
    if (!CreatedMaterial)
    {
        UE_LOG(LogSidefxLabsEditor, Error, TEXT("UHoudiniVatImporter::CreateVatMaterialInstance: Failed to load Material: %s"), *MaterialObjPath);
        return;
    }

    FString MaterialInstanceName = CreatedMaterialName;

    if (MaterialInstanceName.StartsWith(TEXT("M_")))
    {
        MaterialInstanceName = TEXT("MI_") + MaterialInstanceName.RightChop(2);
    }
    else
    {
        MaterialInstanceName = TEXT("MI_") + MaterialInstanceName;
    }

    MaterialInstanceName = ObjectTools::SanitizeObjectName(MaterialInstanceName);
    
    const FString MaterialInstancePackageName = FPaths::Combine(AssetPath, MaterialInstanceName);

	UPackage* MaterialInstancePackage = CreatePackage(*MaterialInstancePackageName);
    if (!MaterialInstancePackage)
    {
        UE_LOG(LogSidefxLabsEditor, Error, TEXT("UHoudiniVatImporter::CreateVatMaterialInstance: Failed to create Material Instance Package: %s"), *MaterialInstancePackageName);
        return;
    }

	UMaterialInstanceConstantFactoryNew* MaterialInstanceFactory = NewObject<UMaterialInstanceConstantFactoryNew>(GetTransientPackage());
    if (!MaterialInstanceFactory)
    {
        UE_LOG(LogSidefxLabsEditor, Error, TEXT("UHoudiniVatImporter::CreateVatMaterialInstance: Failed to create Material Instance Factory"));
        return;
    }

	MaterialInstance = Cast<UMaterialInstanceConstant>(MaterialInstanceFactory->FactoryCreateNew(
		UMaterialInstanceConstant::StaticClass(), 
		MaterialInstancePackage, 
		FName(*MaterialInstanceName), 
		RF_Public | RF_Standalone, 
		nullptr, 
		GWarn
	));
	
	if (!MaterialInstance.IsValid())
	{
		UE_LOG(LogSidefxLabsEditor, Error, TEXT("Failed to create Material Instance: %s"), *MaterialInstanceName);
		return;
	}

	MaterialInstance->SetParentEditorOnly(CreatedMaterial);

	SetBasicMaterialInstanceParameters();

	if (VatProperties->bVatSupportLegacyParametersAndInstancing)
	{
		FullLegacyDataPath = FPaths::ConvertRelativePathToFull(VatProperties->VatLegacyDataFilePath.FilePath);
		LoadLegacyDataFromJson(FullLegacyDataPath);
	}

	AssignTexturesToMaterialInstance();

	FSidefxLabsEditorUtils::MarkPackageDirtyAndRegister(MaterialInstance.Get());
	FSidefxLabsEditorUtils::SavePackages({MaterialInstance->GetPackage()});
}

/**
 * Configures the properties of the AHoudiniVatActor's default object within the created Blueprint.
 */
void UHoudiniVatImporter::ConfigureBlueprintDefaultActor()
{
	if (!Blueprint.Get() || !Blueprint->GeneratedClass)
	{
		return;
	}

	AHoudiniVatActor* DefaultActor = Cast<AHoudiniVatActor>(Blueprint->GeneratedClass->GetDefaultObject());
	if (!DefaultActor || !DefaultActor->Vat_StaticMesh)
	{
		return;
	}

	DefaultActor->Vat_StaticMesh->SetStaticMesh(StaticMesh.Get());
	
	DefaultActor->Vat_MaterialInstances.Empty();
	const int32 NumMaterials = DefaultActor->Vat_StaticMesh->GetNumMaterials();
	
	for (int32 SlotIndex = 0; SlotIndex < NumMaterials; ++SlotIndex)
	{
		DefaultActor->Vat_StaticMesh->SetMaterial(SlotIndex, MaterialInstance.Get());
		DefaultActor->Vat_MaterialInstances.Add(MaterialInstance.Get());
	}
}

/**
 * Creates a Blueprint class that inherits from AHoudiniVatActor and configures its default properties.
 */
void UHoudiniVatImporter::CreateVatBlueprint()
{
    if (!VatProperties)
    {
        UE_LOG(LogSidefxLabsEditor, Error, TEXT("CreateVatBlueprint: VatProperties is null"));
        return;
    }
    
	if (CreatedMaterialName.IsEmpty())
	{
		UE_LOG(LogSidefxLabsEditor, Error, TEXT("UHoudiniVatImporter::CreateVatBlueprint: CreatedMaterialName is empty"));
		return;
	}

    const FString AssetPath = VatProperties->VatAssetPath.Path;

    if (!AssetPath.StartsWith(TEXT("/Game")) || !FPackageName::IsValidLongPackageName(AssetPath))
    {
        UE_LOG(LogSidefxLabsEditor, Error, TEXT("UHoudiniVatImporter::CreateVatBlueprint: Invalid asset path: %s"), *AssetPath);
        return;
    }

    ensureMsgf(IsInGameThread(), TEXT("CreateVatBlueprint must run on the game thread"));

    const FString MaterialObjPath = FString::Printf(TEXT("%s/%s.%s"), *AssetPath, *CreatedMaterialName, *CreatedMaterialName);

    if (!LoadObject<UMaterial>(nullptr, *MaterialObjPath))
    {
        UE_LOG(LogSidefxLabsEditor, Error, TEXT("UHoudiniVatImporter::CreateVatBlueprint: Failed to load Material: %s"), *MaterialObjPath);
        return;
    }

    FString DesiredBlueprintName = CreatedMaterialName;

    if (DesiredBlueprintName.StartsWith(TEXT("M_")))
    {
        DesiredBlueprintName = TEXT("BP_") + DesiredBlueprintName.RightChop(2);
    }
    else
    {
        DesiredBlueprintName = TEXT("BP_") + DesiredBlueprintName;
    }

    DesiredBlueprintName = ObjectTools::SanitizeObjectName(DesiredBlueprintName);
	
	FString UniqueBlueprintName;
	FString UniqueBlueprintPackageName;
    const FString BasePackagePath = FPaths::Combine(AssetPath, DesiredBlueprintName);

    FAssetToolsModule& AssetToolsModule = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools");
    AssetToolsModule.Get().CreateUniqueAssetName(BasePackagePath, TEXT(""), UniqueBlueprintPackageName, UniqueBlueprintName);

    UPackage* BlueprintPackage = CreatePackage(*UniqueBlueprintPackageName);
    if (!BlueprintPackage)
    {
        UE_LOG(LogSidefxLabsEditor, Error, TEXT("UHoudiniVatImporter::CreateVatBlueprint: Failed to create Blueprint Package: %s"), *UniqueBlueprintPackageName);
        return;
    }

    UBlueprintFactory* BlueprintFactory = NewObject<UBlueprintFactory>(GetTransientPackage());
    if (!BlueprintFactory)
    {
        UE_LOG(LogSidefxLabsEditor, Error, TEXT("UHoudiniVatImporter::CreateVatBlueprint: Failed to create UBlueprintFactory"));
        return;
    }

    BlueprintFactory->ParentClass = AHoudiniVatActor::StaticClass();
    BlueprintFactory->bSkipClassPicker = true;

    UBlueprint* NewBP = Cast<UBlueprint>(BlueprintFactory->FactoryCreateNew(
        UBlueprint::StaticClass(),
        BlueprintPackage,
        FName(*UniqueBlueprintName),
        RF_Public | RF_Standalone,
        nullptr,
        GWarn));

    if (!NewBP)
    {
        UE_LOG(LogSidefxLabsEditor, Error, TEXT("UHoudiniVatImporter::CreateVatBlueprint: FactoryCreateNew failed for %s"), *UniqueBlueprintName);
        return;
    }

    Blueprint = NewBP;

    ConfigureBlueprintDefaultActor();

    FSidefxLabsEditorUtils::MarkPackageDirtyAndRegister(NewBP);
    FKismetEditorUtilities::CompileBlueprint(NewBP);
    FSidefxLabsEditorUtils::SavePackages({ NewBP->GetPackage() });
}

/**
 * Forces the base Material to recompile for rendering.
 */
void UHoudiniVatImporter::RecompileVatMaterial()
{
	if (!Material.IsValid())
	{
		UE_LOG(LogSidefxLabsEditor, Warning, TEXT("UHoudiniVatImporter::RecompileVatMaterial: Material is invalid"));
		return;
	}
	
	UMaterial* Mat = Material.Get(); 
	if (!Mat)
	{
		UE_LOG(LogSidefxLabsEditor, Warning, TEXT("UHoudiniVatImporter::RecompileVatMaterial: Material object is invalid"));
		return;
	}

    ensureMsgf(IsInGameThread(), TEXT("RecompileVatMaterial must run on the game thread"));

    UMaterialEditingLibrary::RecompileMaterial(Mat);
    FSidefxLabsEditorUtils::MarkPackageDirtyAndRegister(Mat);

    if (UPackage* Pkg = Mat->GetPackage())
    {
        FSidefxLabsEditorUtils::SavePackages({ Pkg });
    }
    else
    {
        UE_LOG(LogSidefxLabsEditor, Warning, TEXT("UHoudiniVatImporter::RecompileVatMaterial: Material Package is invalid"));
    }
}
