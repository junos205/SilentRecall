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
#include "HoudiniCreateNewVatWindowParameters.generated.h"

/**
 * Defines the type of Vertex Animation Texture (VAT) to create.
 * Each type corresponds to a different animation type exported from Houdini.
 */
UENUM(BlueprintType)
enum class EVatType : uint8
{
	VatType1 UMETA(DisplayName = "Soft-Body Deformation (Soft)"),
	VatType2 UMETA(DisplayName = "Rigid-Body Dynamics (Rigid)"),
	VatType3 UMETA(DisplayName = "Dynamic Remeshing (Fluid)"),
	VatType4 UMETA(DisplayName = "Particle Sprites (Sprite)")
};

/**
 * Properties for creating a new VAT asset.
 * Contains all parameters needed for importing and configuring VAT materials.
 */
UCLASS()
class SIDEFXLABSEDITOR_API UCreateNewVatProperties : public UObject
{
	GENERATED_BODY()

public:
	UCreateNewVatProperties();

	/** The file path to the exported FBX file from the Labs Vertex Animation Textures ROP. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Import", 
		DisplayName = "FBX File Path", 
		meta = (FilePathFilter = "FBX (*.fbx)|*.fbx", 
		ToolTip = "The file path to the exported FBX file from the Labs Vertex Animation Textures ROP."))
	FFilePath VatFbxFilePath;

	/** The file paths to the exported texture files from the Labs Vertex Animation Textures ROP. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Import", 
		DisplayName = "Texture File Path", 
		meta = (FilePathFilter = "Textures (*.exr;*.png)|*.exr;*.png", 
		ToolTip = "The file path to the exported texture files from the Labs Vertex Animation Textures ROP."))
	TArray<FFilePath> VatTextureFilePath;

	/** The Unreal asset path where files will be created and imported. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Import", 
		DisplayName = "Asset Path", 
		meta = (ContentDir, 
		ToolTip = "The Unreal asset path where files will be created and imported."))
	FDirectoryPath VatAssetPath;

	/** Turn this on to create a blueprint that allows for control of VAT functionality. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Import", 
		DisplayName = "Create VAT Blueprint", 
		meta = (ToolTip = "Turn this on to create a blueprint that allows for control of VAT functionality."))
	bool bCreateVatBlueprint;

	/** The name of the created VAT material. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Parameters", 
		DisplayName = "Material Name", 
		meta = (ToolTip = "The name of the created VAT material."))
	FString VatMaterialName;
	
	/** The VAT type depends on what kind of animation you have exported from Houdini. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Parameters", 
		DisplayName = "VAT Type", 
		meta = (ToolTip = "The VAT type depends on what kind of animation you have exported from Houdini. This should match the selected mode in the Labs Vertex Animation Textures ROP."))
	EVatType VatType;

	/** The FPS of the Houdini HIP file when exporting the animation. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Parameters", 
		DisplayName = "Houdini FPS", 
		meta = (ToolTip = "The FPS of the Houdini HIP file when exporting the animation."))
	int32 VatFps;

	/** Interpolates interframe data when the animation frame is a fractional number. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Parameters", 
		DisplayName = "Interframe Interpolation", 
		meta = (ToolTip = "Interpolates interframe data when the animation frame is a fractional number. This results in smooth visuals even when you slow down the animation or when the frame rate is unstable."))
	bool bVatInterpolate;

	/** Determines if VAT animation will loop continuously or stop after a specified number of seconds. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Parameters", 
		DisplayName = "Loop Animation", 
		meta = (ToolTip = "Determines if VAT animation will loop continuously or stop after a specified number of seconds. If disabled, make sure to set the Animation Length parameter accordingly."))
	bool bVatLoopAnimation;

	/** The amount of time, in seconds, the VAT animation will play before stopping. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Parameters", 
		DisplayName = "Animation Length", 
		meta = (EditCondition = "!bVatLoopAnimation", EditConditionHides, 
		ToolTip = "The amount of time, in seconds, the VAT animation will play before stopping."))
	float VatAnimationLength;

	/** If you want to use this Material Instance with ISM/HISM or mesh particles, turn this on. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Parameters", 
		DisplayName = "Support Legacy Parameters and Instancing", 
		meta = (ToolTip = "If you want to use this Material Instance with ISM/HISM or mesh particles, turn this on and turn on Support Real-Time Instancing in Houdini. If you simply want to use the legacy parameters or modify the object's bounds, turn this on and only turn on Allow Exporting Real-Time Data JSON File (Legacy) in Houdini without turning on Support Real-Time Instancing. Legacy parameters are a list of numerical values exported through a JSON file. They contain the embedded data just like the actual mesh, but when Support Legacy Parameters and Instancing is turned on, the shader will read the bounds and the embedded data from the legacy parameters instead of the actual mesh. Using legacy parameters is less convenient, but it does produce more accurate results if the animation spans a huge area; it also leads to lower instruction counts."))
	bool bVatSupportLegacyParametersAndInstancing;

	/** The file path to the exported JSON file from the Labs Vertex Animation Textures ROP. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Parameters", 
		DisplayName = "Data File Path", 
		meta = (FilePathFilter = "JSON (*.json)|*.json", 
		EditCondition = "bVatSupportLegacyParametersAndInstancing", EditConditionHides, 
		ToolTip = "The file path to the exported JSON file from the Labs Vertex Animation Textures ROP."))
	FFilePath VatLegacyDataFilePath;
};
