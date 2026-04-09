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
#include "HoudiniCreateNewVatWindowParameters.h"
#include "HoudiniVatImporter.generated.h"

class UMaterial;
class UMaterialExpression;
class UMaterialFunction;
class UMaterialInstanceConstant;
class UStaticMesh;
class UTexture2D;
class UBlueprint;

/**
 * Handles the import and setup of Vertex Animation Texture (VAT) assets.
 * Manages FBX import, texture import, material creation, and blueprint generation.
 */
UCLASS()
class SIDEFXLABSEDITOR_API UHoudiniVatImporter : public UObject
{
	GENERATED_BODY()

public:
	UHoudiniVatImporter();

	/** Sets the properties to use for VAT import. */
	UFUNCTION()
	void SetProperties(UCreateNewVatProperties* InProperties);

    UMaterialFunction* ResolveMaterialFunctionFor(EVatType Type) const;

	/** Imports all files. */
	UFUNCTION()
	void ImportFiles();

	/** Creates the VAT material. */
	UFUNCTION()
	void CreateVatMaterial();

	/** Creates a material instance from the VAT material. */
	UFUNCTION()
	void CreateVatMaterialInstance();

	/** Creates a blueprint actor for the VAT. */
	UFUNCTION()
	void CreateVatBlueprint();

	/** Recompiles and saves the VAT material. */
	void RecompileVatMaterial();

public:
	/** The material expression for the VAT material function. */
	TWeakObjectPtr<UMaterialExpression> VatMaterialExp;

	/** The created VAT material. */
	TWeakObjectPtr<UMaterial> Material;

	/** The created material instance. */
	TWeakObjectPtr<UMaterialInstanceConstant> MaterialInstance;

	/** The imported static mesh. */
	TWeakObjectPtr<UStaticMesh> StaticMesh;

	/** The created blueprint. */
	TWeakObjectPtr<UBlueprint> Blueprint;

	/** Whether the import was canceled by the user. */
	bool bCanceled;

private:
	/** Configures specific mesh settings for fluid VAT (VatType3). */
	void ConfigureFluidVatMeshSettings();

	/** Imports an FBX file and returns it as a Static Mesh. */
    UStaticMesh* ImportFbx(const FString& FbxPath, const FString& AssetPath);

	/** Imports a texture file as a UTexture2D asset.*/
    UTexture2D* ImportTexture(const FString& TexturePath, const FString& AssetPath);

	/** Sets texture parameters based on file extension. */
	static void SetTextureParameters(TArray<UTexture2D*> Textures);

	/** Connects material outputs based on VAT type. */
	void ConnectMaterialOutputs();

	/** Connects outputs for soft-body deformation (VAT Type 1). */
	void ConnectSoftBodyOutputs();

	/** Connects outputs for rigid-body dynamics (VAT Type 2). */
	void ConnectRigidBodyOutputs();

	/** Connects outputs for dynamic remeshing (VAT Type 3). */
	void ConnectFluidOutputs();

	/** Connects outputs for particle sprites (VAT Type 4). */
	void ConnectSpriteOutputs();

	/** Helper to safely connect a material output. */
	void ConnectOutput(int32 OutputIndex, const FString& OutputName, void (UMaterial::*Connector)(int32, UMaterialExpression*));

	/** Sets basic material instance parameters. */
	void SetBasicMaterialInstanceParameters();

	/** Loads and applies legacy JSON data to material instance. */
	void LoadLegacyDataFromJson(const FString& JsonPath);

	/** Assigns imported textures to material instance. */
	void AssignTexturesToMaterialInstance();

	/** Gets the texture parameter name based on filename. */
	static FName GetTextureParameterName(const FString& FileName);

	/** Configures the blueprint's default actor properties. */
	void ConfigureBlueprintDefaultActor();

private:
	/** Properties for the VAT import. */
	UPROPERTY()
	TObjectPtr<UCreateNewVatProperties> VatProperties;

	/** The material function to use for the VAT. */
	UPROPERTY()
	TObjectPtr<UMaterialFunction> HoudiniVatMaterialFunction;

	/** Name of the created material. */
	FString CreatedMaterialName;

	/** Full path to the imported FBX file. */
	FString FullFbxPath;

	/** Full path to the legacy data JSON file. */
	FString FullLegacyDataPath;
};
