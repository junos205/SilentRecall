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
#include "Modules/ModuleInterface.h"

class FMenuManager;
class FPropertyCustomizationManager;

/**
 * Editor module for SideFX Labs plugin.
 * Manages menu registration, property customizations, and VAT creation workflow.
 */
class SIDEFXLABSEDITOR_API FSidefxLabsEditorModule final : public IModuleInterface
{
public:
	/** IModuleInterface implementation. */
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

	/** Creates and opens VAT creation window. */
	static void CreateNewVat();

private:
	/** Initializes the plugin menu system. */
	void InitializeMenu();
	
	/** Initializes VAT property customizations. */
	void InitializePropertyCustomization();
	
	/** Registers tab spawners for editor windows. */
	void RegisterTabSpawners();
	
	/** Cleans up resources on module shutdown. */
	void Cleanup();

	/** Manages menu registration and configuration. */
	TUniquePtr<FMenuManager> MenuManager;
	
	/** Manages property detail panel customizations. */
	TUniquePtr<FPropertyCustomizationManager> PropertyCustomizationManager;
};
