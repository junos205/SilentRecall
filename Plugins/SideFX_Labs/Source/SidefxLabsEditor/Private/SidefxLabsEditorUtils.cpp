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

#include "SidefxLabsEditorUtils.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "FileHelpers.h"

/**
 * Saves the specified packages to disk.
 *
 * @param Packages The array of UPackage objects to save.
 */
void FSidefxLabsEditorUtils::SavePackages(const TArray<UPackage*>& Packages)
{
	if (Packages.Num() > 0)
	{
		UEditorLoadingAndSavingUtils::SavePackages(Packages, true);
	}
}

/**
 * Marks an object's package as dirty and registers the asset with the Asset Registry.
 *
 * @param Object The UObject whose package should be marked dirty.
 */
void FSidefxLabsEditorUtils::MarkPackageDirtyAndRegister(UObject* Object)
{
	if (!Object)
	{
		return;
	}

	Object->MarkPackageDirty();
	FAssetRegistryModule::AssetCreated(Object);
}
